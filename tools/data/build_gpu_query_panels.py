#!/usr/bin/env python3
"""Build the fixed paper panel from the 31,200 expanded GPU4GST queries.

Selection is independent of algorithm runtime.  Within every selected
dataset/g cell, queries are stratified by rank in log(mean group size), then
chosen by a stable SHA-256 key.  This protects the panel from both accidental
size imbalance and post-hoc cherry-picking.
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
DEFAULT_OUTPUT = ROOT / "experiment_data" / "gpu4gst_panel"
DATASETS = (
    "GPU4GST_Musae",
    "GPU4GST_Twitch",
    "GPU4GST_Github",
    "GPU4GST_Youtube",
    "GPU4GST_DBLP",
    "GPU4GST_Orkut",
    "GPU4GST_LiveJournal",
    "GPU4GST_Reddit",
)
REPRESENTATIVE_DATASETS = (
    "GPU4GST_Musae",
    "GPU4GST_Youtube",
    "GPU4GST_DBLP",
    "GPU4GST_Orkut",
)
ANCHOR_G = (4, 8, 12, 16)
ALL_G = tuple(range(4, 17))
SEED = 20260721


def paper_dataset_name(source_dataset: str) -> str:
    return "DBLP-GPU25" if source_dataset == "GPU4GST_DBLP" else source_dataset


def stable_key(*parts: object) -> str:
    return hashlib.sha256(":".join(map(str, parts)).encode("utf-8")).hexdigest()


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
    bin_count = min(10, count)
    ranked = sorted(records, key=lambda row: (float(row["log_mean_f"]), int(row["source_query_index"])))
    bins: list[list[dict[str, object]]] = [[] for _ in range(bin_count)]
    for rank, record in enumerate(ranked):
        quantile = min(bin_count - 1, rank * bin_count // len(ranked))
        copied = dict(record)
        copied["size_stratum"] = quantile + 1
        bins[quantile].append(copied)

    quota = [count // bin_count + (1 if index < count % bin_count else 0) for index in range(bin_count)]
    selected: list[dict[str, object]] = []
    for bin_index, candidates in enumerate(bins):
        candidates.sort(
            key=lambda row: stable_key(
                seed, dataset, g, row["source_query_index"], row["mean_f"]
            )
        )
        selected.extend(candidates[: quota[bin_index]])
    selected.sort(key=lambda row: int(row["source_query_index"]))
    for panel_index, record in enumerate(selected, 1):
        record["panel_query_index"] = panel_index
        record["selection_key"] = stable_key(
            seed, dataset, g, record["source_query_index"], record["mean_f"]
        )
    return selected


def copy_selected_queries(
    source: Path, destination: Path, selected: list[dict[str, object]], expected_g: int
) -> None:
    wanted = {int(row["source_query_index"]) for row in selected}
    destination.parent.mkdir(parents=True, exist_ok=True)
    with source.open("r", encoding="utf-8") as input_file, destination.open(
        "w", encoding="utf-8", newline="\n"
    ) as output_file:
        query_count = int(input_file.readline().strip())
        output_file.write(f"{len(wanted)}\n")
        copied = 0
        for source_index in range(1, query_count + 1):
            g_line = input_file.readline()
            g = int(g_line.strip())
            group_lines = [input_file.readline() for _ in range(g)]
            if source_index in wanted:
                if g != expected_g:
                    raise ValueError(f"Unexpected g={g} in {source}")
                output_file.write(g_line)
                output_file.writelines(group_lines)
                copied += 1
        if copied != len(wanted):
            raise ValueError(f"Copied {copied}, expected {len(wanted)} from {source}")


def copy_selected_group_ids(source: Path, destination: Path, selected_indices: set[int]) -> None:
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
        for index, line in enumerate(data_lines, 1):
            if index in selected_indices:
                output_file.write(line)


def cell_count(dataset: str, g: int) -> tuple[int, str] | None:
    if g in ANCHOR_G:
        return 10, "all_graph_anchor"
    if dataset in REPRESENTATIVE_DATASETS:
        return 10, "representative_dense_g"
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, default=DATA_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--seed", type=int, default=SEED)
    args = parser.parse_args()

    query_manifest: list[dict[str, object]] = []
    cell_manifest: list[dict[str, object]] = []
    for dataset in DATASETS:
        for g in ALL_G:
            selection = cell_count(dataset, g)
            if selection is None:
                continue
            count, role = selection
            source = args.data_root / dataset / f"query_g{g}.txt"
            records = scan_query_file(source, g)
            selected = select_stratified(records, count, args.seed, dataset, g)
            destination = args.output / dataset / f"paper_related_g{g}.txt"
            copy_selected_queries(source, destination, selected, g)
            group_id_source = args.data_root / dataset / f"query_g{g}.group_ids.txt"
            if group_id_source.exists():
                copy_selected_group_ids(
                    group_id_source,
                    args.output / dataset / f"paper_related_g{g}.group_ids.txt",
                    {int(row["source_query_index"]) for row in selected},
                )

            cell_manifest.append(
                {
                    "dataset": paper_dataset_name(dataset),
                    "source_dataset": dataset,
                    "graph_path": f"data/{dataset}",
                    "g": g,
                    "source_query_path": str(source.relative_to(ROOT)).replace("\\", "/"),
                    "panel_query_path": str(destination.relative_to(ROOT)).replace("\\", "/"),
                    "source_queries": len(records),
                    "selected_queries": len(selected),
                    "role": role,
                    "stratification": "rank deciles of log1p(realized mean group size)",
                }
            )
            for row in selected:
                query_manifest.append(
                    {
                        "dataset": paper_dataset_name(dataset),
                        "source_dataset": dataset,
                        "role": role,
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
        writer = csv.DictWriter(output, fieldnames=list(query_manifest[0]))
        writer.writeheader()
        writer.writerows(query_manifest)
    print(f"Built {len(cell_manifest)} cells and {len(query_manifest)} queries under {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
