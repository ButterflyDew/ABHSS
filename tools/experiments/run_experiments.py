#!/usr/bin/env python3
"""Resumable, per-instance-deadline experiment supervisor.

Native solvers keep a graph loaded while consecutive queries run.  The
supervisor starts (and resets) the 10,000-second deadline on the solver's
``[Ready]``/``[Query]`` markers.  If one query times out, only that process is
killed; the next query resumes in a fresh process.  Thus easy queries do not
subsidize hard queries and a timeout never discards completed results.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import queue
import re
import shlex
import socket
import subprocess
import sys
import threading
import time
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONFIG = ROOT / "experiments" / "paper_matrix.json"
QUERY_RESULT = re.compile(
    r"^\[Query\s+(\d+)\]\s+time=([0-9.eE+-]+)\s+sec,\s+"
    r"weight=([^,]+),\s+query_memory_peak=([0-9.eE+-]+)\s+MiB"
)


@dataclass(frozen=True)
class QueryMeta:
    index: int
    g: int
    mean_f: float
    min_f: int
    max_f: int


@dataclass
class Case:
    suite: str
    case_id: str
    dataset: str
    graph_path: Path
    query_path: Path
    methods: list[str]
    min_g: int | None = None
    max_g: int | None = None
    stp_path: Path | None = None
    connector_cost_sum: float = 0.0
    expected_weight: float | None = None
    attributes: dict[str, Any] | None = None


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def resolve(path: str | Path) -> Path:
    candidate = Path(path)
    return candidate if candidate.is_absolute() else ROOT / candidate


def graph_folder(path: str | Path) -> Path:
    """Normalize manifests that historically named graph.txt itself."""

    candidate = resolve(path)
    if candidate.is_file() and candidate.name.casefold() == "graph.txt":
        return candidate.parent
    return candidate


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def process_environment(
    extra_path_entries: Iterable[str | Path] = (),
    overrides: dict[str, str] | None = None,
) -> dict[str, str]:
    """Create a platform-safe child environment and prepend runtime paths.

    Windows environment names are case-insensitive, so duplicate ``PATH``/``Path``
    spellings must be collapsed.  POSIX names are case-sensitive: keep ``PATH``
    exactly and also prepend the same restored third-party directories to
    ``LD_LIBRARY_PATH`` so dynamically linked SCIP/SoPlex binaries can start.
    """

    cleaned: dict[str, str] = {}
    path_key = "Path" if os.name == "nt" else "PATH"
    if os.name == "nt":
        spelling: dict[str, str] = {}
        for key, value in os.environ.items():
            folded = key.casefold()
            previous = spelling.get(folded)
            if previous is not None:
                cleaned.pop(previous, None)
            canonical = path_key if folded == "path" else key
            spelling[folded] = canonical
            cleaned[canonical] = value
    else:
        cleaned.update(os.environ)
    additions = [str(resolve(entry)) for entry in extra_path_entries]
    if additions:
        current_path = cleaned.get(path_key, "")
        cleaned[path_key] = os.pathsep.join(
            additions + ([current_path] if current_path else [])
        )
        if os.name != "nt":
            current_library_path = cleaned.get("LD_LIBRARY_PATH", "")
            cleaned["LD_LIBRARY_PATH"] = os.pathsep.join(
                additions +
                ([current_library_path] if current_library_path else [])
            )
    if overrides:
        cleaned.update(overrides)
    return cleaned


def render_command(command: Iterable[str]) -> str:
    """用目标平台的 shell 规则只渲染日志，实际执行仍传参数列表。"""

    values = list(command)
    return (
        subprocess.list2cmdline(values)
        if os.name == "nt"
        else shlex.join(values)
    )


def parse_probe_diagnostic(line: str) -> dict[str, Any]:
    """Parse one sparse ``[ProbeDiag] key=value`` event from a probe twin."""

    payload = line[len("[ProbeDiag]") :].strip()
    event: dict[str, Any] = {}
    for token in shlex.split(payload):
        key, separator, raw = token.partition("=")
        if not separator:
            continue
        try:
            value: Any = int(raw)
        except ValueError:
            try:
                value = float(raw)
            except ValueError:
                value = raw
        event[key] = value
    return event


def task_key(case: Case, method: str, query_index: int) -> str:
    return f"{case.suite}|{case.case_id}|{method}|q{query_index}"


def record_path(records_dir: Path, key: str) -> Path:
    return records_dir / f"{hashlib.sha256(key.encode()).hexdigest()}.json"


def write_record(records_dir: Path, record: dict[str, Any]) -> None:
    records_dir.mkdir(parents=True, exist_ok=True)
    target = record_path(records_dir, str(record["task_key"]))
    temporary = target.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(target)


def read_query_metadata(path: Path) -> list[QueryMeta]:
    metadata: list[QueryMeta] = []
    with path.open("r", encoding="utf-8") as query_file:
        first = query_file.readline()
        if not first:
            raise ValueError(f"Empty query file: {path}")
        count = int(first.strip())
        for query_index in range(1, count + 1):
            line = query_file.readline()
            if not line:
                raise ValueError(f"Truncated before query {query_index}: {path}")
            g = int(line.strip())
            sizes: list[int] = []
            for _ in range(g):
                group_line = query_file.readline()
                if not group_line:
                    raise ValueError(f"Truncated inside query {query_index}: {path}")
                size, separator, _ = group_line.partition(" ")
                if not separator:
                    raise ValueError(f"Malformed group line in {path}")
                sizes.append(int(size))
            metadata.append(
                QueryMeta(query_index, g, sum(sizes) / g, min(sizes), max(sizes))
            )
    return metadata


def matches_filter(record: dict[str, Any], filters: dict[str, Any]) -> bool:
    for key, expected in filters.items():
        if key.endswith("_contains"):
            actual_key = key[: -len("_contains")]
            actual = record.get(actual_key, "")
            if isinstance(actual, list):
                if expected not in actual:
                    return False
            elif str(expected) not in str(actual):
                return False
        elif record.get(key) != expected:
            return False
    return True


def format_template(template: str, record: dict[str, Any]) -> Path:
    return resolve(template.format(**record))


def case_from_record(suite: dict[str, Any], record: dict[str, Any]) -> Case:
    graph_spec = suite.get("graph_field")
    if graph_spec:
        graph = graph_folder(record[graph_spec])
    else:
        graph = graph_folder(format_template(suite["graph_template"], record))
    query_spec = suite.get("query_field")
    if query_spec:
        query = resolve(record[query_spec])
    else:
        query = format_template(suite["query_template"], record)
    stp_path = None
    if suite.get("stp_field") and record.get(suite["stp_field"]):
        stp_path = resolve(record[suite["stp_field"]])
    identifier_fields = suite.get("id_fields", ["dataset", "g"])
    suffix = "_".join(str(record.get(field, "")) for field in identifier_fields)
    return Case(
        suite=suite["id"],
        case_id=f"{suite['id']}__{suffix}",
        dataset=str(record.get("dataset", record.get("name", graph.name))),
        graph_path=graph,
        query_path=query,
        methods=list(suite["methods"]),
        min_g=suite.get("min_g"),
        max_g=suite.get("max_g"),
        stp_path=stp_path,
        connector_cost_sum=float(record.get("connector_cost_sum", 0.0)),
        expected_weight=(
            float(record["derived_gst_optimum"])
            if record.get("derived_gst_optimum") is not None
            else None
        ),
        attributes=record,
    )


def expand_cases(config: dict[str, Any]) -> list[Case]:
    cases: list[Case] = []
    for suite in config["suites"]:
        if "cases" in suite:
            for item in suite["cases"]:
                merged = {**suite, **item}
                split_values: list[int | None] = [None]
                if merged.get("split_by_g"):
                    if merged.get("min_g") is None or merged.get("max_g") is None:
                        raise ValueError(f"split_by_g requires min_g/max_g in {suite['id']}")
                    split_values = list(
                        range(int(merged["min_g"]), int(merged["max_g"]) + 1)
                    )
                for split_g in split_values:
                    base_id = item.get("id", f"{suite['id']}__{item['dataset']}")
                    cases.append(
                        Case(
                            suite=suite["id"],
                            case_id=(base_id if split_g is None else f"{base_id}__g{split_g}"),
                            dataset=item["dataset"],
                            graph_path=graph_folder(item["graph"]),
                            query_path=resolve(item["query"]),
                            methods=list(item.get("methods", suite["methods"])),
                            min_g=(merged.get("min_g") if split_g is None else split_g),
                            max_g=(merged.get("max_g") if split_g is None else split_g),
                            attributes={**item, **({"split_g": split_g} if split_g is not None else {})},
                        )
                    )
        elif "manifest" in suite:
            payload = json.loads(resolve(suite["manifest"]).read_text(encoding="utf-8"))
            records = payload[suite["manifest_key"]] if isinstance(payload, dict) else payload
            filters = suite.get("filter", {})
            for record in records:
                if matches_filter(record, filters):
                    cases.append(case_from_record(suite, record))
        elif "query_glob" in suite:
            for query in sorted(ROOT.glob(suite["query_glob"])):
                graph = query.parent if suite.get("graph") == "parent" else resolve(suite["graph"])
                cases.append(
                    Case(
                        suite=suite["id"],
                        case_id=f"{suite['id']}__{query.parent.name}",
                        dataset=query.parent.name,
                        graph_path=graph,
                        query_path=query,
                        methods=list(suite["methods"]),
                        min_g=suite.get("min_g"),
                        max_g=suite.get("max_g"),
                    )
                )
        else:
            raise ValueError(f"Suite {suite['id']} has no case source")
    return cases


def executable_path(method: dict[str, Any]) -> Path:
    candidates = [resolve(method["executable"])]
    for candidate in method.get("fallback_executables", []):
        candidates.append(resolve(candidate))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def group_contiguous(values: Iterable[int]) -> list[tuple[int, int]]:
    sorted_values = sorted(set(values))
    if not sorted_values:
        return []
    groups: list[tuple[int, int]] = []
    start = previous = sorted_values[0]
    for value in sorted_values[1:]:
        if value != previous + 1:
            groups.append((start, previous))
            start = value
        previous = value
    groups.append((start, previous))
    return groups


def make_base_record(
    run_id: str,
    case: Case,
    method_name: str,
    query: QueryMeta,
    timeout: float,
) -> dict[str, Any]:
    key = task_key(case, method_name, query.index)
    record: dict[str, Any] = {
        "schema_version": 1,
        "task_key": key,
        "run_id": run_id,
        "suite": case.suite,
        "case_id": case.case_id,
        "dataset": case.dataset,
        "graph_path": str(case.graph_path.relative_to(ROOT)).replace("\\", "/"),
        "query_path": str(case.query_path.relative_to(ROOT)).replace("\\", "/"),
        "query_index": query.index,
        "g": query.g,
        "mean_f": query.mean_f,
        "min_f": query.min_f,
        "max_f": query.max_f,
        "method": method_name,
        "timeout_seconds": timeout,
        "started_at": utc_now(),
    }
    if case.expected_weight is not None:
        record["expected_weight"] = case.expected_weight
    known_infeasible = set(
        (case.attributes or {}).get("known_infeasible_query_indices", [])
    )
    record["expected_solution_status"] = (
        "infeasible" if query.index in known_infeasible else "feasible"
    )
    return record


def terminate(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    process.kill()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        pass


def process_rss_bytes(process: subprocess.Popen[str]) -> int | None:
    """Read one child RSS sample without adding a runtime dependency."""

    try:
        if sys.platform == "win32":
            import ctypes
            from ctypes import wintypes

            class ProcessMemoryCounters(ctypes.Structure):
                _fields_ = [
                    ("cb", wintypes.DWORD),
                    ("PageFaultCount", wintypes.DWORD),
                    ("PeakWorkingSetSize", ctypes.c_size_t),
                    ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t),
                    ("PeakPagefileUsage", ctypes.c_size_t),
                ]

            counters = ProcessMemoryCounters()
            counters.cb = ctypes.sizeof(counters)
            if ctypes.windll.psapi.GetProcessMemoryInfo(
                int(process._handle), ctypes.byref(counters), counters.cb
            ):
                return int(counters.WorkingSetSize)
            return None
        statm = Path(f"/proc/{process.pid}/statm")
        if statm.exists():
            resident_pages = int(statm.read_text(encoding="ascii").split()[1])
            page_size = int(os.sysconf("SC_PAGE_SIZE"))
            return resident_pages * page_size if page_size > 0 else None
    except (OSError, ValueError, AttributeError):
        return None
    return None


def start_reader(process: subprocess.Popen[str], output_queue: queue.Queue[str | None]) -> threading.Thread:
    def reader() -> None:
        assert process.stdout is not None
        for line in process.stdout:
            output_queue.put(line)
        output_queue.put(None)

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()
    return thread


def run_native_range(
    run_id: str,
    run_dir: Path,
    records_dir: Path,
    case: Case,
    method_name: str,
    method: dict[str, Any],
    query_by_index: dict[int, QueryMeta],
    begin: int,
    end: int,
    timeout: float,
    load_timeout: float,
    wall_deadline: float | None = None,
    probe_diagnostics: bool = False,
) -> bool:
    """Run a native range; return true only when the global budget expires."""

    executable = executable_path(method)
    next_index = begin
    while next_index <= end:
        if wall_deadline is not None and time.monotonic() >= wall_deadline:
            return True
        attempt_id = hashlib.sha256(
            f"{case.case_id}|{method_name}|{next_index}|{end}|{time.time_ns()}".encode()
        ).hexdigest()[:16]
        raw_root = run_dir / "native_output" / attempt_id
        command = [
            str(executable),
            str(case.graph_path),
            str(raw_root),
            str(case.query_path),
            str(ROOT / "data"),
            str(next_index),
            str(end - next_index + 1),
            *map(str, method.get("arguments", [])),
        ]
        log_path = run_dir / "logs" / f"{attempt_id}.log"
        log_path.parent.mkdir(parents=True, exist_ok=True)
        with log_path.open("w", encoding="utf-8", newline="\n") as log:
            log.write("COMMAND " + render_command(command) + "\n")
            log.flush()
            attempt_started = time.monotonic()
            process = subprocess.Popen(
                command,
                cwd=ROOT,
                env=process_environment(
                    method.get("runtime_path", []),
                    {"GST_PROBE_DIAGNOSTICS": "1"} if probe_diagnostics else None,
                ),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                bufsize=1,
            )
            lines: queue.Queue[str | None] = queue.Queue()
            reader = start_reader(process, lines)
            ready = False
            natural_deadline = time.monotonic() + load_timeout
            deadline = (
                min(natural_deadline, wall_deadline)
                if wall_deadline is not None
                else natural_deadline
            )
            deadline_reason = (
                "budget_exhausted"
                if wall_deadline is not None and wall_deadline < natural_deadline
                else "graph_load_timeout"
            )
            query_started = 0.0
            last_completed = next_index - 1
            failure: str | None = None
            diagnostic_events: list[dict[str, Any]] = []
            ready_rss_bytes: int | None = None
            peak_rss_bytes: int | None = (
                process_rss_bytes(process) if probe_diagnostics else None
            )
            while True:
                sampled_rss = process_rss_bytes(process) if probe_diagnostics else None
                if sampled_rss is not None:
                    peak_rss_bytes = max(peak_rss_bytes or 0, sampled_rss)
                remaining = max(0.0, deadline - time.monotonic())
                try:
                    line = lines.get(
                        timeout=min(remaining, 0.2) if probe_diagnostics else remaining
                    )
                except queue.Empty:
                    if time.monotonic() < deadline:
                        continue
                    sampled_rss = (
                        process_rss_bytes(process) if probe_diagnostics else None
                    )
                    if sampled_rss is not None:
                        peak_rss_bytes = max(peak_rss_bytes or 0, sampled_rss)
                    failure = deadline_reason
                    terminate(process)
                    break
                if line is None:
                    break
                log.write(line)
                log.flush()
                stripped = line.strip()
                if stripped.startswith("[Ready]"):
                    ready = True
                    query_started = time.monotonic()
                    ready_rss_bytes = (
                        process_rss_bytes(process) if probe_diagnostics else None
                    )
                    peak_rss_bytes = ready_rss_bytes
                    natural_deadline = query_started + timeout
                    deadline = (
                        min(natural_deadline, wall_deadline)
                        if wall_deadline is not None
                        else natural_deadline
                    )
                    deadline_reason = (
                        "budget_exhausted"
                        if wall_deadline is not None and wall_deadline < natural_deadline
                        else "timeout"
                    )
                    diagnostic_events = []
                    continue
                if stripped.startswith("[ProbeDiag]"):
                    diagnostic_events.append(parse_probe_diagnostic(stripped))
                    continue
                match = QUERY_RESULT.match(stripped)
                if not match:
                    continue
                query_index = int(match.group(1))
                if query_index < next_index or query_index > end:
                    continue
                record = make_base_record(
                    run_id, case, method_name, query_by_index[query_index], timeout
                )
                record["exact_claim"] = bool(method.get("exact_claim", True))
                if diagnostic_events:
                    record["probe_diagnostics"] = diagnostic_events
                if peak_rss_bytes is not None:
                    record["watchdog_peak_rss_mib"] = peak_rss_bytes / (1024 * 1024)
                if ready_rss_bytes is not None and peak_rss_bytes is not None:
                    record["watchdog_query_peak_overhead_mib"] = max(
                        0, peak_rss_bytes - ready_rss_bytes
                    ) / (1024 * 1024)
                parsed_weight = float(match.group(3))
                record.update(
                    {
                        "status": "ok",
                        "solver_seconds": float(match.group(2)),
                        "weight": parsed_weight,
                        "solution_status": (
                            "infeasible" if parsed_weight < 0 else "feasible"
                        ),
                        "query_memory_peak_mib": float(match.group(4)),
                        "watchdog_wall_seconds": time.monotonic() - query_started,
                        "finished_at": utc_now(),
                        "command": command,
                        "log_path": str(log_path.relative_to(ROOT)).replace("\\", "/"),
                    }
                )
                write_record(records_dir, record)
                last_completed = query_index
                query_started = time.monotonic()
                natural_deadline = query_started + timeout
                deadline = (
                    min(natural_deadline, wall_deadline)
                    if wall_deadline is not None
                    else natural_deadline
                )
                deadline_reason = (
                    "budget_exhausted"
                    if wall_deadline is not None and wall_deadline < natural_deadline
                    else "timeout"
                )
                diagnostic_events = []
                ready_rss_bytes = (
                    process_rss_bytes(process) if probe_diagnostics else None
                )
                peak_rss_bytes = ready_rss_bytes

            return_code = process.wait(timeout=30) if process.poll() is None else process.returncode
            reader.join(timeout=2)
            current = last_completed + 1
            if failure and current <= end:
                record = make_base_record(run_id, case, method_name, query_by_index[current], timeout)
                record["exact_claim"] = bool(method.get("exact_claim", True))
                if diagnostic_events:
                    record["probe_diagnostics"] = diagnostic_events
                if peak_rss_bytes is not None:
                    record["watchdog_peak_rss_mib"] = peak_rss_bytes / (1024 * 1024)
                if ready_rss_bytes is not None and peak_rss_bytes is not None:
                    record["watchdog_query_peak_overhead_mib"] = max(
                        0, peak_rss_bytes - ready_rss_bytes
                    ) / (1024 * 1024)
                record.update(
                    {
                        "status": failure,
                        "solver_seconds": None,
                        "weight": None,
                        "query_memory_peak_mib": None,
                        "watchdog_wall_seconds": time.monotonic()
                        - (query_started if ready else attempt_started),
                        "finished_at": utc_now(),
                        "return_code": process.returncode,
                        "command": command,
                        "log_path": str(log_path.relative_to(ROOT)).replace("\\", "/"),
                    }
                )
                write_record(records_dir, record)
                print(f"  {method_name} {case.case_id} q{current}: {failure}", flush=True)
                if failure == "budget_exhausted":
                    return True
                if failure == "graph_load_timeout":
                    return False
                next_index = current + 1
                continue

            if current <= end:
                record = make_base_record(run_id, case, method_name, query_by_index[current], timeout)
                record["exact_claim"] = bool(method.get("exact_claim", True))
                if diagnostic_events:
                    record["probe_diagnostics"] = diagnostic_events
                if peak_rss_bytes is not None:
                    record["watchdog_peak_rss_mib"] = peak_rss_bytes / (1024 * 1024)
                if ready_rss_bytes is not None and peak_rss_bytes is not None:
                    record["watchdog_query_peak_overhead_mib"] = max(
                        0, peak_rss_bytes - ready_rss_bytes
                    ) / (1024 * 1024)
                record.update(
                    {
                        "status": "error",
                        "solver_seconds": None,
                        "weight": None,
                        "query_memory_peak_mib": None,
                        "watchdog_wall_seconds": time.monotonic()
                        - (query_started if ready else attempt_started),
                        "finished_at": utc_now(),
                        "return_code": return_code,
                        "command": command,
                        "log_path": str(log_path.relative_to(ROOT)).replace("\\", "/"),
                    }
                )
                write_record(records_dir, record)
                print(f"  {method_name} {case.case_id} q{current}: error {return_code}", flush=True)
                next_index = current + 1
            else:
                next_index = end + 1
    return False


def parse_scip_output(output: str) -> tuple[str, float | None, float | None]:
    status = "ok" if re.search(r"SCIP Status\s*:.*optimal solution found", output) else "error"
    objective_matches = re.findall(r"Primal Bound\s*:\s*([0-9.eE+-]+)", output)
    time_matches = re.findall(r"Solving Time \(sec\)\s*:\s*([0-9.eE+-]+)", output)
    objective = float(objective_matches[-1]) if objective_matches else None
    seconds = float(time_matches[-1]) if time_matches else None
    return status, objective, seconds


def run_scip_case(
    run_id: str,
    run_dir: Path,
    records_dir: Path,
    case: Case,
    method_name: str,
    method: dict[str, Any],
    query: QueryMeta,
    timeout: float,
    wall_deadline: float | None = None,
) -> bool:
    """Run SCIP-Jack; return true only when the global budget expires."""

    if case.stp_path is None:
        raise ValueError(f"SCIP-Jack case lacks stp_path: {case.case_id}")
    executable = executable_path(method)
    command = [
        str(executable),
        "-s",
        str(resolve(method["settings"])),
        "-f",
        str(case.stp_path),
    ]
    attempt_id = hashlib.sha256(task_key(case, method_name, query.index).encode()).hexdigest()[:16]
    log_path = run_dir / "logs" / f"scip_{attempt_id}.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    record = make_base_record(run_id, case, method_name, query, timeout)
    record["exact_claim"] = bool(method.get("exact_claim", True))
    remaining_budget = (
        max(0.0, wall_deadline - time.monotonic())
        if wall_deadline is not None
        else None
    )
    if remaining_budget is not None and remaining_budget <= 0.0:
        return True
    natural_watchdog = timeout + float(method.get("watchdog_grace_seconds", 30))
    watchdog = (
        min(natural_watchdog, remaining_budget)
        if remaining_budget is not None
        else natural_watchdog
    )
    budget_capped = remaining_budget is not None and remaining_budget < natural_watchdog
    try:
        completed = subprocess.run(
            command,
            cwd=ROOT,
            env=process_environment(method.get("runtime_path", [])),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=watchdog,
        )
        output = completed.stdout + completed.stderr
        log_path.write_text(
            "COMMAND " + render_command(command) + "\n" + output,
            encoding="utf-8",
        )
        status, stp_objective, seconds = parse_scip_output(output)
        adjusted = stp_objective - case.connector_cost_sum if stp_objective is not None else None
        record.update(
            {
                "status": status,
                "solver_seconds": seconds,
                "weight": adjusted,
                "raw_stp_weight": stp_objective,
                "connector_cost_sum": case.connector_cost_sum,
                "query_memory_peak_mib": None,
                "watchdog_wall_seconds": time.monotonic() - started,
                "return_code": completed.returncode,
                "finished_at": utc_now(),
                "command": command,
                "log_path": str(log_path.relative_to(ROOT)).replace("\\", "/"),
            }
        )
    except subprocess.TimeoutExpired as error:
        output = (error.stdout or "") + (error.stderr or "")
        log_path.write_text(
            "COMMAND " + render_command(command) + "\n" + output,
            encoding="utf-8",
        )
        record.update(
            {
                "status": "budget_exhausted" if budget_capped else "timeout",
                "solver_seconds": None,
                "weight": None,
                "query_memory_peak_mib": None,
                "watchdog_wall_seconds": time.monotonic() - started,
                "finished_at": utc_now(),
                "command": command,
                "log_path": str(log_path.relative_to(ROOT)).replace("\\", "/"),
            }
        )
    write_record(records_dir, record)
    return bool(record["status"] == "budget_exhausted")


def combine_records(records_dir: Path, destination: Path) -> int:
    records = [json.loads(path.read_text(encoding="utf-8")) for path in records_dir.glob("*.json")]
    records.sort(key=lambda row: row["task_key"])
    with destination.open("w", encoding="utf-8", newline="\n") as output:
        for record in records:
            output.write(json.dumps(record, sort_keys=True) + "\n")
    return len(records)


def selected(value: str, allowed: set[str] | None) -> bool:
    return allowed is None or value in allowed


def case_shard(case: Case, shard_count: int) -> int:
    """Keep every method/query of one cell together on the same shard.

    This preserves graph-load amortization, paired method order, and thermal
    balancing.  There are many more paper cells than practical shards, so
    case-level hashing still distributes the full matrix well.
    """

    key = f"{case.suite}|{case.case_id}"
    return int(hashlib.sha256(key.encode()).hexdigest(), 16) % shard_count


def balanced_method_order(case: Case, allowed: set[str] | None) -> list[str]:
    """Return a stable cyclic rotation, balanced across experiment cells.

    Native runs intentionally keep each graph resident for a method, so fully
    interleaving methods query by query would charge graph loading repeatedly.
    Rotating the first method by a hash of the case id removes a systematic
    warm-machine/thermal advantage while preserving that amortization.
    """

    names = [name for name in case.methods if selected(name, allowed)]
    if len(names) < 2:
        return names
    offset = int(hashlib.sha256(case.case_id.encode()).hexdigest(), 16) % len(names)
    return names[offset:] + names[:offset]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--run-id")
    parser.add_argument("--run-dir", type=Path, help="resume/use this exact run directory")
    parser.add_argument("--suite", action="append", help="repeat to select suites")
    parser.add_argument("--case", action="append", help="repeat to select exact case ids")
    parser.add_argument("--method", action="append", help="repeat to select methods")
    parser.add_argument(
        "--query-index",
        action="append",
        type=int,
        help="repeat to select explicit 1-based query indices",
    )
    parser.add_argument("--timeout", type=float)
    parser.add_argument("--graph-load-timeout", type=float)
    parser.add_argument(
        "--wall-budget-seconds",
        type=float,
        help="stop/kill the active task when this whole invocation budget expires",
    )
    parser.add_argument(
        "--probe-diagnostics",
        action="store_true",
        help="enable sparse diagnostics in probe-only binaries",
    )
    parser.add_argument("--shard-index", type=int, default=0)
    parser.add_argument("--shard-count", type=int, default=1)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if args.wall_budget_seconds is not None and args.wall_budget_seconds <= 0:
        parser.error("wall-budget-seconds must be positive")
    wall_deadline = (
        time.monotonic() + args.wall_budget_seconds
        if args.wall_budget_seconds is not None
        else None
    )
    if args.shard_count <= 0 or not 0 <= args.shard_index < args.shard_count:
        parser.error("require 0 <= shard-index < shard-count")
    config_path = args.config if args.config.is_absolute() else ROOT / args.config
    config = json.loads(config_path.read_text(encoding="utf-8"))
    timeout = args.timeout or float(config["timeout_seconds"])
    load_timeout = args.graph_load_timeout or float(config.get("graph_load_timeout_seconds", 1800))
    run_id = args.run_id or datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = args.run_dir or (ROOT / "results" / "paper_runs" / run_id)
    if not run_dir.is_absolute():
        run_dir = ROOT / run_dir
    records_dir = run_dir / "records"

    methods = config["methods"]
    suite_filter = set(args.suite) if args.suite else None
    case_filter = set(args.case) if args.case else None
    method_filter = set(args.method) if args.method else None
    query_filter = set(args.query_index) if args.query_index else None
    cases = [
        case
        for case in expand_cases(config)
        if selected(case.suite, suite_filter) and selected(case.case_id, case_filter)
    ]
    case_queries: dict[str, list[QueryMeta]] = {}
    task_count = 0
    for case in cases:
        if not case.graph_path.exists() or not case.query_path.exists():
            raise FileNotFoundError(f"Missing input for {case.case_id}")
        queries = [
            query
            for query in read_query_metadata(case.query_path)
            if (case.min_g is None or query.g >= case.min_g)
            and (case.max_g is None or query.g <= case.max_g)
            and (query_filter is None or query.index in query_filter)
        ]
        case_queries[case.case_id] = queries
        if case_shard(case, args.shard_count) != args.shard_index:
            continue
        for method_name in balanced_method_order(case, method_filter):
            method = methods[method_name]
            for query in queries:
                if method.get("max_g") is not None and query.g > int(method["max_g"]):
                    continue
                task_count += 1

    shard_case_count = sum(
        case_shard(case, args.shard_count) == args.shard_index for case in cases
    )
    print(
        f"Run {run_id}: {shard_case_count}/{len(cases)} cases, {task_count} selected tasks, "
        f"timeout={timeout:g}s, shard={args.shard_index}/{args.shard_count}",
        flush=True,
    )
    if args.dry_run:
        for case in cases:
            if case_shard(case, args.shard_count) != args.shard_index:
                continue
            order = ",".join(balanced_method_order(case, method_filter))
            print(
                f"  {case.suite}: {case.case_id} "
                f"({len(case_queries[case.case_id])} queries; order={order})"
            )
        return 0

    run_dir.mkdir(parents=True, exist_ok=True)
    metadata_path = run_dir / "run_metadata.json"
    if not metadata_path.exists():
        binary_hashes = {}
        for name, method in methods.items():
            path = executable_path(method)
            binary_hashes[name] = sha256_file(path) if path.exists() else None
        metadata_path.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "run_id": run_id,
                    "created_at": utc_now(),
                    "config_path": str(config_path.relative_to(ROOT)).replace("\\", "/"),
                    "config_sha256": sha256_file(config_path),
                    "timeout_seconds": timeout,
                    "graph_load_timeout_seconds": load_timeout,
                    "initial_invocation_wall_budget_seconds": args.wall_budget_seconds,
                    "initial_invocation_query_indices": sorted(query_filter) if query_filter else None,
                    "initial_invocation_probe_diagnostics": args.probe_diagnostics,
                    "shard_index": args.shard_index,
                    "shard_count": args.shard_count,
                    "hostname": socket.gethostname(),
                    "platform": platform.platform(),
                    "processor": platform.processor(),
                    "logical_cpu_count": os.cpu_count(),
                    "python": sys.version,
                    "method_order_policy": "stable cyclic rotation by case_id SHA-256",
                    "binary_sha256": binary_hashes,
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )

    with (run_dir / "invocations.jsonl").open("a", encoding="utf-8", newline="\n") as output:
        output.write(
            json.dumps(
                {
                    "invoked_at": utc_now(),
                    "run_id": run_id,
                    "suite_filter": sorted(suite_filter) if suite_filter else None,
                    "case_filter": sorted(case_filter) if case_filter else None,
                    "method_filter": sorted(method_filter) if method_filter else None,
                    "query_indices": sorted(query_filter) if query_filter else None,
                    "timeout_seconds": timeout,
                    "graph_load_timeout_seconds": load_timeout,
                    "wall_budget_seconds": args.wall_budget_seconds,
                    "probe_diagnostics": args.probe_diagnostics,
                    "shard_index": args.shard_index,
                    "shard_count": args.shard_count,
                },
                sort_keys=True,
            )
            + "\n"
        )

    budget_exhausted = False
    for case in cases:
        if wall_deadline is not None and time.monotonic() >= wall_deadline:
            budget_exhausted = True
            break
        if case_shard(case, args.shard_count) != args.shard_index:
            continue
        queries = case_queries[case.case_id]
        query_by_index = {query.index: query for query in queries}
        for method_name in balanced_method_order(case, method_filter):
            if wall_deadline is not None and time.monotonic() >= wall_deadline:
                budget_exhausted = True
                break
            method = methods[method_name]
            eligible = [
                query
                for query in queries
                if (method.get("max_g") is None or query.g <= int(method["max_g"]))
                and not record_path(
                    records_dir, task_key(case, method_name, query.index)
                ).exists()
            ]
            if not eligible:
                continue
            executable = executable_path(method)
            if not executable.exists():
                raise FileNotFoundError(f"Missing executable for {method_name}: {executable}")
            print(f"{case.case_id}: {method_name}, {len(eligible)} pending", flush=True)
            if method["kind"] == "native":
                for begin, end in group_contiguous(query.index for query in eligible):
                    budget_exhausted = run_native_range(
                        run_id,
                        run_dir,
                        records_dir,
                        case,
                        method_name,
                        method,
                        query_by_index,
                        begin,
                        end,
                        timeout,
                        load_timeout,
                        wall_deadline,
                        args.probe_diagnostics,
                    )
                    if budget_exhausted:
                        break
            elif method["kind"] == "scip_jack":
                for query in eligible:
                    budget_exhausted = run_scip_case(
                        run_id,
                        run_dir,
                        records_dir,
                        case,
                        method_name,
                        method,
                        query,
                        timeout,
                        wall_deadline,
                    )
                    if budget_exhausted:
                        break
            else:
                raise ValueError(f"Unknown method kind: {method['kind']}")
            if budget_exhausted:
                break
        if budget_exhausted:
            break

    count = combine_records(records_dir, run_dir / "records.jsonl")
    print(f"Recorded {count} task results in {run_dir}")
    if budget_exhausted:
        print("Global wall budget exhausted; the run is safely resumable.", flush=True)
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
