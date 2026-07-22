#!/usr/bin/env python3
"""Split the Practical-paper query files into their documented (g, f) cells.

The source files are never modified.  Each output contains the exact ten
source queries from one contiguous block, which makes the original protocol
machine-readable without re-sampling or inferring cells during analysis.
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DATASETS = (
    ("Toronto", "Toronto"),
    ("MovieLens", "MovieLens"),
    ("DBLP", "DBLP-VEW21"),
)
G_VALUES = (5, 6, 7, 8)
F_VALUES = (200, 400, 800, 1600)
QUERIES_PER_CELL = 10


def read_queries(path: Path) -> list[tuple[int, list[str], list[int]]]:
    rows: list[tuple[int, list[str], list[int]]] = []
    with path.open("r", encoding="utf-8") as source:
        declared = int(source.readline().strip())
        for query_index in range(declared):
            g_line = source.readline()
            if not g_line:
                raise ValueError(f"Truncated before query {query_index + 1}: {path}")
            g = int(g_line.strip())
            group_lines = [source.readline() for _ in range(g)]
            if any(not line for line in group_lines):
                raise ValueError(f"Truncated in query {query_index + 1}: {path}")
            sizes = [int(line.partition(" ")[0]) for line in group_lines]
            rows.append((g, [g_line, *group_lines], sizes))
        if source.read(1):
            raise ValueError(f"Trailing content after {declared} queries: {path}")
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, default=ROOT / "data")
    parser.add_argument(
        "--output", type=Path, default=ROOT / "experiment_data" / "practical_original"
    )
    args = parser.parse_args()

    manifest: list[dict[str, object]] = []
    expected_count = len(G_VALUES) * len(F_VALUES) * QUERIES_PER_CELL
    for source_dataset, dataset in SOURCE_DATASETS:
        source_path = args.data_root / source_dataset / "query.txt"
        queries = read_queries(source_path)
        if len(queries) != expected_count:
            raise ValueError(
                f"Expected {expected_count} Practical queries in {source_path}, "
                f"found {len(queries)}"
            )
        block = 0
        for g in G_VALUES:
            for f_target in F_VALUES:
                start = block * QUERIES_PER_CELL
                selected = queries[start : start + QUERIES_PER_CELL]
                block += 1
                if any(row[0] != g for row in selected):
                    raise ValueError(f"Unexpected g in block {block} of {source_path}")
                all_sizes = [size for _, _, sizes in selected for size in sizes]
                if any(size < 0.5 * f_target or size > 1.5 * f_target for size in all_sizes):
                    raise ValueError(
                        f"Group size outside the documented 0.5f..1.5f range in "
                        f"{source_path}, g={g}, f={f_target}"
                    )

                destination = (
                    args.output
                    / source_dataset
                    / f"original_g{g}_f{f_target}.txt"
                )
                destination.parent.mkdir(parents=True, exist_ok=True)
                with destination.open("w", encoding="utf-8", newline="\n") as output:
                    output.write(f"{QUERIES_PER_CELL}\n")
                    for _, lines, _ in selected:
                        output.writelines(lines)

                manifest.append(
                    {
                        "dataset": dataset,
                        "source_dataset": source_dataset,
                        "graph_path": f"data/{source_dataset}",
                        "source_query_path": str(source_path.relative_to(ROOT)).replace("\\", "/"),
                        "query_path": str(destination.relative_to(ROOT)).replace("\\", "/"),
                        "source_query_begin": start + 1,
                        "source_query_end": start + QUERIES_PER_CELL,
                        "g": g,
                        "f_target": f_target,
                        "queries": QUERIES_PER_CELL,
                        "realized_mean_f": sum(all_sizes) / len(all_sizes),
                        "minimum_group_size": min(all_sizes),
                        "maximum_group_size": max(all_sizes),
                        "sampling": "exact copy of the unpublished Practical-paper query block",
                    }
                )

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    with (args.output / "manifest.csv").open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(manifest[0]))
        writer.writeheader()
        writer.writerows(manifest)
    print(f"Built {len(manifest)} exact Practical-paper cells under {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
