#!/usr/bin/env python3
"""Build the fixed P2 cross-g panel from expanded GPU4GST queries.

Selection is independent of algorithm runtime.  Within every selected
dataset/g cell, candidates are stratified into five rank bins of log(mean
group size), then chosen by a stable SHA-256 key.  The first selection round
is the original five-query panel; the second round appends one more query per
bin without changing the identity or order of queries 1--5.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
from typing import TextIO


ROOT = Path(__file__).resolve().parents[2]
DATA_ROOT = ROOT / "data"
DEFAULT_OUTPUT = ROOT / "experiment_data" / "p2_cross_g"
DATASETS = (
    "GPU4GST_Musae",
    "GPU4GST_Twitch",
    "GPU4GST_Youtube",
    "GPU4GST_DBLP",
    "GPU4GST_Orkut",
    "GPU4GST_Reddit",
)
SIZE_CLASS = {
    "GPU4GST_Musae": "small",
    "GPU4GST_Twitch": "small",
    "GPU4GST_Youtube": "medium",
    "GPU4GST_DBLP": "medium",
    "GPU4GST_Orkut": "large",
    "GPU4GST_Reddit": "large",
}
ALL_G = tuple(range(5, 16))
SIZE_STRATA = 5
QUERIES_PER_CELL = 10
ORIGINAL_QUERIES_PER_CELL = 5
SEED = 20260723
CANDIDATE_GENERATOR_SEED = 2025


def paper_dataset_name(source_dataset: str) -> str:
    source_name = source_dataset.removeprefix("GPU4GST_")
    return f"{source_name}-GPU4GST"


def stable_key(*parts: object) -> str:
    return hashlib.sha256(":".join(map(str, parts)).encode("utf-8")).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def manifest_path(path: Path) -> str:
    """Use repository-relative paths for formal outputs and absolute paths for temporary audits."""
    resolved = path.resolve()
    try:
        return resolved.relative_to(ROOT).as_posix()
    except ValueError:
        return resolved.as_posix()


def scan_query_file(path: Path, expected_g: int) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    with path.open("r", encoding="utf-8", errors="strict") as query_file:
        first = query_file.readline()
        if not first:
            raise ValueError(f"Empty query file: {path}")
        query_count = int(first.strip())
        for query_index in range(query_count):
            g_line = query_file.readline()
            if not g_line:
                raise ValueError(f"Truncated before query {query_index}: {path}")
            g = int(g_line.strip())
            if g != expected_g:
                raise ValueError(f"Expected g={expected_g}, got {g} in {path}")
            sizes: list[int] = []
            for _ in range(g):
                group_line = query_file.readline()
                if not group_line:
                    raise ValueError(f"Truncated inside query {query_index}: {path}")
                size_text, separator, _ = group_line.partition(" ")
                if not separator:
                    raise ValueError(f"Malformed group line in {path}")
                sizes.append(int(size_text))
            mean_f = sum(sizes) / g
            records.append(
                {
                    "source_query_index": query_index + 1,
                    "g": g,
                    "mean_f": mean_f,
                    "log_mean_f": math.log1p(mean_f),
                    "minimum_group_size": min(sizes),
                    "maximum_group_size": max(sizes),
                }
            )
        if query_file.read(1):
            raise ValueError(f"Trailing content after declared queries: {path}")
    return records


def select_stratified(
    records: list[dict[str, object]], count: int, seed: int, dataset: str, g: int
) -> list[dict[str, object]]:
    if count > len(records):
        raise ValueError(f"Cannot select {count} from {len(records)} records")
    if count % SIZE_STRATA:
        raise ValueError(f"Query count {count} is not divisible by {SIZE_STRATA} strata")
    ranked = sorted(records, key=lambda row: (float(row["log_mean_f"]), int(row["source_query_index"])))
    bins: list[list[dict[str, object]]] = [[] for _ in range(SIZE_STRATA)]
    for rank, record in enumerate(ranked):
        quantile = min(SIZE_STRATA - 1, rank * SIZE_STRATA // len(ranked))
        copied = dict(record)
        copied["size_stratum"] = quantile + 1
        bins[quantile].append(copied)

    rounds = count // SIZE_STRATA
    for candidates in bins:
        candidates.sort(key=lambda row: stable_key(seed, dataset, g, row["source_query_index"], row["mean_f"]))
        if len(candidates) < rounds:
            raise ValueError(f"A size stratum in {dataset} g={g} has fewer than {rounds} candidates")

    selected: list[dict[str, object]] = []
    for selection_round in range(rounds):
        tranche = [dict(candidates[selection_round]) for candidates in bins]
        tranche.sort(key=lambda row: int(row["source_query_index"]))
        for record in tranche:
            record["panel_tranche"] = selection_round + 1
        selected.extend(tranche)
    for panel_index, record in enumerate(selected, 1):
        record["panel_query_index"] = panel_index
        record["selection_key"] = stable_key(
            seed, dataset, g, record["source_query_index"], record["mean_f"]
        )
    return selected


def copy_selected_queries(
    source: Path, destination: Path, selected: list[dict[str, object]], expected_g: int
) -> None:
    selected_indices = [int(row["source_query_index"]) for row in selected]
    wanted = set(selected_indices)
    destination.parent.mkdir(parents=True, exist_ok=True)
    blocks: dict[int, tuple[str, list[str]]] = {}
    with source.open("r", encoding="utf-8") as input_file:
        query_count = int(input_file.readline().strip())
        for source_index in range(1, query_count + 1):
            g_line = input_file.readline()
            g = int(g_line.strip())
            group_lines = [input_file.readline() for _ in range(g)]
            if source_index in wanted:
                if g != expected_g:
                    raise ValueError(f"Unexpected g={g} in {source}")
                blocks[source_index] = (g_line, group_lines)
    if len(blocks) != len(wanted):
        raise ValueError(f"Copied {len(blocks)}, expected {len(wanted)} from {source}")
    with destination.open("w", encoding="utf-8", newline="\n") as output_file:
        output_file.write(f"{len(selected_indices)}\n")
        for source_index in selected_indices:
            g_line, group_lines = blocks[source_index]
            output_file.write(g_line)
            output_file.writelines(group_lines)


def copy_selected_group_ids(source: Path, destination: Path, selected_indices: list[int]) -> None:
    comments: list[str] = []
    data_lines: list[str] = []
    with source.open("r", encoding="utf-8") as source_file:
        for line in source_file:
            if line.startswith("#"):
                comments.append(line)
            elif line.strip():
                data_lines.append(line)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("w", encoding="utf-8", newline="\n") as output_file:
        output_file.writelines(comments)
        output_file.write(f"# panel_queries={len(selected_indices)}\n")
        output_file.write("# source_query_indices_are_1_based\n")
        by_source_index = {index: line for index, line in enumerate(data_lines, 1)}
        for index in selected_indices:
            output_file.write(by_source_index[index])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, default=DATA_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--seed", type=int, default=SEED)
    args = parser.parse_args()

    query_manifest: list[dict[str, object]] = []
    cell_manifest: list[dict[str, object]] = []
    for dataset in DATASETS:
        source_manifest_path = args.data_root / dataset / "dataset_manifest.json"
        source_manifest = json.loads(source_manifest_path.read_text(encoding="utf-8"))
        candidate_seed = int(source_manifest["base_seed"])
        if candidate_seed != CANDIDATE_GENERATOR_SEED:
            raise ValueError(
                f"{dataset} candidate seed is {candidate_seed}, expected "
                f"{CANDIDATE_GENERATOR_SEED}"
            )
        if int(source_manifest["queries_per_g"]) != 300:
            raise ValueError(f"{dataset} does not contain 300 candidates per g")
        generated_by_g = {
            int(row["g"]): row for row in source_manifest.get("generated", [])
        }
        for g in ALL_G:
            if g not in generated_by_g:
                raise ValueError(f"{dataset} manifest has no generator record for g={g}")
            source = args.data_root / dataset / f"query_g{g}.txt"
            records = scan_query_file(source, g)
            selected = select_stratified(
                records, QUERIES_PER_CELL, args.seed, dataset, g
            )
            destination = args.output / dataset / f"cross_g{g}.txt"
            copy_selected_queries(source, destination, selected, g)
            group_id_source = args.data_root / dataset / f"query_g{g}.group_ids.txt"
            group_id_destination = args.output / dataset / f"cross_g{g}.group_ids.txt"
            if not group_id_source.is_file():
                raise FileNotFoundError(group_id_source)
            copy_selected_group_ids(
                group_id_source,
                group_id_destination,
                [int(row["source_query_index"]) for row in selected],
            )

            cell_manifest.append(
                {
                    "dataset": paper_dataset_name(dataset),
                    "source_dataset": dataset,
                    "graph_path": f"data/{dataset}",
                    "g": g,
                    "source_query_path": manifest_path(source),
                    "source_query_sha256": sha256_file(source),
                    "source_group_ids_path": manifest_path(group_id_source),
                    "source_group_ids_sha256": sha256_file(group_id_source),
                    "panel_query_path": manifest_path(destination),
                    "panel_query_sha256": sha256_file(destination),
                    "group_ids_path": (
                        manifest_path(group_id_destination)
                        if group_id_destination.exists()
                        else None
                    ),
                    "group_ids_sha256": (
                        sha256_file(group_id_destination)
                        if group_id_destination.exists()
                        else None
                    ),
                    "source_queries": len(records),
                    "selected_queries": len(selected),
                    "original_panel_queries": ORIGINAL_QUERIES_PER_CELL,
                    "additional_queries": len(selected) - ORIGINAL_QUERIES_PER_CELL,
                    "size_class": SIZE_CLASS[dataset],
                    "role": "P2_cross_g",
                    "candidate_generator_base_seed": candidate_seed,
                    "candidate_generator_derived_seed": int(generated_by_g[g]["seed"]),
                    "panel_selection_seed": args.seed,
                    "stratification": "five equal-rank strata of log1p(realized mean group size), two stable-hash queries per stratum in two rounds; round one preserves the original panel",
                }
            )
            for row in selected:
                query_manifest.append(
                    {
                        "dataset": paper_dataset_name(dataset),
                        "source_dataset": dataset,
                        "size_class": SIZE_CLASS[dataset],
                        "role": "P2_cross_g",
                        **row,
                    }
                )
            print(f"{dataset} g={g}: {len(records)} -> {len(selected)}")

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "cells.json").write_text(
        json.dumps(cell_manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (args.output / "selected_queries.json").write_text(
        json.dumps(query_manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    with (args.output / "selected_queries.csv").open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(query_manifest[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(query_manifest)
    print(f"Built {len(cell_manifest)} cells and {len(query_manifest)} queries under {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
