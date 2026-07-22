#!/usr/bin/env python3
"""Select a compact audit panel from GPU4GST's original author CSV queries."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

from build_gpu_query_panels import (
    DATASETS,
    ROOT,
    SEED,
    copy_selected_group_ids,
    copy_selected_queries,
    scan_query_file,
    select_stratified,
    paper_dataset_name,
)


DEFAULT_OUTPUT = ROOT / "experiment_data" / "gpu4gst_author_panel"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, default=ROOT / "data")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--queries-per-cell", type=int, default=10)
    parser.add_argument("--seed", type=int, default=SEED + 1)
    args = parser.parse_args()

    cells: list[dict[str, object]] = []
    selected_rows: list[dict[str, object]] = []
    for dataset in DATASETS:
        for g in (5, 7):
            source = args.data_root / dataset / f"query_author_g{g}.txt"
            records = scan_query_file(source, g)
            selected = select_stratified(
                records, args.queries_per_cell, args.seed, dataset + ":author", g
            )
            destination = args.output / dataset / f"paper_author_g{g}.txt"
            copy_selected_queries(source, destination, selected, g)
            group_ids = args.data_root / dataset / f"query_author_g{g}.group_ids.txt"
            copy_selected_group_ids(
                group_ids,
                args.output / dataset / f"paper_author_g{g}.group_ids.txt",
                {int(row["source_query_index"]) for row in selected},
            )
            cells.append(
                {
                    "dataset": paper_dataset_name(dataset),
                    "source_dataset": dataset,
                    "graph_path": f"data/{dataset}",
                    "g": g,
                    "source_query_path": str(source.relative_to(ROOT)).replace("\\", "/"),
                    "panel_query_path": str(destination.relative_to(ROOT)).replace("\\", "/"),
                    "source_queries": len(records),
                    "selected_queries": len(selected),
                    "role": "gpu4gst_author_csv_audit",
                    "stratification": "rank deciles of log1p(realized mean group size)",
                }
            )
            selected_rows.extend(
                {
                    "dataset": paper_dataset_name(dataset),
                    "source_dataset": dataset,
                    "role": "author_csv",
                    **row,
                }
                for row in selected
            )
            print(f"{dataset} author g={g}: {len(records)} -> {len(selected)}")

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "cells.json").write_text(
        json.dumps(cells, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    with (args.output / "selected_queries.csv").open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(selected_rows[0]))
        writer.writeheader()
        writer.writerows(selected_rows)
    print(f"Built {len(cells)} author cells and {len(selected_rows)} queries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
