#!/usr/bin/env python3
"""在固定物理核上并行、可恢复地运行 P1、P2 与受控 <g,f> 探针。"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import queue
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "tools" / "experiments" / "run_experiments.py"
CONFIG = ROOT / "experiments" / "paper_matrix.json"
sys.path.insert(0, str(RUNNER.parent))
import run_experiments as experiment_runner  # noqa: E402

P1_SUITES = ("P1_monogstplus_published", "P1_gpu4gst_published")
P2_SUITE = "P2_cross_g"
GF_SUITE = "S2_controlled_gf"
METHODS = ("abhss_base", "abhss_enhanced", "pruneddp_safe")
DUAL_CPUS = [0, 1]
FORMAL_TIMEOUT = 10000
GF_PROBE_TIMEOUT = 1000


def now() -> str:
    return datetime.now(timezone.utc).isoformat()


def run_id(prefix: str) -> str:
    commit = subprocess.check_output(["git", "rev-parse", "--short=12", "HEAD"], cwd=ROOT, text=True).strip()
    date = datetime.now().astimezone().strftime("%Y%m%d")
    return f"{prefix}_{commit}_{date}"


def invoke(cpu: int, run_dir: Path, run_name: str, extra: list[str], log_name: str) -> None:
    log_dir = run_dir / "campaign_logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    command = ["taskset", "-c", str(cpu), sys.executable, str(RUNNER), "--run-id", run_name, "--run-dir", str(run_dir), *extra]
    with (log_dir / log_name).open("a", encoding="utf-8") as log:
        log.write(f"\n[{now()}] {' '.join(command)}\n")
        log.flush()
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)


def consolidate(run_dir: Path) -> int:
    records_dir = run_dir / "records"
    temporary = run_dir / "records.jsonl.tmp"
    count = experiment_runner.combine_records(records_dir, temporary)
    temporary.replace(run_dir / "records.jsonl")
    return count


def run_shards(cpus: list[int], run_dir: Path, run_name: str, extra: list[str], label: str) -> None:
    run_dir.mkdir(parents=True, exist_ok=True)
    shard_count = len(cpus)
    # 先启动第 0 片，使运行元数据只由一个进程创建；随后各片只写互不重叠的任务记录。
    processes: list[tuple[int, subprocess.Popen, object]] = []
    for shard, cpu in enumerate(cpus):
        log_dir = run_dir / "campaign_logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        log = (log_dir / f"{label}_shard{shard}.log").open("a", encoding="utf-8")
        command = ["taskset", "-c", str(cpu), sys.executable, str(RUNNER), "--run-id", run_name, "--run-dir", str(run_dir), *extra, "--shard-index", str(shard), "--shard-count", str(shard_count)]
        log.write(f"\n[{now()}] {' '.join(command)}\n")
        log.flush()
        processes.append((shard, subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT), log))
        if shard == 0:
            deadline = time.time() + 60
            while not (run_dir / "run_metadata.json").exists() and processes[0][1].poll() is None and time.time() < deadline:
                time.sleep(0.2)
            if processes[0][1].poll() not in (None, 0):
                raise RuntimeError(f"{label} shard 0 failed before metadata creation")
    failures = []
    for shard, process, log in processes:
        code = process.wait()
        log.close()
        if code:
            failures.append((shard, code))
    consolidate(run_dir)
    if failures:
        raise RuntimeError(f"{label} failed shards: {failures}")


def load_records(run_dir: Path) -> list[dict]:
    return [json.loads(path.read_text(encoding="utf-8")) for path in (run_dir / "records").glob("*.json")]


def freeze_probe_timeout(run_dir: Path, suite: str, method: str, expected: int, query_index: int | None = None, ceiling: int = FORMAL_TIMEOUT) -> int:
    selected = [row for row in load_records(run_dir) if row["suite"] == suite and row["method"] == method and (query_index is None or row["query_index"] == query_index)]
    if len(selected) != expected:
        raise RuntimeError(f"{suite}/{method}: expected {expected} records, got {len(selected)}")
    bad = [row for row in selected if row["status"] not in ("ok", "timeout")]
    if bad:
        raise RuntimeError(f"{suite}/{method}: unexpected statuses {sorted({row['status'] for row in bad})}")
    if any(row["status"] == "timeout" for row in selected):
        return ceiling
    maximum = max(float(row["solver_seconds"]) for row in selected)
    return min(ceiling, max(1, math.ceil(2 * maximum)))


def write_policy(run_dir: Path, payload: dict) -> None:
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "campaign_policy.json").write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def case_records(run_dir: Path, suite: str, case_id: str, method: str) -> list[dict]:
    return [row for row in load_records(run_dir) if row["suite"] == suite and row["case_id"] == case_id and row["method"] == method]


def run_p2_frontier(run_dir: Path, run_name: str, cpus: list[int], timeout: int) -> list[dict]:
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    groups: dict[str, list] = {}
    for case in experiment_runner.expand_cases(config):
        if case.suite == P2_SUITE:
            groups.setdefault(case.dataset, []).append(case)
    for cases in groups.values():
        cases.sort(key=lambda case: int(case.attributes["g"]))
    lanes = [(method, dataset, cases) for method in ("abhss_base", "pruneddp_safe") for dataset, cases in sorted(groups.items())]
    cpu_pool: queue.Queue[int] = queue.Queue()
    for cpu in cpus:
        cpu_pool.put(cpu)

    def lane(method: str, dataset: str, cases: list) -> list[dict]:
        cpu = cpu_pool.get()
        skipped: list[dict] = []
        try:
            for position, case in enumerate(cases):
                invoke(cpu, run_dir, run_name, ["--suite", P2_SUITE, "--case", case.case_id, "--method", method, "--timeout", str(timeout)], f"p2_{method}_{dataset}.log")
                rows = case_records(run_dir, P2_SUITE, case.case_id, method)
                if len(rows) != 10:
                    raise RuntimeError(f"{case.case_id}/{method}: expected 10 records, got {len(rows)}")
                if any(row["status"] not in ("ok", "timeout") for row in rows):
                    raise RuntimeError(f"{case.case_id}/{method}: abnormal status")
                if any(row["status"] == "timeout" for row in rows):
                    for later in cases[position + 1:]:
                        skipped.append({"suite": P2_SUITE, "case_id": later.case_id, "dataset": dataset, "g": int(later.attributes["g"]), "method": method, "reason": "not_run_after_first_timeout_cell", "frontier_timeout_seconds": timeout})
                    break
        finally:
            cpu_pool.put(cpu)
        return skipped

    skipped: list[dict] = []
    with ThreadPoolExecutor(max_workers=len(cpus)) as executor:
        futures = [executor.submit(lane, *item) for item in lanes]
        for future in as_completed(futures):
            skipped.extend(future.result())
    return sorted(skipped, key=lambda row: (row["method"], row["dataset"], row["g"]))


def build_balanced_p1_lanes() -> list[list]:
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    cases = [case for case in experiment_runner.expand_cases(config) if case.suite in P1_SUITES]
    reference = ROOT / "results" / "paper_runs" / "p1_full" / "records"
    weights = {case.case_id: 0.0 for case in cases}
    for path in reference.glob("*.json"):
        record = json.loads(path.read_text(encoding="utf-8"))
        if record.get("method") not in ("abhss_base", "abhss_enhanced") or record.get("case_id") not in weights:
            continue
        weights[record["case_id"]] += float(record["solver_seconds"]) if record.get("solver_seconds") is not None else FORMAL_TIMEOUT
    if any(weight <= 0 for weight in weights.values()):
        raise RuntimeError("P1 balancing reference is incomplete")
    lanes: list[list] = [[], []]
    totals = [0.0, 0.0]
    for case in sorted(cases, key=lambda item: weights[item.case_id], reverse=True):
        lane = min(range(2), key=lambda index: totals[index])
        lanes[lane].append(case)
        totals[lane] += weights[case.case_id]
    return lanes


def run_balanced_p1(run_dir: Path, run_name: str, lanes: list[list]) -> None:
    assignment = {"policy": "LPT greedy by archived same-binary Base+Enhanced solver time", "lanes": [[case.case_id for case in lane] for lane in lanes]}
    (run_dir / "p1_lane_assignment.json").write_text(json.dumps(assignment, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    def run_lane(cpu: int, cases: list) -> None:
        for case in cases:
            invoke(cpu, run_dir, run_name, ["--suite", case.suite, "--case", case.case_id, "--method", "abhss_base", "--method", "abhss_enhanced", "--timeout", str(FORMAL_TIMEOUT)], f"p1_lane{cpu}.log")

    with ThreadPoolExecutor(max_workers=2) as executor:
        first = executor.submit(run_lane, DUAL_CPUS[0], lanes[0])
        deadline = time.time() + 60
        while not (run_dir / "run_metadata.json").exists() and not first.done() and time.time() < deadline:
            time.sleep(0.2)
        second = executor.submit(run_lane, DUAL_CPUS[1], lanes[1])
        first.result()
        second.result()
    consolidate(run_dir)


def campaign_p1() -> None:
    name = run_id("p1_dual_full_ours")
    directory = ROOT / "results" / "paper_runs" / name
    extra: list[str] = []
    for suite in P1_SUITES:
        extra += ["--suite", suite]
    for method in ("abhss_base", "abhss_enhanced"):
        extra += ["--method", method]
    extra += ["--timeout", str(FORMAL_TIMEOUT)]
    lanes = build_balanced_p1_lanes()
    write_policy(directory, {"campaign": "P1 full Base and Enhanced with archived PrunedDP++ reference", "created_at": now(), "cpus": DUAL_CPUS, "formal_timeout_seconds_per_query": FORMAL_TIMEOUT, "methods": ["abhss_base", "abhss_enhanced"], "pruneddp_reference": "results/paper_runs/p1_full", "suites": P1_SUITES, "lane_policy": "LPT greedy by archived same-binary Base+Enhanced solver time"})
    run_balanced_p1(directory, name, lanes)


def run_p2_enhanced_graph_lanes(run_dir: Path, run_name: str) -> None:
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    groups: dict[str, list] = {}
    for case in experiment_runner.expand_cases(config):
        if case.suite == P2_SUITE:
            groups.setdefault(case.dataset, []).append(case)
    for cases in groups.values():
        cases.sort(key=lambda case: int(case.attributes["g"]))

    def vertex_count(cases: list) -> int:
        graph_file = cases[0].graph_path / "graph.txt"
        with graph_file.open("r", encoding="utf-8") as source:
            return int(source.readline().split()[0])

    graph_lanes = sorted(groups.items(), key=lambda item: vertex_count(item[1]), reverse=True)
    assignment = {"policy": "dynamic graph lanes ordered by vertex count; ascending g within each graph", "graph_order": [dataset for dataset, _ in graph_lanes]}
    (run_dir / "p2_enhanced_lane_assignment.json").write_text(json.dumps(assignment, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    cpu_pool: queue.Queue[int] = queue.Queue()
    for cpu in DUAL_CPUS:
        cpu_pool.put(cpu)

    def run_graph(dataset: str, cases: list) -> None:
        cpu = cpu_pool.get()
        try:
            for case in cases:
                invoke(cpu, run_dir, run_name, ["--suite", P2_SUITE, "--case", case.case_id, "--method", "abhss_enhanced", "--timeout", str(FORMAL_TIMEOUT)], f"p2_enhanced_{dataset}.log")
        finally:
            cpu_pool.put(cpu)

    with ThreadPoolExecutor(max_workers=2) as executor:
        first = executor.submit(run_graph, *graph_lanes[0])
        deadline = time.time() + 60
        while not (run_dir / "run_metadata.json").exists() and not first.done() and time.time() < deadline:
            time.sleep(0.2)
        futures = [first] + [executor.submit(run_graph, *item) for item in graph_lanes[1:]]
        for future in futures:
            future.result()
    consolidate(run_dir)


def campaign_p2() -> None:
    cpus = DUAL_CPUS
    p2_name = run_id("p2_dual_full_enhanced_adaptive_frontier")
    p2_dir = ROOT / "results" / "paper_runs" / p2_name
    write_policy(p2_dir, {"campaign": "P2 full Enhanced plus adaptive Base/PrunedDP++ frontier", "created_at": now(), "cpus": cpus, "formal_timeout_seconds_per_query": FORMAL_TIMEOUT, "phase": "enhanced_running"})
    run_p2_enhanced_graph_lanes(p2_dir, p2_name)
    p2_timeout = freeze_probe_timeout(p2_dir, P2_SUITE, "abhss_enhanced", 660)
    write_policy(p2_dir, {"campaign": "P2 full Enhanced plus adaptive Base/PrunedDP++ frontier", "created_at": now(), "cpus": cpus, "formal_timeout_seconds_per_query": FORMAL_TIMEOUT, "exploratory_frontier_timeout_seconds": p2_timeout, "frontier_rule": "for each graph and method, run g=5..15 and stop after the first cell containing a timeout; larger g are explicitly not run", "timeout_freeze_rule": "min(10000, ceil(2 * maximum Enhanced solver_seconds)); use 10000 if Enhanced timed out", "phase": "base_pruned_frontier_running"})
    skipped = run_p2_frontier(p2_dir, p2_name, cpus, p2_timeout)
    consolidate(p2_dir)
    policy = json.loads((p2_dir / "campaign_policy.json").read_text(encoding="utf-8"))
    policy.update({"phase": "complete", "completed_at": now(), "skipped_cells": skipped})
    write_policy(p2_dir, policy)


def campaign_gf() -> None:
    cpus = DUAL_CPUS
    gf_name = run_id("gf_dual_q3_probe_1000s_clean")
    gf_dir = ROOT / "results" / "paper_runs" / gf_name
    write_policy(gf_dir, {"campaign": "controlled <g,f> one-query-per-cell probe", "created_at": now(), "cpus": cpus, "probe_timeout_seconds_per_query": GF_PROBE_TIMEOUT, "query_index": 3, "phase": "enhanced_running"})
    run_shards(cpus, gf_dir, gf_name, ["--suite", GF_SUITE, "--method", "abhss_enhanced", "--query-index", "3", "--timeout", str(GF_PROBE_TIMEOUT)], "gf_enhanced")
    gf_timeout = freeze_probe_timeout(gf_dir, GF_SUITE, "abhss_enhanced", 30, 3, GF_PROBE_TIMEOUT)
    run_shards(cpus, gf_dir, gf_name, ["--suite", GF_SUITE, "--method", "abhss_base", "--method", "pruneddp_safe", "--query-index", "3", "--timeout", str(gf_timeout)], "gf_base_pruned")
    policy = json.loads((gf_dir / "campaign_policy.json").read_text(encoding="utf-8"))
    policy.update({"phase": "complete", "completed_at": now(), "exploratory_timeout_seconds": gf_timeout, "timeout_freeze_rule": "min(1000, ceil(2 * maximum Enhanced solver_seconds)); use 1000 if Enhanced timed out"})
    write_policy(gf_dir, policy)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("campaign", choices=("gf", "p1", "p2"))
    args = parser.parse_args()
    if args.campaign == "p1":
        campaign_p1()
    elif args.campaign == "p2":
        campaign_p2()
    else:
        campaign_gf()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
