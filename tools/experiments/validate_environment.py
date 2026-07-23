#!/usr/bin/env python3
"""Validate the frozen P1/P2/S2 paper matrix before a long run."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
PRIMARY_METHODS = {"abhss_base", "abhss_enhanced", "pruneddp_safe"}
P1_DATASETS = {
    "Toronto-MonoGSTPlus",
    "MovieLens-MonoGSTPlus",
    "DBLP-MonoGSTPlus",
    "LinkedMDB-MonoGSTPlus",
    "DBpedia-MonoGSTPlus",
    "Musae-GPU4GST",
    "Twitch-GPU4GST",
    "Github-GPU4GST",
    "Youtube-GPU4GST",
    "DBLP-GPU4GST",
    "Orkut-GPU4GST",
    "LiveJournal-GPU4GST",
    "Reddit-GPU4GST",
}
P2_SIZE_CLASSES = {
    "Musae-GPU4GST": "small",
    "Twitch-GPU4GST": "small",
    "Youtube-GPU4GST": "medium",
    "DBLP-GPU4GST": "medium",
    "Orkut-GPU4GST": "large",
    "Reddit-GPU4GST": "large",
}
S2_DATASETS = {"DBLP-MonoGSTPlus", "IMDb-latest-20260722"}


def load_json(path: str | Path) -> Any:
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = ROOT / candidate
    return json.loads(candidate.read_text(encoding="utf-8"))


def load_runner() -> Any:
    path = ROOT / "tools" / "experiments" / "run_experiments.py"
    spec = importlib.util.spec_from_file_location("abhss_runner", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load experiment runner")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def graph_header(graph_dir: Path) -> tuple[int, int]:
    with (graph_dir / "graph.txt").open("r", encoding="utf-8") as source:
        fields = source.readline().split()
    if len(fields) != 2:
        raise ValueError(f"invalid graph header: {graph_dir / 'graph.txt'}")
    return int(fields[0]), int(fields[1])


def check_hash(path: Path, expected: str | None, failures: list[str], label: str) -> None:
    if not path.is_file():
        failures.append(f"missing {label}: {path}")
        return
    if expected and sha256(path) != expected:
        failures.append(f"{label} hash mismatch: {path}")


def suite(config: dict[str, Any], suite_id: str) -> dict[str, Any]:
    return next(row for row in config["suites"] if row["id"] == suite_id)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--deep",
        action="store_true",
        help="also hash every multi-gigabyte P1 graph",
    )
    parser.add_argument(
        "--require-binaries",
        action="store_true",
        help="require every executable, including optional third-party correctness gates",
    )
    parser.add_argument(
        "--require-performance-binaries",
        action="store_true",
        help="require only ABHSS Base/Enhanced and PrunedDP++-Safe performance binaries",
    )
    args = parser.parse_args()
    failures: list[str] = []
    warnings: list[str] = []

    config = load_json("experiments/paper_matrix.json")
    if config.get("schema_version") != 4:
        failures.append("paper_matrix schema_version must be 4")
    if config.get("submission_target") != "SIGMOD or VLDB":
        failures.append("submission target changed")
    if config.get("primary_baseline") != "pruneddp_safe":
        failures.append("primary baseline must be pruneddp_safe")
    if float(config.get("timeout_seconds", 0)) != 10_000:
        failures.append("per-query timeout must be 10,000 seconds")

    base_method = config["methods"].get("abhss_base", {})
    enhanced_method = config["methods"].get("abhss_enhanced", {})
    expected_chain = {
        "base": [],
        "directed_cut_only": ["directed-cut"],
        "enhanced": ["directed-cut", "adjoint-completion"],
        "dependency": "adjoint-completion requires directed-cut",
        "formal_configurations": ["base", "enhanced"],
    }
    if (
        config.get("abhss_configuration_chain") != expected_chain
        or base_method.get("algorithm_family") != "ABHSS"
        or enhanced_method.get("algorithm_family") != "ABHSS"
        or base_method.get("executable") != enhanced_method.get("executable")
        or base_method.get("arguments") != ["--enhancements=none"]
        or enhanced_method.get("arguments") != ["--enhancements=all"]
        or base_method.get("enabled_enhancements") != []
        or enhanced_method.get("enabled_enhancements")
        != ["directed-cut", "adjoint-completion"]
    ):
        failures.append(
            "ABHSS base/enhanced must be fixed configurations of one executable"
        )

    for suite_id in (
        "P1_monogstplus_published",
        "P1_gpu4gst_published",
        "P2_cross_g",
        "S2_controlled_gf",
    ):
        if set(suite(config, suite_id)["methods"]) != PRIMARY_METHODS:
            failures.append(f"{suite_id} primary method set changed")

    p1 = load_json("experiment_data/p1_published_workloads/manifest.json")
    p1_cases = p1.get("cases", [])
    p1_datasets = p1.get("datasets", [])
    if (
        len(p1_cases) != 29
        or len(p1_datasets) != 13
        or int(p1.get("totals", {}).get("queries", -1)) != 8_318
    ):
        failures.append("P1 must contain 13 datasets, 29 blocks and 8,318 queries")
    if {row["dataset"] for row in p1_datasets} != P1_DATASETS:
        failures.append("P1 dataset identities changed")
    if len([name for name in P1_DATASETS if name.startswith("DBLP-")]) != 2:
        failures.append("the two P1 DBLP identities were not kept distinct")

    graph_by_dataset = {row["dataset"]: row for row in p1_datasets}
    for row in p1_datasets:
        graph_dir = ROOT / row["graph_path"]
        graph = graph_dir / "graph.txt"
        if not graph.is_file():
            failures.append(f"missing P1 graph: {graph}")
            continue
        try:
            if graph_header(graph_dir) != (int(row["vertices"]), int(row["edges"])):
                failures.append(f"P1 graph header changed: {row['dataset']}")
        except (OSError, ValueError) as error:
            failures.append(str(error))
        if args.deep:
            check_hash(graph, row.get("graph_sha256"), failures, str(row["dataset"]))

    p1_query_total = 0
    for row in p1_cases:
        query = ROOT / row["query_path"]
        if not query.is_file():
            failures.append(f"missing P1 query block: {query}")
            continue
        check_hash(query, row.get("query_sha256"), failures, str(row["dataset"]))
        p1_query_total += int(row["queries"])
        if row["dataset"] not in graph_by_dataset:
            failures.append(f"P1 case has unknown dataset: {row['dataset']}")
    if p1_query_total != 8_318:
        failures.append("P1 case query total changed")

    p2 = load_json("experiment_data/p2_cross_g/cells.json")
    p2_grid = {(row["dataset"], int(row["g"])) for row in p2}
    expected_p2_grid = {
        (dataset, g) for dataset in P2_SIZE_CLASSES for g in range(5, 17)
    }
    if len(p2) != 72 or p2_grid != expected_p2_grid:
        failures.append("P2 must be the frozen six-dataset g=5..16 grid")
    if any(
        int(row["source_queries"]) != 300
        or int(row["selected_queries"]) != 5
        or row.get("size_class") != P2_SIZE_CLASSES.get(row["dataset"])
        or int(row.get("candidate_generator_base_seed", -1)) != 2025
        or int(row.get("panel_selection_seed", -1)) != 20260723
        for row in p2
    ):
        failures.append("P2 source count, size class, seed or five-query rule changed")
    for row in p2:
        for path_field, hash_field, label in (
            ("source_query_path", "source_query_sha256", "P2 source query"),
            ("source_group_ids_path", "source_group_ids_sha256", "P2 source group IDs"),
            ("panel_query_path", "panel_query_sha256", "P2 panel query"),
            ("group_ids_path", "group_ids_sha256", "P2 panel group IDs"),
        ):
            check_hash(
                ROOT / row[path_field], row.get(hash_field), failures, label
            )

    s2 = load_json("experiment_data/s1_controlled_gf/cells.json")
    expected_s2_grid = {
        (dataset, g, f)
        for dataset in S2_DATASETS
        for g in (6, 10, 14)
        for f in (200, 400, 800, 1600, 3200)
    }
    actual_s2_grid = {
        (row["dataset"], int(row["g"]), int(row["f_target"])) for row in s2
    }
    if len(s2) != 30 or actual_s2_grid != expected_s2_grid:
        failures.append("S2 controlled <g,f> grid changed")
    if any(int(row["queries"]) != 5 for row in s2):
        failures.append("S2 must retain five queries per cell")
    for row in s2:
        check_hash(
            ROOT / row["query_path"],
            row.get("query_sha256"),
            failures,
            "S2 query",
        )

    runner = load_runner()
    cases = runner.expand_cases(config)
    primary_query_tasks = 0
    case_counts: dict[str, int] = {}
    for case in cases:
        case_counts[case.suite] = case_counts.get(case.suite, 0) + 1
        graph = case.graph_path / "graph.txt"
        if not graph.is_file():
            failures.append(f"case graph missing: {graph}")
            continue
        if not case.query_path.is_file():
            failures.append(f"case query missing: {case.query_path}")
            continue
        try:
            queries = runner.read_query_metadata(case.query_path)
        except (OSError, ValueError) as error:
            failures.append(str(error))
            continue
        selected = [
            row
            for row in queries
            if (case.min_g is None or row.g >= case.min_g)
            and (case.max_g is None or row.g <= case.max_g)
        ]
        if case.suite.startswith("P1_") or case.suite in {
            "P2_cross_g",
            "S2_controlled_gf",
        }:
            primary_query_tasks += len(selected) * len(case.methods)

    if case_counts.get("P1_monogstplus_published") != 5:
        failures.append("P1 MonoGST+ must expand to five executable blocks")
    if case_counts.get("P1_gpu4gst_published") != 24:
        failures.append("P1 GPU4GST must expand to 24 executable blocks")
    if case_counts.get("P2_cross_g") != 72:
        failures.append("P2 must expand to 72 cells")
    if case_counts.get("S2_controlled_gf") != 30:
        failures.append("S2 must expand to 30 cells")
    if primary_query_tasks != 26_484:
        failures.append(
            f"primary/secondary performance task total is {primary_query_tasks}, expected 26,484"
        )

    if args.require_binaries or args.require_performance_binaries:
        required_methods = (
            config["methods"].items()
            if args.require_binaries
            else (
                (name, config["methods"][name])
                for name in sorted(PRIMARY_METHODS)
            )
        )
        for name, method in required_methods:
            executable = runner.executable_path(method)
            if not executable.is_file():
                failures.append(f"missing executable for {name}: {executable}")

    feasibility = ROOT / "experiments" / "query_feasibility_audit.json"
    if not feasibility.is_file():
        warnings.append(
            "query_feasibility_audit.json is absent; regenerate the common-component gate before formal timing"
        )
    else:
        audit = load_json(feasibility)
        totals = audit.get("totals", {})
        if audit.get("matrix_sha256") != sha256(
            ROOT / "experiments" / "paper_matrix.json"
        ):
            failures.append("query-feasibility audit was built for another matrix")
        if int(totals.get("generated_or_gate_infeasible", -1)) != 0:
            failures.append("a generated or correctness-gate query is infeasible")
        if int(totals.get("published_workload_infeasible_retained", -1)) != 55:
            failures.append(
                "the 55 audited infeasible MonoGST+ natural queries were changed or dropped"
            )
        if int(totals.get("unique_query_records", -1)) != 8_840:
            failures.append("query-feasibility audit query total changed")

    for warning in warnings:
        print(f"WARNING: {warning}")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print(
        f"Validated {len(cases)} cases and {primary_query_tasks} performance tasks "
        f"(P1=8,318, P2=360, S2=150 queries across three methods)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
