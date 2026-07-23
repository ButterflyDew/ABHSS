#!/usr/bin/env python3
"""Build this paper's controlled real-label panel on a frozen labelled graph.

The DBLP and IMDb graph-construction families follow PrunedDP++, but the g/f
grid and sample count are deliberately this paper's design.  Labels are sampled
independently of solver behavior.  A deterministic rejection step accepts the
first random set whose realized average frequency is within 2% of ``f``; if no
such set occurs in 2,000 attempts, the closest set is used and its actual mean
remains explicit in the manifest.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import random
import re


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DATASET = ROOT / "data" / "official-latest-20260722" / "imdb-20260722"
DEFAULT_OUTPUT = ROOT / "experiment_data" / "official_latest" / "controlled_labels" / "imdb"
G_SWEEP = tuple(range(4, 17))
F_SWEEP_G = (10,)
F_VALUES = (100, 200, 400, 800, 1600, 3200)
G_SWEEP_F = 400


def stable_seed(*parts: object) -> int:
    payload = ":".join(map(str, parts)).encode("utf-8")
    return int.from_bytes(hashlib.sha256(payload).digest()[:8], "big")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def cells(f_sweep_g: tuple[int, ...]) -> list[tuple[int, int, list[str]]]:
    result: dict[tuple[int, int], list[str]] = {}
    for g in G_SWEEP:
        result.setdefault((g, G_SWEEP_F), []).append("g_sweep")
    for g in f_sweep_g:
        for f in F_VALUES:
            result.setdefault((g, f), []).append("f_sweep")
    return [(g, f, roles) for (g, f), roles in sorted(result.items())]


def load_groups(dataset_dir: Path) -> tuple[list[dict[str, object]], dict[int, list[int]]]:
    metadata: list[dict[str, object]] = []
    with (dataset_dir / "source_group_ids.tsv").open(encoding="utf-8", newline="") as source:
        for row in csv.DictReader(source, delimiter="\t"):
            metadata.append(
                {
                    "group_id": int(row["group_id"]),
                    "token": row["token"],
                    "size": int(row["size"]),
                    "largest_component_members": int(row.get("largest_component_members", row["size"])),
                }
            )
    groups: dict[int, list[int]] = {}
    with (dataset_dir / "candidate_groups.txt").open(encoding="utf-8") as source:
        for expected, line in enumerate(source, 1):
            prefix, separator, values = line.partition(":")
            if not separator or prefix != f"g{expected}":
                raise RuntimeError(f"malformed candidate group at line {expected}")
            groups[expected] = list(map(int, values.split()))
    if len(metadata) != len(groups):
        raise RuntimeError("group map and candidate-group file disagree")
    for row in metadata:
        if len(groups[int(row["group_id"])]) != int(row["size"]):
            raise RuntimeError(f"size mismatch for group {row['group_id']}")
    return metadata, groups


def sample_labels(
    candidates: list[dict[str, object]],
    g: int,
    f: int,
    seed: int,
) -> tuple[list[dict[str, object]], int]:
    if len(candidates) < g:
        raise RuntimeError(f"only {len(candidates)} eligible labels for g={g}, f={f}")
    rng = random.Random(seed)
    best: list[dict[str, object]] | None = None
    best_error: int | None = None
    target_sum = g * f
    tolerance = max(1, round(target_sum * 0.02))
    accepted_attempt = 0
    for attempt in range(1, 2001):
        selected = rng.sample(candidates, g)
        error = abs(sum(int(row["size"]) for row in selected) - target_sum)
        if best_error is None or error < best_error:
            best = selected
            best_error = error
        if error <= tolerance:
            accepted_attempt = attempt
            best = selected
            break
    assert best is not None
    return best, accepted_attempt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-dir", type=Path, default=DEFAULT_DATASET)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--dataset-name", default="IMDb-daily-20260722")
    parser.add_argument(
        "--file-prefix",
        help="stable filename prefix; default is a lowercase slug of --dataset-name",
    )
    parser.add_argument(
        "--f-sweep-g",
        type=int,
        action="append",
        default=[],
        help="repeat to add an f-sweep anchor; default is g=10",
    )
    parser.add_argument("--queries-per-cell", type=int, default=10)
    parser.add_argument("--seed", type=int, default=20260722)
    args = parser.parse_args()
    dataset_dir = args.dataset_dir.resolve()
    output_root = args.output.resolve()
    f_sweep_g = tuple(args.f_sweep_g or F_SWEEP_G)
    file_prefix = args.file_prefix or re.sub(r"[^a-z0-9]+", "_", args.dataset_name.lower()).strip("_")
    metadata, groups = load_groups(dataset_dir)
    cell_manifest: list[dict[str, object]] = []
    query_manifest: list[dict[str, object]] = []

    for g, f, roles in cells(f_sweep_g):
        candidates = [
            row
            for row in metadata
            if int(row["largest_component_members"]) > 0
            and round(f * 0.5) <= int(row["size"]) <= round(f * 1.5)
        ]
        query_path = output_root / f"{file_prefix}_label_g{g}_f{f}.txt"
        ids_path = output_root / f"{file_prefix}_label_g{g}_f{f}.group_ids.txt"
        query_path.parent.mkdir(parents=True, exist_ok=True)
        query_temporary = query_path.with_suffix(".txt.tmp")
        ids_temporary = ids_path.with_suffix(".txt.tmp")
        realized: list[float] = []
        with query_temporary.open("w", encoding="utf-8", newline="\n") as queries, \
             ids_temporary.open("w", encoding="utf-8", newline="\n") as ids:
            queries.write(f"{args.queries_per_cell}\n")
            ids.write("# protocol=ABHSS-controlled-random-real-labels\n")
            ids.write(f"# base_seed={args.seed} target_g={g} target_f={f}\n")
            for query_index in range(args.queries_per_cell):
                derived_seed = stable_seed(args.seed, args.dataset_name, g, f, query_index)
                selected, accepted_attempt = sample_labels(candidates, g, f, derived_seed)
                sizes = [int(row["size"]) for row in selected]
                actual_mean = sum(sizes) / g
                realized.append(actual_mean)
                queries.write(f"{g}\n")
                for row in selected:
                    members = groups[int(row["group_id"])]
                    queries.write(f"{len(members)} {' '.join(map(str, members))}\n")
                ids.write(" ".join(str(row["group_id"]) for row in selected) + "\n")
                query_manifest.append(
                    {
                        "g": g,
                        "f_target": f,
                        "query_index": query_index + 1,
                        "derived_seed": derived_seed,
                        "accepted_attempt": accepted_attempt,
                        "group_ids": [row["group_id"] for row in selected],
                        "tokens": [row["token"] for row in selected],
                        "group_sizes": sizes,
                        "f_realized": actual_mean,
                    }
                )
        os.replace(query_temporary, query_path)
        os.replace(ids_temporary, ids_path)
        cell_manifest.append(
            {
                "dataset": args.dataset_name,
                "graph_path": str(dataset_dir.relative_to(ROOT)).replace("\\", "/"),
                "query_path": str(query_path.relative_to(ROOT)).replace("\\", "/"),
                "query_sha256": sha256(query_path),
                "group_ids_path": str(ids_path.relative_to(ROOT)).replace("\\", "/"),
                "group_ids_sha256": sha256(ids_path),
                "g": g,
                "f_target": f,
                "f_realized_mean": sum(realized) / len(realized),
                "f_realized_min": min(realized),
                "f_realized_max": max(realized),
                "queries": args.queries_per_cell,
                "roles": "+".join(roles),
                "candidate_frequency_window": [round(f * 0.5), round(f * 1.5)],
                "candidate_labels": len(candidates),
                "base_seed": args.seed,
                "selection": "uniform random real labels with deterministic rejection to 2% mean-frequency tolerance; closest of 2000 samples otherwise",
                "design_note": "graph-construction family follows PrunedDP++; g=10 f sweep, g/f grid and 10-query sample count are ABHSS paper choices",
            }
        )
        print(f"g={g} f={f}: candidates={len(candidates)} realized={min(realized):.2f}..{max(realized):.2f}")

    (output_root / "cells.json").write_text(
        json.dumps(cell_manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (output_root / "queries.json").write_text(
        json.dumps(query_manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    with (output_root / "cells.csv").open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(cell_manifest[0]))
        writer.writeheader()
        writer.writerows(cell_manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
