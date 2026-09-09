#!/usr/bin/env python3
"""Aggregate experiment records without hiding timeouts or unpaired runs."""

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


def percentile(values: list[float], probability: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1 - fraction) + ordered[upper] * fraction


def geomean(values: Iterable[float]) -> float | None:
    positive = [value for value in values if value > 0]
    if not positive:
        return None
    return math.exp(sum(math.log(value) for value in positive) / len(positive))


def wilson_interval(successes: int, total: int, z: float = 1.959963984540054) -> tuple[float | None, float | None]:
    if total <= 0:
        return None, None
    proportion = successes / total
    denominator = 1.0 + z * z / total
    center = (proportion + z * z / (2.0 * total)) / denominator
    margin = z * math.sqrt(
        proportion * (1.0 - proportion) / total + z * z / (4.0 * total * total)
    ) / denominator
    return max(0.0, center - margin), min(1.0, center + margin)


def bootstrap_geomean_interval(
    values: list[float], seed_text: str, repetitions: int = 10000
) -> tuple[float | None, float | None]:
    positive = [value for value in values if value > 0]
    if not positive:
        return None, None
    if len(positive) == 1:
        return positive[0], positive[0]
    seed = int.from_bytes(hashlib.sha256(seed_text.encode()).digest()[:8], "big")
    rng = random.Random(seed)
    logs = [math.log(value) for value in positive]
    estimates = []
    for _ in range(repetitions):
        estimates.append(math.exp(sum(rng.choice(logs) for _ in logs) / len(logs)))
    return percentile(estimates, 0.025), percentile(estimates, 0.975)


def load_records(inputs: list[Path]) -> list[dict[str, Any]]:
    by_key: dict[tuple[str, str], dict[str, Any]] = {}
    for input_path in inputs:
        path = input_path if input_path.is_absolute() else ROOT / input_path
        files = list((path / "records").glob("*.json")) if path.is_dir() else [path]
        for file in files:
            if file.suffix == ".jsonl":
                records = [json.loads(line) for line in file.read_text(encoding="utf-8").splitlines() if line]
            else:
                records = [json.loads(file.read_text(encoding="utf-8"))]
            for record in records:
                # The task key is deliberately stable for resume within one
                # run.  Include run_id here so independent repetitions are
                # retained rather than silently collapsed.
                key = (str(record.get("run_id", "")), record["task_key"])
                previous = by_key.get(key)
                if previous is None or record.get("finished_at", "") >= previous.get("finished_at", ""):
                    by_key[key] = record
    return list(by_key.values())


def write_csv(path: Path, rows: list[dict[str, Any]], fields: list[str] | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if fields is None:
        fields = list(rows[0]) if rows else []
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def controlled_target_f(record: dict[str, Any]) -> int | None:
    if record.get("suite") != "S2_controlled_gf":
        return None
    return int(str(record["case_id"]).rsplit("_", 1)[1])


def summarize_cells(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[Any, ...], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        grouped[(record["suite"], record["dataset"], record["g"], controlled_target_f(record), record["method"])].append(record)
    rows: list[dict[str, Any]] = []
    for (suite, dataset, g, target_f, method), group in sorted(grouped.items()):
        solved = [record for record in group if record["status"] == "ok"]
        times = [float(record["solver_seconds"]) for record in solved]
        memories = [
            float(record["query_memory_peak_mib"])
            for record in solved
            if record.get("query_memory_peak_mib") is not None
        ]
        state_counts = [
            int(record["mask_vertex_states"])
            for record in solved
            if record.get("mask_vertex_states") is not None
        ]
        timeout = float(group[0]["timeout_seconds"])
        penalized = [
            float(record["solver_seconds"])
            if record["status"] == "ok"
            else 2.0 * timeout
            for record in group
        ]
        completion_low, completion_high = wilson_interval(len(solved), len(group))
        solved_at_1000 = sum(
            record["status"] == "ok" and float(record["solver_seconds"]) <= 1000.0
            for record in group
        )
        completion_1000_low, completion_1000_high = wilson_interval(
            solved_at_1000, len(group)
        )
        rows.append(
            {
                "suite": suite,
                "dataset": dataset,
                "g": g,
                "target_f": target_f,
                "method": method,
                "instances": len(group),
                "solved": len(solved),
                "timeouts": sum(record["status"] == "timeout" for record in group),
                "errors": sum(record["status"] not in ("ok", "timeout") for record in group),
                "completion_rate": len(solved) / len(group),
                "completion_ci95_low": completion_low,
                "completion_ci95_high": completion_high,
                "solved_within_1000_seconds": solved_at_1000,
                "completion_rate_at_1000_seconds": solved_at_1000 / len(group),
                "completion_at_1000_ci95_low": completion_1000_low,
                "completion_at_1000_ci95_high": completion_1000_high,
                "mean_f": statistics.fmean(float(record["mean_f"]) for record in group),
                "min_realized_mean_f": min(float(record["mean_f"]) for record in group),
                "max_realized_mean_f": max(float(record["mean_f"]) for record in group),
                "mean_solved_seconds": statistics.fmean(times) if times else None,
                "median_solved_seconds": percentile(times, 0.5),
                "p90_solved_seconds": percentile(times, 0.9),
                "geomean_solved_seconds": geomean(times),
                "par2_seconds": statistics.fmean(penalized),
                "query_memory_records": len(memories),
                "median_peak_mib": percentile(memories, 0.5),
                "p90_peak_mib": percentile(memories, 0.9),
                "peak_query_memory_mib_on_completed": max(memories) if memories else None,
                "state_count_records": len(state_counts),
                "mean_mask_vertex_states": (
                    statistics.fmean(state_counts) if state_counts else None
                ),
                "median_mask_vertex_states": percentile(state_counts, 0.5),
                "p90_mask_vertex_states": percentile(state_counts, 0.9),
            }
        )
    return rows


def summarize_datasets(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Aggregate every query block of a graph into one reporting row.

    P1 deliberately keeps GPU4GST's g=3/5/7 files separate while running, but
    the paper reports their combined workload.  A total is only labelled as
    the observed total when every query completed; the capped total charges
    one per-instance timeout limit to every unfinished query.
    """

    grouped: dict[tuple[str, str, str], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        grouped[(record["suite"], record["dataset"], record["method"])].append(record)
    rows: list[dict[str, Any]] = []
    for (suite, dataset, method), group in sorted(grouped.items()):
        solved = [record for record in group if record["status"] == "ok"]
        solved_times = [float(record["solver_seconds"]) for record in solved]
        solved_state_counts = [
            int(record["mask_vertex_states"])
            for record in solved
            if record.get("mask_vertex_states") is not None
        ]
        solved_memories = [
            float(record["query_memory_peak_mib"])
            for record in solved
            if record.get("query_memory_peak_mib") is not None
        ]
        timeout = float(group[0]["timeout_seconds"])
        unfinished = len(group) - len(solved)
        observed_total = sum(solved_times)
        rows.append(
            {
                "suite": suite,
                "dataset": dataset,
                "method": method,
                "query_blocks": len({record["case_id"] for record in group}),
                "g_values": ";".join(
                    map(str, sorted({int(record["g"]) for record in group}))
                ),
                "instances": len(group),
                "solved": len(solved),
                "timeouts": sum(record["status"] == "timeout" for record in group),
                "errors": sum(
                    record["status"] not in ("ok", "timeout") for record in group
                ),
                "completed_infeasible": sum(
                    record.get("solution_status") == "infeasible" for record in group
                ),
                "feasibility_mismatches": sum(
                    record["status"] == "ok"
                    and record.get("solution_status") is not None
                    and record.get("expected_solution_status") is not None
                    and record.get("expected_solution_status")
                    != record.get("solution_status")
                    for record in group
                ),
                "all_solved": unfinished == 0,
                "observed_total_seconds_if_all_solved": (
                    observed_total if unfinished == 0 else None
                ),
                "solved_query_seconds": observed_total,
                "capped_total_seconds": observed_total + unfinished * timeout,
                "mean_solved_seconds": (
                    statistics.fmean(solved_times) if solved_times else None
                ),
                "geomean_solved_seconds": geomean(solved_times),
                "query_memory_records": len(solved_memories),
                "peak_query_memory_mib_on_completed": max(solved_memories) if solved_memories else None,
                "observed_peak_query_memory_mib_if_all_solved": (
                    max(solved_memories)
                    if unfinished == 0 and len(solved_memories) == len(solved)
                    else None
                ),
                "state_count_records": len(solved_state_counts),
                "solved_query_mask_vertex_states": (
                    sum(solved_state_counts) if solved_state_counts else None
                ),
                "observed_total_mask_vertex_states_if_all_solved": (
                    sum(solved_state_counts)
                    if unfinished == 0 and len(solved_state_counts) == len(solved)
                    else None
                ),
                "per_instance_timeout_seconds": timeout,
            }
        )
    return rows


def paired_rows(
    records: list[dict[str, Any]], baseline: str, contenders: list[str]
) -> list[dict[str, Any]]:
    indexed = {
        (
            record.get("run_id", ""),
            record["suite"],
            record["case_id"],
            record["query_index"],
            record["method"],
        ): record
        for record in records
    }
    groups: dict[tuple[str, str, int, int | None, str], list[tuple[dict[str, Any], dict[str, Any]]]] = defaultdict(list)
    for record in records:
        contender = record["method"]
        if contender not in contenders:
            continue
        baseline_record = indexed.get(
            (
                record.get("run_id", ""),
                record["suite"],
                record["case_id"],
                record["query_index"],
                baseline,
            )
        )
        if baseline_record is not None:
            groups[(record["suite"], record["dataset"], record["g"], controlled_target_f(record), contender)].append(
                (record, baseline_record)
            )
    rows: list[dict[str, Any]] = []
    for (suite, dataset, g, target_f, contender), pairs in sorted(groups.items()):
        both = [
            (ours, base)
            for ours, base in pairs
            if ours["status"] == "ok" and base["status"] == "ok"
        ]
        ratios = [
            float(base["solver_seconds"]) / float(ours["solver_seconds"])
            for ours, base in both
            if float(ours["solver_seconds"]) > 0 and float(base["solver_seconds"]) > 0
        ]
        state_ratios = [
            int(base["mask_vertex_states"]) / int(ours["mask_vertex_states"])
            for ours, base in both
            if ours.get("mask_vertex_states") is not None
            and base.get("mask_vertex_states") is not None
            and int(ours["mask_vertex_states"]) > 0
            and int(base["mask_vertex_states"]) > 0
        ]
        speedup_low, speedup_high = bootstrap_geomean_interval(
            ratios, f"{suite}|{dataset}|{g}|{target_f}|{contender}|{baseline}"
        )
        rows.append(
            {
                "suite": suite,
                "dataset": dataset,
                "g": g,
                "target_f": target_f,
                "contender": contender,
                "baseline": baseline,
                "paired_instances": len(pairs),
                "both_solved": len(both),
                "baseline_timeout_contender_solved": sum(
                    ours["status"] == "ok" and base["status"] == "timeout"
                    for ours, base in pairs
                ),
                "contender_timeout_baseline_solved": sum(
                    ours["status"] == "timeout" and base["status"] == "ok"
                    for ours, base in pairs
                ),
                "both_timeout": sum(
                    ours["status"] == "timeout" and base["status"] == "timeout"
                    for ours, base in pairs
                ),
                "geomean_speedup_on_both_solved": geomean(ratios),
                "geomean_speedup_ci95_low": speedup_low,
                "geomean_speedup_ci95_high": speedup_high,
                "median_speedup_on_both_solved": percentile(ratios, 0.5),
                "p10_speedup_on_both_solved": percentile(ratios, 0.1),
                "p90_speedup_on_both_solved": percentile(ratios, 0.9),
                "contender_faster_on_both_solved": sum(ratio > 1 for ratio in ratios),
                "paired_positive_state_counts": len(state_ratios),
                "geomean_baseline_over_contender_mask_vertex_states": geomean(
                    state_ratios
                ),
                "median_baseline_over_contender_mask_vertex_states": percentile(
                    state_ratios, 0.5
                ),
            }
        )
    return rows


def summarize_p2_tranches(records: list[dict[str, Any]], manifest_path: Path, expected_methods: list[str]) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    """Join P2 records to the frozen two-tranche selection without changing the ledger."""

    path = manifest_path if manifest_path.is_absolute() else ROOT / manifest_path
    selection_rows = json.loads(path.read_text(encoding="utf-8"))
    selection: dict[tuple[str, int, int], dict[str, Any]] = {}
    duplicate_selection_keys: list[tuple[str, int, int]] = []
    for row in selection_rows:
        key = (str(row["dataset"]), int(row["g"]), int(row["panel_query_index"]))
        if key in selection:
            duplicate_selection_keys.append(key)
        selection[key] = row

    selection_panels: dict[tuple[str, int, int], list[dict[str, Any]]] = defaultdict(list)
    for row in selection_rows:
        selection_panels[(str(row["dataset"]), int(row["g"]), int(row["panel_tranche"]))].append(row)
    invalid_selection_panels: list[dict[str, Any]] = []
    for (dataset, g, tranche), group in sorted(selection_panels.items()):
        query_indices = sorted(int(row["panel_query_index"]) for row in group)
        size_strata = sorted(int(row["size_stratum"]) for row in group)
        expected_query_indices = list(range(1, 6)) if tranche == 1 else (list(range(6, 11)) if tranche == 2 else [])
        if len(group) != 5 or query_indices != expected_query_indices or size_strata != [1, 2, 3, 4, 5]:
            invalid_selection_panels.append(
                {
                    "dataset": dataset,
                    "g": g,
                    "panel_tranche": tranche,
                    "entries": len(group),
                    "panel_query_indices": query_indices,
                    "size_strata": size_strata,
                }
            )

    p2 = [record for record in records if record["suite"] == "P2_cross_g"]
    observed_methods = sorted({str(record["method"]) for record in p2})
    methods = list(dict.fromkeys(expected_methods))
    missing_methods = sorted(set(methods) - set(observed_methods))
    unexpected_methods = sorted(set(observed_methods) - set(methods))
    joined: list[tuple[dict[str, Any], dict[str, Any]]] = []
    unexpected_records: list[str] = []
    duplicate_record_keys: list[tuple[str, int, int, str]] = []
    mean_f_mismatches: list[dict[str, Any]] = []
    seen_records: set[tuple[str, int, int, str]] = set()
    for record in p2:
        selection_key = (str(record["dataset"]), int(record["g"]), int(record["query_index"]))
        panel = selection.get(selection_key)
        record_key = (*selection_key, str(record["method"]))
        if record_key in seen_records:
            duplicate_record_keys.append(record_key)
        seen_records.add(record_key)
        if panel is None:
            unexpected_records.append(str(record["task_key"]))
            continue
        difference = abs(float(record["mean_f"]) - float(panel["mean_f"]))
        if difference > 1e-9 * max(1.0, abs(float(panel["mean_f"]))):
            mean_f_mismatches.append(
                {
                    "task_key": record["task_key"],
                    "record_mean_f": record["mean_f"],
                    "selection_mean_f": panel["mean_f"],
                }
            )
        joined.append((record, panel))

    expected_record_keys = {
        (dataset, g, query_index, method)
        for dataset, g, query_index in selection
        for method in methods
    }
    missing_record_keys = sorted(expected_record_keys - seen_records)
    grouped: dict[tuple[str, int, int, str], list[tuple[dict[str, Any], dict[str, Any]]]] = defaultdict(list)
    for record, panel in joined:
        grouped[(str(record["dataset"]), int(record["g"]), int(panel["panel_tranche"]), str(record["method"]))].append((record, panel))

    summary_rows: list[dict[str, Any]] = []
    invalid_tranche_sizes: list[tuple[str, int, int, str, int]] = []
    for (dataset, g, tranche, method), group in sorted(grouped.items()):
        if len(group) != 5:
            invalid_tranche_sizes.append((dataset, g, tranche, method, len(group)))
        timeout = float(group[0][0]["timeout_seconds"])
        solved = [record for record, _ in group if record["status"] == "ok"]
        penalized = [float(record["solver_seconds"]) if record["status"] == "ok" else 2.0 * timeout for record, _ in group]
        summary_rows.append(
            {
                "dataset": dataset,
                "g": g,
                "panel_tranche": tranche,
                "method": method,
                "panel_query_indices": ";".join(map(str, sorted(int(panel["panel_query_index"]) for _, panel in group))),
                "size_strata": ";".join(map(str, sorted(int(panel["size_stratum"]) for _, panel in group))),
                "instances": len(group),
                "solved": len(solved),
                "timeouts": sum(record["status"] == "timeout" for record, _ in group),
                "completion_rate": len(solved) / len(group),
                "mean_solved_seconds": statistics.fmean(float(record["solver_seconds"]) for record in solved) if solved else None,
                "par2_seconds": statistics.fmean(penalized),
            }
        )

    structural_errors = bool(duplicate_selection_keys or invalid_selection_panels or duplicate_record_keys or unexpected_records or mean_f_mismatches or unexpected_methods)
    complete = not missing_record_keys and not missing_methods and not invalid_tranche_sizes and not structural_errors
    audit = {
        "schema_version": 1,
        "status": "pass" if complete else ("fail" if structural_errors else "partial"),
        "selection_manifest": str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path),
        "selection_manifest_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "selection_entries": len(selection_rows),
        "selection_unique_keys": len(selection),
        "selection_duplicate_keys": [list(key) for key in duplicate_selection_keys],
        "invalid_selection_panels": invalid_selection_panels,
        "p2_records": len(p2),
        "expected_methods": methods,
        "observed_methods": observed_methods,
        "missing_methods": missing_methods,
        "unexpected_methods": unexpected_methods,
        "expected_records": len(selection) * len(methods),
        "joined_records": len(joined),
        "missing_record_count": len(missing_record_keys),
        "missing_record_examples": [list(key) for key in missing_record_keys[:20]],
        "duplicate_record_keys": [list(key) for key in duplicate_record_keys],
        "unexpected_record_task_keys": unexpected_records,
        "mean_f_mismatches": mean_f_mismatches,
        "tranche_summary_rows": len(summary_rows),
        "invalid_tranche_sizes": [list(item) for item in invalid_tranche_sizes],
        "full_panel_complete": complete,
        "join_key": ["dataset", "g", "panel_query_index"],
        "tranche_semantics": "panel_tranche 1 is query indices 1--5 and panel_tranche 2 is query indices 6--10; both use five frozen, independently selected queries per dataset/g cell",
        "formal_ledger_mutation": "none; this is a reporting-only join",
    }
    return summary_rows, audit


def quality_rows(
    records: list[dict[str, Any]], tolerance: float, include_audit_methods: bool
) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, str, str, int], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        if not include_audit_methods and (
            record.get("exact_claim") is False
            or record.get("method") == "pruneddp_strict"
        ):
            continue
        if record["status"] == "ok" and record.get("weight") is not None:
            grouped[
                (
                    str(record.get("run_id", "")),
                    record["suite"],
                    record["case_id"],
                    record["query_index"],
                )
            ].append(record)
    mismatches: list[dict[str, Any]] = []
    for key, group in sorted(grouped.items()):
        weights = [float(record["weight"]) for record in group]
        expected_values = [
            float(record["expected_weight"])
            for record in group
            if record.get("expected_weight") is not None
        ]
        reference = expected_values[0] if expected_values else min(weights)
        scale = max(1.0, abs(reference))
        bad = [
            record
            for record in group
            if abs(float(record["weight"]) - reference) > tolerance * scale
        ]
        if bad:
            mismatches.append(
                {
                    "run_id": key[0],
                    "suite": key[1],
                    "case_id": key[2],
                    "query_index": key[3],
                    "reference_weight": reference,
                    "methods_and_weights": ";".join(
                        f"{record['method']}={record['weight']}" for record in group
                    ),
                }
            )
    return mismatches


def feasibility_rows(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for record in records:
        if (
            record.get("status") != "ok"
            or record.get("solution_status") is None
            or record.get("expected_solution_status") is None
        ):
            continue
        if record.get("expected_solution_status") == record.get("solution_status"):
            continue
        rows.append(
            {
                "run_id": record.get("run_id", ""),
                "suite": record["suite"],
                "case_id": record["case_id"],
                "query_index": record["query_index"],
                "method": record["method"],
                "expected_solution_status": record.get("expected_solution_status"),
                "actual_solution_status": record.get("solution_status"),
                "weight": record.get("weight"),
            }
        )
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, action="append", default=[])
    parser.add_argument(
        "--input-glob",
        action="append",
        default=[],
        help="workspace-relative glob for shard directories; may be repeated",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", default="pruneddp_safe")
    parser.add_argument(
        "--contender", action="append", help="repeat to override the default Base/Enhanced contenders")
    parser.add_argument("--weight-tolerance", type=float, default=1e-6)
    parser.add_argument(
        "--p2-selection-manifest",
        type=Path,
        default=Path("experiment_data/p2_cross_g/selected_queries.json"),
        help="frozen P2 panel manifest used for reporting-only tranche joins",
    )
    parser.add_argument(
        "--include-audit-methods-in-quality",
        action="store_true",
        help="include methods that explicitly make no exactness claim",
    )
    args = parser.parse_args()
    inputs = list(args.input)
    for pattern in args.input_glob:
        inputs.extend(sorted(ROOT.glob(pattern)))
    if not inputs:
        parser.error("at least one --input or --input-glob is required")
    output = args.output if args.output.is_absolute() else ROOT / args.output
    records = load_records(inputs)
    if not records:
        raise ValueError("No records found")
    contenders = args.contender or ["abhss_base", "abhss_enhanced"]

    cell_rows = summarize_cells(records)
    dataset_rows = summarize_datasets(records)
    paired = paired_rows(records, args.baseline, contenders)
    mismatches = quality_rows(
        records, args.weight_tolerance, args.include_audit_methods_in_quality
    )
    feasibility_mismatches = feasibility_rows(records)
    p2_tranches, p2_tranche_audit = summarize_p2_tranches(records, args.p2_selection_manifest, [args.baseline, *contenders])
    write_csv(output / "summary_by_cell.csv", cell_rows)
    write_csv(output / "summary_by_dataset.csv", dataset_rows)
    write_csv(output / "paired_speedups.csv", paired)
    write_csv(
        output / "p2_summary_by_tranche.csv",
        p2_tranches,
        [
            "dataset",
            "g",
            "panel_tranche",
            "method",
            "panel_query_indices",
            "size_strata",
            "instances",
            "solved",
            "timeouts",
            "completion_rate",
            "mean_solved_seconds",
            "par2_seconds",
        ],
    )
    (output / "p2_tranche_audit.json").write_text(json.dumps(p2_tranche_audit, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    write_csv(
        output / "quality_mismatches.csv",
        mismatches,
        [
            "run_id",
            "suite",
            "case_id",
            "query_index",
            "reference_weight",
            "methods_and_weights",
        ],
    )
    write_csv(
        output / "feasibility_mismatches.csv",
        feasibility_mismatches,
        [
            "run_id",
            "suite",
            "case_id",
            "query_index",
            "method",
            "expected_solution_status",
            "actual_solution_status",
            "weight",
        ],
    )
    (output / "summary_metadata.json").write_text(
        json.dumps(
            {
                "records": len(records),
                "cells": len(cell_rows),
                "dataset_rows": len(dataset_rows),
                "paired_cells": len(paired),
                "p2_tranche_rows": len(p2_tranches),
                "p2_tranche_audit_status": p2_tranche_audit["status"],
                "quality_mismatches": len(mismatches),
                "feasibility_mismatches": len(feasibility_mismatches),
                "baseline": args.baseline,
                "contenders": contenders,
                "audit_methods_in_quality_check": args.include_audit_methods_in_quality,
                "timeout_handling": "PAR-2; timeout/error contributes 2 * per-instance limit",
                "dataset_total_handling": "observed total is reported only when all queries finish; capped total charges one per-instance limit to each unfinished query",
                "speedup_censoring": "geomean uses only mutually solved pairs; timeout directions reported separately",
                "state_count_handling": "mask_vertex_states counts distinct main-state entries first admitted by a completed query; state ratios use mutually solved pairs with positive counts only; timed-out queries have no final count",
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    print(
        f"Summarized {len(records)} records; {len(mismatches)} weight mismatches, "
        f"{len(feasibility_mismatches)} feasibility mismatches, "
        f"P2 tranche audit {p2_tranche_audit['status']} -> {output}"
    )
    return 1 if mismatches or feasibility_mismatches or p2_tranche_audit["status"] == "fail" else 0


if __name__ == "__main__":
    raise SystemExit(main())
