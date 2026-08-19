#!/usr/bin/env python3
"""Run the frozen dual-core paper campaign with strict stop/provenance rules."""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "tools" / "experiments" / "run_experiments.py"
CONFIG = ROOT / "experiments" / "paper_matrix.json"
PLAN = ROOT / "experiments" / "final_campaign_plan.json"
ABLATION_PLAN = ROOT / "experiments" / "ablation_plan.json"
ABLATION_BUILDER = ROOT / "tools" / "experiments" / "build_endpoint_floor_ablation.py"
sys.path.insert(0, str(RUNNER.parent))
import run_experiments as experiment_runner  # noqa: E402

CPUS = (4, 5)
TIMEOUT = 10_000
P1_SUITES = ("P1_monogstplus_published", "P1_gpu4gst_published")
P2_SUITE = "P2_cross_g"
S2_SUITE = "S2_controlled_gf"
ABHSS_SHA256 = "793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced"
PRUNED_SHA256 = "4c1d3599f03da6073d368a6a83fcbd31ea0a625f9ba90892b22b0b239eb42bf2"
DEFAULT_RUN_DIR = ROOT / "results" / "paper_runs" / "final_g15_campaign_793d4e_20260820"
P1_OURS_DIRS = (
    ROOT / "results" / "paper_runs" / "final_793d4e_p1_w0_cpu4_20260818",
    ROOT / "results" / "paper_runs" / "final_793d4e_p1_w1_cpu5_20260818",
)
P1_PRUNED_DIR = ROOT / "results" / "paper_runs" / "p1_full"
P1_AUDIT = ROOT / "results" / "paper_runs" / "final_793d4e_p1_audit.json"
ORKUT_DIRS = (
    ROOT / "results" / "paper_runs" / "final_793d4e_orkut_g15_w0_cpu4",
    ROOT / "results" / "paper_runs" / "final_793d4e_orkut_g15_w1_cpu5",
)
ORKUT_AUDIT = ROOT / "results" / "paper_runs" / "final_793d4e_orkut_g15_audit.json"
OLD_P2_ESTIMATE_DIR = ROOT / "results" / "paper_runs" / "p2_dual_full_enhanced_adaptive_frontier_672bd253cdde_20260802"
OLD_S2_ESTIMATE_DIR = ROOT / "results" / "paper_runs" / "gf_dual_q3_probe_1000s_clean_672bd253cdde_20260802"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def records_from(directory: Path) -> list[dict[str, Any]]:
    records = directory / "records"
    if records.is_dir():
        return [read_json(path) for path in records.glob("*.json")]
    combined = directory / "records.jsonl"
    if combined.exists():
        return [json.loads(line) for line in combined.read_text(encoding="utf-8").splitlines() if line]
    return []


def record_map_under(directory: Path) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    if not directory.exists():
        return result
    for path in directory.glob("cpu*/records/*.json"):
        record = read_json(path)
        key = record["task_key"]
        if key in result and result[key] != record:
            raise RuntimeError(f"conflicting records for {key}")
        result[key] = record
    return result


def load_cases() -> list[Any]:
    return experiment_runner.expand_cases(read_json(CONFIG))


def case_query_map(cases: list[Any]) -> dict[str, list[Any]]:
    result = {}
    for case in cases:
        result[case.case_id] = [
            query
            for query in experiment_runner.read_query_metadata(case.query_path)
            if (case.min_g is None or query.g >= case.min_g)
            and (case.max_g is None or query.g <= case.max_g)
        ]
    return result


def expected_keys(cases: list[Any], queries: dict[str, list[Any]], suites: set[str], methods: tuple[str, ...]) -> set[str]:
    return {
        experiment_runner.task_key(case, method, query.index)
        for case in cases
        if case.suite in suites
        for method in methods
        for query in queries[case.case_id]
    }


def topology(cpu: int) -> tuple[int, int]:
    root = Path(f"/sys/devices/system/cpu/cpu{cpu}/topology")
    return (int((root / "physical_package_id").read_text()), int((root / "core_id").read_text()))


def validate_identity(cases: list[Any], queries: dict[str, list[Any]]) -> dict[str, Any]:
    if subprocess.run(["git", "diff", "--quiet"], cwd=ROOT).returncode or subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=ROOT).returncode:
        raise RuntimeError("tracked files must be clean before preparing or running the formal campaign")
    static_plan = read_json(PLAN)
    production = static_plan.get("production_identity", {})
    if production.get("abhss_sha256") != ABHSS_SHA256 or production.get("pruneddp_sha256") != PRUNED_SHA256 or production.get("physical_cpus") != list(CPUS) or int(production.get("timeout_seconds_per_query", -1)) != TIMEOUT:
        raise RuntimeError("static campaign plan and scheduler production identities disagree")
    if sha256_file(ROOT / "build" / "abhss") != ABHSS_SHA256:
        raise RuntimeError("build/abhss is not the frozen production binary")
    if sha256_file(ROOT / "build" / "pruneddp") != PRUNED_SHA256:
        raise RuntimeError("build/pruneddp is not the frozen baseline binary")
    allowed = os.sched_getaffinity(0)
    if any(cpu not in allowed for cpu in CPUS):
        raise RuntimeError(f"CPUs {CPUS} are not both in the current affinity set")
    if topology(CPUS[0]) == topology(CPUS[1]):
        raise RuntimeError(f"CPUs {CPUS} share one physical core")

    p2_cases = [case for case in cases if case.suite == P2_SUITE]
    p2_grid = {(case.dataset, int(case.attributes["g"])) for case in p2_cases}
    if len(p2_cases) != 66 or {g for _, g in p2_grid} != set(range(5, 16)):
        raise RuntimeError("P2 must be six graphs times g=5..15")
    if any(len(queries[case.case_id]) != 10 for case in p2_cases):
        raise RuntimeError("every P2 cell must contain ten queries")
    s2_cases = [case for case in cases if case.suite == S2_SUITE]
    if len(s2_cases) != 30 or any(len(queries[case.case_id]) != 5 for case in s2_cases):
        raise RuntimeError("S2 must contain 30 five-query cells")

    p1_audit = read_json(P1_AUDIT)
    if not p1_audit.get("correctness_passed") or not p1_audit.get("performance_passed"):
        raise RuntimeError("the frozen P1 audit did not pass")
    if p1_audit.get("binary_sha256") != ABHSS_SHA256:
        raise RuntimeError("the P1 audit used a different ABHSS binary")
    p1_rows = []
    for directory in P1_OURS_DIRS:
        metadata = read_json(directory / "run_metadata.json")
        if metadata.get("binary_sha256", {}).get("abhss_base") != ABHSS_SHA256 or metadata.get("binary_sha256", {}).get("abhss_enhanced") != ABHSS_SHA256:
            raise RuntimeError(f"historical P1 worker has another ABHSS binary: {directory}")
        p1_rows.extend(records_from(directory))
    pruned_metadata = read_json(P1_PRUNED_DIR / "run_metadata.json")
    if pruned_metadata.get("binary_sha256", {}).get("pruneddp_safe") != PRUNED_SHA256:
        raise RuntimeError("historical P1 baseline has another PrunedDP++ binary")
    p1_rows.extend(row for row in records_from(P1_PRUNED_DIR) if row.get("method") == "pruneddp_safe")
    p1_actual = {row["task_key"] for row in p1_rows}
    p1_expected = expected_keys(cases, queries, set(P1_SUITES), ("abhss_base", "abhss_enhanced", "pruneddp_safe"))
    if len(p1_rows) != len(p1_actual) or p1_actual != p1_expected or any(row.get("status") != "ok" for row in p1_rows):
        raise RuntimeError("historical P1 records do not exactly cover the current P1 task identities")

    orkut_audit = read_json(ORKUT_AUDIT)
    if not orkut_audit.get("passed") or orkut_audit.get("binary_sha256") != ABHSS_SHA256:
        raise RuntimeError("the frozen Orkut g=15 audit is not reusable")
    orkut_rows = []
    for directory in ORKUT_DIRS:
        metadata = read_json(directory / "run_metadata.json")
        if metadata.get("binary_sha256", {}).get("abhss_enhanced") != ABHSS_SHA256:
            raise RuntimeError(f"historical Orkut worker has another ABHSS binary: {directory}")
        orkut_rows.extend(records_from(directory))
    orkut_case = next(case for case in p2_cases if case.dataset == "Orkut-GPU4GST" and int(case.attributes["g"]) == 15)
    orkut_expected = {experiment_runner.task_key(orkut_case, "abhss_enhanced", query.index) for query in queries[orkut_case.case_id]}
    if len(orkut_rows) != 10 or {row["task_key"] for row in orkut_rows} != orkut_expected or any(row.get("status") != "ok" for row in orkut_rows):
        raise RuntimeError("historical Orkut g=15 records do not cover q1--q10 exactly")

    return {
        "validated_at": utc_now(),
        "current_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "current_matrix_sha256": sha256_file(CONFIG),
        "binary_sha256": {"abhss": ABHSS_SHA256, "pruneddp": PRUNED_SHA256},
        "cpus": [{"cpu": cpu, "package_core": topology(cpu)} for cpu in CPUS],
        "p1": {"status": "reused", "records": len(p1_rows), "sources": [str(path.relative_to(ROOT)) for path in (*P1_OURS_DIRS, P1_PRUNED_DIR)], "audit": str(P1_AUDIT.relative_to(ROOT))},
        "p2_orkut_g15_enhanced": {"status": "reused", "records": len(orkut_rows), "sources": [str(path.relative_to(ROOT)) for path in ORKUT_DIRS], "audit": str(ORKUT_AUDIT.relative_to(ROOT))},
    }


def estimate_tables() -> tuple[dict[str, float], dict[str, str]]:
    values: dict[str, float] = {}
    sources: dict[str, str] = {}
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in records_from(OLD_P2_ESTIMATE_DIR):
        if row.get("method") == "abhss_enhanced" and row.get("g", 99) <= 15:
            grouped[row["case_id"]].append(row)
    for case_id, rows in grouped.items():
        cost = sum(float(row.get("solver_seconds") or row.get("timeout_seconds") or TIMEOUT) for row in rows)
        values[case_id] = cost * 10.0 / len(rows)
        sources[case_id] = "historical five-query P2 Enhanced probe scaled to ten queries"
    for row in records_from(OLD_S2_ESTIMATE_DIR):
        if row.get("method") != "abhss_enhanced" or row.get("query_index") != 3:
            continue
        values[row["case_id"]] = 5.0 * float(row.get("solver_seconds") or TIMEOUT)
        sources[row["case_id"]] = "historical predeclared q3 Enhanced probe scaled to five queries"
    return values, sources


@dataclass(frozen=True)
class Job:
    phase: str
    suite: str
    case_id: str
    dataset: str
    method: str
    query_indices: tuple[int, ...]
    estimate_seconds: float
    estimate_source: str
    stop_on_timeout: bool = False

    @property
    def job_id(self) -> str:
        indices = "-".join(map(str, self.query_indices))
        return f"{self.phase}__{self.method}__{self.case_id}__q{indices}"


def estimate_job(case: Any, estimate: dict[str, float], sources: dict[str, str], query_count: int) -> tuple[float, str]:
    if case.case_id in estimate:
        base = estimate[case.case_id]
        total = 10 if case.suite == P2_SUITE else 5
        return base * query_count / total, sources[case.case_id]
    g = int((case.attributes or {}).get("g", 1))
    with (case.graph_path / "graph.txt").open("r", encoding="utf-8") as source:
        n, m = map(int, source.readline().split()[:2])
    return 1e9 + (2**g) * (n + m) * query_count / 1e6, "deterministic graph-size/subset-count fallback used only for queue ordering"


def build_enhanced_jobs(cases: list[Any], queries: dict[str, list[Any]]) -> list[Job]:
    estimate, sources = estimate_tables()
    jobs = []
    for case in cases:
        if case.suite not in (P2_SUITE, S2_SUITE):
            continue
        if case.suite == P2_SUITE and case.dataset == "Orkut-GPU4GST" and int(case.attributes["g"]) == 15:
            continue
        indices = tuple(query.index for query in queries[case.case_id])
        cost, source = estimate_job(case, estimate, sources, len(indices))
        jobs.append(Job("enhanced_main", case.suite, case.case_id, case.dataset, "abhss_enhanced", indices, cost, source, True))
    return sorted(jobs, key=lambda job: (job.estimate_seconds, job.suite, job.case_id))


def prepare(run_dir: Path) -> dict[str, Any]:
    cases = load_cases()
    queries = case_query_map(cases)
    reuse = validate_identity(cases, queries)
    jobs = build_enhanced_jobs(cases, queries)
    plan = {
        "schema_version": 1,
        "prepared_at": utc_now(),
        "static_plan": str(PLAN.relative_to(ROOT)),
        "run_dir": str(run_dir.relative_to(ROOT)),
        "cpus": list(CPUS),
        "timeout_seconds_per_query": TIMEOUT,
        "phase_order": ["reuse_p1_and_orkut_g15", "enhanced_main_fast_to_slow", "p2_base_pruned_ascending_g", "s2_base_pruned_fast_to_slow", "minimal_ablations"],
        "enhanced_jobs": [asdict(job) | {"job_id": job.job_id} for job in jobs],
        "enhanced_new_tasks": sum(len(job.query_indices) for job in jobs),
        "enhanced_reused_p2_tasks": 10,
        "p1_reused_tasks": 24_954,
        "p2_competitor_tasks_before_likely_timeout_stops": 1_320,
        "s2_competitor_tasks": 300,
        "ablation_new_tasks": 135,
    }
    run_dir.mkdir(parents=True, exist_ok=True)
    write_json(run_dir / "reuse_manifest.json", reuse)
    write_json(run_dir / "execution_plan.json", plan)
    state_path = run_dir / "campaign_state.json"
    if not state_path.exists():
        write_json(state_path, {"schema_version": 1, "status": "prepared", "phase": "prepared", "updated_at": utc_now(), "completed_jobs": 0})
    return plan


class CampaignStop(RuntimeError):
    pass


@dataclass
class ActiveJob:
    cpu: int
    job: Job
    process: subprocess.Popen[Any]
    log: Any
    worker_dir: Path
    expected_keys: set[str]


def set_state(run_dir: Path, **updates: Any) -> None:
    path = run_dir / "campaign_state.json"
    state = read_json(path) if path.exists() else {"schema_version": 1}
    state.update(updates)
    state["updated_at"] = utc_now()
    write_json(path, state)


def job_keys(job: Job, case_by_id: dict[str, Any]) -> set[str]:
    case = case_by_id[job.case_id]
    return {experiment_runner.task_key(case, job.method, index) for index in job.query_indices}


def terminate_active(active: dict[int, ActiveJob]) -> None:
    for running in active.values():
        if running.process.poll() is None:
            try:
                os.killpg(running.process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
    deadline = time.time() + 5
    while time.time() < deadline and any(item.process.poll() is None for item in active.values()):
        time.sleep(0.1)
    for running in active.values():
        if running.process.poll() is None:
            try:
                os.killpg(running.process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        running.log.close()


def run_jobs(run_dir: Path, jobs: list[Job], case_by_id: dict[str, Any], worker_name: str, config: Path = CONFIG, allow_timeout: bool = True) -> None:
    worker_root = run_dir / worker_name
    logs = run_dir / "scheduler_logs"
    logs.mkdir(parents=True, exist_ok=True)
    pending = list(jobs)
    active: dict[int, ActiveJob] = {}
    previous_handlers = {kind: signal.getsignal(kind) for kind in (signal.SIGINT, signal.SIGTERM)}

    def Interrupt(_kind: int, _frame: Any) -> None:
        raise KeyboardInterrupt

    for kind in previous_handlers:
        signal.signal(kind, Interrupt)
    try:
        while pending or active:
            records = record_map_under(worker_root)
            while pending and len(active) < len(CPUS):
                job = pending.pop(0)
                keys = job_keys(job, case_by_id)
                missing = sorted(index for index in job.query_indices if experiment_runner.task_key(case_by_id[job.case_id], job.method, index) not in records)
                if not missing:
                    continue
                cpu = next(value for value in CPUS if value not in active)
                worker_dir = worker_root / f"cpu{cpu}"
                log_path = logs / (hashlib.sha256(job.job_id.encode()).hexdigest()[:16] + ".log")
                log = log_path.open("a", encoding="utf-8")
                command = ["taskset", "-c", str(cpu), sys.executable, str(RUNNER), "--config", str(config), "--run-id", run_dir.name, "--run-dir", str(worker_dir), "--suite", job.suite, "--case", job.case_id, "--method", job.method, "--timeout", str(TIMEOUT)]
                for index in missing:
                    command += ["--query-index", str(index)]
                if job.stop_on_timeout:
                    command.append("--stop-on-timeout")
                log.write(f"\n[{utc_now()}] {' '.join(command)}\n")
                log.flush()
                process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                active[cpu] = ActiveJob(cpu, job, process, log, worker_dir, keys)
                set_state(run_dir, status="running", phase=job.phase, active={str(item.cpu): {"pid": item.process.pid, "job_id": item.job.job_id} for item in active.values()})
            if not active:
                continue
            time.sleep(1)
            for cpu, running in list(active.items()):
                code = running.process.poll()
                if code is None:
                    continue
                running.log.close()
                del active[cpu]
                records = record_map_under(worker_root)
                rows = [records[key] for key in running.expected_keys if key in records]
                statuses = Counter(row.get("status") for row in rows)
                if code not in (0, 4):
                    raise CampaignStop(f"{running.job.job_id} exited with code {code}")
                if any(status not in ("ok", "timeout") for status in statuses):
                    raise CampaignStop(f"{running.job.job_id} produced abnormal statuses {dict(statuses)}")
                if not allow_timeout and statuses.get("timeout", 0):
                    raise CampaignStop(f"Enhanced reached the 10000-second limit in {running.job.job_id}")
                if code == 4:
                    raise CampaignStop(f"stop-on-timeout triggered in {running.job.job_id}")
                if len(rows) != len(running.expected_keys):
                    raise CampaignStop(f"{running.job.job_id} produced {len(rows)}/{len(running.expected_keys)} records")
                state = read_json(run_dir / "campaign_state.json")
                set_state(run_dir, completed_jobs=int(state.get("completed_jobs", 0)) + 1, active={str(item.cpu): {"pid": item.process.pid, "job_id": item.job.job_id} for item in active.values()})
    except BaseException:
        terminate_active(active)
        raise
    finally:
        for kind, handler in previous_handlers.items():
            signal.signal(kind, handler)
    set_state(run_dir, active={})


def historical_orkut_map() -> dict[str, dict[str, Any]]:
    return {row["task_key"]: row for directory in ORKUT_DIRS for row in records_from(directory)}


def validate_enhanced_complete(run_dir: Path, cases: list[Any], queries: dict[str, list[Any]]) -> None:
    rows = record_map_under(run_dir / "workers") | historical_orkut_map()
    expected = expected_keys(cases, queries, {P2_SUITE, S2_SUITE}, ("abhss_enhanced",))
    if set(rows) & expected != expected:
        raise CampaignStop(f"Enhanced coverage is {len(set(rows) & expected)}/{len(expected)}")
    bad = [rows[key] for key in expected if rows[key].get("status") != "ok"]
    if bad:
        raise CampaignStop(f"Enhanced has non-ok records: {Counter(row.get('status') for row in bad)}")


def write_predicted_stops(path: Path, rows: list[dict[str, Any]]) -> None:
    current = {}
    if path.exists():
        current = {row["task_key"]: row for row in (json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line)}
    for row in rows:
        current.setdefault(row["task_key"], row)
    temporary = path.with_suffix(".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        for key in sorted(current):
            output.write(json.dumps(current[key], sort_keys=True) + "\n")
    temporary.replace(path)


def validate_completed_quality(run_dir: Path, case_by_id: dict[str, Any], suites: set[str]) -> None:
    records = record_map_under(run_dir / "workers")
    references = records | historical_orkut_map()
    mismatches = []
    for row in records.values():
        if row.get("suite") not in suites or row.get("method") not in ("abhss_base", "pruneddp_safe") or row.get("status") != "ok":
            continue
        case = case_by_id[row["case_id"]]
        key = experiment_runner.task_key(case, "abhss_enhanced", int(row["query_index"]))
        reference = references.get(key)
        if reference is None or reference.get("status") != "ok" or reference.get("solution_status") != row.get("solution_status"):
            mismatches.append((row["task_key"], key, "status"))
        elif row.get("solution_status") == "feasible" and abs(float(reference["weight"]) - float(row["weight"])) > 1e-6:
            mismatches.append((row["task_key"], key, "weight"))
    if mismatches:
        raise CampaignStop(f"completed main-method quality mismatches: {mismatches[:5]}")


def run_p2_competitors(run_dir: Path, cases: list[Any], queries: dict[str, list[Any]], case_by_id: dict[str, Any]) -> None:
    estimate, sources = estimate_tables()
    p2 = {(case.dataset, int(case.attributes["g"])): case for case in cases if case.suite == P2_SUITE}
    datasets = sorted({dataset for dataset, _ in p2})
    stopped: set[tuple[str, str]] = set()
    predicted_path = run_dir / "predicted_stops.jsonl"
    if predicted_path.exists():
        for line in predicted_path.read_text(encoding="utf-8").splitlines():
            row = json.loads(line)
            stopped.add((row["method"], row["dataset"]))
    for g in range(5, 16):
        tranche1 = []
        for method in ("abhss_base", "pruneddp_safe"):
            for dataset in datasets:
                if (method, dataset) in stopped:
                    continue
                case = p2[(dataset, g)]
                cost, source = estimate_job(case, estimate, sources, 5)
                tranche1.append(Job("p2_competitors_tranche1", P2_SUITE, case.case_id, dataset, method, (1, 2, 3, 4, 5), cost, source))
        tranche1.sort(key=lambda job: (job.estimate_seconds, job.method, job.dataset))
        run_jobs(run_dir, tranche1, case_by_id, "workers", allow_timeout=True)
        validate_completed_quality(run_dir, case_by_id, {P2_SUITE})
        main_records = record_map_under(run_dir / "workers")
        enhanced_records = main_records | historical_orkut_map()
        tranche2 = []
        predicted = []
        for job in tranche1:
            case = case_by_id[job.case_id]
            rows = [main_records.get(experiment_runner.task_key(case, job.method, index)) for index in job.query_indices]
            if any(row is None for row in rows):
                raise CampaignStop(f"missing completed tranche-1 records for {job.job_id}")
            if all(row.get("status") == "timeout" for row in rows):
                enhanced_keys = [experiment_runner.task_key(case, "abhss_enhanced", index) for index in job.query_indices]
                if any(enhanced_records.get(key, {}).get("status") != "ok" for key in enhanced_keys):
                    raise CampaignStop(f"cannot certify likely-timeout stop without solved Enhanced references for {job.job_id}")
                evidence = [row["task_key"] for row in rows]
                for later_g in range(g, 16):
                    later = p2[(job.dataset, later_g)]
                    begin = 6 if later_g == g else 1
                    for query in queries[later.case_id]:
                        if query.index < begin:
                            continue
                        key = experiment_runner.task_key(later, job.method, query.index)
                        if key in main_records:
                            continue
                        predicted.append({"schema_version": 1, "task_key": key, "suite": P2_SUITE, "case_id": later.case_id, "dataset": job.dataset, "g": later_g, "method": job.method, "query_index": query.index, "status": "not_run_likely_timeout", "not_a_formal_timeout": True, "criterion": "all five preregistered tranche-1 size-stratum queries at the current g reached the real 10000-second limit while Enhanced solved all five", "evidence_timeout_task_keys": evidence, "recorded_at": utc_now()})
                stopped.add((job.method, job.dataset))
            else:
                cost, source = estimate_job(case, estimate, sources, 5)
                tranche2.append(Job("p2_competitors_tranche2", P2_SUITE, case.case_id, job.dataset, job.method, (6, 7, 8, 9, 10), cost, source))
        if predicted:
            write_predicted_stops(predicted_path, predicted)
        tranche2.sort(key=lambda job: (job.estimate_seconds, job.method, job.dataset))
        run_jobs(run_dir, tranche2, case_by_id, "workers", allow_timeout=True)
        validate_completed_quality(run_dir, case_by_id, {P2_SUITE})
    set_state(run_dir, phase="p2_competitors_complete", p2_likely_timeout_stopped_lanes=[{"method": method, "dataset": dataset} for method, dataset in sorted(stopped)])


def run_s2_competitors(run_dir: Path, cases: list[Any], queries: dict[str, list[Any]], case_by_id: dict[str, Any]) -> None:
    estimate, sources = estimate_tables()
    jobs = []
    for case in cases:
        if case.suite != S2_SUITE:
            continue
        indices = tuple(query.index for query in queries[case.case_id])
        cost, source = estimate_job(case, estimate, sources, len(indices))
        for method in ("abhss_base", "pruneddp_safe"):
            jobs.append(Job("s2_competitors", S2_SUITE, case.case_id, case.dataset, method, indices, cost, source))
    jobs.sort(key=lambda job: (job.estimate_seconds, job.method, job.case_id))
    run_jobs(run_dir, jobs, case_by_id, "workers", allow_timeout=True)
    validate_completed_quality(run_dir, case_by_id, {S2_SUITE})


def prepare_ablation_config(run_dir: Path) -> Path:
    config = read_json(CONFIG)
    config["methods"].update({
        "abhss_directed_cut_only": {"kind": "native", "executable": "build/abhss", "arguments": ["--enhancements=directed-cut"]},
        "abhss_no_endpoint_base": {"kind": "native", "executable": "build-ablation-no-endpoint-floor/abhss", "arguments": ["--enhancements=none"]},
        "abhss_no_endpoint_enhanced": {"kind": "native", "executable": "build-ablation-no-endpoint-floor/abhss", "arguments": ["--enhancements=all"]},
    })
    for suite in config["suites"]:
        if suite["id"] == P2_SUITE:
            suite["methods"] = list(suite["methods"]) + ["abhss_directed_cut_only", "abhss_no_endpoint_base", "abhss_no_endpoint_enhanced"]
    path = run_dir / "ablation_matrix.json"
    write_json(path, config)
    return path


def run_ablations(run_dir: Path, cases: list[Any], queries: dict[str, list[Any]]) -> None:
    subprocess.run([sys.executable, str(ABLATION_BUILDER), "--jobs", "16"], cwd=ROOT, check=True)
    config = prepare_ablation_config(run_dir)
    ablation = read_json(ABLATION_PLAN)
    wanted_datasets = set(ablation["selection"]["datasets"])
    wanted_g = set(map(int, ablation["selection"]["g"]))
    case_by_id = {case.case_id: case for case in experiment_runner.expand_cases(read_json(config))}
    estimate, sources = estimate_tables()
    jobs = []
    for case in case_by_id.values():
        if case.suite != P2_SUITE or case.dataset not in wanted_datasets or int(case.attributes["g"]) not in wanted_g:
            continue
        cost, source = estimate_job(case, estimate, sources, 5)
        for method in ("abhss_directed_cut_only", "abhss_no_endpoint_base", "abhss_no_endpoint_enhanced"):
            jobs.append(Job("minimal_ablations", P2_SUITE, case.case_id, case.dataset, method, (1, 2, 3, 4, 5), cost, source))
    jobs.sort(key=lambda job: (job.estimate_seconds, job.method, job.case_id))
    run_jobs(run_dir, jobs, case_by_id, "ablation_workers", config, allow_timeout=True)
    main = record_map_under(run_dir / "workers") | historical_orkut_map()
    ablated = record_map_under(run_dir / "ablation_workers")
    mismatches = []
    for row in ablated.values():
        if row.get("status") != "ok":
            continue
        case = case_by_id[row["case_id"]]
        reference_key = experiment_runner.task_key(case, "abhss_enhanced", int(row["query_index"]))
        reference = main.get(reference_key)
        if reference is None or reference.get("status") != "ok" or abs(float(reference["weight"]) - float(row["weight"])) > 1e-6:
            mismatches.append((row["task_key"], reference_key))
    if mismatches:
        raise CampaignStop(f"ablation objective mismatches: {mismatches[:5]}")


def status(run_dir: Path) -> int:
    state = read_json(run_dir / "campaign_state.json") if (run_dir / "campaign_state.json").exists() else {"status": "not prepared"}
    main = record_map_under(run_dir / "workers")
    ablation = record_map_under(run_dir / "ablation_workers")
    predicted = [json.loads(line) for line in (run_dir / "predicted_stops.jsonl").read_text().splitlines()] if (run_dir / "predicted_stops.jsonl").exists() else []
    print(json.dumps({"state": state, "main_records": len(main), "main_statuses": {str(key): value for key, value in Counter((row.get("method"), row.get("status")) for row in main.values()).items()}, "ablation_records": len(ablation), "likely_timeout_not_run": len(predicted)}, indent=2, sort_keys=True))
    return 0


def run_campaign(run_dir: Path) -> int:
    lock_path = run_dir / "campaign.lock"
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with lock_path.open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError("another campaign scheduler already holds the lock")
        plan = prepare(run_dir)
        cases = load_cases()
        queries = case_query_map(cases)
        case_by_id = {case.case_id: case for case in cases}
        try:
            jobs = [Job(**{key: tuple(value) if key == "query_indices" else value for key, value in row.items() if key != "job_id"}) for row in plan["enhanced_jobs"]]
            run_jobs(run_dir, jobs, case_by_id, "workers", allow_timeout=False)
            validate_enhanced_complete(run_dir, cases, queries)
            set_state(run_dir, phase="enhanced_main_complete")
            run_p2_competitors(run_dir, cases, queries, case_by_id)
            run_s2_competitors(run_dir, cases, queries, case_by_id)
            set_state(run_dir, phase="main_matrix_complete")
            run_ablations(run_dir, cases, queries)
            predicted = run_dir / "predicted_stops.jsonl"
            final_status = "complete_with_likely_timeout_stops" if predicted.exists() and predicted.stat().st_size else "complete"
            set_state(run_dir, status=final_status, phase="complete", completed_at=utc_now())
            return 0
        except CampaignStop as error:
            set_state(run_dir, status="stopped", phase="stopped", stop_reason=str(error), stopped_at=utc_now(), active={})
            print(f"CAMPAIGN STOPPED: {error}", file=sys.stderr, flush=True)
            return 4
        except KeyboardInterrupt:
            set_state(run_dir, status="interrupted", phase="interrupted", stopped_at=utc_now(), active={})
            print("CAMPAIGN INTERRUPTED: active process groups were terminated; rerun is resumable.", file=sys.stderr, flush=True)
            return 130


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("prepare", "run", "status"))
    parser.add_argument("--run-dir", type=Path, default=DEFAULT_RUN_DIR)
    args = parser.parse_args()
    run_dir = args.run_dir if args.run_dir.is_absolute() else ROOT / args.run_dir
    if args.command == "prepare":
        plan = prepare(run_dir)
        print(json.dumps({"run_dir": str(run_dir), "enhanced_jobs": len(plan["enhanced_jobs"]), "enhanced_new_tasks": plan["enhanced_new_tasks"], "p1_reused_tasks": plan["p1_reused_tasks"]}, indent=2))
        return 0
    if args.command == "status":
        return status(run_dir)
    return run_campaign(run_dir)


if __name__ == "__main__":
    raise SystemExit(main())
