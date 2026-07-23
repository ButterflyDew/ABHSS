#!/usr/bin/env python3
"""Select the fixed 10-query/cell formal panel from official-source pools."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path

from build_gpu_query_panels import (
    copy_selected_group_ids,
    copy_selected_queries,
    scan_query_file,
    select_stratified,
)


ROOT = Path(__file__).resolve().parents[2]
FREEZE_ID = "official-latest-20260722"
DEFAULT_DATA = ROOT / "data" / FREEZE_ID
DEFAULT_OUTPUT = ROOT / "experiment_data" / "official_latest" / "related"
DEFAULT_DATASETS = (
    "snap-wikipedia-2018",
    "snap-twitch-2018",
    "snap-github-2019",
    "snap-youtube",
    "snap-orkut",
    "snap-livejournal",
    "movielens-32m",
    "toronto-current",
)
G_VALUES = tuple(range(4, 17))
SEED = 20260722


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-root", type=Path, default=DEFAULT_DATA)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--dataset", action="append", choices=DEFAULT_DATASETS, default=[])
    parser.add_argument("--seed", type=int, default=SEED)
    parser.add_argument("--queries-per-cell", type=int, default=10)
    args = parser.parse_args()
    datasets = args.dataset or list(DEFAULT_DATASETS)
    cells: list[dict[str, object]] = []
    selections: list[dict[str, object]] = []
    for dataset in datasets:
        for g in G_VALUES:
            source = args.data_root / dataset / f"query_g{g}.txt"
            records = scan_query_file(source, g)
            selected = select_stratified(records, args.queries_per_cell, args.seed, dataset, g)
            destination = args.output / dataset / f"paper_related_g{g}.txt"
            copy_selected_queries(source, destination, selected, g)
            group_id_source = args.data_root / dataset / f"query_g{g}.group_ids.txt"
            group_id_destination = args.output / dataset / f"paper_related_g{g}.group_ids.txt"
            copy_selected_group_ids(
                group_id_source,
                group_id_destination,
                {int(row["source_query_index"]) for row in selected},
            )
            cells.append(
                {
                    "dataset": dataset,
                    "graph_path": str((args.data_root / dataset).relative_to(ROOT)).replace("\\", "/"),
                    "g": g,
                    "source_query_path": str(source.relative_to(ROOT)).replace("\\", "/"),
                    "panel_query_path": str(destination.relative_to(ROOT)).replace("\\", "/"),
                    "panel_query_sha256": sha256(destination),
                    "group_ids_path": str(group_id_destination.relative_to(ROOT)).replace("\\", "/"),
                    "group_ids_sha256": sha256(group_id_destination),
                    "source_queries": len(records),
                    "selected_queries": len(selected),
                    "selection_seed": args.seed,
                    "stratification": "one stable-hash choice from each rank decile of log1p(realized mean group size)",
                }
            )
            selections.extend({"dataset": dataset, **row} for row in selected)
            print(f"{dataset} g={g}: {len(records)} -> {len(selected)}")

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "cells.json").write_text(
        json.dumps(cells, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (args.output / "selected_queries.json").write_text(
        json.dumps(selections, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    with (args.output / "selected_queries.csv").open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(selections[0]))
        writer.writeheader()
        writer.writerows(selections)
    print(f"Built {len(cells)} cells / {len(selections)} queries under {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
