#!/usr/bin/env python3
"""严格联结正式配置与隔离变体，生成预登记的最小消融报告。"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[2]
TIMEOUT = 3600.0
FORMAL_RECORDS = 28_434
MATRIX_SHA256 = "12dbac04c42bd14619b11332623b64dba8c6db3356062058967bef91047142ab"
ABHSS_SHA256 = "793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced"
PRUNED_SHA256 = "4c1d3599f03da6073d368a6a83fcbd31ea0a625f9ba90892b22b0b239eb42bf2"
NO_ENDPOINT_SHA256 = "76e3c2a8b7ef74be0e353f12dc2334638bd38ad3fcc73467f88241fb377540d9"
ABLATION_MATRIX_SHA256 = "56409bed7e6cafae5b29744287e839741398d79de8424be0223fd6c75bd8239f"
ABLATION_RUN_ID = "final_3600s_campaign_793d4e_20260828"
ABLATION_WORKER_METADATA_SHA256 = {"cpu4": "ca53af82e05834ee79806f4a8c5c131526262d7bd2a874403c40086946b3548f", "cpu5": "622e98e44772af9a62544a2a7f5e5791c2d536feb1d932991feac046fe5b3aad"}
FORMAL_METHODS = ("abhss_base", "abhss_enhanced", "pruneddp_safe")
EXPECTED_SOURCE_COUNTS = {"historical_formal": 25_714, "source_campaign_workers": 2_645, "backfill_workers": 75}
EXPECTED_SUITE_METHOD_COUNTS = {
    f"{suite}|{method}": count
    for suite, count in (("P1_monogstplus_published", 1_118), ("P1_gpu4gst_published", 7_200), ("P2_cross_g", 660), ("S2_controlled_gf", 500))
    for method in FORMAL_METHODS
}
METHODS = (
    "abhss_base",
    "abhss_directed_cut_only",
    "abhss_enhanced",
    "abhss_no_endpoint_base",
    "abhss_no_endpoint_enhanced",
)
LABELS = {
    "abhss_base": "Base",
    "abhss_directed_cut_only": "DirectedCutOnly",
    "abhss_enhanced": "Enhanced",
    "abhss_no_endpoint_base": "Base without endpoint-floor",
    "abhss_no_endpoint_enhanced": "Enhanced without endpoint-floor",
}
COMPARISONS = (
    ("directed_cut", "abhss_base", "abhss_directed_cut_only"),
    ("adjoint", "abhss_directed_cut_only", "abhss_enhanced"),
    ("endpoint_floor_base", "abhss_no_endpoint_base", "abhss_base"),
    ("endpoint_floor_enhanced", "abhss_no_endpoint_enhanced", "abhss_enhanced"),
)


def resolve(path: Path) -> Path:
    """把命令行相对路径解释为仓库相对路径。"""

    return path if path.is_absolute() else ROOT / path


def sha256_file(path: Path) -> str:
    """流式计算大文件 SHA-256。"""

    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def source_files(path: Path) -> list[Path]:
    """枚举规范 JSONL、单 worker 或多 worker 的原子记录。"""

    if path.is_file():
        return [path]
    if (path / "records").is_dir():
        return sorted((path / "records").glob("*.json"))
    files = sorted(path.glob("cpu*/records/*.json"))
    if not files:
        raise ValueError(f"no records under {path}")
    return files


def load_records(paths: list[Path], label: str) -> tuple[dict[str, dict[str, Any]], list[dict[str, Any]]]:
    """读取记录并拒绝跨 source 的重复 task key。"""

    indexed: dict[str, dict[str, Any]] = {}
    rows: list[dict[str, Any]] = []
    for raw_path in paths:
        path = resolve(raw_path)
        for file in source_files(path):
            if file.suffix == ".jsonl":
                current = [json.loads(line) for line in file.read_text(encoding="utf-8").splitlines() if line.strip()]
            else:
                current = [json.loads(file.read_text(encoding="utf-8"))]
            for row in current:
                key = str(row.get("task_key", ""))
                if not key or key in indexed:
                    raise RuntimeError(f"{label} has missing/duplicate task key: {key}")
                indexed[key] = row
                rows.append(row)
    return indexed, rows


def validate_ablation_metadata(paths: list[Path]) -> list[dict[str, Any]]:
    """锁定消融 worker 的矩阵、二进制和逐查询运行合同。"""

    expected_binaries = {
        "abhss_base": ABHSS_SHA256,
        "abhss_directed_cut_only": ABHSS_SHA256,
        "abhss_enhanced": ABHSS_SHA256,
        "abhss_no_endpoint_base": NO_ENDPOINT_SHA256,
        "abhss_no_endpoint_enhanced": NO_ENDPOINT_SHA256,
    }
    metadata_paths = []
    for raw_path in paths:
        path = resolve(raw_path)
        current = [path / "run_metadata.json"] if (path / "records").is_dir() else sorted(path.glob("cpu*/run_metadata.json"))
        if not current or any(not item.is_file() for item in current):
            raise RuntimeError(f"ablation worker metadata is missing under {path}")
        metadata_paths.extend(current)
    if len(set(metadata_paths)) != len(metadata_paths):
        raise RuntimeError("duplicate ablation worker metadata input")
    if Counter(path.parent.name for path in metadata_paths) != Counter(ABLATION_WORKER_METADATA_SHA256.keys()):
        raise RuntimeError("ablation inputs must resolve to the frozen cpu4 and cpu5 worker metadata")
    validated = []
    for path in metadata_paths:
        metadata = json.loads(path.read_text(encoding="utf-8"))
        metadata_sha = sha256_file(path)
        matrix_path = resolve(Path(str(metadata.get("config_path", ""))))
        checks = {
            "metadata_sha256": metadata_sha == ABLATION_WORKER_METADATA_SHA256[path.parent.name],
            "run_id": metadata.get("run_id") == ABLATION_RUN_ID,
            "config_sha256": metadata.get("config_sha256") == ABLATION_MATRIX_SHA256,
            "config_file_sha256": matrix_path.is_file() and sha256_file(matrix_path) == ABLATION_MATRIX_SHA256,
            "timeout_seconds": float(metadata.get("timeout_seconds", -1.0)) == TIMEOUT,
            "stop_on_timeout": metadata.get("initial_invocation_stop_on_timeout") is False,
            "probe_diagnostics": metadata.get("initial_invocation_probe_diagnostics") is False,
            "binary_sha256": all(metadata.get("binary_sha256", {}).get(method) == expected for method, expected in expected_binaries.items()),
        }
        failed = sorted(name for name, passed in checks.items() if not passed)
        if failed:
            raise RuntimeError(f"ablation worker metadata failed checks {failed}: {path}")
        validated.append({"path": str(path.relative_to(ROOT)), "sha256": metadata_sha})
    return validated


def validate_canonical_formal_input(paths: list[Path], rows: list[dict[str, Any]], allow_incomplete: bool) -> dict[str, Any]:
    """要求论文报告读取 materializer 产生且哈希闭合的唯一规范账本。"""

    if allow_incomplete:
        return {"mode": "incomplete-test-override", "records": len(rows), "manifest_validated": False}
    if len(paths) != 1:
        raise RuntimeError("paper ablation reporting requires exactly one canonical formal ledger")
    ledger = resolve(paths[0])
    if not ledger.is_file() or ledger.name != "records.jsonl":
        raise RuntimeError("paper ablation reporting requires a canonical records.jsonl file")
    manifest_path = ledger.parent / "manifest.json"
    if not manifest_path.is_file():
        raise RuntimeError(f"canonical manifest is missing: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    strict_binary_identity = {"abhss": ABHSS_SHA256, "pruneddp": PRUNED_SHA256}
    generic_binary_identity = {"abhss_base": ABHSS_SHA256, "abhss_enhanced": ABHSS_SHA256, "pruneddp_safe": PRUNED_SHA256}
    status_counts = dict(Counter(str(row.get("status")) for row in rows))
    suite_method_counts = dict(Counter(f"{row.get('suite')}|{row.get('method')}" for row in rows))
    checks = {
        "records": int(manifest.get("records", -1)) == FORMAL_RECORDS == len(rows),
        "record_sha256": manifest.get("record_sha256") == sha256_file(ledger),
        "paper_matrix_sha256": manifest.get("paper_matrix_sha256") == MATRIX_SHA256,
        "binary_sha256": manifest.get("binary_sha256") in (strict_binary_identity, generic_binary_identity),
        "source_counts": manifest.get("source_counts") == EXPECTED_SOURCE_COUNTS,
        "suite_method_counts": manifest.get("suite_method_counts") == EXPECTED_SUITE_METHOD_COUNTS == suite_method_counts,
        "status_counts": manifest.get("status_counts") == status_counts and set(status_counts) <= {"ok", "timeout"},
        "record_run_id": all(row.get("run_id") == manifest.get("run_id") for row in rows),
        "optional_manifest_timeout": manifest.get("timeout_seconds_per_query") is None or float(manifest["timeout_seconds_per_query"]) == TIMEOUT,
        "objective_mismatches": int(manifest.get("objective_mismatches", -1)) == 0,
        "feasibility_mismatches": int(manifest.get("feasibility_mismatches", -1)) == 0,
        "completed_records_missing_state_count": int(manifest.get("completed_records_missing_state_count", -1)) == 0,
        "p2_s2_cells_with_non_ten_method_records": int(manifest.get("p2_s2_cells_with_non_ten_method_records", -1)) == 0,
    }
    failed = sorted(name for name, passed in checks.items() if not passed)
    if failed:
        raise RuntimeError(f"canonical formal ledger failed manifest checks: {failed}")
    for row in rows:
        method = str(row.get("method"))
        if method not in FORMAL_METHODS:
            raise RuntimeError(f"canonical ledger has an unexpected method: {row.get('task_key')}")
        validate_outcome(row, method, str(row.get("task_key")))
    return {
        "mode": "canonical-materialized-ledger",
        "records": len(rows),
        "manifest": str(manifest_path.relative_to(ROOT)),
        "manifest_sha256": sha256_file(manifest_path),
        "record_sha256": manifest["record_sha256"],
        "manifest_validated": True,
    }


def percentile(values: list[float], probability: float) -> float | None:
    """用线性插值计算确定性分位数。"""

    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def geomean(values: Iterable[float]) -> float | None:
    """只对严格正的成对比值计算几何平均。"""

    positive = [float(value) for value in values if float(value) > 0.0]
    return math.exp(statistics.fmean(math.log(value) for value in positive)) if positive else None


def validate_outcome(row: dict[str, Any], expected_method: str, key: str) -> None:
    """验证完成与真实 timeout 的互斥字段合同。"""

    if row.get("method") != expected_method or row.get("status") not in {"ok", "timeout"} or row.get("exact_claim") is not True:
        raise RuntimeError(f"invalid method/status/exactness: {key}")
    if float(row.get("timeout_seconds", -1.0)) != TIMEOUT:
        raise RuntimeError(f"wrong timeout: {key}")
    if row["status"] == "timeout":
        censored = ("solver_seconds", "weight", "solution_status", "query_memory_peak_mib", "mask_vertex_states")
        if float(row.get("watchdog_wall_seconds") or 0.0) < TIMEOUT or any(row.get(field) is not None for field in censored):
            raise RuntimeError(f"malformed real timeout: {key}")
        return
    seconds = float(row.get("solver_seconds"))
    memory = float(row.get("query_memory_peak_mib"))
    raw_states = row.get("mask_vertex_states")
    if isinstance(raw_states, bool) or not isinstance(raw_states, int):
        raise RuntimeError(f"mask_vertex_states is not an integer: {key}")
    states = raw_states
    weight = float(row.get("weight"))
    solution = row.get("solution_status")
    if not math.isfinite(seconds) or not 0.0 <= seconds <= TIMEOUT or not math.isfinite(memory) or memory < 0.0 or states < 0:
        raise RuntimeError(f"malformed completed resource fields: {key}")
    if solution not in {"feasible", "infeasible"} or not math.isfinite(weight):
        raise RuntimeError(f"malformed completed solution: {key}")
    if (solution == "feasible" and weight < 0.0) or (solution == "infeasible" and weight != -1.0):
        raise RuntimeError(f"objective/status mismatch: {key}")


def expected_keys(plan: dict[str, Any]) -> tuple[dict[str, tuple[str, int, int, str]], dict[str, tuple[str, int, int, str]], list[tuple[str, int, int]]]:
    """从消融计划机械展开 45 个查询和五个配置。"""

    selection = plan["selection"]
    identities = [(str(dataset), int(g), int(query)) for dataset in selection["datasets"] for g in selection["g"] for query in selection["panel_query_indices"]]
    formal = {}
    ablation = {}
    for dataset, g, query in identities:
        case = f"P2_cross_g__{dataset}_{g}"
        for method in ("abhss_base", "abhss_enhanced"):
            formal[f"P2_cross_g|{case}|{method}|q{query}"] = (dataset, g, query, method)
        for method in ("abhss_directed_cut_only", "abhss_no_endpoint_base", "abhss_no_endpoint_enhanced"):
            ablation[f"P2_cross_g|{case}|{method}|q{query}"] = (dataset, g, query, method)
    if len(identities) != 45 or len(formal) != 90 or len(ablation) != 135:
        raise RuntimeError("ablation plan no longer expands to 45 queries and 225 records")
    return formal, ablation, identities


def canonical_rows(formal: dict[str, dict[str, Any]], ablation: dict[str, dict[str, Any]], plan: dict[str, Any]) -> list[dict[str, Any]]:
    """选择恰好 225 条目标记录并执行逐查询精确性核对。"""

    formal_keys, ablation_keys, identities = expected_keys(plan)
    missing_formal = sorted(set(formal_keys) - set(formal))
    missing_ablation = sorted(set(ablation_keys) - set(ablation))
    extra_ablation = sorted(set(ablation) - set(ablation_keys))
    if missing_formal or missing_ablation or extra_ablation:
        raise RuntimeError(f"ablation coverage mismatch: missing_formal={missing_formal[:3]} missing_ablation={missing_ablation[:3]} extra_ablation={extra_ablation[:3]}")
    selected = [(key, formal[key], formal_keys[key]) for key in sorted(formal_keys)] + [(key, ablation[key], ablation_keys[key]) for key in sorted(ablation_keys)]
    rows = []
    for key, row, (dataset, g, query, method) in selected:
        case = f"P2_cross_g__{dataset}_{g}"
        raw_g = row.get("g")
        raw_query = row.get("query_index")
        integer_identity = isinstance(raw_g, int) and not isinstance(raw_g, bool) and isinstance(raw_query, int) and not isinstance(raw_query, bool)
        if row.get("task_key") != key or row.get("suite") != "P2_cross_g" or row.get("case_id") != case or row.get("dataset") != dataset or not integer_identity or raw_g != g or raw_query != query:
            raise RuntimeError(f"record identity disagrees with selected task key: {key}")
        validate_outcome(row, method, key)
        rows.append(row)
    by_identity: dict[tuple[str, int, int], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        by_identity[(str(row["dataset"]), int(row["g"]), int(row["query_index"]))].append(row)
    for identity in identities:
        group = by_identity[identity]
        if len(group) != 5 or {str(row["method"]) for row in group} != set(METHODS):
            raise RuntimeError(f"five-configuration identity mismatch: {identity}")
        reference = next(row for row in group if row["method"] == "abhss_base")
        for row in group:
            minimum = row.get("min_f")
            maximum = row.get("max_f")
            mean = row.get("mean_f")
            valid_sizes = isinstance(minimum, int) and not isinstance(minimum, bool) and isinstance(maximum, int) and not isinstance(maximum, bool)
            valid_mean = isinstance(mean, (int, float)) and not isinstance(mean, bool) and math.isfinite(float(mean))
            if not valid_sizes or not valid_mean or minimum != reference["min_f"] or maximum != reference["max_f"] or not math.isclose(float(mean), float(reference["mean_f"]), rel_tol=1e-12, abs_tol=1e-12):
                raise RuntimeError(f"five-configuration query metadata mismatch: {identity}")
        solved = [row for row in group if row["status"] == "ok"]
        statuses = {str(row["solution_status"]) for row in solved}
        feasible_weights = [float(row["weight"]) for row in solved if row["solution_status"] == "feasible"]
        if len(statuses) > 1 or (feasible_weights and max(feasible_weights) - min(feasible_weights) > 1e-6):
            raise RuntimeError(f"exact configurations disagree: {identity}")
    return rows


def penalized_seconds(row: dict[str, Any]) -> float:
    """返回统一 3600 秒口径下的 PAR-2 单项代价。"""

    return float(row["solver_seconds"]) if row["status"] == "ok" else 2.0 * TIMEOUT


def summarize_cells(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """按图、g、配置汇总五条固定查询。"""

    groups: dict[tuple[str, int, str], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        groups[(str(row["dataset"]), int(row["g"]), str(row["method"]))].append(row)
    output = []
    for (dataset, g, method), group in sorted(groups.items()):
        solved = [row for row in group if row["status"] == "ok"]
        times = [float(row["solver_seconds"]) for row in solved]
        memories = [float(row["query_memory_peak_mib"]) for row in solved]
        states = [int(row["mask_vertex_states"]) for row in solved]
        output.append({
            "dataset": dataset,
            "g": g,
            "method": method,
            "label": LABELS[method],
            "instances": len(group),
            "solved": len(solved),
            "timeouts": len(group) - len(solved),
            "completion_rate": len(solved) / len(group),
            "par2_seconds": statistics.fmean(penalized_seconds(row) for row in group),
            "mean_solved_seconds": statistics.fmean(times) if times else None,
            "median_solved_seconds": percentile(times, 0.5),
            "peak_rss_mib_on_completed": max(memories) if memories else None,
            "median_rss_mib_on_completed": percentile(memories, 0.5),
            "mean_mask_vertex_states_on_completed": statistics.fmean(states) if states else None,
            "median_mask_vertex_states_on_completed": percentile(states, 0.5),
        })
    if len(output) != 45 or set(row["instances"] for row in output) != {5}:
        raise RuntimeError("ablation cell summary is not 9 cells x 5 methods x 5 queries")
    return output


def summarize_pairs(rows: list[dict[str, Any]], identities: list[tuple[str, int, int]]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    """逐 cell 与全局报告四个预声明的有向机制比较。"""

    indexed = {(str(row["dataset"]), int(row["g"]), int(row["query_index"]), str(row["method"])): row for row in rows}
    cell_rows = []
    overall_rows = []
    for comparison, control, treatment in COMPARISONS:
        all_pairs = []
        for dataset in sorted({str(row["dataset"]) for row in rows}):
            for g in sorted({int(row["g"]) for row in rows if row["dataset"] == dataset}):
                queries = sorted(query for current_dataset, current_g, query in identities if current_dataset == dataset and current_g == g)
                if len(queries) != 5:
                    raise RuntimeError(f"ablation cell does not contain five preregistered queries: {dataset}, g={g}")
                pairs = [(indexed[(dataset, g, query, control)], indexed[(dataset, g, query, treatment)]) for query in queries]
                all_pairs.extend(pairs)
                cell_rows.append(pair_summary(comparison, control, treatment, dataset, g, pairs))
        overall_rows.append(pair_summary(comparison, control, treatment, "ALL", None, all_pairs))
    return cell_rows, overall_rows


def pair_summary(comparison: str, control: str, treatment: str, dataset: str, g: int | None, pairs: list[tuple[dict[str, Any], dict[str, Any]]]) -> dict[str, Any]:
    """定义 control/treatment 方向，保证大于 1 表示 treatment 更快。"""

    both = [(control_row, treatment_row) for control_row, treatment_row in pairs if control_row["status"] == "ok" and treatment_row["status"] == "ok"]
    time_ratios = [float(control_row["solver_seconds"]) / float(treatment_row["solver_seconds"]) for control_row, treatment_row in both if float(control_row["solver_seconds"]) > 0.0 and float(treatment_row["solver_seconds"]) > 0.0]
    state_ratios = [int(control_row["mask_vertex_states"]) / int(treatment_row["mask_vertex_states"]) for control_row, treatment_row in both if int(control_row["mask_vertex_states"]) > 0 and int(treatment_row["mask_vertex_states"]) > 0]
    memory_ratios = [float(control_row["query_memory_peak_mib"]) / float(treatment_row["query_memory_peak_mib"]) for control_row, treatment_row in both if float(control_row["query_memory_peak_mib"]) > 0.0 and float(treatment_row["query_memory_peak_mib"]) > 0.0]
    control_par2 = statistics.fmean(penalized_seconds(control_row) for control_row, _ in pairs)
    treatment_par2 = statistics.fmean(penalized_seconds(treatment_row) for _, treatment_row in pairs)
    return {
        "comparison": comparison,
        "control": control,
        "treatment": treatment,
        "dataset": dataset,
        "g": g,
        "paired_instances": len(pairs),
        "both_solved": len(both),
        "control_timeout_treatment_solved": sum(control_row["status"] == "timeout" and treatment_row["status"] == "ok" for control_row, treatment_row in pairs),
        "treatment_timeout_control_solved": sum(control_row["status"] == "ok" and treatment_row["status"] == "timeout" for control_row, treatment_row in pairs),
        "both_timeout": sum(control_row["status"] == "timeout" and treatment_row["status"] == "timeout" for control_row, treatment_row in pairs),
        "control_par2_seconds": control_par2,
        "treatment_par2_seconds": treatment_par2,
        "par2_speedup_control_over_treatment": control_par2 / treatment_par2 if treatment_par2 > 0.0 else None,
        "geomean_speedup_on_both_solved": geomean(time_ratios),
        "median_speedup_on_both_solved": percentile(time_ratios, 0.5),
        "geomean_control_over_treatment_states": geomean(state_ratios),
        "geomean_control_over_treatment_rss": geomean(memory_ratios),
    }


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    """按首行稳定字段顺序写 CSV。"""

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def plot(cell_rows: list[dict[str, Any]], pair_rows: list[dict[str, Any]], datasets: list[str], g_values: list[int], output: Path) -> None:
    """生成每图一行、两个机制列的主消融图。"""

    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(len(datasets), 2, figsize=(9.2, 8.2), squeeze=False)
    chain = ("abhss_base", "abhss_directed_cut_only", "abhss_enhanced")
    colors = {"abhss_base": "#4C78A8", "abhss_directed_cut_only": "#F58518", "abhss_enhanced": "#54A24B"}
    for row_index, dataset in enumerate(datasets):
        left = axes[row_index][0]
        for method in chain:
            points = sorted((int(row["g"]), float(row["par2_seconds"])) for row in cell_rows if row["dataset"] == dataset and row["method"] == method)
            left.plot([point[0] for point in points], [point[1] for point in points], marker="o", color=colors[method], label=LABELS[method])
        left.set_yscale("log")
        left.set_xticks(g_values)
        left.set_xlabel("Number of groups g")
        left.set_ylabel("PAR-2 seconds (5 queries)")
        left.set_title(f"{dataset}: configuration chain")
        left.grid(True, which="both", alpha=0.25)
        left.legend(frameon=False, fontsize="small")

        right = axes[row_index][1]
        for comparison, label, color in (("endpoint_floor_base", "Base", "#4C78A8"), ("endpoint_floor_enhanced", "Enhanced", "#54A24B")):
            points = sorted((int(row["g"]), float(row["par2_speedup_control_over_treatment"])) for row in pair_rows if row["dataset"] == dataset and row["comparison"] == comparison)
            right.plot([point[0] for point in points], [point[1] for point in points], marker="o", color=color, label=label)
        right.axhline(1.0, color="black", linewidth=0.8, linestyle="--")
        right.set_yscale("log")
        right.set_xticks(g_values)
        right.set_xlabel("Number of groups g")
        right.set_ylabel("PAR-2 speedup from endpoint-floor")
        right.set_title(f"{dataset}: common A1 continuation")
        right.grid(True, which="both", alpha=0.25)
        right.legend(frameon=False, fontsize="small")
    fig.tight_layout()
    fig.savefig(output / "ablation_two_mechanism_panels.pdf", bbox_inches="tight")
    fig.savefig(output / "ablation_two_mechanism_panels.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--formal-input", type=Path, action="append", required=True)
    parser.add_argument("--ablation-input", type=Path, action="append", required=True)
    parser.add_argument("--plan", type=Path, default=Path("experiments/ablation_plan.json"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--figures", action="store_true")
    parser.add_argument("--allow-incomplete-formal-input-for-testing", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()

    plan_path = resolve(args.plan)
    plan = json.loads(plan_path.read_text(encoding="utf-8"))
    if plan.get("status") != "preregistered-before-results" or float(plan.get("timeout_seconds_per_query", -1.0)) != TIMEOUT:
        raise RuntimeError("ablation plan identity or timeout changed")
    ablation_metadata = validate_ablation_metadata(args.ablation_input)
    formal, formal_rows = load_records(args.formal_input, "formal")
    ablation, ablation_rows = load_records(args.ablation_input, "ablation")
    formal_ledger = validate_canonical_formal_input(args.formal_input, formal_rows, args.allow_incomplete_formal_input_for_testing)
    rows = canonical_rows(formal, ablation, plan)
    cells = summarize_cells(rows)
    _, _, identities = expected_keys(plan)
    pairs, overall = summarize_pairs(rows, identities)
    output = resolve(args.output)
    output.mkdir(parents=True, exist_ok=True)
    write_csv(output / "ablation_summary_by_cell.csv", cells)
    write_csv(output / "ablation_pairs_by_cell.csv", pairs)
    write_csv(output / "ablation_pairs_overall.csv", overall)
    if args.figures:
        plot(cells, pairs, [str(dataset) for dataset in plan["selection"]["datasets"]], [int(g) for g in plan["selection"]["g"]], output)
    metadata = {
        "schema_version": 1,
        "status": "pass",
        "plan": str(plan_path.relative_to(ROOT)),
        "plan_sha256": sha256_file(plan_path),
        "formal_records_loaded": len(formal_rows),
        "formal_ledger": formal_ledger,
        "ablation_worker_metadata": ablation_metadata,
        "ablation_records_loaded": len(ablation_rows),
        "selected_queries": 45,
        "selected_records": len(rows),
        "method_counts": dict(sorted(Counter(str(row["method"]) for row in rows).items())),
        "cell_rows": len(cells),
        "pair_cell_rows": len(pairs),
        "pair_overall_rows": len(overall),
        "objective_mismatches": 0,
        "feasibility_mismatches": 0,
        "timeout_seconds_per_query": TIMEOUT,
        "timeout_handling": "PAR-2 uses all five queries per cell; paired speedup uses mutually solved pairs and timeout directions are separate",
        "figures_generated": bool(args.figures),
    }
    (output / "ablation_report_metadata.json").write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(metadata, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
