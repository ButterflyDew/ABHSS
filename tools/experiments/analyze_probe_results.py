#!/usr/bin/env python3
"""Turn a completed 15-hour probe into evidence tables and a Chinese report."""

from __future__ import annotations

import argparse
from collections import defaultdict
import csv
import hashlib
import json
import math
from pathlib import Path
import random
import statistics
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[2]
CENTRAL = ("abhss_light", "abhss_heavy", "pruneddp_safe")
ABLATION_METHODS = (
    "abhss_light",
    "abhss_light_no_early",
    "abhss_light_no_witness",
    "abhss_heavy",
    "abhss_heavy_forward",
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def aggregate_files(paths: Iterable[Path], relative_to: Path) -> dict[str, Any]:
    digest = hashlib.sha256()
    count = 0
    byte_count = 0
    for path in sorted({path.resolve() for path in paths}, key=lambda item: str(item).casefold()):
        if not path.is_file():
            continue
        try:
            relative = path.relative_to(relative_to.resolve()).as_posix()
        except ValueError:
            relative = str(path)
        file_digest = sha256_file(path)
        size = path.stat().st_size
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(file_digest.encode("ascii"))
        digest.update(b"\0")
        count += 1
        byte_count += size
    return {
        "file_count": count,
        "byte_count": byte_count,
        "aggregate_sha256": digest.hexdigest(),
        "aggregate_definition": "SHA256 over sorted relative_path NUL file_sha256 NUL tuples",
    }


def build_reproducibility_snapshot(run_dir: Path) -> dict[str, Any]:
    source_candidates = [ROOT / "CMakeLists.txt"]
    for folder in (ROOT / "src", ROOT / "tools" / "experiments"):
        if folder.exists():
            source_candidates.extend(
                path
                for path in folder.rglob("*")
                if path.is_file() and path.suffix.casefold() in {".cpp", ".h", ".hpp", ".py"}
            )
    fixed_inputs = [
        ROOT / "experiments" / "paper_matrix.json",
        ROOT / "experiments" / "environment_lock.json",
        ROOT / "experiments" / "probe_15h_plan.json",
        run_dir / "probe_runtime_config.json",
        run_dir / "probe_manifest.json",
        run_dir / "run_metadata.json",
        run_dir / "ANALYSIS_AMENDMENTS.md",
        ROOT / "build" / "CMakeCache.txt",
    ]
    fixed_inputs.extend((ROOT / "build" / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    files: dict[str, dict[str, Any]] = {}
    for path in [*source_candidates, *fixed_inputs]:
        if not path.is_file():
            continue
        try:
            name = path.resolve().relative_to(ROOT.resolve()).as_posix()
        except ValueError:
            name = str(path.resolve())
        files[name] = {"sha256": sha256_file(path), "bytes": path.stat().st_size}
    metadata_path = run_dir / "run_metadata.json"
    metadata = (
        json.loads(metadata_path.read_text(encoding="utf-8"))
        if metadata_path.exists()
        else {}
    )
    return {
        "schema_version": 1,
        "purpose": "Reproduce the exact probe binaries, controller/analyzer code, and canonical record set without relying on a workspace Git commit.",
        "generated_at_analysis": True,
        "source_and_config_files": dict(sorted(files.items())),
        "binary_sha256_from_run_metadata": metadata.get("binary_sha256", {}),
        "records": aggregate_files((run_dir / "records").glob("*.json"), run_dir),
        "logs": aggregate_files((run_dir / "logs").glob("*.log"), run_dir),
        "canonical_result_note": "records/*.json are authoritative; records.jsonl and invocations.jsonl are convenience journals.",
    }


def load_records(run_dir: Path) -> list[dict[str, Any]]:
    by_key: dict[str, dict[str, Any]] = {}
    for path in (run_dir / "records").glob("*.json"):
        record = json.loads(path.read_text(encoding="utf-8"))
        previous = by_key.get(record["task_key"])
        if previous is None or record.get("finished_at", "") >= previous.get("finished_at", ""):
            by_key[record["task_key"]] = record
    return list(by_key.values())


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields: list[str] = []
    for row in rows:
        for key in row:
            if key not in fields:
                fields.append(key)
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def geomean(values: Iterable[float]) -> float | None:
    positive = [value for value in values if value > 0 and math.isfinite(value)]
    if not positive:
        return None
    return math.exp(sum(math.log(value) for value in positive) / len(positive))


def fmt(value: Any, digits: int = 3) -> str:
    if value is None:
        return "—"
    if isinstance(value, float):
        if not math.isfinite(value):
            return "∞"
        return f"{value:.{digits}g}"
    return str(value)


def read_selected_queries(path: Path, indices: set[int]) -> dict[int, list[set[int]]]:
    selected: dict[int, list[set[int]]] = {}
    with path.open("r", encoding="utf-8") as source:
        count = int(source.readline().strip())
        for query_index in range(1, count + 1):
            group_count = int(source.readline().strip())
            groups: list[set[int]] = []
            keep = query_index in indices
            for _ in range(group_count):
                values = [int(value) for value in source.readline().split()]
                if not values or values[0] != len(values) - 1:
                    raise ValueError(f"Malformed group in {path}, query {query_index}")
                if keep:
                    groups.append(set(values[1:]))
            if keep:
                selected[query_index] = groups
    return selected


def graph_features(graph_folder: Path, sample_count: int = 4096) -> dict[str, Any]:
    graph_path = graph_folder / "graph.txt" if graph_folder.is_dir() else graph_folder
    with graph_path.open("rb") as source:
        header = source.readline()
        vertices, edges = map(int, header.split()[:2])
        header_end = source.tell()
        byte_count = graph_path.stat().st_size
        rng = random.Random(
            int.from_bytes(hashlib.sha256(str(graph_path).encode()).digest()[:8], "big")
        )
        weights: list[float] = []
        if byte_count > header_end:
            for _ in range(min(sample_count, max(1, edges))):
                offset = rng.randrange(header_end, byte_count)
                source.seek(offset)
                if offset != header_end:
                    source.readline()
                line = source.readline()
                if not line:
                    continue
                fields = line.split()
                if len(fields) >= 3:
                    try:
                        weights.append(float(fields[2]))
                    except ValueError:
                        pass
    return {
        "graph_vertices": vertices,
        "graph_edges": edges,
        "graph_bytes": byte_count,
        "graph_average_degree": (2.0 * edges / vertices) if vertices else None,
        "graph_density": (
            2.0 * edges / (vertices * (vertices - 1)) if vertices > 1 else None
        ),
        "edge_weight_sample_kind": "deterministic-random-byte-offset",
        "edge_weight_sample_n": len(weights),
        "edge_weight_sample_zero_fraction": (
            sum(weight == 0.0 for weight in weights) / len(weights) if weights else None
        ),
        "edge_weight_sample_mean": statistics.fmean(weights) if weights else None,
        "edge_weight_sample_median": statistics.median(weights) if weights else None,
        "edge_weight_sample_min": min(weights) if weights else None,
        "edge_weight_sample_max": max(weights) if weights else None,
    }


def query_features(groups: list[set[int]], graph: dict[str, Any]) -> dict[str, Any]:
    sizes = [len(group) for group in groups]
    union = set().union(*groups) if groups else set()
    intersections: list[int] = []
    jaccards: list[float] = []
    intersecting_pairs = 0
    for left in range(len(groups)):
        for right in range(left + 1, len(groups)):
            intersection = len(groups[left] & groups[right])
            pair_union = len(groups[left] | groups[right])
            intersections.append(intersection)
            jaccards.append(intersection / pair_union if pair_union else 0.0)
            intersecting_pairs += intersection > 0
    total_memberships = sum(sizes)
    pair_count = len(intersections)
    mean_size = statistics.fmean(sizes) if sizes else 0.0
    return {
        "feature_g": len(groups),
        "feature_mean_f": mean_size,
        "feature_min_f": min(sizes) if sizes else 0,
        "feature_max_f": max(sizes) if sizes else 0,
        "feature_f_stddev": statistics.pstdev(sizes) if len(sizes) > 1 else 0.0,
        "feature_f_cv": (
            statistics.pstdev(sizes) / mean_size if len(sizes) > 1 and mean_size else 0.0
        ),
        "group_memberships": total_memberships,
        "group_union_vertices": len(union),
        "duplicate_membership_fraction": (
            1.0 - len(union) / total_memberships if total_memberships else 0.0
        ),
        "group_union_graph_fraction": (
            len(union) / graph["graph_vertices"] if graph["graph_vertices"] else None
        ),
        "pair_count": pair_count,
        "intersecting_pair_fraction": intersecting_pairs / pair_count if pair_count else 0.0,
        "pair_intersection_mean": statistics.fmean(intersections) if intersections else 0.0,
        "pair_intersection_max": max(intersections) if intersections else 0,
        "pair_jaccard_mean": statistics.fmean(jaccards) if jaccards else 0.0,
        "pair_jaccard_max": max(jaccards) if jaccards else 0.0,
    }


def build_feature_rows(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    unique: dict[tuple[str, int], dict[str, Any]] = {}
    for record in records:
        unique.setdefault((record["case_id"], int(record["query_index"])), record)
    by_query_path: dict[Path, set[int]] = defaultdict(set)
    for record in unique.values():
        by_query_path[ROOT / record["query_path"]].add(int(record["query_index"]))
    selected_groups = {
        path: read_selected_queries(path, indices) for path, indices in by_query_path.items()
    }
    graph_cache: dict[Path, dict[str, Any]] = {}
    rows = []
    for (case_id, query_index), record in sorted(unique.items()):
        graph_folder = ROOT / record["graph_path"]
        if graph_folder not in graph_cache:
            graph_cache[graph_folder] = graph_features(graph_folder)
        graph = graph_cache[graph_folder]
        groups = selected_groups[ROOT / record["query_path"]][query_index]
        row = {
            "suite": record["suite"],
            "case_id": case_id,
            "dataset": record["dataset"],
            "query_index": query_index,
            "query_path": record["query_path"],
            "graph_path": record["graph_path"],
            **graph,
            **query_features(groups, graph),
        }
        rows.append(row)
    return rows


def last_diag(record: dict[str, Any], phase: str | None = None) -> dict[str, Any] | None:
    events = record.get("probe_diagnostics", [])
    if phase is not None:
        events = [event for event in events if event.get("phase") == phase]
    return events[-1] if events else None


def last_diag_with(record: dict[str, Any], key: str) -> dict[str, Any] | None:
    events = [event for event in record.get("probe_diagnostics", []) if key in event]
    return events[-1] if events else None


def phase_seconds(record: dict[str, Any], phases: Iterable[str]) -> dict[str, float]:
    wanted = set(phases)
    result: dict[str, float] = {}
    for event in record.get("probe_diagnostics", []):
        phase = str(event.get("phase", ""))
        if phase in wanted and event.get("seconds") is not None:
            result[phase] = float(event["seconds"])
    return result


def build_pair_rows(
    records: list[dict[str, Any]], feature_rows: list[dict[str, Any]], wave_map: dict[tuple[str, int], dict[str, Any]]
) -> list[dict[str, Any]]:
    features = {(row["case_id"], row["query_index"]): row for row in feature_rows}
    grouped: dict[tuple[str, int], dict[str, dict[str, Any]]] = defaultdict(dict)
    for record in records:
        grouped[(record["case_id"], int(record["query_index"]))][record["method"]] = record
    rows = []
    for key, methods in sorted(grouped.items()):
        baseline = methods.get("pruneddp_safe")
        if baseline is None:
            continue
        base_time = (
            float(baseline["solver_seconds"]) if baseline.get("status") == "ok" else None
        )
        feature = features.get(key, {})
        plan = wave_map.get(key, {})
        for contender in ("abhss_light", "abhss_heavy"):
            ours = methods.get(contender)
            if ours is None:
                continue
            ours_time = float(ours["solver_seconds"]) if ours.get("status") == "ok" else None
            speedup = base_time / ours_time if base_time and ours_time else None
            weight_agrees = None
            if baseline.get("status") == "ok" and ours.get("status") == "ok":
                left = float(baseline["weight"])
                right = float(ours["weight"])
                weight_agrees = abs(left - right) <= 1e-7 * max(1.0, abs(left), abs(right))
            base_final = last_diag(baseline, "search_end") or last_diag(baseline)
            ours_final = last_diag(ours)
            ours_scalars = last_diag_with(ours, "scalars")
            row = {
                "suite": baseline["suite"],
                "case_id": key[0],
                "dataset": baseline["dataset"],
                "query_index": key[1],
                "g": baseline["g"],
                "mean_f": baseline["mean_f"],
                "wave": plan.get("wave", "unregistered"),
                "outcome_adaptive": bool(plan.get("outcome_adaptive", False)),
                "contender": contender,
                "baseline_status": baseline.get("status"),
                "contender_status": ours.get("status"),
                "baseline_seconds": base_time,
                "contender_seconds": ours_time,
                "baseline_over_contender_speedup": speedup,
                "weight_agrees": weight_agrees,
                "baseline_last_phase": (last_diag(baseline) or {}).get("phase"),
                "contender_last_phase": (last_diag(ours) or {}).get("phase"),
                "baseline_states": (base_final or {}).get("states"),
                "baseline_settled": (base_final or {}).get("settled"),
                "baseline_mst_calls": (base_final or {}).get("mst_calls"),
                "contender_last_scalars": (ours_scalars or {}).get("scalars"),
                "graph_vertices": feature.get("graph_vertices"),
                "graph_edges": feature.get("graph_edges"),
                "graph_average_degree": feature.get("graph_average_degree"),
                "feature_f_cv": feature.get("feature_f_cv"),
                "duplicate_membership_fraction": feature.get("duplicate_membership_fraction"),
                "intersecting_pair_fraction": feature.get("intersecting_pair_fraction"),
                "pair_jaccard_mean": feature.get("pair_jaccard_mean"),
                "group_union_graph_fraction": feature.get("group_union_graph_fraction"),
            }
            rows.append(row)
    return rows


def exact_disagreements(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, int], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        if record.get("status") == "ok" and record.get("exact_claim", True):
            grouped[(record["case_id"], int(record["query_index"]))].append(record)
    disagreements = []
    for (case_id, query_index), group in sorted(grouped.items()):
        if len(group) < 2:
            continue
        reference = float(group[0]["weight"])
        if any(
            abs(float(record["weight"]) - reference)
            > 1e-7 * max(1.0, abs(float(record["weight"])), abs(reference))
            for record in group[1:]
        ):
            disagreements.append(
                {
                    "case_id": case_id,
                    "query_index": query_index,
                    "methods": {
                        record["method"]: record["weight"] for record in group
                    },
                }
            )
    return disagreements


def ablation_signal(
    base: dict[str, Any] | None, ablation: dict[str, Any] | None
) -> tuple[float | None, str]:
    if base is None or ablation is None:
        return None, "missing"
    base_status = str(base.get("status"))
    ablation_status = str(ablation.get("status"))
    if base_status == "ok" and ablation_status == "ok":
        ratio = float(ablation["solver_seconds"]) / max(
            float(base["solver_seconds"]), 1e-12
        )
        if ratio >= 1.2:
            return ratio, "component-helpful"
        if ratio <= 1.0 / 1.2:
            return ratio, "component-costly"
        return ratio, "within-20pct"
    if base_status == "ok" and ablation_status == "timeout":
        return None, "component-strongly-helpful-ablation-timeout"
    if base_status == "timeout" and ablation_status == "ok":
        return None, "component-harmful-base-timeout"
    if base_status == "budget_exhausted" or ablation_status == "budget_exhausted":
        return None, "global-budget-censored"
    return None, f"inconclusive-{base_status}-vs-{ablation_status}"


def build_ablation_rows(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, int], dict[str, dict[str, Any]]] = defaultdict(dict)
    for record in records:
        if record.get("suite") == "S2_ablation":
            grouped[(record["case_id"], int(record["query_index"]))][
                record["method"]
            ] = record
    rows = []
    for (case_id, query_index), methods in sorted(grouped.items()):
        representative = next(iter(methods.values()))
        early_ratio, early_signal = ablation_signal(
            methods.get("abhss_light"), methods.get("abhss_light_no_early")
        )
        witness_ratio, witness_signal = ablation_signal(
            methods.get("abhss_light"), methods.get("abhss_light_no_witness")
        )
        adjoint_ratio, adjoint_signal = ablation_signal(
            methods.get("abhss_heavy"), methods.get("abhss_heavy_forward")
        )
        row: dict[str, Any] = {
            "case_id": case_id,
            "dataset": representative["dataset"],
            "query_index": query_index,
            "g": representative["g"],
            "mean_f": representative["mean_f"],
        }
        for method in ABLATION_METHODS:
            record = methods.get(method)
            row[f"{method}_status"] = record.get("status") if record else "missing"
            row[f"{method}_seconds"] = (
                record.get("solver_seconds") if record else None
            )
        row.update(
            {
                "no_early_over_light": early_ratio,
                "early_anchor_signal": early_signal,
                "no_witness_over_light": witness_ratio,
                "witness_signal": witness_signal,
                "heavy_forward_over_heavy": adjoint_ratio,
                "adjoint_signal": adjoint_signal,
            }
        )
        rows.append(row)
    return rows


def build_coverage_rows(
    records: list[dict[str, Any]], wave_map: dict[tuple[str, int], dict[str, Any]]
) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, int], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        grouped[(record["case_id"], int(record["query_index"]))].append(record)
    rows = []
    for key, group in sorted(grouped.items()):
        representative = group[0]
        methods = {record["method"]: record for record in group}
        plan = wave_map.get(key, {})
        central_present = [method for method in CENTRAL if method in methods]
        rows.append(
            {
                "case_id": key[0],
                "query_index": key[1],
                "suite": representative["suite"],
                "dataset": representative["dataset"],
                "g": representative["g"],
                "mean_f": representative["mean_f"],
                "wave": plan.get("wave", "unregistered"),
                "outcome_adaptive": bool(plan.get("outcome_adaptive", False)),
                "task_record_count": len(group),
                "methods_recorded": " | ".join(sorted(methods)),
                "statuses": " | ".join(
                    f"{method}:{methods[method].get('status')}" for method in sorted(methods)
                ),
                "central_methods_present": len(central_present),
                "central_triplet_recorded": len(central_present) == len(CENTRAL),
                "central_triplet_all_ok": len(central_present) == len(CENTRAL)
                and all(methods[method].get("status") == "ok" for method in CENTRAL),
            }
        )
    return rows


def record_error_line(record: dict[str, Any]) -> str | None:
    log_name = record.get("log_path")
    if not log_name:
        return None
    path = ROOT / str(log_name)
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return None
    errors = [line.strip() for line in lines if line.strip().startswith("Error:")]
    return errors[-1] if errors else None


def build_incident_rows(
    records: list[dict[str, Any]], feature_rows: list[dict[str, Any]]
) -> list[dict[str, Any]]:
    features = {(row["case_id"], row["query_index"]): row for row in feature_rows}
    grouped: dict[tuple[str, int], dict[str, dict[str, Any]]] = defaultdict(dict)
    for record in records:
        grouped[(record["case_id"], int(record["query_index"]))][
            record["method"]
        ] = record
    incidents = []
    for key, methods in sorted(grouped.items()):
        for method, record in sorted(methods.items()):
            status = str(record.get("status"))
            if status not in {"error", "graph_load_timeout"}:
                continue
            error_line = record_error_line(record)
            lowered = (error_line or "").casefold()
            if "witness is disconnected" in lowered:
                category = "witness-construction-invariant"
            elif "bad_alloc" in lowered or "out of memory" in lowered:
                category = "memory-allocation"
            elif status == "graph_load_timeout":
                category = "pre-ready-graph-load"
            else:
                category = "runtime-error"
            feature = features.get(key, {})
            incidents.append(
                {
                    "case_id": key[0],
                    "dataset": record["dataset"],
                    "query_index": key[1],
                    "g": record["g"],
                    "mean_f": record["mean_f"],
                    "method": method,
                    "status": status,
                    "category": category,
                    "error_line": error_line,
                    "last_phase": (last_diag(record) or {}).get("phase"),
                    "watchdog_wall_seconds": record.get("watchdog_wall_seconds"),
                    "watchdog_peak_rss_mib": record.get("watchdog_peak_rss_mib"),
                    "watchdog_query_peak_overhead_mib": record.get(
                        "watchdog_query_peak_overhead_mib"
                    ),
                    "peer_statuses": {
                        peer: value.get("status") for peer, value in sorted(methods.items())
                    },
                    "log_path": record.get("log_path"),
                    "graph_average_degree": feature.get("graph_average_degree"),
                    "edge_weight_sample_zero_fraction": feature.get(
                        "edge_weight_sample_zero_fraction"
                    ),
                    "feature_f_cv": feature.get("feature_f_cv"),
                    "duplicate_membership_fraction": feature.get(
                        "duplicate_membership_fraction"
                    ),
                    "intersecting_pair_fraction": feature.get(
                        "intersecting_pair_fraction"
                    ),
                    "group_union_graph_fraction": feature.get(
                        "group_union_graph_fraction"
                    ),
                }
            )
    return incidents


def method_explanation(
    baseline: dict[str, Any], ours: list[dict[str, Any]]
) -> tuple[list[str], dict[str, Any]]:
    evidence: dict[str, Any] = {}
    reasons: list[str] = []
    base_phases = phase_seconds(baseline, ["group_sssp_end", "route_dp_end", "search_end"])
    evidence["baseline_phase_seconds"] = base_phases
    base_final = last_diag(baseline, "search_end") or last_diag(baseline)
    if base_final:
        evidence["baseline_last_diagnostic"] = base_final
    evidence["baseline_watchdog_memory"] = {
        "peak_rss_mib": baseline.get("watchdog_peak_rss_mib"),
        "query_peak_overhead_mib": baseline.get("watchdog_query_peak_overhead_mib"),
    }
    if baseline.get("status") == "ok" and base_phases:
        total = float(baseline["solver_seconds"])
        preprocessing = base_phases.get("group_sssp_end", 0.0) + base_phases.get("route_dp_end", 0.0)
        if total > 0 and preprocessing / total >= 0.7:
            reasons.append(
                "PrunedDP++ 的时间主要是共同组最短路/组路线预处理，随后稀疏搜索很快闭合；这类询问没有触发其指数状态劣势。"
            )
    base_states = (base_final or {}).get("states")
    for record in ours:
        phases = phase_seconds(
            record,
            [
                "prepare_end",
                "early_anchor_end",
                "ordinary_end",
                "anchored_end",
                "low_anchor_end",
                "adjoint_end",
            ],
        )
        evidence[f"{record['method']}_phase_seconds"] = phases
        final = last_diag(record)
        if final:
            evidence[f"{record['method']}_last_diagnostic"] = final
        evidence[f"{record['method']}_watchdog_memory"] = {
            "peak_rss_mib": record.get("watchdog_peak_rss_mib"),
            "query_peak_overhead_mib": record.get("watchdog_query_peak_overhead_mib"),
        }
        scalars = (last_diag_with(record, "scalars") or {}).get("scalars")
        if base_states and scalars and float(scalars) / float(base_states) >= 10:
            reasons.append(
                f"{record['method']} 物化标量约为 baseline 发现状态的至少 10 倍，说明本方法的半格/锚格在该实例上剪枝不足，而 baseline 保持了稀疏状态优势。"
            )
        status = str(record.get("status"))
        timed_out = status == "timeout"
        if phases and not timed_out:
            dominant = max(phases, key=phases.get)
            if dominant == "prepare_end" and record["method"] == "abhss_heavy":
                reasons.append(
                    "ABHSS-Heavy 的 eager directed-cut/dual-primal 预处理占主导；当 baseline 的稀疏搜索本来就很小时，这笔固定投入无法摊薄。"
                )
            elif dominant == "ordinary_end":
                reasons.append(
                    f"{record['method']} 的 ordinary 子集行阶段占主导，瓶颈来自中间子集状态生成而不是最终锚接合。"
                )
            elif dominant in {"anchored_end", "adjoint_end"}:
                reasons.append(
                    f"{record['method']} 的最终锚/adjoint 阶段占主导，表明普通行尚可，但跨半格接合没有被 incumbent/下界充分剪掉。"
                )
        if timed_out:
            phase = str((last_diag(record) or {}).get("phase", "unknown"))
            completed_before = {
                "ordinary_layer": ["prepare_end", "early_anchor_end"],
                "anchored_layer": ["prepare_end", "early_anchor_end", "ordinary_end"],
                "adjoint_layer": ["prepare_end", "ordinary_end", "low_anchor_end"],
            }.get(phase, [])
            prior = sum(phases.get(name, 0.0) for name in completed_before)
            observed = float(record.get("watchdog_wall_seconds") or 0.0)
            active_lower = max(0.0, observed - prior)
            reasons.append(
                f"{record['method']} 在探针截止时最后位于 `{phase}`；扣除已完成阶段后，"
                f"至少约 {active_lower:.2f} 秒消耗在该活动阶段，因此不能用较早的 completed phase 误判瓶颈。"
            )
        elif status == "graph_load_timeout":
            reasons.append(
                f"{record['method']} 在 `[Ready]` 之前耗尽图加载预算；这不是 GST 搜索阶段的性能证据，"
                "应先核验该可执行文件的解析/建图路径和峰值内存。"
            )
        elif status == "error":
            reasons.append(
                f"{record['method']} 以程序错误结束；该单元首先是正确性/鲁棒性问题，不能按普通速度退化解释。"
            )
    if not reasons:
        reasons.append("当前粗粒度计数不足以唯一归因；需要在正式复跑中针对该单元增加层级计数。")
    return list(dict.fromkeys(reasons)), evidence


def data_explanation(feature: dict[str, Any]) -> list[str]:
    reasons: list[str] = []
    average_degree = feature.get("graph_average_degree")
    if average_degree is not None:
        if average_degree >= 100:
            reasons.append(
                f"图平均度约 {average_degree:.1f}，非常稠密；该特征与 ABHSS 物化行传播反复扫描大量邻边的机制一致，"
                "而稀疏 baseline 若只弹出少数状态可能更占优。"
            )
        elif average_degree <= 4:
            reasons.append(
                f"图平均度约 {average_degree:.1f}，接近树状/稀疏；局部最短路与稀疏状态搜索容易快速闭合，baseline 的容器开销较小。"
            )
    overlap = feature.get("duplicate_membership_fraction") or 0.0
    pair_overlap = feature.get("intersecting_pair_fraction") or 0.0
    if overlap >= 0.05 or pair_overlap >= 0.25:
        reasons.append(
            f"询问组有明显重叠（重复 membership 比例 {overlap:.1%}，相交组对 {pair_overlap:.1%}）；"
            "共同顶点可能形成很强的早期可行解，这与 baseline 提前闭合的现象相符，但单凭该特征不能证明因果。"
        )
    union_fraction = feature.get("group_union_graph_fraction")
    if union_fraction is not None and union_fraction >= 0.1:
        reasons.append(
            f"组并集覆盖图的 {union_fraction:.1%}，终端非常密集；最短连接通常很短，削弱了复杂下界/见证结构的收益。"
        )
    cv = feature.get("feature_f_cv") or 0.0
    if cv >= 0.5:
        reasons.append(
            f"组大小变异系数为 {cv:.2f}，组极不均衡；固定锚与按组对称的半格工作量可能被少数大组主导。"
        )
    return reasons


def build_anomalies(
    records: list[dict[str, Any]], feature_rows: list[dict[str, Any]]
) -> list[dict[str, Any]]:
    features = {(row["case_id"], row["query_index"]): row for row in feature_rows}
    grouped: dict[tuple[str, int], dict[str, dict[str, Any]]] = defaultdict(dict)
    for record in records:
        grouped[(record["case_id"], int(record["query_index"]))][record["method"]] = record
    anomalies = []
    for key, methods in grouped.items():
        baseline = methods.get("pruneddp_safe")
        ours = [methods.get("abhss_light"), methods.get("abhss_heavy")]
        if baseline is None or baseline.get("status") != "ok" or any(record is None for record in ours):
            continue
        ours_records = [record for record in ours if record is not None]
        baseline_time = float(baseline["solver_seconds"])
        denominator = max(baseline_time, 1e-12)
        solved = [
            float(record["solver_seconds"])
            for record in ours_records
            if record.get("status") == "ok"
        ]
        affected = [
            record
            for record in ours_records
            if record.get("status") in {"timeout", "graph_load_timeout", "error"}
            or (
                record.get("status") == "ok"
                and float(record["solver_seconds"]) / denominator >= 2.0
            )
        ]
        # A final budget_exhausted record is order-censored by the global
        # 15-hour boundary, so it is retained in raw tables but is not treated
        # as evidence that the algorithm itself is weak.
        if not affected:
            continue
        best_ratio = min(solved) / denominator if solved else None
        affected_solved_ratios = [
            float(record["solver_seconds"]) / denominator
            for record in affected
            if record.get("status") == "ok"
        ]
        affected_has_failure = any(record.get("status") != "ok" for record in affected)
        worst_ratio = (
            None if affected_has_failure else max(affected_solved_ratios, default=None)
        )
        method_reasons, method_evidence = method_explanation(baseline, affected)
        feature = features.get(key, {})
        anomalies.append(
            {
                "case_id": key[0],
                "query_index": key[1],
                "suite": baseline["suite"],
                "dataset": baseline["dataset"],
                "g": baseline["g"],
                "mean_f": baseline["mean_f"],
                "baseline_seconds": baseline_time,
                "abhss_light_status": ours_records[0].get("status"),
                "abhss_light_seconds": ours_records[0].get("solver_seconds"),
                "abhss_heavy_status": ours_records[1].get("status"),
                "abhss_heavy_seconds": ours_records[1].get("solver_seconds"),
                "affected_contenders": [record["method"] for record in affected],
                "baseline_advantage_over_best_abhss": best_ratio,
                "baseline_advantage_infinite": not solved,
                "baseline_advantage_over_worst_affected": worst_ratio,
                "affected_has_failure": affected_has_failure,
                "method_level_explanation": method_reasons,
                "method_evidence": method_evidence,
                "data_level_explanation": data_explanation(feature),
                "data_evidence": feature,
            }
        )
    anomalies.sort(
        key=lambda row: (
            0 if row["affected_has_failure"] else 1,
            -float(row["baseline_advantage_over_worst_affected"] or 0.0),
            row["case_id"],
            row["query_index"],
        )
    )
    return anomalies


def percentile_rank(values: list[float], value: float) -> float | None:
    finite = sorted(item for item in values if math.isfinite(item))
    if not finite or not math.isfinite(value):
        return None
    lower = sum(item < value for item in finite)
    equal = sum(item == value for item in finite)
    return (lower + 0.5 * equal) / len(finite)


def annotate_anomaly_context(
    anomalies: list[dict[str, Any]],
    feature_rows: list[dict[str, Any]],
    pair_rows: list[dict[str, Any]],
) -> None:
    graph_unique: dict[str, dict[str, Any]] = {}
    for row in feature_rows:
        graph_unique.setdefault(str(row.get("graph_path")), row)
    graph_degree_values = [
        float(row["graph_average_degree"])
        for row in graph_unique.values()
        if row.get("graph_average_degree") is not None
    ]
    query_feature_names = (
        "feature_f_cv",
        "duplicate_membership_fraction",
        "intersecting_pair_fraction",
        "group_union_graph_fraction",
    )
    query_values = {
        name: [
            float(row[name])
            for row in feature_rows
            if row.get(name) is not None
        ]
        for name in query_feature_names
    }

    pair_index: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in pair_rows:
        pair_index[(row["case_id"], row["contender"])].append(row)

    for anomaly in anomalies:
        feature = anomaly.get("data_evidence", {})
        ranks: dict[str, float | None] = {}
        if feature.get("graph_average_degree") is not None:
            ranks["graph_average_degree"] = percentile_rank(
                graph_degree_values, float(feature["graph_average_degree"])
            )
        for name in query_feature_names:
            if feature.get(name) is not None:
                ranks[name] = percentile_rank(
                    query_values[name], float(feature[name])
                )
        anomaly["feature_percentile_ranks"] = ranks
        percentile_reasons = []
        labels = {
            "graph_average_degree": "图平均度",
            "feature_f_cv": "组大小 CV",
            "duplicate_membership_fraction": "重复 membership",
            "intersecting_pair_fraction": "相交组对比例",
            "group_union_graph_fraction": "组并集图覆盖率",
        }
        for name, rank in ranks.items():
            if rank is None:
                continue
            if rank >= 0.9:
                percentile_reasons.append(
                    f"{labels[name]}位于已观测样本约第 {rank:.0%} 百分位，属于本批高端；"
                    "这是描述性关联，不单独作为因果证据。"
                )
            elif rank <= 0.1:
                percentile_reasons.append(
                    f"{labels[name]}位于已观测样本约第 {rank:.0%} 百分位，属于本批低端；"
                    "这是描述性关联，不单独作为因果证据。"
                )
        anomaly["data_level_explanation"].extend(percentile_reasons)
        if not anomaly["data_level_explanation"]:
            anomaly["data_level_explanation"].append(
                "组重叠、组大小离散度和图平均度均不极端；数据特征没有给出单一解释，应优先依据阶段/状态计数判断。"
            )

        recurrence: dict[str, Any] = {}
        for contender in anomaly["affected_contenders"]:
            observations = []
            for row in sorted(
                pair_index.get((anomaly["case_id"], contender), []),
                key=lambda item: int(item["query_index"]),
            ):
                affected = (
                    row["baseline_status"] == "ok"
                    and (
                        row["contender_status"] in {"timeout", "graph_load_timeout", "error"}
                        or (
                            row["contender_status"] == "ok"
                            and row["baseline_over_contender_speedup"] is not None
                            and float(row["baseline_over_contender_speedup"]) <= 0.5
                        )
                    )
                )
                observations.append(
                    {
                        "query_index": row["query_index"],
                        "wave": row["wave"],
                        "outcome_adaptive": row["outcome_adaptive"],
                        "baseline_status": row["baseline_status"],
                        "contender_status": row["contender_status"],
                        "baseline_over_contender_speedup": row[
                            "baseline_over_contender_speedup"
                        ],
                        "affected": affected,
                    }
                )
            recurrence[contender] = {
                "observed_queries": len(observations),
                "affected_queries": sum(item["affected"] for item in observations),
                "observations": observations,
            }
        anomaly["same_case_recurrence"] = recurrence


def planned_pair_counts(manifest: dict[str, Any]) -> dict[str, dict[str, int]]:
    result = {
        contender: {"small_g_le_8": 0, "large_g_ge_9": 0}
        for contender in ("abhss_light", "abhss_heavy")
    }
    seen: set[tuple[str, int, str]] = set()
    for section in ("breadth", "fixed_depth"):
        for entry in manifest.get(section, []):
            methods = set(entry.get("methods", []))
            if "pruneddp_safe" not in methods:
                continue
            interval = "large_g_ge_9" if int(entry["g"]) >= 9 else "small_g_le_8"
            for contender in result:
                key = (entry["case_id"], int(entry["query_index"]), contender)
                if contender in methods and key not in seen:
                    seen.add(key)
                    result[contender][interval] += 1
    return result


def gate_stats(
    pair_rows: list[dict[str, Any]], contender: str, large: bool, predeclared_pairs: int
) -> dict[str, Any]:
    eligible = [
        row
        for row in pair_rows
        if row["contender"] == contender
        and row["wave"] in {"breadth", "depth"}
        and not row["outcome_adaptive"]
        and ((int(row["g"]) >= 9) if large else (int(row["g"]) <= 8))
    ]
    both = [
        row
        for row in eligible
        if row["baseline_status"] == "ok" and row["contender_status"] == "ok"
    ]
    speedups = [float(row["baseline_over_contender_speedup"]) for row in both]
    guardrail_passes = sum(
        float(row["contender_seconds"])
        <= max(
            1.2 * float(row["baseline_seconds"]),
            float(row["baseline_seconds"]) + 0.05,
        )
        for row in both
    )
    completion_wins = sum(
        row["baseline_status"] == "timeout" and row["contender_status"] == "ok"
        for row in eligible
    )
    completion_losses = sum(
        row["baseline_status"] == "ok"
        and row["contender_status"] in {"timeout", "graph_load_timeout", "error"}
        for row in eligible
    )
    contender_nonsearch_failures = sum(
        row["contender_status"] in {"graph_load_timeout", "error"}
        for row in eligible
    )
    baseline_nonsearch_failures = sum(
        row["baseline_status"] in {"graph_load_timeout", "error"}
        for row in eligible
    )
    guardrail_denominator = len(both) + completion_wins + completion_losses
    guardrail_successes = guardrail_passes + completion_wins
    return {
        "predeclared_pairs": predeclared_pairs,
        "observed_pairs": len(eligible),
        "observed_pair_coverage": (
            len(eligible) / predeclared_pairs if predeclared_pairs else None
        ),
        "both_solved": len(both),
        "completion_wins": completion_wins,
        "completion_losses": completion_losses,
        "contender_nonsearch_failures": contender_nonsearch_failures,
        "baseline_nonsearch_failures": baseline_nonsearch_failures,
        "geomean_speedup_both_solved": geomean(speedups),
        "median_speedup_both_solved": statistics.median(speedups) if speedups else None,
        "not_worse_within_20pct_or_50ms": guardrail_passes,
        "guardrail_successes": guardrail_successes,
        "guardrail_denominator": guardrail_denominator,
        "guardrail_rate": (
            guardrail_successes / guardrail_denominator if guardrail_denominator else None
        ),
        "order_magnitude_both_solved": sum(speedup >= 10.0 for speedup in speedups),
        "order_magnitude_or_completion_win": sum(speedup >= 10.0 for speedup in speedups)
        + completion_wins,
    }


def conclusion(small: dict[str, Any], large: dict[str, Any]) -> str:
    small_denominator = small["both_solved"] + small["completion_losses"]
    small_pass = (
        (small["not_worse_within_20pct_or_50ms"] + small["completion_wins"])
        / (small_denominator + small["completion_wins"])
        if small_denominator + small["completion_wins"]
        else 0.0
    )
    large_denominator = max(1, large["observed_pairs"])
    large_signal = large["order_magnitude_or_completion_win"] / large_denominator
    large_loss = large["completion_losses"] / large_denominator
    if small["contender_nonsearch_failures"] or large["contender_nonsearch_failures"]:
        return "执行可靠性门失败：存在程序错误或查询前图加载失败，性能主张必须暂停。"
    coverage_sufficient = (
        small["observed_pairs"] >= 8
        and large["observed_pairs"] >= 8
        and (small["observed_pair_coverage"] or 0.0) >= 0.5
        and (large["observed_pair_coverage"] or 0.0) >= 0.5
    )
    if not coverage_sufficient:
        return "证据覆盖尚不足：不能把当前有利/不利样本外推为高概率投稿结论。"
    if small_pass >= 0.8 and large_signal >= 0.3 and large_loss <= 0.1:
        return "高概率可达：小 g 守门通过，且 g≥9 已出现稳定的数量级/完成率信号。"
    if small_pass >= 0.65 and large_signal >= 0.15 and large_loss <= 0.2:
        return "方向可达但需收敛：已有预期趋势，正式实验前应优先修复报告中的异常图/阶段。"
    return "投稿风险较高：当前探针不足以支撑预期主张，应先处理小 g 退化或大 g 缺少数量级优势的问题。"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", type=Path, required=True)
    args = parser.parse_args()
    run_dir = args.run_dir if args.run_dir.is_absolute() else ROOT / args.run_dir
    manifest = json.loads((run_dir / "probe_manifest.json").read_text(encoding="utf-8"))
    records = load_records(run_dir)

    wave_map: dict[tuple[str, int], dict[str, Any]] = {}
    # A query that was predeclared in fixed_depth remains predeclared even if
    # the controller executes it early as an anomaly follow-up.  Adaptive and
    # filler labels may only fill keys absent from the registered waves.
    for section in ["breadth", "fixed_depth"]:
        for entry in manifest.get(section, []):
            wave_map[(entry["case_id"], int(entry["query_index"]))] = entry
    for section in ["adaptive_followups", "exploratory_filler"]:
        for entry in manifest.get(section, []):
            wave_map.setdefault(
                (entry["case_id"], int(entry["query_index"])), entry
            )

    feature_rows = build_feature_rows(records)
    pair_rows = build_pair_rows(records, feature_rows, wave_map)
    anomalies = build_anomalies(records, feature_rows)
    annotate_anomaly_context(anomalies, feature_rows, pair_rows)
    ablation_rows = build_ablation_rows(records)
    coverage_rows = build_coverage_rows(records, wave_map)
    incident_rows = build_incident_rows(records, feature_rows)
    planned_counts = planned_pair_counts(manifest)
    gates = {
        method: {
            "small_g_le_8": gate_stats(
                pair_rows, method, False, planned_counts[method]["small_g_le_8"]
            ),
            "large_g_ge_9": gate_stats(
                pair_rows, method, True, planned_counts[method]["large_g_ge_9"]
            ),
        }
        for method in ("abhss_light", "abhss_heavy")
    }

    status_path = run_dir / "probe_status.json"
    controller_status = (
        json.loads(status_path.read_text(encoding="utf-8")) if status_path.exists() else {}
    )
    manifest_execution = {
        section: {
            status: sum(entry.get("status", "pending") == status for entry in manifest.get(section, []))
            for status in sorted(
                {str(entry.get("status", "pending")) for entry in manifest.get(section, [])}
            )
        }
        for section in ("breadth", "fixed_depth", "adaptive_followups", "exploratory_filler")
    }
    all_exact_disagreements = exact_disagreements(records)

    write_csv(run_dir / "probe_query_features.csv", feature_rows)
    write_csv(run_dir / "probe_pairs.csv", pair_rows)
    write_csv(run_dir / "probe_ablations.csv", ablation_rows)
    write_csv(run_dir / "probe_coverage.csv", coverage_rows)
    write_csv(
        run_dir / "probe_incidents.csv",
        [
            row
            | {"peer_statuses": json.dumps(row["peer_statuses"], sort_keys=True)}
            for row in incident_rows
        ],
    )
    (run_dir / "probe_incidents.json").write_text(
        json.dumps(incident_rows, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    write_csv(
        run_dir / "probe_anomalies.csv",
        [
            {
                key: value
                for key, value in row.items()
                if key
                not in {
                    "method_evidence",
                    "data_evidence",
                    "same_case_recurrence",
                    "feature_percentile_ranks",
                }
            }
            | {
                "method_level_explanation": " | ".join(row["method_level_explanation"]),
                "data_level_explanation": " | ".join(row["data_level_explanation"]),
                "affected_contenders": " | ".join(row["affected_contenders"]),
                "feature_percentile_ranks": json.dumps(
                    row["feature_percentile_ranks"], sort_keys=True
                ),
                "same_case_recurrence": json.dumps(
                    row["same_case_recurrence"], sort_keys=True
                ),
            }
            for row in anomalies
        ],
    )
    (run_dir / "probe_anomalies.json").write_text(
        json.dumps(anomalies, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    summary = {
        "schema_version": 1,
        "run_id": manifest["run_id"],
        "record_count": len(records),
        "unique_case_queries": len({(r["case_id"], r["query_index"]) for r in records}),
        "status_counts": {
            status: sum(record.get("status") == status for record in records)
            for status in sorted({str(record.get("status")) for record in records})
        },
        "gates": gates,
        "controller_status_at_analysis": controller_status,
        "manifest_execution": manifest_execution,
        "anomaly_count": len(anomalies),
        "ablation_cell_count": len(ablation_rows),
        "coverage_cell_count": len(coverage_rows),
        "execution_incident_count": len(incident_rows),
        "weight_disagreements": len(all_exact_disagreements),
        "weight_disagreement_pairs": all_exact_disagreements,
    }
    (run_dir / "probe_summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    reproducibility = build_reproducibility_snapshot(run_dir)
    (run_dir / "probe_reproducibility.json").write_text(
        json.dumps(reproducibility, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    lines = [
        "# 15 小时全数据探针报告",
        "",
        "> 本报告用于 SIGMOD/VLDB 投稿前的可行性门控，不是论文最终结果。探针使用 180/600 秒短截止；正式矩阵仍使用每询问 10,000 秒。",
        "",
        "## 1. 执行覆盖",
        "",
        f"- 记录任务：{len(records)}；不同 case/query：{summary['unique_case_queries']}。",
        f"- 状态：`{json.dumps(summary['status_counts'], ensure_ascii=False, sort_keys=True)}`。",
        f"- 求解预算：{fmt(controller_status.get('elapsed_seconds'))} / "
        f"{fmt(controller_status.get('budget_seconds'))} 秒（分析启动时控制器状态："
        f"`{controller_status.get('state', 'unknown')}`）。",
        f"- 波次执行状态：`{json.dumps(manifest_execution, ensure_ascii=False, sort_keys=True)}`。",
        f"- 精确答案不一致：{summary['weight_disagreements']}（若非 0，性能结论立即暂停）。",
        "- 询问抽样由固定 seed 在求解前完成；adaptive 样本只用于原因分析，不进入下述主门控。",
        "",
        "### 1.1 来源与参数覆盖",
        "",
        "| suite | case/query | task records | central triplets | g values |",
        "|---|---:|---:|---:|---|",
    ]
    for suite in sorted({row["suite"] for row in coverage_rows}):
        selected = [row for row in coverage_rows if row["suite"] == suite]
        lines.append(
            f"| {suite} | {len(selected)} | "
            f"{sum(int(row['task_record_count']) for row in selected)} | "
            f"{sum(bool(row['central_triplet_recorded']) for row in selected)} | "
            f"{', '.join(map(str, sorted({int(row['g']) for row in selected})))} |"
        )
    lines.extend(
        [
            "",
        "## 2. 投稿主张门控",
        "",
        "| 方法 | 区间 | 预注册/已观测（覆盖） | 双方完成 | 完成胜/负 | 非搜索失败 ours/base | 双方完成几何平均加速 | 守门通过 | ≥10x 或完成胜 | 初步判断 |",
            "|---|---:|---:|---:|---:|---:|---:|---:|---:|---|",
        ]
    )
    method_conclusions = []
    for method in ("abhss_light", "abhss_heavy"):
        small = gates[method]["small_g_le_8"]
        large = gates[method]["large_g_ge_9"]
        lines.append(
            f"| {method} | g≤8 | {small['predeclared_pairs']}/{small['observed_pairs']} "
            f"({fmt(small['observed_pair_coverage'])}) | {small['both_solved']} | "
            f"{small['completion_wins']}/{small['completion_losses']} | "
            f"{small['contender_nonsearch_failures']}/{small['baseline_nonsearch_failures']} | "
            f"{fmt(small['geomean_speedup_both_solved'])} | "
            f"{small['guardrail_successes']}/{small['guardrail_denominator']} | "
            f"{small['order_magnitude_or_completion_win']} | 小 g 守门 |"
        )
        lines.append(
            f"| {method} | g≥9 | {large['predeclared_pairs']}/{large['observed_pairs']} "
            f"({fmt(large['observed_pair_coverage'])}) | {large['both_solved']} | "
            f"{large['completion_wins']}/{large['completion_losses']} | "
            f"{large['contender_nonsearch_failures']}/{large['baseline_nonsearch_failures']} | "
            f"{fmt(large['geomean_speedup_both_solved'])} | "
            f"{large['guardrail_successes']}/{large['guardrail_denominator']} | "
            f"{large['order_magnitude_or_completion_win']} | 大 g 信号 |"
        )
        judgement = (
            "精确性门失败，所有性能判断暂停。"
            if all_exact_disagreements
            else conclusion(small, large)
        )
        method_conclusions.append(f"- **{method}：{judgement}**")
    lines.extend(["", *method_conclusions, ""])
    lines.extend(
        [
            "这里分别评价 Light 与 Heavy，不使用逐实例 best-of-two 作为论文主结果。小 g 的探针守门定义为运行时间不超过 baseline 的 1.2 倍，或绝对差不超过 0.05 秒；这是为了避免单次短探针被计时噪声误判，正式重复实验仍报告原始比值。`probe_pairs.csv` 中保留全部逐询问证据。",
            "",
            "## 3. 预注册组件消融",
            "",
        ]
    )
    if not ablation_rows:
        lines.append("预算内尚未完成任何 S2 消融记录。")
    else:
        lines.extend(
            [
                "| case / q | Light | no-early | no-witness | Heavy | Heavy-forward | early / witness / adjoint 信号 |",
                "|---|---:|---:|---:|---:|---:|---|",
            ]
        )
        for row in ablation_rows:
            cells = {}
            for method in ABLATION_METHODS:
                status_name = row[f"{method}_status"]
                seconds = row[f"{method}_seconds"]
                cells[method] = (
                    f"ok:{fmt(seconds)}" if status_name == "ok" else str(status_name)
                )
            lines.append(
                f"| `{row['case_id']}` / q{row['query_index']} | "
                f"{cells['abhss_light']} | {cells['abhss_light_no_early']} | "
                f"{cells['abhss_light_no_witness']} | {cells['abhss_heavy']} | "
                f"{cells['abhss_heavy_forward']} | {row['early_anchor_signal']} / "
                f"{row['witness_signal']} / {row['adjoint_signal']} |"
            )
        lines.extend(
            [
                "",
                "倍率列在 `probe_ablations.csv` 中定义为 ablation/base；`component-helpful` 表示去除组件至少慢 20%，"
                "`component-costly` 表示去除后至少快 20%。任一侧 timeout 时只给方向性删失信号，不伪造倍率。",
                "",
            ]
        )
    lines.extend(
        [
            "## 4. 排除边界确认",
            "",
            "GPU4GST 作者 CPU/GPU artifact、旧作者图和近似算法均按预注册边界排除；探针不产生这些方法的性能记录。",
            "",
        ]
    )
    lines.extend(
        [
            "## 5. 执行可靠性事件",
            "",
        ]
    )
    if not incident_rows:
        lines.append("未出现程序错误或 `[Ready]` 前图加载失败。")
    else:
        lines.extend(
            [
                "| case / q | method | status / category | last phase | error | peer statuses |",
                "|---|---|---|---|---|---|",
            ]
        )
        for row in incident_rows:
            lines.append(
                f"| `{row['case_id']}` / q{row['query_index']} | {row['method']} | "
                f"{row['status']} / {row['category']} | {row['last_phase']} | "
                f"{row['error_line'] or '—'} | "
                f"`{json.dumps(row['peer_statuses'], sort_keys=True)}` |"
            )
        lines.extend(
            [
                "",
                "这些事件不进入搜索速度倍率，但会使执行可靠性门失败；必须先完成根因修复和定向回归，才能启动正式矩阵。",
                "",
            ]
        )
    lines.extend(
        [
            "## 6. baseline 强而 ABHSS 弱的单元",
            "",
        ]
    )
    if not anomalies:
        lines.append("未发现 Safe 完成且某个 ABHSS 变体失败，或 Safe 比该变体至少快 2 倍的完整配对。")
    else:
        lines.extend(
            [
                "| case / q | g / mean f | 弱势变体 | Safe(s) | Light | Heavy | Safe 相对最弱受影响变体 |",
                "|---|---:|---|---:|---:|---:|---:|",
            ]
        )
        for row in anomalies[:20]:
            lines.append(
                f"| `{row['case_id']}` / q{row['query_index']} | {row['g']} / {fmt(row['mean_f'])} | "
                f"{', '.join(row['affected_contenders'])} | {fmt(row['baseline_seconds'])} | "
                f"{row['abhss_light_status']}:{fmt(row['abhss_light_seconds'])} | "
                f"{row['abhss_heavy_status']}:{fmt(row['abhss_heavy_seconds'])} | "
                f"{'∞' if row['affected_has_failure'] else fmt(row['baseline_advantage_over_worst_affected']) + 'x'} |"
            )
        for index, row in enumerate(anomalies[:10], 1):
            lines.extend(
                [
                    "",
                    f"### 6.{index} `{row['case_id']}` / q{row['query_index']}",
                    "",
                    "方法层：",
                    "",
                    *[f"- {reason}" for reason in row["method_level_explanation"]],
                    "",
                    "同 case 复核层：",
                    "",
                    *[
                        f"- {method}: 已有 {context['observed_queries']} 条配对记录，其中 "
                        f"{context['affected_queries']} 条达到同一弱势判据；含 "
                        f"{sum(item['outcome_adaptive'] for item in context['observations'])} 条 outcome-adaptive 诊断。"
                        for method, context in row["same_case_recurrence"].items()
                    ],
                    "",
                    "数据层：",
                    "",
                    *[f"- {reason}" for reason in row["data_level_explanation"]],
                ]
            )
    lines.extend(
        [
            "",
            "## 7. 下一步决策",
            "",
            "1. 若小 g 守门失败，先按异常报告中占主导的 phase 修复；不要直接扩大正式矩阵。",
            "2. 若 g≥9 只有完成率优势但没有双方完成的 ≥10x 样本，正式 10,000 秒实验仍可主打 time-to-solution/completion，但措辞不能写成全域数量级加速。",
            "3. adaptive 单元只用于机制解释；论文表格的随机样本与置信区间继续使用 `FULL_EXPERIMENT_PLAN.md` 的预注册矩阵。",
            "4. 图权重统计是确定性随机字节偏移样本，适合诊断，不替代完整数据描述。",
        "",
        "机器可读文件：`probe_summary.json`、`probe_coverage.csv`、`probe_pairs.csv`、`probe_ablations.csv`、`probe_incidents.json/csv`、`probe_query_features.csv`、`probe_anomalies.json`。",
        "精确二进制、源码/配置及 canonical record/log 集合哈希见 `probe_reproducibility.json`；它不依赖工作区 Git commit。",
        "独立重算排程、记录键、二进制哈希、预算和精确权重的最终验收见 `probe_audit.json`（由 `audit_probe_run.py` 在控制器完成后生成）。",
        "运行中发生的纯分析/审计口径修订见 `ANALYSIS_AMENDMENTS.md`；其中逐次记录修改时间、理由和当时的结果可见性。",
    ]
    )
    (run_dir / "PROBE_15H_REPORT.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Probe analysis written to {run_dir / 'PROBE_15H_REPORT.md'}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
