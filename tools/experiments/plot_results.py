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

from summarize_results import controlled_target_f, geomean, load_records


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


def performance_profile(records: list[dict], methods: list[str], output: Path, stem: str) -> None:
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
            for method in methods:
                ratios[method].append(math.inf)
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
    # Cover every finite ratio so the right endpoint reflects only real timeouts.
    maximum = max(2.0, max(finite, default=2.0) * 1.05)
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
    axis.set_ylabel("Fraction of all instances")
    axis.grid(True, which="both", alpha=0.25)
    axis.legend(frameon=False)
    save(fig, output, stem)


def timeout_label(records: list[dict]) -> str:
    limits = sorted({float(row["timeout_seconds"]) for row in records})
    if len(limits) != 1:
        raise ValueError(f"A paper panel must use one timeout limit, found {limits}")
    value = limits[0]
    return f"{value:,.0f} s" if value.is_integer() else f"{value:g} s"


def make_facets(names: list[str], columns: int, width: float = 4.0, height: float = 3.2) -> tuple[plt.Figure, list[plt.Axes]]:
    rows = math.ceil(len(names) / columns)
    fig, grid = plt.subplots(rows, columns, figsize=(width * columns, height * rows), squeeze=False)
    axes = list(grid.flat)
    for axis, name in zip(axes, names):
        axis.set_title(name)
    for axis in axes[len(names):]:
        axis.set_visible(False)
    return fig, axes[:len(names)]


def aggregate_metric(group: list[dict], metric: str) -> float:
    if metric == "completion":
        return sum(row["status"] == "ok" for row in group) / len(group)
    if metric == "par2":
        return sum(float(row["solver_seconds"]) if row["status"] == "ok" else 2.0 * float(row["timeout_seconds"]) for row in group) / len(group)
    raise ValueError(f"Unknown metric: {metric}")


def denominator_label(groups: list[list[dict]], unit: str) -> str:
    sizes = sorted({len(group) for group in groups})
    size = str(sizes[0]) if len(sizes) == 1 else f"{sizes[0]}--{sizes[-1]}"
    return f"n={size}/method/{unit}"


def paired_speedups(records: list[dict], contenders: list[str], baseline: str) -> dict[tuple[str, int, int | None, str], list[float]]:
    indexed = {
        (row.get("run_id", ""), row["suite"], row["case_id"], row["query_index"], row["method"]): row
        for row in records
    }
    ratios: dict[tuple[str, int, int | None, str], list[float]] = defaultdict(list)
    for row in records:
        if row["method"] not in contenders or row["status"] != "ok":
            continue
        base = indexed.get((row.get("run_id", ""), row["suite"], row["case_id"], row["query_index"], baseline))
        if base is None or base["status"] != "ok" or float(row["solver_seconds"]) <= 0 or float(base["solver_seconds"]) <= 0:
            continue
        ratios[(row["dataset"], int(row["g"]), controlled_target_f(row), row["method"])].append(float(base["solver_seconds"]) / float(row["solver_seconds"]))
    return ratios


def plot_p2_metric(records: list[dict], methods: list[str], output: Path, metric: str) -> None:
    p2 = [row for row in records if row["suite"] == "P2_cross_g" and row["method"] in methods]
    if not p2:
        return
    datasets = sorted({row["dataset"] for row in p2})
    grouped: dict[tuple[str, int, str], list[dict]] = defaultdict(list)
    for row in p2:
        grouped[(row["dataset"], int(row["g"]), row["method"])].append(row)
    fig, axes = make_facets(datasets, min(3, len(datasets)))
    for axis, dataset in zip(axes, datasets):
        for method in methods:
            points = sorted((g, aggregate_metric(group, metric)) for (name, g, current), group in grouped.items() if name == dataset and current == method)
            if points:
                axis.plot([point[0] for point in points], [point[1] for point in points], marker="o", label=LABELS.get(method, method))
        axis.set_title(f"{dataset} ({denominator_label([group for (name, _, _), group in grouped.items() if name == dataset], 'g')})")
        axis.set_xticks(sorted({g for name, g, _ in grouped if name == dataset}))
        axis.set_xlabel("Number of groups g")
        axis.grid(True, alpha=0.25)
        if metric == "completion":
            axis.set_ylim(0, 1.05)
            axis.set_ylabel(f"Completion within {timeout_label(p2)}")
        else:
            axis.set_yscale("log")
            axis.set_ylabel("PAR-2 seconds (all queries)")
        axis.legend(frameon=False, fontsize="small")
    save(fig, output, f"p2_{metric}_by_g")


def plot_p2_speedup(records: list[dict], contenders: list[str], baseline: str, output: Path) -> None:
    p2 = [row for row in records if row["suite"] == "P2_cross_g"]
    if not p2:
        return
    datasets = sorted({row["dataset"] for row in p2})
    ratios = paired_speedups(p2, contenders, baseline)
    fig, axes = make_facets(datasets, min(3, len(datasets)))
    for axis, dataset in zip(axes, datasets):
        for contender in contenders:
            points = sorted((g, geomean(values)) for (name, g, target_f, method), values in ratios.items() if name == dataset and target_f is None and method == contender and values)
            if points:
                axis.plot([point[0] for point in points], [point[1] for point in points], marker="o", label=LABELS.get(contender, contender))
        axis.axhline(1.0, color="black", linewidth=0.8, linestyle="--")
        axis.set_yscale("log")
        axis.set_xticks(sorted({int(row["g"]) for row in p2 if row["dataset"] == dataset}))
        axis.set_xlabel("Number of groups g")
        axis.set_ylabel(f"Speedup over {LABELS.get(baseline, baseline)}\n(mutually solved only)")
        axis.grid(True, which="both", alpha=0.25)
        axis.legend(frameon=False, fontsize="small")
    save(fig, output, "p2_paired_speedup_by_g")


def plot_s2_metric(records: list[dict], methods: list[str], output: Path, metric: str) -> None:
    s2 = [row for row in records if row["suite"] == "S2_controlled_gf" and row["method"] in methods]
    if not s2:
        return
    facets = [(dataset, g) for dataset in sorted({row["dataset"] for row in s2}) for g in sorted({int(row["g"]) for row in s2 if row["dataset"] == dataset})]
    grouped: dict[tuple[str, int, int, str], list[dict]] = defaultdict(list)
    for row in s2:
        target_f = controlled_target_f(row)
        if target_f is None:
            raise ValueError(f"S2 record has no target f: {row['case_id']}")
        grouped[(row["dataset"], int(row["g"]), target_f, row["method"])].append(row)
    fig, axes = make_facets([f"{dataset}, g={g}" for dataset, g in facets], max(1, len({g for _, g in facets})), width=3.25, height=3.0)
    for axis, (dataset, g) in zip(axes, facets):
        for method in methods:
            points = sorted((target_f, aggregate_metric(group, metric)) for (name, current_g, target_f, current), group in grouped.items() if name == dataset and current_g == g and current == method)
            if points:
                axis.plot([point[0] for point in points], [point[1] for point in points], marker="o", label=LABELS.get(method, method))
        axis.set_title(f"{dataset}, g={g} ({denominator_label([group for (name, current_g, _, _), group in grouped.items() if name == dataset and current_g == g], 'f')})")
        axis.set_xscale("log", base=2)
        axis.set_xticks(sorted({target_f for name, current_g, target_f, _ in grouped if name == dataset and current_g == g}))
        axis.tick_params(axis="x", labelrotation=45)
        axis.set_xlabel("Target mean group size f")
        axis.grid(True, which="both", alpha=0.25)
        if metric == "completion":
            axis.set_ylim(0, 1.05)
            axis.set_ylabel(f"Completion within {timeout_label(s2)}")
        else:
            axis.set_yscale("log")
            axis.set_ylabel("PAR-2 seconds (all queries)")
        axis.legend(frameon=False, fontsize="x-small")
    save(fig, output, f"s2_{metric}_by_f")


def plot_s2_speedup(records: list[dict], contenders: list[str], baseline: str, output: Path) -> None:
    s2 = [row for row in records if row["suite"] == "S2_controlled_gf"]
    if not s2:
        return
    facets = [(dataset, g) for dataset in sorted({row["dataset"] for row in s2}) for g in sorted({int(row["g"]) for row in s2 if row["dataset"] == dataset})]
    ratios = paired_speedups(s2, contenders, baseline)
    fig, axes = make_facets([f"{dataset}, g={g}" for dataset, g in facets], max(1, len({g for _, g in facets})), width=3.25, height=3.0)
    for axis, (dataset, g) in zip(axes, facets):
        plotted = False
        for contender in contenders:
            points = sorted((target_f, geomean(values)) for (name, current_g, target_f, method), values in ratios.items() if name == dataset and current_g == g and target_f is not None and method == contender and values)
            if points:
                axis.plot([point[0] for point in points], [point[1] for point in points], marker="o", label=LABELS.get(contender, contender))
                plotted = True
        axis.axhline(1.0, color="black", linewidth=0.8, linestyle="--")
        axis.set_xscale("log", base=2)
        axis.set_yscale("log")
        axis.set_xticks(sorted({controlled_target_f(row) for row in s2 if row["dataset"] == dataset and int(row["g"]) == g}))
        axis.tick_params(axis="x", labelrotation=45)
        axis.set_xlabel("Target mean group size f")
        axis.set_ylabel(f"Speedup over {LABELS.get(baseline, baseline)}\n(mutually solved only)")
        axis.grid(True, which="both", alpha=0.25)
        if plotted:
            axis.legend(frameon=False, fontsize="x-small")
        else:
            axis.text(0.5, 0.5, "No mutually solved pairs", ha="center", va="center", transform=axis.transAxes, fontsize="small")
    save(fig, output, "s2_paired_speedup_by_f")


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
    for suite in sorted({row["suite"] for row in records}):
        performance_profile([row for row in records if row["suite"] == suite], methods, output, f"{suite.lower()}_performance_profile")
    contenders = [method for method in methods if method != args.baseline]
    plot_p2_metric(records, methods, output, "completion")
    plot_p2_metric(records, methods, output, "par2")
    plot_p2_speedup(records, contenders, args.baseline, output)
    plot_s2_metric(records, methods, output, "completion")
    plot_s2_metric(records, methods, output, "par2")
    plot_s2_speedup(records, contenders, args.baseline, output)
    print(f"Wrote PDF and PNG figures to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
