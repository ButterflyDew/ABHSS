#!/usr/bin/env python3
"""Independently audit a 15-hour probe's schedule, provenance, and records."""

from __future__ import annotations

import argparse
from dataclasses import asdict
from datetime import datetime
import hashlib
import json
import math
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

import run_experiments as runner  # noqa: E402
import run_probe_15h as probe  # noqa: E402


ENTRY_FIELDS = (
    "wave",
    "case_id",
    "suite",
    "dataset",
    "g",
    "mean_f",
    "query_index",
    "query_rank",
    "timeout_seconds",
    "methods",
    "reason",
    "outcome_adaptive",
)
ALLOWED_STATUSES = {
    "ok",
    "timeout",
    "graph_load_timeout",
    "budget_exhausted",
    "error",
}


def write_json(path: Path, payload: Any) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(payload, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def canonical_entry(entry: dict[str, Any]) -> dict[str, Any]:
    return {field: entry.get(field) for field in ENTRY_FIELDS}


def parse_utc(value: str) -> datetime:
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def close(left: float, right: float) -> bool:
    return math.isclose(left, right, rel_tol=1e-9, abs_tol=1e-9)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument(
        "--allow-running",
        action="store_true",
        help="audit a live prefix without requiring final state/count/budget invariants",
    )
    parser.add_argument("--no-write", action="store_true")
    args = parser.parse_args()

    run_dir = args.run_dir if args.run_dir.is_absolute() else ROOT / args.run_dir
    run_dir = run_dir.resolve()
    failures: list[str] = []
    warnings: list[str] = []

    required = [
        "probe_manifest.json",
        "probe_runtime_config.json",
        "probe_status.json",
        "run_metadata.json",
    ]
    for name in required:
        if not (run_dir / name).is_file():
            failures.append(f"missing required run file: {name}")
    if failures:
        result = {"schema_version": 1, "passed": False, "failures": failures}
        if not args.no_write:
            run_dir.mkdir(parents=True, exist_ok=True)
            write_json(run_dir / "probe_audit.json", result)
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return 1

    manifest = json.loads((run_dir / "probe_manifest.json").read_text(encoding="utf-8"))
    runtime_config = json.loads(
        (run_dir / "probe_runtime_config.json").read_text(encoding="utf-8")
    )
    status = json.loads((run_dir / "probe_status.json").read_text(encoding="utf-8"))
    metadata = json.loads((run_dir / "run_metadata.json").read_text(encoding="utf-8"))
    paper_config = json.loads(
        (ROOT / "experiments" / "paper_matrix.json").read_text(encoding="utf-8")
    )

    # Reconstruct the probe runtime config semantically.
    expected_runtime = json.loads(json.dumps(paper_config))
    for method, executable in {
        "abhss_light": "build/Release/abhss_light_probe.exe",
        "abhss_heavy": "build/Release/abhss_heavy_probe.exe",
        "pruneddp_safe": "build/Release/pruneddp_probe.exe",
    }.items():
        expected_runtime["methods"][method]["executable"] = executable
        expected_runtime["methods"][method]["fallback_executables"] = []
    if runtime_config != expected_runtime:
        failures.append("probe_runtime_config.json is not the expected probe-only transform")
    config_hash = runner.sha256_file(run_dir / "probe_runtime_config.json")
    if metadata.get("config_sha256") != config_hash:
        failures.append("run_metadata config_sha256 does not match probe_runtime_config.json")

    # Rebuild all non-adaptive schedule sections from the frozen seed/config.
    cases = runner.expand_cases(paper_config)
    case_by_id = {case.case_id: case for case in cases}
    breadth, depth, filler, context = probe.build_schedule(
        cases,
        int(manifest["seed"]),
        float(manifest["breadth_timeout_seconds"]),
        float(manifest["depth_timeout_seconds"]),
    )
    rebuilt = {
        "breadth": [canonical_entry(asdict(entry)) for entry in breadth],
        "fixed_depth": [canonical_entry(asdict(entry)) for entry in depth],
        "exploratory_filler": [canonical_entry(asdict(entry)) for entry in filler],
    }
    for section, expected in rebuilt.items():
        observed = [canonical_entry(entry) for entry in manifest.get(section, [])]
        if observed != expected:
            failures.append(f"manifest {section} does not match deterministic reconstruction")
    if manifest.get("schedule_context") != context:
        failures.append("manifest schedule_context does not match reconstruction")

    query_cache: dict[str, list[Any]] = {}
    adaptive = manifest.get("adaptive_followups", [])
    if len(adaptive) > 16:
        failures.append("adaptive_followups exceeds the declared 8 cases x 2 queries cap")
    for entry in adaptive:
        case = case_by_id.get(entry.get("case_id"))
        if case is None:
            failures.append(f"adaptive entry has unknown case: {entry.get('case_id')}")
            continue
        rank = int(entry.get("query_rank", 0))
        ranked = query_cache.setdefault(
            case.case_id, probe.ranked_queries(case, int(manifest["seed"]), {})
        )
        if rank not in {2, 3} or rank > len(ranked):
            failures.append(f"invalid adaptive rank for {case.case_id}: {rank}")
        elif int(entry["query_index"]) != ranked[rank - 1].index:
            failures.append(f"adaptive query violates deterministic rank: {case.case_id}")
        if entry.get("wave") != "adaptive_diagnosis" or not entry.get("outcome_adaptive"):
            failures.append(f"adaptive entry is not visibly outcome-adaptive: {case.case_id}")

    # Verify current binaries against the hashes captured by the first runner.
    for method, expected_hash in metadata.get("binary_sha256", {}).items():
        config_method = runtime_config.get("methods", {}).get(method)
        if config_method is None or expected_hash is None:
            continue
        executable = runner.executable_path(config_method)
        if not executable.is_file():
            failures.append(f"missing executable recorded for {method}: {executable}")
        elif runner.sha256_file(executable) != expected_hash:
            failures.append(f"binary hash drift for {method}: {executable}")

    records: list[dict[str, Any]] = []
    seen_keys: set[str] = set()
    record_query_cache: dict[str, list[Any]] = {}
    for path in sorted((run_dir / "records").glob("*.json")):
        try:
            record = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            failures.append(f"malformed record {path.name}: {error}")
            continue
        records.append(record)
        key = str(record.get("task_key"))
        if key in seen_keys:
            failures.append(f"duplicate canonical task_key: {key}")
        seen_keys.add(key)
        expected_name = hashlib.sha256(key.encode()).hexdigest() + ".json"
        if path.name != expected_name:
            failures.append(f"record filename does not hash task_key: {path.name}")
        case = case_by_id.get(str(record.get("case_id")))
        if case is None:
            failures.append(f"record has unknown case: {record.get('case_id')}")
            continue
        method = str(record.get("method"))
        if method not in case.methods:
            failures.append(f"record method is not enabled for {case.case_id}: {method}")
        query_index = int(record.get("query_index", 0))
        case_queries = record_query_cache.setdefault(
            case.case_id, runner.read_query_metadata(case.query_path)
        )
        query = next((query for query in case_queries if query.index == query_index), None)
        if query is None:
            failures.append(f"record has invalid query index: {case.case_id} q{query_index}")
            continue
        expected_key = runner.task_key(case, method, query_index)
        if key != expected_key:
            failures.append(f"record task_key fields disagree: {key}")
        if int(record.get("g", -1)) != query.g or not close(
            float(record.get("mean_f", -1.0)), query.mean_f
        ):
            failures.append(f"record query metadata drift: {key}")
        record_status = str(record.get("status"))
        if record_status not in ALLOWED_STATUSES:
            failures.append(f"unknown record status {record_status}: {key}")
        if record_status == "ok" and (
            record.get("solver_seconds") is None or record.get("weight") is None
        ):
            failures.append(f"successful record lacks time/weight: {key}")
        log_path = ROOT / str(record.get("log_path", ""))
        if not log_path.is_file():
            failures.append(f"record log is missing: {key}")

    grouped: dict[tuple[str, int], list[dict[str, Any]]] = {}
    for record in records:
        if record.get("status") == "ok" and record.get("exact_claim", True):
            grouped.setdefault(
                (str(record["case_id"]), int(record["query_index"])), []
            ).append(record)
    disagreements: list[dict[str, Any]] = []
    for (case_id, query_index), group in grouped.items():
        if len(group) < 2:
            continue
        weights = [float(record["weight"]) for record in group]
        reference = weights[0]
        if any(
            abs(weight - reference) > 1e-7 * max(1.0, abs(weight), abs(reference))
            for weight in weights[1:]
        ):
            disagreements.append(
                {
                    "case_id": case_id,
                    "query_index": query_index,
                    "methods": {record["method"]: record["weight"] for record in group},
                }
            )
    if disagreements:
        failures.append(f"exact weight disagreements: {len(disagreements)}")

    budget_seconds = float(status.get("budget_seconds", 0.0))
    started = parse_utc(status["started_at"])
    deadline = parse_utc(status["deadline_at"])
    if not close((deadline - started).total_seconds(), budget_seconds):
        failures.append("status deadline-start interval does not equal budget_seconds")
    if status.get("run_id") != manifest.get("run_id") or status.get("run_id") != metadata.get("run_id"):
        failures.append("run_id differs among manifest/status/metadata")
    if args.allow_running:
        if status.get("state") not in {"starting", "running", "analyzing", "complete"}:
            failures.append(f"unexpected live controller state: {status.get('state')}")
    else:
        if status.get("state") != "complete":
            failures.append(f"controller is not complete: {status.get('state')}")
        if float(status.get("elapsed_seconds", 0.0)) < 0.99 * budget_seconds:
            failures.append("completed probe used less than 99% of the 15-hour budget")
        if int(status.get("record_count", -1)) != len(records):
            failures.append("final status record_count differs from canonical records")
        status_counts = status.get("status_counts", {})
        for name in ALLOWED_STATUSES:
            actual = sum(record.get("status") == name for record in records)
            if int(status_counts.get(name, 0)) != actual:
                failures.append(f"final status count mismatch for {name}")

    result = {
        "schema_version": 1,
        "run_id": manifest.get("run_id"),
        "mode": "live-prefix" if args.allow_running else "final",
        "passed": not failures,
        "failure_count": len(failures),
        "warning_count": len(warnings),
        "failures": failures,
        "warnings": warnings,
        "schedule": {
            "breadth_entries": len(breadth),
            "fixed_depth_entries": len(depth),
            "filler_entries": len(filler),
            "adaptive_entries": len(adaptive),
        },
        "records": {
            "count": len(records),
            "status_counts": {
                name: sum(record.get("status") == name for record in records)
                for name in sorted(ALLOWED_STATUSES)
            },
            "exact_weight_disagreements": disagreements,
        },
        "budget": {
            "state": status.get("state"),
            "elapsed_seconds": status.get("elapsed_seconds"),
            "budget_seconds": budget_seconds,
            "started_at": status.get("started_at"),
            "deadline_at": status.get("deadline_at"),
        },
        "config_sha256": config_hash,
    }
    if not args.no_write:
        write_json(run_dir / "probe_audit.json", result)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
