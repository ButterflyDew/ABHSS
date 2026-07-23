#!/usr/bin/env python3
"""Run a time-bounded, stratified feasibility probe of the paper matrix.

The probe is deliberately not a paper-result generator.  It chooses cells by
predeclared source/g/f strata, chooses queries by a seeded SHA-256 rank before
any solver runs, executes a short breadth wave, then spends remaining budget
on claim-critical depth and explicitly labelled anomaly follow-ups.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[2]
RUNNER_PATH = ROOT / "tools" / "experiments" / "run_experiments.py"
PAPER_CONFIG = ROOT / "experiments" / "paper_matrix.json"
PLAN_CONFIG = ROOT / "experiments" / "probe_15h_plan.json"


def load_runner() -> Any:
    spec = importlib.util.spec_from_file_location("paper_experiment_runner", RUNNER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot import {RUNNER_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


runner = load_runner()


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def clean_environment() -> dict[str, str]:
    """Deduplicate Windows' case-insensitive environment before child launch."""

    cleaned: dict[str, str] = {}
    spelling: dict[str, str] = {}
    for key, value in os.environ.items():
        folded = key.casefold()
        previous = spelling.get(folded)
        if previous is not None:
            cleaned.pop(previous, None)
        canonical = "Path" if folded == "path" else key
        spelling[folded] = canonical
        cleaned[canonical] = value
    return cleaned


@dataclass
class ProbeEntry:
    wave: str
    case_id: str
    suite: str
    dataset: str
    g: int
    mean_f: float
    query_index: int
    query_rank: int
    timeout_seconds: float
    methods: list[str]
    reason: str
    outcome_adaptive: bool = False
    status: str = "pending"


def case_g(case: Any, queries: list[Any]) -> int:
    attributes = case.attributes or {}
    value = attributes.get("g", attributes.get("split_g"))
    if value is not None:
        return int(value)
    if queries and all(query.g == queries[0].g for query in queries):
        return int(queries[0].g)
    match = re.search(r"_g(\d+)", case.case_id)
    if match:
        return int(match.group(1))
    raise ValueError(f"Cannot infer g for {case.case_id}")


def case_f(case: Any) -> int | None:
    value = (case.attributes or {}).get("f_target")
    return int(value) if value is not None else None


def eligible_queries(case: Any) -> list[Any]:
    return [
        query
        for query in runner.read_query_metadata(case.query_path)
        if (case.min_g is None or query.g >= case.min_g)
        and (case.max_g is None or query.g <= case.max_g)
    ]


def ranked_queries(case: Any, seed: int, cache: dict[str, list[Any]]) -> list[Any]:
    queries = cache.setdefault(case.case_id, eligible_queries(case))
    return sorted(
        queries,
        key=lambda query: hashlib.sha256(
            f"{seed}|{case.suite}|{case.case_id}|q{query.index}".encode()
        ).digest(),
    )


def round_robin(buckets: list[list[Any]]) -> list[Any]:
    result: list[Any] = []
    for index in range(max((len(bucket) for bucket in buckets), default=0)):
        for bucket in buckets:
            if index < len(bucket):
                result.append(bucket[index])
    return result


def build_schedule(
    cases: list[Any], seed: int, breadth_timeout: float, depth_timeout: float
) -> tuple[list[ProbeEntry], list[ProbeEntry], list[ProbeEntry], dict[str, Any]]:
    query_cache: dict[str, list[Any]] = {}
    by_id = {case.case_id: case for case in cases}
    indexed: dict[tuple[str, str, int, int | None], Any] = {}
    by_suite: dict[str, list[Any]] = {}
    for case in cases:
        by_suite.setdefault(case.suite, []).append(case)
        queries = ranked_queries(case, seed, query_cache)
        if not queries:
            continue
        indexed[(case.suite, case.dataset, case_g(case, queries), case_f(case))] = case

    central = ["abhss_light", "abhss_heavy", "pruneddp_safe"]
    ablation = [
        "abhss_light",
        "abhss_light_no_early",
        "abhss_light_no_witness",
        "abhss_heavy",
        "abhss_heavy_forward",
    ]
    used: set[tuple[str, int, tuple[str, ...]]] = set()

    def make(
        case: Any,
        rank: int,
        wave: str,
        timeout: float,
        methods: list[str],
        reason: str,
        adaptive: bool = False,
    ) -> ProbeEntry | None:
        queries = ranked_queries(case, seed, query_cache)
        if rank < 1 or rank > len(queries):
            return None
        query = queries[rank - 1]
        actual_methods = [method for method in methods if method in case.methods]
        key = (case.case_id, query.index, tuple(actual_methods))
        if not actual_methods or key in used:
            return None
        used.add(key)
        return ProbeEntry(
            wave=wave,
            case_id=case.case_id,
            suite=case.suite,
            dataset=case.dataset,
            g=query.g,
            mean_f=query.mean_f,
            query_index=query.index,
            query_rank=rank,
            timeout_seconds=timeout,
            methods=actual_methods,
            reason=reason,
            outcome_adaptive=adaptive,
        )

    def lookup(suite: str, dataset: str, g: int, f: int | None = None) -> Any:
        key = (suite, dataset, g, f)
        if key not in indexed:
            raise KeyError(f"Missing planned probe cell {key}")
        return indexed[key]

    controlled_cells: list[tuple[Any, str]] = []
    for suite, dataset in (
        ("A_controlled_dblp", "DBLP-AMiner-V18"),
        ("A_controlled_imdb", "IMDb-daily-20260722"),
    ):
        for g in (4, 8, 12, 16):
            controlled_cells.append(
                (lookup(suite, dataset, g, 400), f"A-g{g}-f400")
            )
        for f in (100, 3200):
            controlled_cells.append(
                (lookup(suite, dataset, 10, f), f"A-g10-f{f}")
            )

    related_datasets = [
        "snap-wikipedia-2018",
        "snap-twitch-2018",
        "snap-github-2019",
        "snap-youtube",
        "snap-orkut",
        "snap-livejournal",
        "movielens-32m",
        "toronto-current",
    ]
    representative = [
        "snap-wikipedia-2018",
        "snap-orkut",
        "movielens-32m",
        "toronto-current",
    ]
    related_cells: list[tuple[Any, str]] = []
    for g in (4, 12):
        for dataset in related_datasets:
            related_cells.append(
                (lookup("B_related_cross_g", dataset, g), f"B-related-g{g}")
            )
    for dataset in representative:
        related_cells.append(
            (
                lookup("B_related_cross_g", dataset, 16),
                "B-related-high-g16",
            )
        )

    natural_cells: list[tuple[Any, str]] = []
    for g in (1, 4, 8, 10, 11, 12):
        natural_cells.append(
            (
                lookup("C_dbpedia_natural", "DBpedia-2022.12-en", g),
                f"C-DBpedia-g{g}",
            )
        )
    for g in (1, 4, 8, 9, 12):
        natural_cells.append(
            (
                lookup("C_linkedmdb_natural", "LinkedMDB-2012", g),
                f"C-LinkedMDB-g{g}",
            )
        )

    ablation_cells = [
        (
            lookup("S2_ablation", "IMDb-daily-20260722", 10),
            "A-IMDb-ablation",
        ),
        (
            lookup("S2_ablation", "snap-wikipedia-2018", 12),
            "B-Wikipedia-ablation",
        ),
        (
            lookup("S2_ablation", "LinkedMDB-2012", 8),
            "C-LinkedMDB-ablation",
        ),
    ]

    breadth_buckets: list[list[ProbeEntry]] = []
    for cells, methods in [
        (controlled_cells, central),
        (related_cells, central),
        (natural_cells, central),
        (ablation_cells, ablation),
    ]:
        bucket = []
        for case, reason in cells:
            entry = make(case, 1, "breadth", breadth_timeout, methods, reason)
            if entry is not None:
                bucket.append(entry)
        breadth_buckets.append(bucket)
    breadth = round_robin(breadth_buckets)

    fixed_depth_cells: list[tuple[Any, str]] = []
    for dataset in related_datasets:
        fixed_depth_cells.append(
            (lookup("B_related_cross_g", dataset, 16), "B-all-datasets-g16-depth")
        )
    for suite, dataset in (
        ("A_controlled_dblp", "DBLP-AMiner-V18"),
        ("A_controlled_imdb", "IMDb-daily-20260722"),
    ):
        fixed_depth_cells.append(
            (lookup(suite, dataset, 16, 400), "A-controlled-g16-depth")
        )
    fixed_depth_cells.extend(
        [
            (
                lookup("C_dbpedia_natural", "DBpedia-2022.12-en", 10),
                "C-DBpedia-g10-depth",
            ),
            (
                lookup("C_dbpedia_natural", "DBpedia-2022.12-en", 12),
                "C-DBpedia-g12-depth",
            ),
            (
                lookup("C_linkedmdb_natural", "LinkedMDB-2012", 9),
                "C-LinkedMDB-g9-depth",
            ),
            (
                lookup("C_linkedmdb_natural", "LinkedMDB-2012", 12),
                "C-LinkedMDB-g12-depth",
            ),
        ]
    )

    depth: list[ProbeEntry] = []
    for case, reason in fixed_depth_cells:
        entry = make(case, 2, "depth", depth_timeout, central, reason)
        if entry is not None:
            depth.append(entry)

    # The filler is a seeded order over every remaining central cell.  It is
    # explicitly exploratory and is reached only if the required waves finish.
    filler_candidates = [
        case
        for case in cases
        if case.suite.startswith(("A_", "B_", "C_"))
    ]
    filler_candidates.sort(
        key=lambda case: hashlib.sha256(
            f"{seed}|filler|{case.suite}|{case.case_id}".encode()
        ).digest()
    )
    filler: list[ProbeEntry] = []
    for rank in (1, 2, 3):
        for case in filler_candidates:
            entry = make(
                case,
                rank,
                "exploratory_filler",
                min(300.0, depth_timeout),
                central,
                "seeded-full-matrix-fill",
            )
            if entry is not None:
                filler.append(entry)

    context = {
        "case_count": len(cases),
        "breadth_entries": len(breadth),
        "fixed_depth_entries": len(depth),
        "filler_entries": len(filler),
        "case_ids": sorted(by_id),
    }
    return breadth, depth, filler, context


def load_records(run_dir: Path) -> list[dict[str, Any]]:
    records = []
    for path in (run_dir / "records").glob("*.json"):
        try:
            records.append(json.loads(path.read_text(encoding="utf-8")))
        except (OSError, json.JSONDecodeError):
            continue
    return records


def anomaly_case_ids(records: list[dict[str, Any]]) -> list[tuple[str, float, str]]:
    grouped: dict[tuple[str, int], dict[str, dict[str, Any]]] = {}
    for record in records:
        grouped.setdefault((record["case_id"], int(record["query_index"])), {})[
            record["method"]
        ] = record
    anomalies: dict[str, tuple[float, str]] = {}
    for (case_id, _), methods in grouped.items():
        baseline = methods.get("pruneddp_safe")
        ours = [methods.get("abhss_light"), methods.get("abhss_heavy")]
        if baseline is None or baseline.get("status") != "ok":
            continue
        baseline_time = float(baseline["solver_seconds"])
        solved_ours = [
            float(record["solver_seconds"])
            for record in ours
            if record is not None and record.get("status") == "ok"
        ]
        timed_out = sum(
            record is not None and record.get("status") == "timeout" for record in ours
        )
        if not solved_ours and timed_out:
            score, reason = 1e9, "baseline-solved-both-abhss-unsolved"
        elif solved_ours:
            score = min(solved_ours) / max(baseline_time, 1e-12)
            if score < 2.0 and timed_out == 0:
                continue
            reason = (
                "baseline-solved-one-abhss-timeout"
                if timed_out
                else f"baseline-at-least-{score:.2f}x-faster-than-best-abhss"
            )
        else:
            continue
        previous = anomalies.get(case_id)
        if previous is None or score > previous[0]:
            anomalies[case_id] = (score, reason)
    return sorted(
        ((case_id, score, reason) for case_id, (score, reason) in anomalies.items()),
        key=lambda item: (-item[1], item[0]),
    )


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--hours", type=float, default=15.0)
    parser.add_argument("--seed", type=int, default=20260722)
    parser.add_argument("--breadth-timeout", type=float, default=180.0)
    parser.add_argument("--depth-timeout", type=float, default=600.0)
    parser.add_argument("--graph-load-timeout", type=float, default=600.0)
    parser.add_argument("--run-id")
    parser.add_argument("--run-dir", type=Path)
    parser.add_argument("--max-entries", type=int, help="development-only schedule cap")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    if args.hours <= 0:
        parser.error("hours must be positive")

    run_id = args.run_id or f"probe15h_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    run_dir = args.run_dir or (ROOT / "results" / "paper_runs" / run_id)
    if not run_dir.is_absolute():
        run_dir = ROOT / run_dir
    run_dir.mkdir(parents=True, exist_ok=True)

    paper_config = json.loads(PAPER_CONFIG.read_text(encoding="utf-8"))
    probe_config = json.loads(json.dumps(paper_config))
    for method, executable in {
        "abhss_light": "build/Release/abhss_light_probe.exe",
        "abhss_heavy": "build/Release/abhss_heavy_probe.exe",
        "pruneddp_safe": "build/Release/pruneddp_probe.exe",
    }.items():
        probe_config["methods"][method]["executable"] = executable
        probe_config["methods"][method]["fallback_executables"] = []
    runtime_config = run_dir / "probe_runtime_config.json"
    write_json(runtime_config, probe_config)

    cases = runner.expand_cases(paper_config)
    case_by_id = {case.case_id: case for case in cases}
    breadth, depth, filler, schedule_context = build_schedule(
        cases, args.seed, args.breadth_timeout, args.depth_timeout
    )
    plan_payload: dict[str, Any] = {
        "schema_version": 1,
        "purpose": "SIGMOD/VLDB submission feasibility gate; not final paper evidence",
        "created_at": utc_now(),
        "run_id": run_id,
        "seed": args.seed,
        "budget_hours": args.hours,
        "breadth_timeout_seconds": args.breadth_timeout,
        "depth_timeout_seconds": args.depth_timeout,
        "graph_load_timeout_seconds": args.graph_load_timeout,
        "selection_policy": (
            "predeclared source/g/f cells; query indices ranked by SHA-256 before solving; "
            "outcome-adaptive entries are diagnostic only"
        ),
        "static_plan_path": str(PLAN_CONFIG.relative_to(ROOT)).replace("\\", "/"),
        "runtime_config_path": str(runtime_config.relative_to(ROOT)).replace("\\", "/"),
        "schedule_context": schedule_context,
        "breadth": [asdict(entry) for entry in breadth],
        "fixed_depth": [asdict(entry) for entry in depth],
        "exploratory_filler": [asdict(entry) for entry in filler],
        "adaptive_followups": [],
    }
    manifest_path = run_dir / "probe_manifest.json"

    def refresh_manifest() -> None:
        plan_payload["breadth"] = [asdict(entry) for entry in breadth]
        plan_payload["fixed_depth"] = [asdict(entry) for entry in depth]
        plan_payload["exploratory_filler"] = [asdict(entry) for entry in filler]
        plan_payload["adaptive_followups"] = [asdict(entry) for entry in adaptive]
        write_json(manifest_path, plan_payload)

    adaptive: list[ProbeEntry] = []
    refresh_manifest()
    if args.dry_run:
        print(
            f"Probe plan: {len(breadth)} breadth, {len(depth)} depth, "
            f"{len(filler)} filler entries; manifest={manifest_path}",
            flush=True,
        )
        return 0

    started = time.monotonic()
    wall_deadline = started + args.hours * 3600.0
    probe_started_at = utc_now()
    probe_deadline_at = datetime.fromtimestamp(
        time.time() + args.hours * 3600.0, timezone.utc
    ).isoformat()
    status_path = run_dir / "probe_status.json"
    executed_entries = 0
    clean_env = clean_environment()

    def update_status(state: str, current: ProbeEntry | None = None) -> None:
        records = load_records(run_dir)
        write_json(
            status_path,
            {
                "schema_version": 1,
                "run_id": run_id,
                "controller_pid": os.getpid(),
                "started_at": probe_started_at,
                "deadline_at": probe_deadline_at,
                "state": state,
                "updated_at": utc_now(),
                "elapsed_seconds": time.monotonic() - started,
                "budget_seconds": args.hours * 3600.0,
                "remaining_seconds": max(0.0, wall_deadline - time.monotonic()),
                "executed_entries": executed_entries,
                "record_count": len(records),
                "status_counts": {
                    name: sum(record.get("status") == name for record in records)
                    for name in ["ok", "timeout", "error", "graph_load_timeout", "budget_exhausted"]
                },
                "current": asdict(current) if current is not None else None,
            },
        )

    def run_entry(entry: ProbeEntry) -> bool:
        nonlocal executed_entries
        remaining = wall_deadline - time.monotonic()
        if remaining <= 5.0:
            entry.status = "not_started_budget"
            return False
        if args.max_entries is not None and executed_entries >= args.max_entries:
            entry.status = "not_started_cap"
            return False

        pending_methods = []
        records = load_records(run_dir)
        completed_keys = {record["task_key"] for record in records}
        case = case_by_id[entry.case_id]
        for method in entry.methods:
            if runner.task_key(case, method, entry.query_index) not in completed_keys:
                pending_methods.append(method)
        if not pending_methods:
            entry.status = "already_complete"
            return True

        # Near the boundary, divide the remaining solver allowance across the
        # pending methods.  The runner still owns the hard global kill point.
        effective_timeout = min(
            entry.timeout_seconds,
            max(5.0, (remaining - 5.0) / max(1, len(pending_methods))),
        )
        entry.status = "running"
        update_status("running", entry)
        print(
            f"[ProbeCell] wave={entry.wave} case={entry.case_id} q={entry.query_index} "
            f"g={entry.g} methods={','.join(pending_methods)} timeout={effective_timeout:.1f}s "
            f"remaining={remaining / 3600.0:.2f}h reason={entry.reason}",
            flush=True,
        )
        command = [
            sys.executable,
            str(RUNNER_PATH),
            "--config",
            str(runtime_config),
            "--run-id",
            run_id,
            "--run-dir",
            str(run_dir),
            "--case",
            entry.case_id,
            "--query-index",
            str(entry.query_index),
            "--timeout",
            str(effective_timeout),
            "--graph-load-timeout",
            str(min(args.graph_load_timeout, max(5.0, remaining - 5.0))),
            "--wall-budget-seconds",
            str(max(1.0, remaining)),
            "--probe-diagnostics",
        ]
        for method in pending_methods:
            command.extend(["--method", method])
        completed = subprocess.run(command, cwd=ROOT, env=clean_env)
        executed_entries += 1
        entry.status = "budget_exhausted" if completed.returncode == 3 else (
            "complete" if completed.returncode == 0 else f"runner_error_{completed.returncode}"
        )
        update_status("running", entry)
        refresh_manifest()
        return completed.returncode in (0, 3)

    update_status("starting")
    stop = False
    for entry in breadth:
        if not run_entry(entry):
            stop = True
            break
        if time.monotonic() >= wall_deadline:
            stop = True
            break

    # Runtime-adaptive samples are kept separate from claim estimation.  They
    # exist solely to obtain a second/third query for causal diagnosis.
    if not stop:
        anomalies = anomaly_case_ids(load_records(run_dir))
        for case_id, _, reason in anomalies[:8]:
            case = case_by_id.get(case_id)
            if case is None:
                continue
            for rank in (2, 3):
                queries = ranked_queries(case, args.seed, {})
                if rank > len(queries):
                    continue
                query = queries[rank - 1]
                entry = ProbeEntry(
                    wave="adaptive_diagnosis",
                    case_id=case.case_id,
                    suite=case.suite,
                    dataset=case.dataset,
                    g=query.g,
                    mean_f=query.mean_f,
                    query_index=query.index,
                    query_rank=rank,
                    timeout_seconds=args.depth_timeout,
                    methods=[
                        method
                        for method in ["abhss_light", "abhss_heavy", "pruneddp_safe"]
                        if method in case.methods
                    ],
                    reason=reason,
                    outcome_adaptive=True,
                )
                adaptive.append(entry)
        refresh_manifest()

    remaining_schedule: Iterable[ProbeEntry] = [*adaptive, *depth, *filler]
    if not stop:
        for entry in remaining_schedule:
            if not run_entry(entry):
                break
            if time.monotonic() >= wall_deadline:
                break

    update_status("analyzing")
    analyzer = ROOT / "tools" / "experiments" / "analyze_probe_results.py"
    if analyzer.exists():
        subprocess.run(
            [sys.executable, str(analyzer), "--run-dir", str(run_dir)],
            cwd=ROOT,
            env=clean_env,
        )
    refresh_manifest()
    update_status("complete")
    print(f"[ProbeComplete] run_dir={run_dir}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
