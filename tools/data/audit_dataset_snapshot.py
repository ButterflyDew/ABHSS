#!/usr/bin/env python3
"""Create a machine-readable audit of every graph and primary query corpus."""

from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import statistics


ROOT = Path(__file__).resolve().parents[2]
DATASETS = (
    "Toronto",
    "MovieLens",
    "DBLP",
    "LinkedMDB",
    "DBpedia",
    "GPU4GST_Musae",
    "GPU4GST_Twitch",
    "GPU4GST_Github",
    "GPU4GST_Youtube",
    "GPU4GST_DBLP",
    "GPU4GST_Orkut",
    "GPU4GST_LiveJournal",
    "GPU4GST_Reddit",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def graph_record(path: Path, include_hash: bool) -> dict[str, object]:
    with path.open("r", encoding="utf-8", errors="strict") as graph:
        fields = graph.readline().split()
    if len(fields) != 2:
        raise ValueError(f"Invalid graph header: {path}")
    record: dict[str, object] = {
        "path": str(path.relative_to(ROOT)).replace("\\", "/"),
        "vertices": int(fields[0]),
        "edges": int(fields[1]),
        "bytes": path.stat().st_size,
    }
    if include_hash:
        record["sha256"] = sha256(path)
    return record


def query_record(path: Path, include_hash: bool) -> dict[str, object]:
    g_counts: Counter[int] = Counter()
    realized_means: list[float] = []
    with path.open("r", encoding="utf-8") as queries:
        count = int(queries.readline().strip())
        for query_index in range(count):
            g = int(queries.readline().strip())
            sizes = []
            for _ in range(g):
                line = queries.readline()
                if not line:
                    raise ValueError(f"Truncated query {query_index + 1}: {path}")
                sizes.append(int(line.partition(" ")[0]))
            g_counts[g] += 1
            realized_means.append(sum(sizes) / g)
    record: dict[str, object] = {
        "path": str(path.relative_to(ROOT)).replace("\\", "/"),
        "queries": count,
        "g_counts": {str(key): value for key, value in sorted(g_counts.items())},
        "realized_mean_f_min": min(realized_means),
        "realized_mean_f_median": statistics.median(realized_means),
        "realized_mean_f_mean": statistics.fmean(realized_means),
        "realized_mean_f_max": max(realized_means),
        "bytes": path.stat().st_size,
    }
    if include_hash:
        record["sha256"] = sha256(path)
    return record


def file_record(path: Path, include_hash: bool = True) -> dict[str, object]:
    record: dict[str, object] = {
        "path": str(path.relative_to(ROOT)).replace("\\", "/"),
        "bytes": path.stat().st_size,
    }
    if include_hash:
        record["sha256"] = sha256(path)
    return record


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output", type=Path, default=ROOT / "experiments" / "data_snapshot.json"
    )
    parser.add_argument("--hash", action="store_true", help="hash every graph and primary query")
    args = parser.parse_args()

    datasets = []
    for name in DATASETS:
        folder = ROOT / "data" / name
        primary_query = folder / "query.txt"
        record: dict[str, object] = {
            "dataset": name,
            "graph": graph_record(folder / "graph.txt", args.hash),
        }
        if primary_query.exists():
            record["primary_queries"] = query_record(primary_query, args.hash)
        additional_queries = [
            path
            for path in sorted(folder.glob("query*.txt"))
            if path != primary_query
        ]
        if additional_queries:
            record["additional_query_files"] = [
                file_record(path, args.hash) for path in additional_queries
            ]
        manifest = folder / "dataset_manifest.json"
        if manifest.exists():
            record["dataset_manifest"] = file_record(manifest, args.hash)
        datasets.append(record)
        print(f"Audited {name}")

    sources = []
    for path in sorted((ROOT / "data").glob("*/*.zip")):
        sources.append(file_record(path, args.hash))
    for filename in ("WRP3.tgz", "WRP4.tgz"):
        path = ROOT / "data_sources" / "steinlib" / filename
        sources.append(file_record(path, True))
    boost_archive = ROOT / "third_party" / "_downloads" / "boost_1_85_0.tar.bz2"
    if boost_archive.exists():
        sources.append(file_record(boost_archive, True))

    derived_panels = []
    panel_specs = (
        (
            "controlled",
            ROOT / "experiment_data" / "controlled" / "manifest.json",
            "query_path",
            [ROOT / "experiment_data" / "controlled" / "manifest.csv"],
        ),
        (
            "practical_original",
            ROOT / "experiment_data" / "practical_original" / "manifest.json",
            "query_path",
            [ROOT / "experiment_data" / "practical_original" / "manifest.csv"],
        ),
        (
            "gpu4gst_related",
            ROOT / "experiment_data" / "gpu4gst_panel" / "cells.json",
            "panel_query_path",
            [
                ROOT / "experiment_data" / "gpu4gst_panel" / "selected_queries.json",
                ROOT / "experiment_data" / "gpu4gst_panel" / "selected_queries.csv",
            ],
        ),
        (
            "gpu4gst_author",
            ROOT / "experiment_data" / "gpu4gst_author_panel" / "cells.json",
            "panel_query_path",
            [ROOT / "experiment_data" / "gpu4gst_author_panel" / "selected_queries.csv"],
        ),
    )
    for name, manifest_path, query_field, support_paths in panel_specs:
        records = json.loads(manifest_path.read_text(encoding="utf-8"))
        derived_panels.append(
            {
                "name": name,
                "cells": len(records),
                "queries": sum(
                    int(row.get("queries", row.get("selected_queries", 0)))
                    for row in records
                ),
                "manifest": file_record(manifest_path, True),
                "support_files": [file_record(path, True) for path in support_paths],
                "query_files": [
                    file_record(ROOT / row[query_field], True) for row in records
                ],
            }
        )

    steinlib_index = ROOT / "experiment_data" / "steinlib" / "index.json"
    steinlib_records = json.loads(steinlib_index.read_text(encoding="utf-8"))
    converted = [row for row in steinlib_records if row.get("converted")]
    derived_panels.append(
        {
            "name": "steinlib_wrp",
            "cells": len(converted),
            "queries": len(converted),
            "manifest": file_record(steinlib_index, True),
            "support_files": [
                file_record(ROOT / "experiment_data" / "steinlib" / "index.csv", True)
            ],
            "instances": [
                {
                    "name": row["name"],
                    "graph": file_record(
                        ROOT / "experiment_data" / "steinlib" / row["name"] / "graph.txt",
                        True,
                    ),
                    "query": file_record(
                        ROOT / "experiment_data" / "steinlib" / row["name"] / "query.txt",
                        True,
                    ),
                    "metadata": file_record(
                        ROOT / "experiment_data" / "steinlib" / row["name"] / "metadata.json",
                        True,
                    ),
                }
                for row in converted
            ],
        }
    )

    output = args.output if args.output.is_absolute() else ROOT / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            {
                "schema_version": 2,
                "generated_at": datetime.now(timezone.utc).isoformat(),
                "hash_graphs_and_primary_queries": args.hash,
                "datasets": datasets,
                "source_archives": sources,
                "derived_panels": derived_panels,
                "same_name_policy": (
                    "same-named datasets must share one canonical weighted graph; "
                    "query protocols may differ and are separately named"
                ),
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
