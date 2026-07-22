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


def summarize_cells(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[Any, ...], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        grouped[(record["suite"], record["dataset"], record["g"], record["method"])].append(record)
    rows: list[dict[str, Any]] = []
    for (suite, dataset, g, method), group in sorted(grouped.items()):
        solved = [record for record in group if record["status"] == "ok"]
        times = [float(record["solver_seconds"]) for record in solved]
        memories = [
            float(record["query_memory_peak_mib"])
            for record in solved
            if record.get("query_memory_peak_mib") is not None
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
                "mean_solved_seconds": statistics.fmean(times) if times else None,
                "median_solved_seconds": percentile(times, 0.5),
                "p90_solved_seconds": percentile(times, 0.9),
                "geomean_solved_seconds": geomean(times),
                "par2_seconds": statistics.fmean(penalized),
                "median_peak_mib": percentile(memories, 0.5),
                "p90_peak_mib": percentile(memories, 0.9),
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
    groups: dict[tuple[str, str, int, str], list[tuple[dict[str, Any], dict[str, Any]]]] = defaultdict(list)
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
            groups[(record["suite"], record["dataset"], record["g"], contender)].append(
                (record, baseline_record)
            )
    rows: list[dict[str, Any]] = []
    for (suite, dataset, g, contender), pairs in sorted(groups.items()):
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
        speedup_low, speedup_high = bootstrap_geomean_interval(
            ratios, f"{suite}|{dataset}|{g}|{contender}|{baseline}"
        )
        rows.append(
            {
                "suite": suite,
                "dataset": dataset,
                "g": g,
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
            }
        )
    return rows


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
        "--contender", action="append", default=["abhss_light", "abhss_heavy"]
    )
    parser.add_argument("--weight-tolerance", type=float, default=1e-6)
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

    cell_rows = summarize_cells(records)
    paired = paired_rows(records, args.baseline, args.contender)
    mismatches = quality_rows(
        records, args.weight_tolerance, args.include_audit_methods_in_quality
    )
    write_csv(output / "summary_by_cell.csv", cell_rows)
    write_csv(output / "paired_speedups.csv", paired)
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
    (output / "summary_metadata.json").write_text(
        json.dumps(
            {
                "records": len(records),
                "cells": len(cell_rows),
                "paired_cells": len(paired),
                "quality_mismatches": len(mismatches),
                "baseline": args.baseline,
                "contenders": args.contender,
                "audit_methods_in_quality_check": args.include_audit_methods_in_quality,
                "timeout_handling": "PAR-2; timeout/error contributes 2 * per-instance limit",
                "speedup_censoring": "geomean uses only mutually solved pairs; timeout directions reported separately",
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    print(
        f"Summarized {len(records)} records; {len(mismatches)} weight mismatches -> {output}"
    )
    return 1 if mismatches else 0


if __name__ == "__main__":
    raise SystemExit(main())
