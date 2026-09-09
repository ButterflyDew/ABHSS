#!/usr/bin/env python3
"""Validate split formal records and materialize one canonical reporting ledger."""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import sys
from typing import Any


FORMAL_SUITES = (
    "P1_monogstplus_published",
    "P1_gpu4gst_published",
    "P2_cross_g",
    "S2_controlled_gf",
)
FORMAL_METHODS = ("abhss_base", "abhss_enhanced", "pruneddp_safe")


def find_workspace(start: Path) -> Path:
    for candidate in (start, *start.parents):
        if (candidate / "experiments" / "paper_matrix.json").is_file() and (candidate / "tools" / "experiments" / "run_experiments.py").is_file():
            return candidate
    raise RuntimeError("cannot locate the repository root")


ROOT = find_workspace(Path(__file__).resolve().parent)
sys.path.insert(0, str(ROOT / "tools" / "experiments"))
import run_experiments as runner  # noqa: E402


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def resolve(path: Path) -> Path:
    return path if path.is_absolute() else ROOT / path


def parse_assignments(values: list[str], option: str) -> dict[str, str]:
    parsed: dict[str, str] = {}
    for value in values:
        name, separator, payload = value.partition("=")
        if not separator or not name or not payload or name in parsed:
            raise ValueError(f"{option} requires unique NAME=VALUE entries: {value!r}")
        parsed[name] = payload
    return parsed


def source_files(path: Path) -> tuple[list[Path], list[Path]]:
    if path.is_file():
        return [path], []
    if not path.is_dir():
        raise FileNotFoundError(path)
    if path.name == "records":
        files = sorted(path.glob("*.json"))
        metadata = [path.parent / "run_metadata.json"]
    elif (path / "records").is_dir():
        files = sorted((path / "records").glob("*.json"))
        metadata = [path / "run_metadata.json"]
    else:
        files = sorted(path.glob("cpu*/records/*.json"))
        metadata = sorted(path.glob("cpu*/run_metadata.json"))
    metadata = [item for item in metadata if item.is_file()]
    if not files:
        raise ValueError(f"no record files under {path}")
    return files, metadata


def load_source(path: Path) -> tuple[list[dict[str, Any]], list[Path], list[Path]]:
    files, metadata = source_files(path)
    rows: list[dict[str, Any]] = []
    if len(files) == 1 and files[0].suffix == ".jsonl":
        rows = [json.loads(line) for line in files[0].read_text(encoding="utf-8").splitlines() if line]
    else:
        for file in files:
            payload = json.loads(file.read_text(encoding="utf-8"))
            if not isinstance(payload, dict):
                raise ValueError(f"record is not an object: {file}")
            rows.append(payload)
    return rows, files, metadata


def digest_source(path: Path, files: list[Path]) -> str:
    anchor = path if path.is_dir() else path.parent
    digest = hashlib.sha256()
    for file in files:
        digest.update(file.relative_to(anchor).as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(bytes.fromhex(sha256_file(file)))
    return digest.hexdigest()


def expected_records(config: dict[str, Any], suites: set[str], methods: set[str]) -> tuple[dict[str, tuple[Any, Any, str]], dict[str, Any]]:
    expected: dict[str, tuple[Any, Any, str]] = {}
    cases = runner.expand_cases(config)
    case_by_id = {case.case_id: case for case in cases}
    for case in cases:
        if case.suite not in suites:
            continue
        queries = [query for query in runner.read_query_metadata(case.query_path) if (case.min_g is None or query.g >= case.min_g) and (case.max_g is None or query.g <= case.max_g)]
        for method in case.methods:
            if method not in methods:
                continue
            max_g = config["methods"][method].get("max_g")
            for query in queries:
                if max_g is not None and query.g > int(max_g):
                    continue
                key = runner.task_key(case, method, query.index)
                if key in expected:
                    raise RuntimeError(f"matrix expands duplicate task key: {key}")
                expected[key] = (case, query, method)
    return expected, case_by_id


def validate_metadata(paths: list[Path], matrix_sha: str, timeout: float, binary_sha: dict[str, str]) -> None:
    for path in paths:
        metadata = json.loads(path.read_text(encoding="utf-8"))
        if metadata.get("config_sha256") != matrix_sha:
            raise RuntimeError(f"matrix hash mismatch in {path}")
        if metadata.get("timeout_seconds") is None or float(metadata["timeout_seconds"]) != timeout:
            raise RuntimeError(f"timeout mismatch in {path}")
        recorded = metadata.get("binary_sha256", {})
        for method, expected in binary_sha.items():
            if recorded.get(method) != expected:
                raise RuntimeError(f"binary hash mismatch for {method} in {path}")


def validate_row(row: dict[str, Any], expected: dict[str, tuple[Any, Any, str]], timeout: float) -> None:
    key = str(row.get("task_key", ""))
    if key not in expected:
        raise RuntimeError(f"record is outside the selected matrix: {key}")
    case, query, method = expected[key]
    query_index = row.get("query_index")
    group_count = row.get("g")
    if not isinstance(query_index, int) or isinstance(query_index, bool) or not isinstance(group_count, int) or isinstance(group_count, bool):
        raise RuntimeError(f"query_index and g must be integers: {key}")
    recomputed = runner.task_key(case, str(row.get("method")), query_index)
    if recomputed != key or row.get("suite") != case.suite or row.get("case_id") != case.case_id or row.get("dataset") != case.dataset or row.get("method") != method:
        raise RuntimeError(f"record identity fields disagree with task key: {key}")
    if group_count != query.g:
        raise RuntimeError(f"query metadata disagrees with record: {key}")
    minimum_group_size = row.get("min_f")
    maximum_group_size = row.get("max_f")
    mean_group_size = row.get("mean_f")
    sizes_are_integers = isinstance(minimum_group_size, int) and not isinstance(minimum_group_size, bool) and isinstance(maximum_group_size, int) and not isinstance(maximum_group_size, bool)
    mean_is_numeric = isinstance(mean_group_size, (int, float)) and not isinstance(mean_group_size, bool) and math.isfinite(float(mean_group_size))
    if not sizes_are_integers or not mean_is_numeric or minimum_group_size != query.min_f or maximum_group_size != query.max_f or not math.isclose(float(mean_group_size), query.mean_f, rel_tol=1e-12, abs_tol=1e-12):
        raise RuntimeError(f"query group-size metadata disagrees with record: {key}")
    if row.get("status") not in {"ok", "timeout"} or row.get("exact_claim") is not True:
        raise RuntimeError(f"non-formal status or exactness claim: {key}")
    if float(row.get("timeout_seconds", -1)) != timeout:
        raise RuntimeError(f"wrong per-query timeout: {key}")
    if row["status"] == "ok":
        seconds = row.get("solver_seconds")
        memory = row.get("query_memory_peak_mib")
        states = row.get("mask_vertex_states")
        solution = row.get("solution_status")
        weight = row.get("weight")
        valid = seconds is not None and math.isfinite(float(seconds)) and 0.0 <= float(seconds) <= timeout
        valid = valid and memory is not None and math.isfinite(float(memory)) and float(memory) >= 0.0
        valid = valid and isinstance(states, int) and not isinstance(states, bool) and states >= 0 and solution in {"feasible", "infeasible"}
        valid = valid and weight is not None and math.isfinite(float(weight))
        valid = valid and ((solution == "feasible" and float(weight) >= 0.0) or (solution == "infeasible" and float(weight) == -1.0))
        if not valid:
            raise RuntimeError(f"malformed completed record: {key}")
        if row.get("expected_solution_status") is not None and row["expected_solution_status"] != solution:
            raise RuntimeError(f"known feasibility mismatch: {key}")
    else:
        fields = ("solver_seconds", "weight", "solution_status", "query_memory_peak_mib", "mask_vertex_states")
        watchdog = row.get("watchdog_wall_seconds")
        if any(row.get(field) is not None for field in fields) or watchdog is None or not math.isfinite(float(watchdog)) or float(watchdog) < timeout:
            raise RuntimeError(f"malformed timeout record: {key}")


def validate_objectives(rows: list[dict[str, Any]], tolerance: float) -> None:
    grouped: dict[tuple[str, str, int], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        if row["status"] == "ok":
            grouped[(row["suite"], row["case_id"], int(row["query_index"]))].append(row)
    for identity, group in grouped.items():
        statuses = {row["solution_status"] for row in group}
        weights = [float(row["weight"]) for row in group if row["solution_status"] == "feasible"]
        if len(statuses) > 1 or (weights and max(weights) - min(weights) > tolerance):
            raise RuntimeError(f"exact methods disagree for {identity}")


def write_outputs(output_dir: Path, run_id: str, rows: list[dict[str, Any]], manifest: dict[str, Any]) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    lines = "".join(json.dumps(row, sort_keys=True) + "\n" for row in rows)
    target = output_dir / "records.jsonl"
    if target.exists() and target.read_text(encoding="utf-8") != lines:
        raise RuntimeError(f"refusing to replace a different canonical ledger: {target}")
    temporary = target.with_suffix(".jsonl.tmp")
    temporary.write_text(lines, encoding="utf-8", newline="\n")
    temporary.replace(target)
    manifest["record_sha256"] = sha256_file(target)
    manifest_target = output_dir / "manifest.json"
    manifest_target.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=Path("experiments/paper_matrix.json"))
    parser.add_argument("--source", action="append", required=True, help="repeat LABEL=JSONL_OR_RECORD_DIRECTORY")
    parser.add_argument("--expect-source-count", action="append", default=[], help="repeat LABEL=COUNT")
    parser.add_argument("--suite", action="append", default=[])
    parser.add_argument("--method", action="append", default=[])
    parser.add_argument("--expected-matrix-sha")
    parser.add_argument("--expected-binary-sha", action="append", default=[], help="repeat METHOD=SHA256")
    parser.add_argument("--mark-source", action="append", default=[], help="repeat LABEL=BOOLEAN_FIELD to preserve a source-provenance marker")
    parser.add_argument("--expected-records", type=int)
    parser.add_argument("--weight-tolerance", type=float, default=1e-6)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    config_path = resolve(args.config)
    config = json.loads(config_path.read_text(encoding="utf-8"))
    matrix_sha = sha256_file(config_path)
    if args.expected_matrix_sha and matrix_sha != args.expected_matrix_sha:
        raise RuntimeError(f"matrix SHA-256 changed: {matrix_sha}")
    timeout = float(config["timeout_seconds"])
    suites = set(args.suite or FORMAL_SUITES)
    methods = set(args.method or FORMAL_METHODS)
    expected, _ = expected_records(config, suites, methods)
    if args.expected_records is not None and len(expected) != args.expected_records:
        raise RuntimeError(f"matrix expands {len(expected)} records, expected {args.expected_records}")

    sources = parse_assignments(args.source, "--source")
    expected_counts = {name: int(value) for name, value in parse_assignments(args.expect_source_count, "--expect-source-count").items()}
    if expected_counts and set(expected_counts) != set(sources):
        raise RuntimeError("--expect-source-count labels must exactly match --source labels")
    binary_sha = parse_assignments(args.expected_binary_sha, "--expected-binary-sha")
    source_markers = parse_assignments(args.mark_source, "--mark-source")
    if not set(source_markers) <= set(sources) or any(not field.isidentifier() for field in source_markers.values()):
        raise RuntimeError("--mark-source requires an existing source label and a valid field name")
    for method, expected_sha in binary_sha.items():
        if method not in config["methods"]:
            raise RuntimeError(f"unknown method in --expected-binary-sha: {method}")
        actual_sha = sha256_file(runner.executable_path(config["methods"][method]))
        if actual_sha != expected_sha:
            raise RuntimeError(f"current executable hash mismatch for {method}: {actual_sha}")

    by_key: dict[str, dict[str, Any]] = {}
    origins: dict[str, str] = {}
    source_manifest: dict[str, Any] = {}
    for label, raw_path in sources.items():
        path = resolve(Path(raw_path))
        rows, files, metadata = load_source(path)
        if path.is_dir() and not metadata:
            raise RuntimeError(f"record directory has no worker metadata: {path}")
        if label in expected_counts and len(rows) != expected_counts[label]:
            raise RuntimeError(f"source {label} has {len(rows)} records, expected {expected_counts[label]}")
        validate_metadata(metadata, matrix_sha, timeout, binary_sha)
        source_manifest[label] = {"path": str(path.relative_to(ROOT)), "records": len(rows), "content_sha256": digest_source(path, files), "metadata_files": [str(item.relative_to(ROOT)) for item in metadata]}
        for row in rows:
            key = str(row.get("task_key", ""))
            if key in by_key:
                raise RuntimeError(f"duplicate task key across sources: {key}")
            validate_row(row, expected, timeout)
            by_key[key] = row
            origins[key] = label

    actual = set(by_key)
    wanted = set(expected)
    if actual != wanted:
        missing = sorted(wanted - actual)
        extra = sorted(actual - wanted)
        raise RuntimeError(f"formal coverage mismatch: actual={len(actual)} expected={len(wanted)} missing={len(missing)} extra={len(extra)} first_missing={missing[:3]} first_extra={extra[:3]}")
    validate_objectives(list(by_key.values()), args.weight_tolerance)
    panel_counts = Counter((row["suite"], row["case_id"], row["method"]) for row in by_key.values() if row["suite"] in {"P2_cross_g", "S2_controlled_gf"})
    malformed_panels = {key: count for key, count in panel_counts.items() if count != 10}
    if malformed_panels:
        raise RuntimeError(f"P2/S2 method cells must contain ten records: {malformed_panels}")

    materialized: list[dict[str, Any]] = []
    for key in sorted(by_key):
        row = dict(by_key[key])
        row["materialized_from_run_id"] = row.get("run_id")
        row["materialized_from"] = origins[key]
        row["run_id"] = args.run_id
        if origins[key] in source_markers:
            row[source_markers[origins[key]]] = True
        materialized.append(row)
    status_counts = Counter(row["status"] for row in materialized)
    suite_method_counts = Counter((row["suite"], row["method"]) for row in materialized)
    manifest = {
        "schema_version": 1,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "run_id": args.run_id,
        "records": len(materialized),
        "paper_matrix_sha256": matrix_sha,
        "timeout_seconds_per_query": timeout,
        "source_counts": {label: details["records"] for label, details in source_manifest.items()},
        "sources": source_manifest,
        "binary_sha256": binary_sha,
        "status_counts": dict(status_counts),
        "suite_method_counts": {f"{suite}|{method}": count for (suite, method), count in sorted(suite_method_counts.items())},
        "objective_mismatches": 0,
        "feasibility_mismatches": 0,
        "completed_records_missing_state_count": 0,
        "p2_s2_cells_with_non_ten_method_records": 0,
        "provenance": "source records are unchanged; run_id is normalized only in this canonical reporting ledger",
    }
    write_outputs(resolve(args.output_dir), args.run_id, materialized, manifest)
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
