#!/usr/bin/env python3
"""Validate one converted dataset and write its tracked identity manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CATALOG_PATH = ROOT / "experiments" / "official_sources.json"
FREEZE = ROOT / "data_sources" / "official" / "official-latest-20260722"
SLUG_TO_ID = {
    "snap-wikipedia-2018": "SNAP-Wikipedia-2018",
    "snap-twitch-2018": "SNAP-Twitch-2018",
    "snap-github-2019": "SNAP-GitHub-2019",
    "snap-youtube": "SNAP-YouTube",
    "snap-orkut": "SNAP-Orkut",
    "snap-livejournal": "SNAP-LiveJournal",
    "movielens-32m": "MovieLens-32M",
    "linkedmdb": "LinkedMDB-2012",
    "dbpedia-2022.12-en": "DBpedia-2022.12-en",
    "imdb-20260722": "IMDb-daily-20260722",
    "dblp-aminer-v18": "DBLP-AMiner-V18",
    "toronto-current": "Toronto-current",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(8 * 1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def relative(path: Path) -> str:
    return str(path.resolve().relative_to(ROOT)).replace("\\", "/")


def validate_graph(path: Path) -> dict[str, int | float]:
    with path.open("r", encoding="ascii", errors="strict", buffering=8 * 1024 * 1024) as source:
        header = source.readline().split()
        if len(header) != 2:
            raise RuntimeError(f"invalid graph header: {path}")
        n, expected_m = map(int, header)
        if n <= 0 or expected_m < 0:
            raise RuntimeError(f"invalid graph counts: {path}")
        observed_m = 0
        self_loops = 0
        zero_weights = 0
        minimum_weight = float("inf")
        maximum_weight = 0.0
        for line_number, line in enumerate(source, 2):
            fields = line.split()
            if len(fields) != 3:
                raise RuntimeError(f"graph row {line_number} does not contain three fields")
            u, v = int(fields[0]), int(fields[1])
            weight = float(fields[2])
            if not (1 <= u <= n and 1 <= v <= n) or weight < 0:
                raise RuntimeError(f"invalid graph row {line_number}: {line[:120]!r}")
            self_loops += u == v
            zero_weights += weight == 0
            minimum_weight = min(minimum_weight, weight)
            maximum_weight = max(maximum_weight, weight)
            observed_m += 1
    if observed_m != expected_m:
        raise RuntimeError(f"graph edge count mismatch: header={expected_m}, observed={observed_m}")
    if self_loops:
        raise RuntimeError(f"converted graph contains {self_loops} self-loops")
    return {
        "vertices": n,
        "edges": observed_m,
        "minimum_edge_weight": minimum_weight if observed_m else 0.0,
        "maximum_edge_weight": maximum_weight,
        "zero_weight_edges": zero_weights,
    }


def validate_groups(path: Path, n: int) -> dict[str, int]:
    groups = 0
    memberships = 0
    minimum_size: int | None = None
    maximum_size = 0
    with path.open("r", encoding="utf-8", errors="strict") as source:
        for line_number, line in enumerate(source, 1):
            if not line.strip():
                continue
            prefix, separator, values = line.partition(":")
            groups += 1
            if not separator or prefix != f"g{groups}":
                raise RuntimeError(f"invalid candidate-group label at line {line_number}")
            vertices = list(map(int, values.split()))
            if not vertices or len(vertices) != len(set(vertices)):
                raise RuntimeError(f"empty or duplicate-containing candidate group g{groups}")
            if min(vertices) < 1 or max(vertices) > n:
                raise RuntimeError(f"out-of-range candidate group g{groups}")
            size = len(vertices)
            memberships += size
            minimum_size = size if minimum_size is None else min(minimum_size, size)
            maximum_size = max(maximum_size, size)
    if groups == 0:
        raise RuntimeError("candidate-group file is empty")
    return {
        "candidate_groups": groups,
        "candidate_group_memberships": memberships,
        "minimum_candidate_group_size": minimum_size or 0,
        "maximum_candidate_group_size": maximum_size,
    }


def validate_query(path: Path, expected_n: int, expected_g: int) -> int:
    with path.open("r", encoding="utf-8", errors="strict") as source:
        first = source.readline()
        if not first:
            raise RuntimeError(f"empty query file: {path}")
        q = int(first)
        for query_index in range(q):
            g = int(source.readline())
            if g != expected_g:
                raise RuntimeError(f"{path} query {query_index + 1}: expected g={expected_g}, got {g}")
            for group_index in range(g):
                fields = list(map(int, source.readline().split()))
                if not fields or fields[0] <= 0 or fields[0] != len(fields) - 1:
                    raise RuntimeError(f"{path} query {query_index + 1} group {group_index + 1}: bad size")
                vertices = fields[1:]
                if len(vertices) != len(set(vertices)) or min(vertices) < 1 or max(vertices) > expected_n:
                    raise RuntimeError(f"{path} query {query_index + 1} group {group_index + 1}: bad vertices")
        if source.read(1):
            raise RuntimeError(f"trailing content in {path}")
    return q


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    args = parser.parse_args()
    dataset_dir = args.dataset_dir.resolve()
    slug = dataset_dir.name
    dataset_id = SLUG_TO_ID.get(slug)
    if dataset_id is None:
        parser.error(f"unknown official dataset slug {slug!r}")

    catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    catalog_record = next(record for record in catalog["datasets"] if record["id"] == dataset_id)
    downloads = json.loads((FREEZE / "download_manifest.json").read_text(encoding="utf-8"))
    raw_files = [
        {
            key: record.get(key)
            for key in ("path", "role", "bytes", "sha256", "requested_url", "final_url", "etag", "last_modified", "retrieved_utc")
        }
        for record in downloads["files"]
        if record["dataset"] == dataset_id
    ]
    expected_raw_count = len(catalog_record.get("files", []))
    if len(raw_files) != expected_raw_count:
        raise RuntimeError(f"raw freeze is incomplete for {dataset_id}: {len(raw_files)}/{expected_raw_count}")

    graph_path = dataset_dir / "graph.txt"
    groups_path = dataset_dir / "candidate_groups.txt"
    graph_stats = validate_graph(graph_path)
    group_stats = validate_groups(groups_path, int(graph_stats["vertices"]))
    prior_manifest_path = dataset_dir / "dataset_manifest.json"
    prior_manifest = json.loads(prior_manifest_path.read_text(encoding="utf-8")) if prior_manifest_path.exists() else None
    prior_query_generation = None
    if prior_manifest:
        # Before first finalization, several generators store their generation
        # recipe directly in dataset_manifest.json.  On later finalizations,
        # keep only the already-extracted recipe instead of recursively nesting
        # the complete identity manifest.
        prior_query_generation = (
            prior_manifest.get("query_generation")
            if "graph" in prior_manifest
            else prior_manifest
        )
    normalization_path = dataset_dir / "source_normalization.json"
    normalization = json.loads(normalization_path.read_text(encoding="utf-8")) if normalization_path.exists() else None

    query_pools: list[dict[str, object]] = []
    for g in range(1, 17):
        path = dataset_dir / f"query_g{g}.txt"
        if not path.exists():
            continue
        query_pools.append(
            {
                "g": g,
                "queries": validate_query(path, int(graph_stats["vertices"]), g),
                "path": relative(path),
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
                "group_ids_sha256": sha256(dataset_dir / f"query_g{g}.group_ids.txt"),
            }
        )

    auxiliary_hashes = {}
    for name in (
        "source_group_ids.tsv",
        "source_vertex_ids.tsv",
        "source_normalization.json",
        "source_query_manifest.json",
        "relation_frequencies.tsv",
        "build_manifest.json",
        "build_stats.json",
        "poi_assignments.jsonl",
    ):
        path = dataset_dir / name
        if path.exists():
            auxiliary_hashes[name] = {"bytes": path.stat().st_size, "sha256": sha256(path)}
    # SNAP community source-vertex maps live in ignored work space; retain their
    # independently computed hash through source_normalization.json.

    manifest = {
        "schema_version": 1,
        "freeze_id": catalog["freeze_id"],
        "dataset": dataset_id,
        "slug": slug,
        "status": catalog_record["status"],
        "snapshot": catalog_record["snapshot"],
        "official_page": catalog_record["official_page"],
        "transform": catalog_record["transform"],
        "graph": {
            "path": relative(graph_path),
            "bytes": graph_path.stat().st_size,
            "sha256": sha256(graph_path),
            **graph_stats,
        },
        "candidate_groups": {
            "path": relative(groups_path),
            "bytes": groups_path.stat().st_size,
            "sha256": sha256(groups_path),
            **group_stats,
        },
        "raw_files": sorted(raw_files, key=lambda record: str(record["path"])),
        "source_normalization": normalization,
        "query_generation": prior_query_generation,
        "query_pools": query_pools,
        "auxiliary_hashes": auxiliary_hashes,
        "interface_contract": "undirected graph, 1-based dense vertices, one nonnegative edge weight per row; query groups are non-empty deduplicated vertex sets",
    }
    source_query_manifest_path = dataset_dir / "source_query_manifest.json"
    if source_query_manifest_path.exists():
        source_query_manifest = json.loads(source_query_manifest_path.read_text(encoding="utf-8"))
        manifest["source_query_panel"] = {
            "path": relative(source_query_manifest_path),
            "sha256": sha256(source_query_manifest_path),
            "protocol": source_query_manifest.get("protocol"),
            "source_query_path": source_query_manifest.get("source_query_path"),
            "source_query_sha256": source_query_manifest.get("source_query_sha256"),
            "source_queries": source_query_manifest.get("source_queries"),
            "feasible_queries_in_requested_g_range": source_query_manifest.get(
                "feasible_queries_in_requested_g_range"
            ),
            "source_text_is_officially_preprocessed": source_query_manifest.get(
                "source_text_is_officially_preprocessed"
            ),
        }
    temporary = prior_manifest_path.with_suffix(".json.tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        json.dump(manifest, output, indent=2, sort_keys=True, ensure_ascii=False)
        output.write("\n")
    os.replace(temporary, prior_manifest_path)
    print(
        f"{dataset_id}: n={graph_stats['vertices']} m={graph_stats['edges']} "
        f"groups={group_stats['candidate_groups']} query_pools={len(query_pools)}"
    )
    print(f"graph_sha256={manifest['graph']['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
