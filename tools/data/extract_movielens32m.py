#!/usr/bin/env python3
"""Verify and extract the two MovieLens 32M members used by the converter."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "data_sources" / "official" / "official-latest-20260722"
ARCHIVE_RELATIVE = "movielens-32m/ml-32m.zip"
MEMBERS = {
    "ml-32m/ratings.csv": "cf12b74f9ad4b94a011f079e26d4270a",
    "ml-32m/movies.csv": "0df90835c19151f9d819d0822e190797",
    "ml-32m/README.txt": None,
}


def digest(path: Path, algorithm: str) -> str:
    value = hashlib.new(algorithm)
    with path.open("rb") as source:
        while chunk := source.read(8 * 1024 * 1024):
            value.update(chunk)
    return value.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freeze", type=Path, default=FREEZE)
    args = parser.parse_args()
    freeze = args.freeze.resolve()
    manifest = json.loads((freeze / "download_manifest.json").read_text(encoding="utf-8"))
    records = {record["path"]: record for record in manifest["files"]}
    record = records.get(ARCHIVE_RELATIVE)
    archive_path = freeze / "raw" / ARCHIVE_RELATIVE
    if record is None or not archive_path.is_file() or archive_path.stat().st_size != record["bytes"]:
        raise RuntimeError("MovieLens archive is not complete in the download manifest")
    if digest(archive_path, "sha256") != record["sha256"]:
        raise RuntimeError("MovieLens archive SHA-256 mismatch")

    destination_root = freeze / "extracted" / "movielens-32m"
    destination_root.mkdir(parents=True, exist_ok=True)
    with ZipFile(archive_path) as archive:
        bad = archive.testzip()
        if bad:
            raise RuntimeError(f"MovieLens ZIP CRC failed at {bad}")
        for member, expected_md5 in MEMBERS.items():
            destination = destination_root / Path(member).name
            temporary = destination.with_suffix(destination.suffix + ".tmp")
            with archive.open(member) as source, temporary.open("wb") as output:
                while chunk := source.read(8 * 1024 * 1024):
                    output.write(chunk)
            if expected_md5 and digest(temporary, "md5") != expected_md5:
                raise RuntimeError(f"upstream internal MD5 mismatch for {member}")
            os.replace(temporary, destination)
            print(f"{destination}: {destination.stat().st_size} bytes sha256={digest(destination, 'sha256')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
