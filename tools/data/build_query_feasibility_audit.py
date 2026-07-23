#!/usr/bin/env python3
"""Audit connected-component feasibility for every formal query file.

The experiment matrix can reference the same query file more than once (for
example, in the main comparison and an ablation).  This script audits every
unique graph/query-file pair once, records content hashes, and writes a stable
machine-readable report for paper review.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.experiments.run_experiments import expand_cases  # noqa: E402


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def relative(path: Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def query_count(path: Path) -> int:
    with path.open("r", encoding="utf-8", errors="strict") as source:
        return int(source.readline())


def locate_executable(requested: Path | None) -> Path:
    candidates = []
    if requested is not None:
        candidates.append(requested)
    candidates.extend(
        [
            ROOT / "build" / "Release" / "audit_query_feasibility.exe",
            ROOT / "build" / "audit_query_feasibility.exe",
            ROOT / "build" / "audit_query_feasibility",
        ]
    )
    for candidate in candidates:
        resolved = candidate if candidate.is_absolute() else ROOT / candidate
        if resolved.is_file():
            return resolved.resolve()
    raise FileNotFoundError(
        "audit_query_feasibility executable not found; build its CMake target first"
    )


def load_audit_inputs(matrix_path: Path) -> dict[Path, set[Path]]:
    config = json.loads(matrix_path.read_text(encoding="utf-8"))
    grouped: dict[Path, set[Path]] = defaultdict(set)
    for case in expand_cases(config):
        graph_file = (case.graph_path / "graph.txt").resolve()
        query_file = case.query_path.resolve()
        if not graph_file.is_file():
            raise FileNotFoundError(graph_file)
        if not query_file.is_file():
            raise FileNotFoundError(query_file)
        grouped[graph_file].add(query_file)
    return grouped


def audit_graph(
    executable: Path,
    graph_file: Path,
    query_files: list[Path],
    output: Path,
) -> dict[str, Any]:
    command = [
        str(executable),
        str(graph_file),
        str(output),
        *(str(path) for path in query_files),
    ]
    completed = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    if completed.stdout.strip():
        print(f"[{relative(graph_file.parent)}] {completed.stdout.strip()}", flush=True)
    if completed.returncode not in (0, 2):
        raise RuntimeError(
            f"audit failed for {graph_file}: {completed.stderr.strip()}"
        )
    return json.loads(output.read_text(encoding="utf-8"))


def cached_cpp_audit(parts_directory: Path, graph_file: Path, query_files: list[Path]) -> dict[str, Any] | None:
    expected_queries = {path.resolve() for path in query_files}
    for candidate in parts_directory.glob("*.json"):
        try:
            record = json.loads(candidate.read_text(encoding="utf-8"))
            cached_graph = Path(record["graph_path"]).resolve()
            cached_queries = {
                Path(item["path"]).resolve() for item in record["query_files"]
            }
        except (KeyError, OSError, ValueError, json.JSONDecodeError):
            continue
        if cached_graph != graph_file.resolve() or cached_queries != expected_queries:
            continue
        newest_input = max(
            [graph_file.stat().st_mtime, *(path.stat().st_mtime for path in query_files)]
        )
        cached_counts = {
            Path(item["path"]).resolve(): int(item["queries"])
            for item in record["query_files"]
        }
        cached_counts_match = all(
            query_count(path) == cached_counts[path.resolve()] for path in query_files
        )
        if (
            candidate.stat().st_mtime >= newest_input
            and cached_counts_match
        ):
            return record
    return None


def normalize_audit(
    record: dict[str, Any], graph_file: Path, query_files: list[Path]
) -> dict[str, Any]:
    by_resolved_path = {
        Path(item["path"]).resolve(): item for item in record["query_files"]
    }
    normalized_queries = []
    for query_file in query_files:
        item = dict(by_resolved_path[query_file.resolve()])
        item["path"] = relative(query_file)
        item["sha256"] = sha256_file(query_file)
        normalized_queries.append(item)
    normalized = dict(record)
    normalized["graph_path"] = relative(graph_file)
    normalized["graph_sha256"] = sha256_file(graph_file)
    normalized["largest_component_fraction"] = round(
        normalized["largest_component_vertices"] / normalized["vertices"], 12
    )
    normalized["query_files"] = normalized_queries
    return normalized


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--matrix",
        type=Path,
        default=ROOT / "experiments" / "paper_matrix.json",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "experiments" / "query_feasibility_audit.json",
    )
    parser.add_argument("--executable", type=Path)
    parser.add_argument(
        "--reuse-component-scans",
        action="store_true",
        help="development shortcut: reuse a newer matching C++ part file; omit for the final audit",
    )
    args = parser.parse_args()

    matrix_path = args.matrix if args.matrix.is_absolute() else ROOT / args.matrix
    output_path = args.output if args.output.is_absolute() else ROOT / args.output
    executable = locate_executable(args.executable)
    grouped = load_audit_inputs(matrix_path)
    matrix = json.loads(matrix_path.read_text(encoding="utf-8"))
    published_pairs = defaultdict(set)
    for case in expand_cases(matrix):
        if case.suite.startswith("P1_"):
            published_pairs[(case.graph_path / "graph.txt").resolve()].add(
                case.query_path.resolve()
            )

    graph_records: list[dict[str, Any]] = []
    parts_directory = ROOT / "results" / "query_feasibility_audit_parts"
    parts_directory.mkdir(parents=True, exist_ok=True)
    for graph_file in sorted(grouped, key=relative):
        query_files = sorted(grouped[graph_file], key=relative)
        raw_record = (
            cached_cpp_audit(parts_directory, graph_file, query_files)
            if args.reuse_component_scans
            else None
        )
        if raw_record is None:
            cache_key = hashlib.sha256(relative(graph_file).encode("utf-8")).hexdigest()[:16]
            raw_record = audit_graph(
                executable,
                graph_file,
                query_files,
                parts_directory / f"graph-{cache_key}.json",
            )
        else:
            print(f"[{relative(graph_file.parent)}] reused component scan", flush=True)
        graph_records.append(normalize_audit(raw_record, graph_file, query_files))

    totals = {
        "graphs": len(graph_records),
        "query_files": sum(len(item["query_files"]) for item in graph_records),
        "unique_query_records": sum(
            query["queries"]
            for graph in graph_records
            for query in graph["query_files"]
        ),
        "feasible": sum(
            query["feasible"]
            for graph in graph_records
            for query in graph["query_files"]
        ),
        "infeasible": sum(
            query["infeasible"]
            for graph in graph_records
            for query in graph["query_files"]
        ),
    }
    published_infeasible = 0
    generated_infeasible = 0
    for graph in graph_records:
        graph_path = (ROOT / graph["graph_path"]).resolve()
        for query in graph["query_files"]:
            query_path = (ROOT / query["path"]).resolve()
            if query_path in published_pairs.get(graph_path, set()):
                published_infeasible += int(query["infeasible"])
            else:
                generated_infeasible += int(query["infeasible"])
    totals["published_workload_infeasible_retained"] = published_infeasible
    totals["generated_or_gate_infeasible"] = generated_infeasible
    payload = {
        "schema_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "scope": "all unique graph/query-file pairs referenced by paper_matrix.json",
        "policy": "retain and run infeasible queries present in the exact published P1 workload; require every generated P2/S2 and correctness-gate query to be feasible",
        "matrix_path": relative(matrix_path),
        "matrix_sha256": sha256_file(matrix_path),
        "audit_executable": relative(executable),
        "totals": totals,
        "graphs": graph_records,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(
        "audit totals: "
        f"graphs={totals['graphs']} query_files={totals['query_files']} "
        f"queries={totals['unique_query_records']} "
        f"feasible={totals['feasible']} infeasible={totals['infeasible']} "
        f"published_infeasible={published_infeasible} generated_infeasible={generated_infeasible}"
    )
    return 0 if generated_infeasible == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
