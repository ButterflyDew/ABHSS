#!/usr/bin/env python3
"""Verify and stream-decompress the frozen 2026-07-22 IMDb source files."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_FREEZE = ROOT / "data_sources" / "official" / "official-latest-20260722"
FILES = (
    "title.basics.tsv.gz",
    "title.principals.tsv.gz",
    "name.basics.tsv.gz",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freeze", type=Path, default=DEFAULT_FREEZE)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    freeze = args.freeze.resolve()
    manifest = json.loads(
        (freeze / "download_manifest.json").read_text(encoding="utf-8")
    )
    records = {row["path"]: row for row in manifest["files"]}
    products: list[dict[str, object]] = []
    for name in FILES:
        relative = f"imdb-20260722/{name}"
        record = records.get(relative)
        source = freeze / "raw" / relative
        if record is None or not source.is_file():
            raise FileNotFoundError(f"missing frozen IMDb source: {source}")
        if source.stat().st_size != int(record["bytes"]) or sha256(source) != record["sha256"]:
            raise RuntimeError(f"frozen IMDb source identity mismatch: {source}")
        destination = freeze / "extracted" / "imdb-20260722" / name.removesuffix(".gz")
        if destination.exists() and not args.force:
            products.append(
                {
                    "path": str(destination.relative_to(ROOT)).replace("\\", "/"),
                    "bytes": destination.stat().st_size,
                    "sha256": sha256(destination),
                    "reused": True,
                }
            )
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_suffix(destination.suffix + ".tmp")
        digest = hashlib.sha256()
        size = 0
        with gzip.open(source, "rb") as compressed, temporary.open("wb") as output:
            while block := compressed.read(8 * 1024 * 1024):
                output.write(block)
                digest.update(block)
                size += len(block)
        os.replace(temporary, destination)
        products.append(
            {
                "path": str(destination.relative_to(ROOT)).replace("\\", "/"),
                "bytes": size,
                "sha256": digest.hexdigest(),
                "reused": False,
            }
        )
    output = freeze / "extracted" / "imdb_extraction_manifest.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            {"schema_version": 1, "freeze_id": "imdb-20260722", "products": products},
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"Verified and extracted {len(products)} IMDb files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
