#!/usr/bin/env python3
"""Normalize official SNAP archives for the ABHSS graph builder.

This stage performs only source-format work: dense ID mapping, undirected edge
canonicalization and candidate-group extraction.  It never computes solver
results and never reads the former GPU4GST author artifacts.
"""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[2]
FREEZE_ID = "official-latest-20260722"
DEFAULT_FREEZE = ROOT / "data_sources" / "official" / FREEZE_ID
DEFAULT_OUTPUT = ROOT / "data" / FREEZE_ID
CHUNK_BYTES = 8 * 1024 * 1024


FEATURE_DATASETS = {
    "SNAP-Wikipedia-2018": {
        "slug": "snap-wikipedia-2018",
        "archive": "snap-wikipedia/wikipedia.zip",
        "parts": [
            ("wikipedia/chameleon/musae_chameleon_edges.csv", "wikipedia/chameleon/musae_chameleon_features.json", "wikipedia/chameleon/musae_chameleon_target.csv"),
            ("wikipedia/crocodile/musae_crocodile_edges.csv", "wikipedia/crocodile/musae_crocodile_features.json", "wikipedia/crocodile/musae_crocodile_target.csv"),
            ("wikipedia/squirrel/musae_squirrel_edges.csv", "wikipedia/squirrel/musae_squirrel_features.json", "wikipedia/squirrel/musae_squirrel_target.csv"),
        ],
    },
    "SNAP-Twitch-2018": {
        "slug": "snap-twitch-2018",
        "archive": "snap-twitch/twitch.zip",
        "parts": [
            (
                f"twitch/{language}/musae_{language}_edges.csv",
                f"twitch/{language}/musae_{language}.json" if language == "DE" else f"twitch/{language}/musae_{language}_features.json",
                f"twitch/{language}/musae_{language}_target.csv",
            )
            for language in ("DE", "ENGB", "ES", "FR", "PTBR", "RU")
        ],
    },
    "SNAP-GitHub-2019": {
        "slug": "snap-github-2019",
        "archive": "snap-github/git_web_ml.zip",
        "parts": [
            ("git_web_ml/musae_git_edges.csv", "git_web_ml/musae_git_features.json", "git_web_ml/musae_git_target.csv"),
        ],
    },
}


COMMUNITY_DATASETS = {
    "SNAP-YouTube": {
        "slug": "snap-youtube",
        "graph": "snap-youtube/com-youtube.ungraph.txt.gz",
        "groups": "snap-youtube/com-youtube.top5000.cmty.txt.gz",
        "vertices": 1_134_890,
        "edges": 2_987_624,
    },
    "SNAP-Orkut": {
        "slug": "snap-orkut",
        "graph": "snap-orkut/com-orkut.ungraph.txt.gz",
        "groups": "snap-orkut/com-orkut.top5000.cmty.txt.gz",
        "vertices": 3_072_441,
        "edges": 117_185_083,
    },
    "SNAP-LiveJournal": {
        "slug": "snap-livejournal",
        "graph": "snap-livejournal/com-lj.ungraph.txt.gz",
        "groups": "snap-livejournal/com-lj.top5000.cmty.txt.gz",
        "vertices": 3_997_962,
        "edges": 34_681_189,
    },
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(CHUNK_BYTES):
            digest.update(chunk)
    return digest.hexdigest()


def replace(temporary: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    os.replace(temporary, destination)


def load_download_manifest(freeze: Path) -> dict[str, dict]:
    path = freeze / "download_manifest.json"
    with path.open("r", encoding="utf-8") as source:
        manifest = json.load(source)
    return {entry["path"]: entry for entry in manifest["files"]}


def verified_raw(freeze: Path, records: dict[str, dict], relative: str) -> Path:
    path = freeze / "raw" / relative
    record = records.get(relative)
    if record is None:
        raise RuntimeError(f"download manifest has no completed record for {relative}")
    if not path.is_file() or path.stat().st_size != record["bytes"]:
        raise RuntimeError(f"raw file is absent or has the wrong size: {path}")
    actual = sha256(path)
    if actual != record["sha256"]:
        raise RuntimeError(f"raw SHA-256 mismatch for {path}: {actual} != {record['sha256']}")
    return path


def target_node_count(archive: ZipFile, member: str) -> int:
    with archive.open(member) as binary:
        text = io.TextIOWrapper(binary, encoding="utf-8", newline="")
        reader = csv.DictReader(text)
        # Twitch keeps both the original platform ID and the dense graph ID;
        # Wikipedia/GitHub use the first column as their dense ID.
        id_column = "new_id" if "new_id" in reader.fieldnames else reader.fieldnames[0]
        ids = [int(row[id_column]) for row in reader]
    if not ids:
        raise RuntimeError(f"empty target table: {member}")
    unique_ids = set(ids)
    # The official FR target CSV repeats two rows; graph IDs themselves remain
    # a dense set of 6,549 vertices, matching the SNAP dataset statistics.
    if min(unique_ids) != 0 or max(unique_ids) + 1 != len(unique_ids):
        raise RuntimeError(f"node IDs are not dense 0..n-1 in {member}")
    return len(unique_ids)


def read_csv_edges(archive: ZipFile, member: str, offset: int, n: int, edges: set[tuple[int, int]]) -> int:
    input_rows = 0
    with archive.open(member) as binary:
        text = io.TextIOWrapper(binary, encoding="utf-8", newline="")
        reader = csv.reader(text)
        header = next(reader, None)
        if not header or len(header) < 2:
            raise RuntimeError(f"invalid edge CSV header: {member}")
        for row in reader:
            if not row:
                continue
            input_rows += 1
            u, v = int(row[0]), int(row[1])
            if not (0 <= u < n and 0 <= v < n):
                raise RuntimeError(f"out-of-range edge ({u}, {v}) in {member}")
            if u == v:
                continue
            u += offset + 1
            v += offset + 1
            edges.add((u, v) if u < v else (v, u))
    return input_rows


def read_features(
    archive: ZipFile,
    member: str,
    offset: int,
    n: int,
    groups: dict[int, set[int]],
) -> int:
    with archive.open(member) as source:
        values = json.load(source)
    memberships = 0
    for node_text, feature_ids in values.items():
        node = int(node_text)
        if not 0 <= node < n:
            raise RuntimeError(f"out-of-range feature node {node} in {member}")
        dense = offset + node + 1
        for feature in set(map(int, feature_ids)):
            groups.setdefault(feature, set()).add(dense)
            memberships += 1
    return memberships


def write_groups(path: Path, ordered_groups: list[tuple[str, list[int]]]) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.parent.mkdir(parents=True, exist_ok=True)
    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        for index, (_source_id, vertices) in enumerate(ordered_groups, 1):
            if not vertices:
                raise RuntimeError("candidate groups must be non-empty")
            output.write(f"g{index}: {' '.join(map(str, vertices))}\n")
    replace(temporary, path)


def write_group_map(path: Path, ordered_groups: list[tuple[str, list[int]]]) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        output.write("group_id\tsource_group_id\tsize\n")
        for index, (source_id, vertices) in enumerate(ordered_groups, 1):
            output.write(f"{index}\t{source_id}\t{len(vertices)}\n")
    replace(temporary, path)


def normalize_feature_dataset(dataset: str, config: dict, freeze: Path, output_root: Path, records: dict[str, dict]) -> dict:
    archive_path = verified_raw(freeze, records, config["archive"])
    edges: set[tuple[int, int]] = set()
    groups: dict[int, set[int]] = {}
    offset = 0
    input_edge_rows = 0
    memberships = 0
    parts: list[dict] = []
    with ZipFile(archive_path) as archive:
        bad = archive.testzip()
        if bad:
            raise RuntimeError(f"corrupt member {bad} in {archive_path}")
        for edge_member, feature_member, target_member in config["parts"]:
            n = target_node_count(archive, target_member)
            rows = read_csv_edges(archive, edge_member, offset, n, edges)
            member_count = read_features(archive, feature_member, offset, n, groups)
            parts.append({"name": edge_member.split("/")[-2], "vertices": n, "edge_rows": rows})
            input_edge_rows += rows
            memberships += member_count
            offset += n

    work_dir = freeze / "work" / config["slug"]
    work_dir.mkdir(parents=True, exist_ok=True)
    edge_path = work_dir / "edges.txt"
    temporary = edge_path.with_suffix(".txt.tmp")
    with temporary.open("w", encoding="ascii", newline="\n", buffering=8 * 1024 * 1024) as output:
        output.write(f"{offset} {len(edges)}\n")
        for u, v in sorted(edges):
            output.write(f"{u} {v}\n")
    replace(temporary, edge_path)

    ordered_groups = [(str(source_id), sorted(vertices)) for source_id, vertices in sorted(groups.items())]
    dataset_dir = output_root / config["slug"]
    dataset_dir.mkdir(parents=True, exist_ok=True)
    write_groups(dataset_dir / "candidate_groups.txt", ordered_groups)
    write_group_map(dataset_dir / "source_group_ids.tsv", ordered_groups)
    return {
        "dataset": dataset,
        "slug": config["slug"],
        "source_kind": "SNAP feature archives",
        "vertices": offset,
        "edges": len(edges),
        "input_edge_rows": input_edge_rows,
        "self_loops_or_duplicate_orientations_removed": input_edge_rows - len(edges),
        "candidate_groups": len(ordered_groups),
        "memberships": sum(len(vertices) for _source, vertices in ordered_groups),
        "parts": parts,
        "normalized_edges": str(edge_path.relative_to(ROOT)).replace("\\", "/"),
        "candidate_groups_path": str((dataset_dir / "candidate_groups.txt").relative_to(ROOT)).replace("\\", "/"),
        "raw_files": [{"path": config["archive"], "sha256": records[config["archive"]]["sha256"]}],
    }


def normalize_community_dataset(dataset: str, config: dict, freeze: Path, output_root: Path, records: dict[str, dict]) -> dict:
    graph_path = verified_raw(freeze, records, config["graph"])
    group_path = verified_raw(freeze, records, config["groups"])
    n, expected_m = config["vertices"], config["edges"]
    work_dir = freeze / "work" / config["slug"]
    work_dir.mkdir(parents=True, exist_ok=True)
    edge_path = work_dir / "edges.txt"
    temporary = edge_path.with_suffix(".txt.tmp")
    count = 0
    source_to_dense: dict[int, int] = {}
    dense_to_source: list[int] = []

    def dense(source_id: int) -> int:
        mapped = source_to_dense.get(source_id)
        if mapped is None:
            mapped = len(dense_to_source) + 1
            source_to_dense[source_id] = mapped
            dense_to_source.append(source_id)
        return mapped

    with gzip.open(graph_path, "rt", encoding="ascii", errors="strict") as source, temporary.open(
        "w", encoding="ascii", newline="\n", buffering=8 * 1024 * 1024
    ) as output:
        output.write(f"{n} {expected_m}\n")
        for line in source:
            if not line or line.startswith("#"):
                continue
            fields = line.split()
            if len(fields) != 2:
                raise RuntimeError(f"invalid SNAP edge row: {line[:100]!r}")
            source_u, source_v = map(int, fields)
            if source_u < 0 or source_v < 0 or source_u == source_v:
                raise RuntimeError(f"invalid SNAP edge ({source_u}, {source_v})")
            u, v = dense(source_u), dense(source_v)
            if u > v:
                u, v = v, u
            output.write(f"{u} {v}\n")
            count += 1
    if count != expected_m or len(dense_to_source) != n:
        raise RuntimeError(
            f"SNAP graph count mismatch: observed n={len(dense_to_source)},m={count}; expected n={n},m={expected_m}"
        )
    replace(temporary, edge_path)

    vertex_map_path = work_dir / "source_vertex_ids.tsv"
    vertex_map_temporary = vertex_map_path.with_suffix(".tsv.tmp")
    with vertex_map_temporary.open("w", encoding="ascii", newline="\n", buffering=8 * 1024 * 1024) as output:
        output.write("vertex_id\tsource_vertex_id\n")
        for vertex_id, source_id in enumerate(dense_to_source, 1):
            output.write(f"{vertex_id}\t{source_id}\n")
    replace(vertex_map_temporary, vertex_map_path)

    ordered_groups: list[tuple[str, list[int]]] = []
    with gzip.open(group_path, "rt", encoding="ascii", errors="strict") as source:
        for source_index, line in enumerate(source, 1):
            source_vertices = set(map(int, line.split()))
            if not source_vertices:
                continue
            missing = source_vertices.difference(source_to_dense)
            if missing:
                sample = sorted(missing)[:5]
                raise RuntimeError(f"community {source_index} contains vertices outside the official LCC: {sample}")
            vertices = sorted(source_to_dense[source_id] for source_id in source_vertices)
            ordered_groups.append((str(source_index), vertices))
    if len(ordered_groups) != 5000:
        raise RuntimeError(f"expected 5000 official top communities, observed {len(ordered_groups)}")
    dataset_dir = output_root / config["slug"]
    dataset_dir.mkdir(parents=True, exist_ok=True)
    write_groups(dataset_dir / "candidate_groups.txt", ordered_groups)
    write_group_map(dataset_dir / "source_group_ids.tsv", ordered_groups)
    return {
        "dataset": dataset,
        "slug": config["slug"],
        "source_kind": "SNAP largest connected component and official top-5000 communities",
        "vertices": n,
        "edges": count,
        "candidate_groups": len(ordered_groups),
        "memberships": sum(len(vertices) for _source, vertices in ordered_groups),
        "vertex_id_mapping": "dense IDs assigned by first occurrence in the frozen official edge stream",
        "vertex_id_mapping_path": str(vertex_map_path.relative_to(ROOT)).replace("\\", "/"),
        "vertex_id_mapping_sha256": sha256(vertex_map_path),
        "normalized_edges": str(edge_path.relative_to(ROOT)).replace("\\", "/"),
        "candidate_groups_path": str((dataset_dir / "candidate_groups.txt").relative_to(ROOT)).replace("\\", "/"),
        "raw_files": [
            {"path": config["graph"], "sha256": records[config["graph"]]["sha256"]},
            {"path": config["groups"], "sha256": records[config["groups"]]["sha256"]},
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freeze", type=Path, default=DEFAULT_FREEZE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--dataset", action="append", choices=sorted(FEATURE_DATASETS | COMMUNITY_DATASETS), required=True)
    args = parser.parse_args()
    freeze = args.freeze.resolve()
    output = args.output.resolve()
    records = load_download_manifest(freeze)
    for dataset in args.dataset:
        print(f"[{dataset}] verifying and normalizing", flush=True)
        if dataset in FEATURE_DATASETS:
            result = normalize_feature_dataset(dataset, FEATURE_DATASETS[dataset], freeze, output, records)
        else:
            result = normalize_community_dataset(dataset, COMMUNITY_DATASETS[dataset], freeze, output, records)
        manifest_path = output / result["slug"] / "source_normalization.json"
        temporary = manifest_path.with_suffix(".json.tmp")
        with temporary.open("w", encoding="utf-8", newline="\n") as manifest_file:
            json.dump(result, manifest_file, indent=2, sort_keys=True, ensure_ascii=False)
            manifest_file.write("\n")
        replace(temporary, manifest_path)
        print(
            f"  n={result['vertices']} m={result['edges']} groups={result['candidate_groups']} "
            f"memberships={result['memberships']}",
            flush=True,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
