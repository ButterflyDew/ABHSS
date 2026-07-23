#!/usr/bin/env python3
"""Validate the frozen official-source paper environment before long runs."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[2]
FREEZE_ID = "official-latest-20260722"
REQUIRED_SLUGS = {
    "snap-wikipedia-2018",
    "snap-twitch-2018",
    "snap-github-2019",
    "snap-youtube",
    "snap-orkut",
    "snap-livejournal",
    "movielens-32m",
    "linkedmdb",
    "dbpedia-2022.12-en",
    "imdb-20260722",
    "dblp-aminer-v18",
    "toronto-current",
}
PANEL_EXPECTATIONS = (
    ("experiment_data/official_latest/controlled_labels/dblp/cells.json", 18, 180),
    ("experiment_data/official_latest/controlled_labels/imdb/cells.json", 18, 180),
    ("experiment_data/official_latest/related/cells.json", 104, 1040),
    ("experiment_data/official_latest/natural/linkedmdb/cells.json", 12, 200),
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def load_json(relative: str | Path) -> Any:
    return json.loads((ROOT / relative).read_text(encoding="utf-8"))


def load_runner() -> Any:
    path = ROOT / "tools" / "experiments" / "run_experiments.py"
    spec = importlib.util.spec_from_file_location("paper_runner", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def check_hash_record(
    record: dict[str, Any], failures: list[str], label: str, *, hash_file: bool
) -> None:
    path = ROOT / str(record["path"])
    if not path.is_file():
        failures.append(f"missing {label}: {record['path']}")
        return
    if record.get("bytes") is not None and path.stat().st_size != int(record["bytes"]):
        failures.append(f"size mismatch for {label}: {record['path']}")
    if hash_file and sha256(path).casefold() != str(record["sha256"]).casefold():
        failures.append(f"SHA-256 mismatch for {label}: {record['path']}")


def check_named_hash(
    path_text: str,
    expected: str,
    failures: list[str],
    label: str,
    *,
    hash_file: bool,
) -> None:
    path = ROOT / path_text
    if not path.is_file():
        failures.append(f"missing {label}: {path_text}")
    elif hash_file and sha256(path).casefold() != expected.casefold():
        failures.append(f"SHA-256 mismatch for {label}: {path_text}")


def panel_rows(path: Path) -> list[dict[str, Any]]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    return payload if isinstance(payload, list) else payload["cells"]


def query_file_records(row: dict[str, Any]) -> Iterable[tuple[str, str]]:
    for path_key, hash_key in (
        ("query_path", "query_sha256"),
        ("panel_query_path", "panel_query_sha256"),
        ("group_ids_path", "group_ids_sha256"),
    ):
        if row.get(path_key) and row.get(hash_key):
            yield str(row[path_key]), str(row[hash_key])


def validate_dataset_manifest(
    manifest_path: Path,
    download_hashes: set[str],
    failures: list[str],
    *,
    deep: bool,
) -> dict[str, Any]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("freeze_id") != FREEZE_ID:
        failures.append(f"wrong freeze in {manifest_path.relative_to(ROOT)}")
    if manifest.get("slug") != manifest_path.parent.name:
        failures.append(f"slug/path mismatch in {manifest_path.relative_to(ROOT)}")

    graph = manifest["graph"]
    check_hash_record(graph, failures, "interface graph", hash_file=deep)
    candidate = manifest.get("candidate_groups")
    if candidate:
        check_hash_record(candidate, failures, "candidate groups", hash_file=deep)
    for query in manifest.get("query_pools", []):
        check_hash_record(query, failures, "query pool", hash_file=deep)
        group_ids_path = str(query["path"]).removesuffix(".txt") + ".group_ids.txt"
        if query.get("group_ids_sha256"):
            check_named_hash(
                group_ids_path,
                str(query["group_ids_sha256"]),
                failures,
                "query group-ID trace",
                hash_file=deep,
            )

    for name, record in manifest.get("auxiliary_hashes", {}).items():
        check_hash_record(
            {"path": (manifest_path.parent / name).relative_to(ROOT).as_posix(), **record},
            failures,
            "interface auxiliary file",
            hash_file=deep,
        )
    for raw in manifest.get("raw_files", []):
        if str(raw["sha256"]).casefold() not in download_hashes:
            failures.append(
                f"raw hash in {manifest_path.relative_to(ROOT)} is absent from download manifest"
            )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-binaries", action="store_true")
    parser.add_argument("--skip-query-scan", action="store_true")
    parser.add_argument(
        "--deep-snapshot",
        action="store_true",
        help="rehash all official raw/interface/panel files (large and intentionally slow)",
    )
    args = parser.parse_args()

    failures: list[str] = []
    lock = load_json("experiments/environment_lock.json")
    config = load_json("experiments/paper_matrix.json")
    catalog = load_json("experiments/official_sources.json")
    download = load_json(
        "data_sources/official/official-latest-20260722/download_manifest.json"
    )
    snapshot = load_json("experiments/data_snapshot.json")
    feasibility_audit = load_json("experiments/query_feasibility_audit.json")

    if lock.get("schema_version") != 2 or lock.get("data_freeze") != FREEZE_ID:
        failures.append("environment lock does not select the official 2026-07-22 freeze")
    if config.get("data_freeze") != FREEZE_ID:
        failures.append("paper matrix does not select the official 2026-07-22 freeze")
    if float(config["timeout_seconds"]) != 10000:
        failures.append("paper_matrix timeout is not 10,000 seconds")
    if config.get("primary_baseline") != "pruneddp_safe":
        failures.append("paper_matrix primary baseline is not pruneddp_safe")
    safe_arguments = config["methods"]["pruneddp_safe"].get("arguments", [])
    if "--lb2-pathmax=off" not in safe_arguments:
        failures.append("primary PrunedDP++ baseline is not in safe reopen mode")
    if config["methods"]["pruneddp_strict"].get("exact_claim") is not False:
        failures.append("paper-pathmax audit is not marked exact_claim=false")
    excluded = " ".join(config.get("excluded_from_formal_matrix", [])).casefold()
    if "author" not in excluded or "gpu4gst" not in excluded:
        failures.append("paper matrix does not explicitly exclude author graph artifacts")

    if catalog.get("freeze_id") != FREEZE_ID:
        failures.append("official source catalog has the wrong freeze ID")
    states = {row["id"]: row["status"] for row in catalog["datasets"]}
    if states.get("LinkedMDB-2012") != "include":
        failures.append("LinkedMDB graph and official-mirror natural queries are not included")
    if states.get("IMDb-daily-20260722") != "include":
        failures.append("current IMDb snapshot is not included in primary experiment A")
    if states.get("DBLP-AMiner-V18") != "include" or states.get("Toronto-current") != "include":
        failures.append("DBLP-AMiner or Toronto-current is not included in the formal matrix")

    for source in lock.get("required_source_archives", []):
        path = ROOT / source["path"]
        if not path.is_file():
            failures.append(f"missing known-optimum source archive: {source['path']}")
        elif sha256(path).casefold() != str(source["sha256"]).casefold():
            failures.append(f"source archive hash mismatch: {source['path']}")

    if snapshot.get("freeze_id") != FREEZE_ID:
        failures.append("data snapshot has the wrong freeze ID")
    check_hash_record(snapshot["catalog"], failures, "catalog snapshot", hash_file=True)
    check_hash_record(
        snapshot["download_manifest"], failures, "download-manifest snapshot", hash_file=True
    )
    for record in snapshot.get("interface_datasets", []):
        check_hash_record(record, failures, "interface-manifest snapshot", hash_file=True)
    for record in snapshot.get("paper_panels", []):
        check_hash_record(record, failures, "paper-panel manifest snapshot", hash_file=True)
    check_hash_record(
        snapshot["known_optimum_panel"],
        failures,
        "known-optimum manifest snapshot",
        hash_file=True,
    )

    raw_root = ROOT / "data_sources" / "official" / FREEZE_ID / "raw"
    download_hashes: set[str] = set()
    for row in download["files"]:
        download_hashes.add(str(row["sha256"]).casefold())
        check_hash_record(
            {
                "path": (raw_root / row["path"]).relative_to(ROOT).as_posix(),
                "bytes": row["bytes"],
                "sha256": row["sha256"],
            },
            failures,
            "official raw download",
            hash_file=args.deep_snapshot,
        )

    interface_root = ROOT / "data" / FREEZE_ID
    manifest_paths = sorted(interface_root.glob("*/dataset_manifest.json"))
    slugs = {path.parent.name for path in manifest_paths}
    if slugs != REQUIRED_SLUGS:
        failures.append(
            f"interface dataset set mismatch: missing={sorted(REQUIRED_SLUGS - slugs)} "
            f"extra={sorted(slugs - REQUIRED_SLUGS)}"
        )
    dataset_manifests: dict[str, dict[str, Any]] = {}
    for path in manifest_paths:
        dataset_manifests[path.parent.name] = validate_dataset_manifest(
            path, download_hashes, failures, deep=args.deep_snapshot
        )

    imdb_manifest = dataset_manifests.get("imdb-20260722", {})
    imdb_normalization = imdb_manifest.get("source_normalization") or {}
    imdb_vertices = int(imdb_manifest.get("graph", {}).get("vertices", -1))
    if (
        int(imdb_normalization.get("title_vertices", -1))
        + int(imdb_normalization.get("person_vertices", -1))
        != imdb_vertices
    ):
        failures.append("IMDb title/person vertex counts do not sum to graph n")
    if (
        int(imdb_normalization.get("title_vertices_with_metadata", -1))
        + int(imdb_normalization.get("person_vertices_with_metadata", -1))
        + int(imdb_normalization.get("vertices_without_metadata", -1))
        != imdb_vertices
    ):
        failures.append("IMDb metadata-coverage counts do not sum to graph n")

    dbpedia_queries = load_json(
        "data/official-latest-20260722/dbpedia-2022.12-en/source_query_manifest.json"
    )
    counts = {int(row["g"]): int(row["queries"]) for row in dbpedia_queries["generated_pools"]}
    expected_counts = {1: 5, 2: 76, 3: 80, 4: 59, 5: 60, 6: 71, 7: 54, 8: 27, 9: 14, 10: 13, 11: 2, 12: 3}
    if int(dbpedia_queries.get("source_queries", -1)) != 467 or counts != expected_counts:
        failures.append("DBpedia official source-query mapping count changed")
    if int(dbpedia_queries.get("mapped_queries_in_requested_g_range", -1)) != 466:
        failures.append("DBpedia source mapping no longer contains 466 nonempty-group queries")
    if int(dbpedia_queries.get("feasible_queries_in_requested_g_range", -1)) != 464:
        failures.append("DBpedia component-feasible count is not 464")
    exclusions = dbpedia_queries.get("component_infeasible_queries", [])
    if {
        (row.get("source_query_id"), int(row.get("source_query_index", -1)))
        for row in exclusions
    } != {("INEX_XER-109", 124), ("QALD2_tr-17", 234)}:
        failures.append("DBpedia component-infeasible source-query audit changed")
    if sum(counts.get(g, 0) for g in range(1, 13)) != 464:
        failures.append("DBpedia primary g=1..12 panel is not exactly 464 feasible source queries")
    for row in dbpedia_queries["generated_pools"]:
        for path_text, expected in query_file_records(row):
            check_named_hash(
                path_text,
                expected,
                failures,
                "DBpedia source-query pool",
                hash_file=args.deep_snapshot,
            )

    for relative, expected_cells, expected_queries in PANEL_EXPECTATIONS:
        path = ROOT / relative
        rows = panel_rows(path)
        query_count = sum(
            int(row.get("queries", row.get("selected_queries", 0))) for row in rows
        )
        if len(rows) != expected_cells or query_count != expected_queries:
            failures.append(
                f"manifest count mismatch: {relative} has {len(rows)} cells/{query_count} queries"
            )
        for row in rows:
            for path_text, expected in query_file_records(row):
                check_named_hash(
                    path_text,
                    expected,
                    failures,
                    "formal panel payload",
                    hash_file=args.deep_snapshot,
                )

    controlled_expected = {
        (g, 400) for g in range(4, 17)
    } | {(10, f) for f in (100, 200, 800, 1600, 3200)}
    for relative in (
        "experiment_data/official_latest/controlled_labels/dblp/cells.json",
        "experiment_data/official_latest/controlled_labels/imdb/cells.json",
    ):
        rows = panel_rows(ROOT / relative)
        cells = {(int(row["g"]), int(row["f_target"])) for row in rows}
        if cells != controlled_expected or any(int(row["queries"]) != 10 for row in rows):
            failures.append(f"controlled A grid changed: {relative}")
        for row in rows:
            target = float(row["f_target"])
            if (
                float(row["f_realized_min"]) < 0.98 * target
                or float(row["f_realized_max"]) > 1.02 * target
            ):
                failures.append(
                    f"controlled A realized f exceeds +/-2%: {relative} g={row['g']} f={row['f_target']}"
                )

    related_rows = panel_rows(
        ROOT / "experiment_data/official_latest/related/cells.json"
    )
    related_datasets = {
        "snap-wikipedia-2018",
        "snap-twitch-2018",
        "snap-github-2019",
        "snap-youtube",
        "snap-orkut",
        "snap-livejournal",
        "movielens-32m",
        "toronto-current",
    }
    if (
        {str(row["dataset"]) for row in related_rows} != related_datasets
        or {(str(row["dataset"]), int(row["g"])) for row in related_rows}
        != {(dataset, g) for dataset in related_datasets for g in range(4, 17)}
        or any(int(row["selected_queries"]) != 10 for row in related_rows)
    ):
        failures.append("related B dataset/g grid changed")

    linked_rows = panel_rows(
        ROOT / "experiment_data/official_latest/natural/linkedmdb/cells.json"
    )
    linked_quotas = {1: 20, 2: 20, 3: 20, 4: 20, 5: 20, 6: 20, 7: 20, 8: 25, 9: 26, 10: 6, 11: 2, 12: 1}
    if {int(row["g"]): int(row["queries"]) for row in linked_rows} != linked_quotas:
        failures.append("LinkedMDB natural low/high-g quota panel changed")

    wrp = load_json("experiment_data/steinlib/index.json")
    if sum(bool(row.get("converted")) for row in wrp) != 11:
        failures.append("SteinLib panel does not contain exactly 11 known-optimum instances")

    runner = load_runner()
    cases = runner.expand_cases(config)
    total_tasks = 0
    query_cache: dict[Path, list[Any]] = {}
    used_methods: set[str] = set()
    for case in cases:
        if not case.graph_path.is_dir() or not (case.graph_path / "graph.txt").is_file():
            failures.append(f"invalid graph folder for case {case.case_id}: {case.graph_path}")
            continue
        if not case.query_path.is_file():
            failures.append(f"missing query file for case {case.case_id}: {case.query_path}")
            continue
        if case.stp_path is not None and not case.stp_path.is_file():
            failures.append(f"missing STP source for case {case.case_id}: {case.stp_path}")
        if args.skip_query_scan:
            selected_count = 1
        else:
            if case.query_path not in query_cache:
                query_cache[case.query_path] = runner.read_query_metadata(case.query_path)
            queries = [
                query
                for query in query_cache[case.query_path]
                if (case.min_g is None or query.g >= case.min_g)
                and (case.max_g is None or query.g <= case.max_g)
            ]
            selected_count = len(queries)
            if not queries:
                failures.append(f"case has no selected queries: {case.case_id}")
        for method_name in case.methods:
            used_methods.add(method_name)
            method = config["methods"][method_name]
            max_g = method.get("max_g")
            if args.skip_query_scan or max_g is None:
                total_tasks += selected_count
            else:
                total_tasks += sum(query.g <= int(max_g) for query in queries)

    matrix_path = ROOT / "experiments" / "paper_matrix.json"
    if feasibility_audit.get("matrix_sha256") != sha256(matrix_path):
        failures.append("query-feasibility audit was not built from the current paper matrix")
    audit_graph_paths: set[str] = set()
    audit_pairs: set[tuple[str, str]] = set()
    audit_query_records = 0
    audit_feasible = 0
    audit_infeasible = 0
    for graph in feasibility_audit.get("graphs", []):
        graph_path_text = str(graph["graph_path"])
        graph_path = ROOT / graph_path_text
        audit_graph_paths.add(graph_path_text)
        if not graph_path.is_file():
            failures.append(f"missing graph in query-feasibility audit: {graph_path_text}")
        elif args.deep_snapshot and sha256(graph_path) != str(graph["graph_sha256"]):
            failures.append(f"graph hash mismatch in query-feasibility audit: {graph_path_text}")
        for query in graph.get("query_files", []):
            query_path_text = str(query["path"])
            query_path = ROOT / query_path_text
            audit_pairs.add((graph_path_text, query_path_text))
            audit_query_records += int(query["queries"])
            audit_feasible += int(query["feasible"])
            audit_infeasible += int(query["infeasible"])
            if not query_path.is_file():
                failures.append(f"missing query in feasibility audit: {query_path_text}")
            elif args.deep_snapshot and sha256(query_path) != str(query["sha256"]):
                failures.append(f"query hash mismatch in feasibility audit: {query_path_text}")
    expected_pairs = {
        (
            (case.graph_path / "graph.txt").relative_to(ROOT).as_posix(),
            case.query_path.relative_to(ROOT).as_posix(),
        )
        for case in cases
    }
    expected_graph_paths = {graph for graph, _query in expected_pairs}
    expected_graph_paths.update(
        str(record["graph"]["path"]) for record in dataset_manifests.values()
    )
    if audit_pairs != expected_pairs:
        failures.append("query-feasibility audit graph/query pair set differs from the matrix")
    if audit_graph_paths != expected_graph_paths:
        failures.append("query-feasibility audit does not cover every formal or held official graph")
    totals = feasibility_audit.get("totals", {})
    if (
        int(totals.get("graphs", -1)) != len(audit_graph_paths)
        or int(totals.get("query_files", -1)) != len(audit_pairs)
        or int(totals.get("unique_query_records", -1)) != audit_query_records
        or int(totals.get("feasible", -1)) != audit_feasible
        or int(totals.get("infeasible", -1)) != audit_infeasible
    ):
        failures.append("query-feasibility audit totals are internally inconsistent")
    if audit_infeasible != 0 or audit_feasible != audit_query_records:
        failures.append("formal query panel contains a component-infeasible query")

    if not args.skip_binaries:
        for method_name in sorted(used_methods):
            path = runner.executable_path(config["methods"][method_name])
            if not path.is_file():
                failures.append(f"missing binary for {method_name}: {path}")

    if args.deep_snapshot:
        print(
            f"Deep-hashed {len(download['files'])} raw downloads, "
            f"{len(manifest_paths)} official interfaces and all formal panel payloads"
        )
    print(f"Validated {len(cases)} cases and {total_tasks} per-instance method tasks")
    if failures:
        print("Environment validation FAILED:")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print("Environment validation PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
