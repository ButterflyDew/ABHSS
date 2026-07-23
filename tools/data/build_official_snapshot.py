#!/usr/bin/env python3
"""Build the compact, tracked identity snapshot for the official data freeze.

The large archives and converted interface files deliberately stay out of Git.
This file records the hashes of the catalogs/manifests that, in turn, pin every
raw and converted byte used by the paper suites.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
FREEZE_ID = "official-latest-20260722"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def file_record(path: Path) -> dict[str, Any]:
    return {
        "path": relative(path),
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def panel_record(path: Path) -> dict[str, Any]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    rows = payload if isinstance(payload, list) else payload.get("cells", [])
    queries = sum(
        int(row.get("queries", row.get("selected_queries", 0))) for row in rows
    )
    return {
        **file_record(path),
        "cells": len(rows),
        "queries": queries,
    }


def main() -> int:
    catalog_path = ROOT / "experiments" / "official_sources.json"
    download_path = (
        ROOT
        / "data_sources"
        / "official"
        / FREEZE_ID
        / "download_manifest.json"
    )
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    download = json.loads(download_path.read_text(encoding="utf-8"))

    interface_root = ROOT / "data" / FREEZE_ID
    datasets: list[dict[str, Any]] = []
    for manifest_path in sorted(interface_root.glob("*/dataset_manifest.json")):
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        datasets.append(
            {
                **file_record(manifest_path),
                "dataset": manifest["dataset"],
                "slug": manifest["slug"],
                "graph": manifest["graph"],
                "candidate_groups": manifest.get("candidate_groups"),
                "query_pools": len(manifest.get("query_pools", [])),
                "raw_files": len(manifest.get("raw_files", [])),
            }
        )

    raw_root = download_path.parent / "raw"
    raw_files = []
    for row in download["files"]:
        raw_files.append(
            {
                "dataset": row["dataset"],
                "role": row["role"],
                "path": relative(raw_root / row["path"]),
                "bytes": row["bytes"],
                "sha256": row["sha256"],
                "requested_url": row["requested_url"],
                "final_url": row["final_url"],
                "retrieved_utc": row["retrieved_utc"],
            }
        )

    panel_paths = [
        ROOT
        / "experiment_data"
        / "official_latest"
        / "controlled_labels"
        / "dblp"
        / "cells.json",
        ROOT
        / "experiment_data"
        / "official_latest"
        / "controlled_labels"
        / "imdb"
        / "cells.json",
        ROOT / "experiment_data" / "official_latest" / "related" / "cells.json",
        ROOT
        / "experiment_data"
        / "official_latest"
        / "natural"
        / "linkedmdb"
        / "cells.json",
    ]
    steinlib_path = ROOT / "experiment_data" / "steinlib" / "index.json"
    steinlib = json.loads(steinlib_path.read_text(encoding="utf-8"))

    states = {str(row["id"]): str(row["status"]) for row in catalog["datasets"]}
    snapshot = {
        "schema_version": 2,
        "freeze_id": FREEZE_ID,
        "freeze_date": catalog["freeze_date"],
        "identity_rule": catalog["policy"]["identity_rule"],
        "catalog": file_record(catalog_path),
        "download_manifest": file_record(download_path),
        "raw_files": sorted(raw_files, key=lambda row: (row["dataset"], row["path"])),
        "interface_datasets": datasets,
        "paper_panels": [panel_record(path) for path in panel_paths],
        "known_optimum_panel": {
            **file_record(steinlib_path),
            "converted_instances": sum(bool(row.get("converted")) for row in steinlib),
        },
        "catalog_states": states,
        "notes": [
            "Raw archives and expanded interfaces are local, ignored payloads; their hashes are public in this snapshot and the referenced manifests.",
            "An interface dataset is not interchangeable with another snapshot bearing the same informal graph name.",
            "Every executable query must have a graph connected component intersecting every group; component-infeasible mapped source rows remain audit records rather than solver timeouts.",
            "The primary paper matrix has three families: A controlled DBLP/IMDb, B related-group cross-g, and C natural DBpedia/LinkedMDB; correctness and ablation remain secondary.",
            "LinkedMDB natural questions come from Meta ParlAI's official WikiMovies mirror, whose archive hash equals the retired original source archive recorded in the catalog.",
        ],
    }
    output = ROOT / "experiments" / "data_snapshot.json"
    output.write_text(
        json.dumps(snapshot, indent=2, ensure_ascii=False, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(
        f"Wrote {relative(output)}: {len(raw_files)} raw files, "
        f"{len(datasets)} interfaces, {len(panel_paths)} panels"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
