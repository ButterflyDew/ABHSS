#!/usr/bin/env python3
"""Validate the frozen paper environment before launching long experiments."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def snapshot_files(value: Any) -> list[tuple[str, str]]:
    found: list[tuple[str, str]] = []
    if isinstance(value, dict):
        if isinstance(value.get("path"), str) and isinstance(value.get("sha256"), str):
            found.append((value["path"], value["sha256"]))
        for child in value.values():
            found.extend(snapshot_files(child))
    elif isinstance(value, list):
        for child in value:
            found.extend(snapshot_files(child))
    return found


def load_runner() -> Any:
    path = ROOT / "tools" / "experiments" / "run_experiments.py"
    spec = importlib.util.spec_from_file_location("paper_runner", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--skip-binaries", action="store_true")
    parser.add_argument("--skip-large-graph-hashes", action="store_true")
    parser.add_argument(
        "--deep-snapshot",
        action="store_true",
        help="rehash every file recorded in data_snapshot.json (about 14 GB)",
    )
    args = parser.parse_args()

    failures: list[str] = []
    lock = json.loads((ROOT / "experiments" / "environment_lock.json").read_text(encoding="utf-8"))
    config = json.loads((ROOT / "experiments" / "paper_matrix.json").read_text(encoding="utf-8"))

    if float(config["timeout_seconds"]) != 10000:
        failures.append("paper_matrix timeout is not 10,000 seconds")
    if config.get("primary_baseline") != "pruneddp_safe":
        failures.append("paper_matrix primary baseline is not pruneddp_safe")
    safe_arguments = config["methods"]["pruneddp_safe"].get("arguments", [])
    if "--lb2-pathmax=off" not in safe_arguments:
        failures.append("primary PrunedDP++ baseline is not in safe reopen mode")
    if config["methods"]["pruneddp_strict"].get("exact_claim") is not False:
        failures.append("paper-pathmax audit is not marked exact_claim=false")

    for source in [*lock.get("reference_documents", []), *lock["source_archives"]]:
        path = ROOT / source["path"]
        if not path.is_file():
            failures.append(f"missing source archive: {source['path']}")
            continue
        expected = source.get("sha256")
        if expected and sha256(path).casefold() != str(expected).casefold():
            failures.append(f"source archive hash mismatch: {source['path']}")

    for source in lock["git_sources"]:
        path = ROOT / source["path"]
        if not path.is_dir():
            failures.append(f"missing git source: {source['path']}")
            continue
        completed = subprocess.run(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        actual = completed.stdout.strip()
        if completed.returncode or actual.casefold() != source["commit"].casefold():
            failures.append(
                f"git revision mismatch: {source['path']} expected {source['commit']} got {actual}"
            )

    expected_manifests = (
        ("experiment_data/controlled/manifest.json", 66, 660),
        ("experiment_data/practical_original/manifest.json", 48, 480),
        ("experiment_data/gpu4gst_panel/cells.json", 68, 680),
        ("experiment_data/gpu4gst_author_panel/cells.json", 16, 160),
    )
    for relative, expected_cells, expected_queries in expected_manifests:
        records = json.loads((ROOT / relative).read_text(encoding="utf-8"))
        query_count = sum(int(row.get("queries", row.get("selected_queries", 0))) for row in records)
        if len(records) != expected_cells or query_count != expected_queries:
            failures.append(
                f"manifest count mismatch: {relative} has {len(records)} cells/{query_count} queries"
            )
    wrp = json.loads((ROOT / "experiment_data" / "steinlib" / "index.json").read_text(encoding="utf-8"))
    if sum(bool(row.get("converted")) for row in wrp) != 11:
        failures.append("SteinLib panel does not contain exactly 11 converted known-optimum instances")

    graph_audit = json.loads((ROOT / "experiments" / "graph_identity_audit.json").read_text(encoding="utf-8"))
    if not args.skip_large_graph_hashes:
        for comparison in graph_audit["comparisons"]:
            for side in ("left", "right"):
                record = comparison[side]
                if sha256(ROOT / record["path"]).casefold() != record["sha256"].casefold():
                    failures.append(f"graph identity hash mismatch: {record['path']}")

    if args.deep_snapshot:
        snapshot = json.loads(
            (ROOT / "experiments" / "data_snapshot.json").read_text(encoding="utf-8")
        )
        unique_files = dict(snapshot_files(snapshot))
        for relative, expected in unique_files.items():
            path = ROOT / relative
            if not path.is_file():
                failures.append(f"snapshot file is missing: {relative}")
            elif sha256(path).casefold() != expected.casefold():
                failures.append(f"snapshot hash mismatch: {relative}")
        print(f"Deep-checked {len(unique_files)} snapshot files")

    runner = load_runner()
    cases = runner.expand_cases(config)
    total_tasks = 0
    for case in cases:
        if not case.graph_path.is_dir() or not (case.graph_path / "graph.txt").is_file():
            failures.append(f"invalid graph folder for case {case.case_id}: {case.graph_path}")
            continue
        if not case.query_path.is_file():
            failures.append(f"missing query file for case {case.case_id}: {case.query_path}")
            continue
        queries = [
            query
            for query in runner.read_query_metadata(case.query_path)
            if (case.min_g is None or query.g >= case.min_g)
            and (case.max_g is None or query.g <= case.max_g)
        ]
        if not queries:
            failures.append(f"case has no selected queries: {case.case_id}")
        for method_name in case.methods:
            method = config["methods"][method_name]
            total_tasks += sum(
                method.get("max_g") is None or query.g <= int(method["max_g"])
                for query in queries
            )

    if not args.skip_binaries:
        for method_name, method in config["methods"].items():
            path = runner.executable_path(method)
            if not path.is_file():
                failures.append(f"missing binary for {method_name}: {path}")

    print(f"Validated {len(cases)} cases and {total_tasks} per-instance tasks")
    if failures:
        print("Environment validation FAILED:")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print("Environment validation PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
