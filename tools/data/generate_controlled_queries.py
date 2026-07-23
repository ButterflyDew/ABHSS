#!/usr/bin/env python3
"""Generate the paper's controlled (g, f) query cells without touching raw data.

This is a versioned refinement of ``data/generate_queries_for_noGPU4GST.py``.
It retains uniform vertex sampling and 15% within-query group-size variation,
but balances the sizes so every query has *exactly* the requested mean f.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import random


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = ROOT / "experiment_data" / "controlled"
# (directory name, source-qualified paper name).  The DBLP graph in this
# corpus is not the same weighted graph as GPU4GST's same-sized DBLP file.
DATASETS = (
    ("Toronto", "Toronto"),
    ("MovieLens", "MovieLens"),
    ("DBLP", "DBLP-VEW21"),
)
G_SWEEP = tuple(range(4, 17))
F_SWEEP_G = (6, 10, 14)
F_VALUES = (200, 400, 800, 1600)
G_SWEEP_F = 400
QUERIES_PER_CELL = 10
STD_RATIO = 0.15
BASE_SEED = 20260721


def node_count(path: Path) -> int:
    with path.open("r", encoding="utf-8", errors="ignore") as graph_file:
        fields = graph_file.readline().split()
    if len(fields) < 2:
        raise ValueError(f"Invalid graph header: {path}")
    return int(fields[0])


def stable_seed(*parts: object) -> int:
    text = ":".join(map(str, parts)).encode("utf-8")
    return int.from_bytes(hashlib.sha256(text).digest()[:8], "big")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def balanced_sizes(rng: random.Random, g: int, target: int, n: int) -> list[int]:
    if target > n:
        raise ValueError(f"Requested mean group size f={target} exceeds n={n}")
    low = max(1, round(target * 0.5))
    high = min(n, round(target * 1.5))
    sizes = [max(low, min(high, round(rng.gauss(target, target * STD_RATIO)))) for _ in range(g)]
    remaining = g * target - sum(sizes)
    direction = 1 if remaining > 0 else -1
    while remaining:
        candidates = [
            index
            for index, size in enumerate(sizes)
            if (direction > 0 and size < high) or (direction < 0 and size > low)
        ]
        if not candidates:
            raise RuntimeError("Unable to balance generated group sizes")
        rng.shuffle(candidates)
        for index in candidates:
            if remaining == 0:
                break
            sizes[index] += direction
            remaining -= direction
    return sizes


def cells() -> list[tuple[int, int, list[str]]]:
    mapping: dict[tuple[int, int], list[str]] = {}
    for g in G_SWEEP:
        mapping.setdefault((g, G_SWEEP_F), []).append("g_sweep")
    for g in F_SWEEP_G:
        for f in F_VALUES:
            mapping.setdefault((g, f), []).append("f_sweep")
    return [(g, f, roles) for (g, f), roles in sorted(mapping.items())]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, default=ROOT / "data")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--dataset",
        action="append",
        default=[],
        metavar="ID=GRAPH_DIR",
        help="override the legacy dataset list; GRAPH_DIR is relative to the repository or absolute",
    )
    parser.add_argument("--queries-per-cell", type=int, default=QUERIES_PER_CELL)
    parser.add_argument("--seed", type=int, default=BASE_SEED)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.data_root = args.data_root.resolve()

    if args.dataset:
        datasets: list[tuple[str, str, Path]] = []
        for declaration in args.dataset:
            dataset, separator, graph_text = declaration.partition("=")
            if not separator or not dataset or not graph_text:
                parser.error(f"invalid --dataset {declaration!r}; expected ID=GRAPH_DIR")
            graph_dir = Path(graph_text)
            if not graph_dir.is_absolute():
                graph_dir = ROOT / graph_dir
            datasets.append((dataset, dataset, graph_dir / "graph.txt" if graph_dir.is_dir() else graph_dir))
    else:
        datasets = [
            (source_dataset, dataset, args.data_root / source_dataset / "graph.txt")
            for source_dataset, dataset in DATASETS
        ]

    metadata: list[dict[str, object]] = []
    for source_dataset, dataset, graph_path in datasets:
        n = node_count(graph_path)
        output_dir = args.output / source_dataset
        output_dir.mkdir(parents=True, exist_ok=True)
        for g, f, roles in cells():
            output_path = output_dir / f"controlled_g{g}_f{f}.txt"
            query_rows: list[tuple[list[int], list[list[int]]]] = []
            with output_path.open("w", encoding="utf-8", newline="\n") as output:
                output.write(f"{args.queries_per_cell}\n")
                for query_index in range(args.queries_per_cell):
                    rng = random.Random(
                        stable_seed(args.seed, source_dataset, g, f, query_index)
                    )
                    sizes = balanced_sizes(rng, g, f, n)
                    groups = [rng.sample(range(1, n + 1), size) for size in sizes]
                    query_rows.append((sizes, groups))
                    output.write(f"{g}\n")
                    for size, group in zip(sizes, groups):
                        output.write(f"{size} {' '.join(map(str, group))}\n")

            all_sizes = [size for sizes, _ in query_rows for size in sizes]
            metadata.append(
                {
                    "dataset": dataset,
                    "graph_path": str(graph_path.parent.relative_to(ROOT)).replace("\\", "/"),
                    "query_path": str(output_path.relative_to(ROOT)).replace("\\", "/"),
                    "query_sha256": sha256(output_path),
                    "g": g,
                    "f_target": f,
                    "f_realized_per_query": f,
                    "minimum_group_size": min(all_sizes),
                    "maximum_group_size": max(all_sizes),
                    "queries": args.queries_per_cell,
                    "roles": "+".join(roles),
                    "seed": args.seed,
                    "sampling": "uniform without replacement within each group",
                }
            )

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "manifest.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    with (args.output / "manifest.csv").open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(metadata[0]))
        writer.writeheader()
        writer.writerows(metadata)
    print(f"Generated {len(metadata)} controlled cells under {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
