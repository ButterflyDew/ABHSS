#!/usr/bin/env python3
"""Generate the paper's secondary controlled ``<g,f>`` workload.

The protocol follows the MonoGST+ generator supplied with its data: every
group size is sampled independently from ``N(f, (0.15f)^2)``, rounded and
clamped to ``[0.5f, 1.5f]``; its vertices are then sampled uniformly without
replacement from the whole graph.  Groups may overlap.  We deliberately do
not rebalance a query to make its realized mean exactly ``f``.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import random
import re


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = ROOT / "experiment_data" / "s1_controlled_gf"
DEFAULT_DATASETS = (
    ("DBLP-MonoGSTPlus", ROOT / "data" / "DBLP"),
    (
        "IMDb-latest-20260722",
        ROOT / "data" / "official-latest-20260722" / "imdb-20260722",
    ),
)
G_VALUES = (6, 10, 14)
F_VALUES = (200, 400, 800, 1600, 3200)
QUERIES_PER_CELL = 5
STD_RATIO = 0.15
BASE_SEED = 20260723


def stable_seed(*parts: object) -> int:
    payload = ":".join(map(str, parts)).encode("utf-8")
    return int.from_bytes(hashlib.sha256(payload).digest()[:8], "big")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def graph_header(graph_dir: Path) -> tuple[int, int]:
    graph = graph_dir / "graph.txt"
    with graph.open("r", encoding="utf-8", errors="strict") as source:
        fields = source.readline().split()
    if len(fields) != 2:
        raise ValueError(f"invalid graph header in {graph}")
    vertices, edges = map(int, fields)
    if vertices < max(F_VALUES) or edges < 0:
        raise ValueError(f"graph is too small or malformed: {graph}")
    return vertices, edges


def slug(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", text.casefold()).strip("_")


def parse_dataset(value: str) -> tuple[str, Path]:
    name, separator, path_text = value.partition("=")
    if not separator or not name or not path_text:
        raise argparse.ArgumentTypeError("dataset must be NAME=GRAPH_DIR")
    path = Path(path_text)
    if not path.is_absolute():
        path = ROOT / path
    return name, path


def sampled_size(rng: random.Random, f_target: int, vertices: int) -> int:
    low = max(1, round(0.5 * f_target))
    high = min(vertices, round(1.5 * f_target))
    return max(
        low,
        min(high, vertices, round(rng.gauss(f_target, STD_RATIO * f_target))),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dataset",
        action="append",
        type=parse_dataset,
        default=[],
        metavar="NAME=GRAPH_DIR",
        help="override the two frozen paper datasets; repeat exactly twice",
    )
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--seed", type=int, default=BASE_SEED)
    args = parser.parse_args()
    datasets = tuple(args.dataset) if args.dataset else DEFAULT_DATASETS
    if len(datasets) != 2:
        parser.error("the controlled experiment requires exactly two datasets")
    output_root = args.output.resolve()

    cells: list[dict[str, object]] = []
    queries: list[dict[str, object]] = []
    for dataset, graph_dir in datasets:
        graph_dir = graph_dir.resolve()
        vertices, edges = graph_header(graph_dir)
        dataset_output = output_root / slug(dataset)
        dataset_output.mkdir(parents=True, exist_ok=True)
        for g in G_VALUES:
            for f_target in F_VALUES:
                path = dataset_output / f"g{g}_f{f_target}.txt"
                realized_means: list[float] = []
                all_sizes: list[int] = []
                with path.open("w", encoding="utf-8", newline="\n") as output:
                    output.write(f"{QUERIES_PER_CELL}\n")
                    for query_index in range(1, QUERIES_PER_CELL + 1):
                        derived_seed = stable_seed(
                            args.seed, dataset, g, f_target, query_index
                        )
                        rng = random.Random(derived_seed)
                        sizes = [
                            sampled_size(rng, f_target, vertices) for _ in range(g)
                        ]
                        output.write(f"{g}\n")
                        for size in sizes:
                            members = rng.sample(range(1, vertices + 1), size)
                            output.write(f"{size} {' '.join(map(str, members))}\n")
                        mean_f = sum(sizes) / g
                        realized_means.append(mean_f)
                        all_sizes.extend(sizes)
                        queries.append(
                            {
                                "dataset": dataset,
                                "g": g,
                                "f_target": f_target,
                                "query_index": query_index,
                                "derived_seed": derived_seed,
                                "group_sizes": sizes,
                                "f_realized": mean_f,
                            }
                        )

                cells.append(
                    {
                        "dataset": dataset,
                        "graph_path": str(graph_dir.relative_to(ROOT)).replace("\\", "/"),
                        "graph_vertices": vertices,
                        "graph_edges": edges,
                        "query_path": str(path.relative_to(ROOT)).replace("\\", "/"),
                        "query_sha256": sha256(path),
                        "g": g,
                        "f_target": f_target,
                        "queries": QUERIES_PER_CELL,
                        "f_realized_mean": sum(realized_means) / len(realized_means),
                        "f_realized_min": min(realized_means),
                        "f_realized_max": max(realized_means),
                        "minimum_group_size": min(all_sizes),
                        "maximum_group_size": max(all_sizes),
                        "base_seed": args.seed,
                        "sampling": "MonoGST+ protocol: independent rounded/clamped Gaussian group sizes; uniform vertices without replacement within each group",
                    }
                )

    output_root.mkdir(parents=True, exist_ok=True)
    (output_root / "cells.json").write_text(
        json.dumps(cells, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (output_root / "queries.json").write_text(
        json.dumps(queries, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    with (output_root / "cells.csv").open(
        "w", encoding="utf-8", newline=""
    ) as output:
        writer = csv.DictWriter(output, fieldnames=list(cells[0]))
        writer.writeheader()
        writer.writerows(cells)
    print(
        f"Generated {len(cells)} cells and {len(queries)} queries under {output_root}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
