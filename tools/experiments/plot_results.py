#!/usr/bin/env python3
"""Create the preregistered paper plots from supervisor JSON records."""

from __future__ import annotations

import argparse
from collections import defaultdict
import math
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from summarize_results import geomean, load_records


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_METHODS = ("abhss_base", "abhss_enhanced", "pruneddp_safe")
LABELS = {
    "abhss_base": "ABHSS-Base",
    "abhss_enhanced": "ABHSS-Enhanced",
    "pruneddp_safe": "PrunedDP++-Safe",
}


def save(fig: plt.Figure, output: Path, stem: str) -> None:
    fig.tight_layout()
    fig.savefig(output / f"{stem}.pdf", bbox_inches="tight")
    fig.savefig(output / f"{stem}.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


def performance_profile(records: list[dict], methods: list[str], output: Path) -> None:
    instances: dict[tuple, dict[str, dict]] = defaultdict(dict)
    for row in records:
        if row["method"] in methods:
            key = (row.get("run_id", ""), row["suite"], row["case_id"], row["query_index"])
            instances[key][row["method"]] = row
    ratios: dict[str, list[float]] = {method: [] for method in methods}
    for rows in instances.values():
        solved_times = [
            float(row["solver_seconds"])
            for row in rows.values()
            if row["status"] == "ok" and float(row["solver_seconds"]) > 0
        ]
        if not solved_times:
            continue
        best = min(solved_times)
        for method in methods:
            row = rows.get(method)
            ratios[method].append(
                float(row["solver_seconds"]) / best
                if row is not None and row["status"] == "ok"
                else math.inf
            )
    finite = [value for values in ratios.values() for value in values if math.isfinite(value)]
    maximum = max(2.0, min(1e4, max(finite, default=2.0) * 1.05))
    grid = [10 ** (index * math.log10(maximum) / 300) for index in range(301)]
    fig, axis = plt.subplots(figsize=(6.2, 4.0))
    for method in methods:
        values = ratios[method]
        if not values:
            continue
        axis.step(
            grid,
            [sum(value <= threshold for value in values) / len(values) for threshold in grid],
            where="post",
            label=LABELS.get(method, method),
        )
    axis.set_xscale("log")
    axis.set_ylim(0, 1.01)
    axis.set_xlabel("Performance ratio to fastest exact method")
    axis.set_ylabel("Fraction of instances")
    axis.grid(True, which="both", alpha=0.25)
    axis.legend(frameon=False)
    save(fig, output, "performance_profile")


def completion_by_g(records: list[dict], methods: list[str], output: Path) -> None:
    grouped: dict[tuple[int, str], list[dict]] = defaultdict(list)
    for row in records:
        if row["method"] in methods:
            grouped[(int(row["g"]), row["method"])].append(row)
    fig, axis = plt.subplots(figsize=(6.2, 4.0))
    for method in methods:
        points = sorted(
            (g, sum(row["status"] == "ok" for row in group) / len(group))
            for (g, name), group in grouped.items()
            if name == method
        )
        if points:
            axis.plot(
                [point[0] for point in points],
                [point[1] for point in points],
                marker="o",
                label=LABELS.get(method, method),
            )
    axis.set_xticks(sorted({g for g, _ in grouped}))
    axis.set_ylim(0, 1.05)
    axis.set_xlabel("Number of groups g")
    axis.set_ylabel("Completion rate within 10,000 s")
    axis.grid(True, alpha=0.25)
    axis.legend(frameon=False)
    save(fig, output, "completion_by_g")


def speedup_by_g(
    records: list[dict], contenders: list[str], baseline: str, output: Path
) -> None:
    indexed = {
        (row.get("run_id", ""), row["suite"], row["case_id"], row["query_index"], row["method"]): row
        for row in records
    }
    ratios: dict[tuple[int, str], list[float]] = defaultdict(list)
    for row in records:
        if row["method"] not in contenders or row["status"] != "ok":
            continue
        base = indexed.get(
            (row.get("run_id", ""), row["suite"], row["case_id"], row["query_index"], baseline)
        )
        if base is not None and base["status"] == "ok" and float(row["solver_seconds"]) > 0:
            ratios[(int(row["g"]), row["method"])].append(
                float(base["solver_seconds"]) / float(row["solver_seconds"])
            )
    fig, axis = plt.subplots(figsize=(6.2, 4.0))
    for contender in contenders:
        points = sorted(
            (g, geomean(values))
            for (g, method), values in ratios.items()
            if method == contender and values
        )
        if points:
            axis.plot(
                [point[0] for point in points],
                [point[1] for point in points],
                marker="o",
                label=LABELS.get(contender, contender),
            )
    axis.axhline(1.0, color="black", linewidth=0.8, linestyle="--")
    axis.set_yscale("log")
    axis.set_xticks(sorted({g for g, _ in ratios}))
    axis.set_xlabel("Number of groups g")
    axis.set_ylabel(f"Geometric-mean speedup over {LABELS.get(baseline, baseline)}\n(mutually solved only)")
    axis.grid(True, which="both", alpha=0.25)
    axis.legend(frameon=False)
    save(fig, output, "paired_speedup_by_g")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, action="append", default=[])
    parser.add_argument("--input-glob", action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--suite", action="append")
    parser.add_argument("--method", action="append")
    parser.add_argument("--baseline", default="pruneddp_safe")
    args = parser.parse_args()
    inputs = list(args.input)
    for pattern in args.input_glob:
        inputs.extend(sorted(ROOT.glob(pattern)))
    if not inputs:
        parser.error("at least one --input or --input-glob is required")
    records = load_records(inputs)
    if args.suite:
        allowed = set(args.suite)
        records = [row for row in records if row["suite"] in allowed]
    methods = args.method or list(DEFAULT_METHODS)
    output = args.output if args.output.is_absolute() else ROOT / args.output
    output.mkdir(parents=True, exist_ok=True)
    performance_profile(records, methods, output)
    completion_by_g(records, methods, output)
    contenders = [method for method in methods if method != args.baseline]
    speedup_by_g(records, contenders, args.baseline, output)
    print(f"Wrote PDF and PNG figures to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
