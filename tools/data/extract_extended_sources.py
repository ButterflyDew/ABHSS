#!/usr/bin/env python3
"""Verify and stream-decompress the frozen RDF, IMDb, WikiMovies and AMiner files.

The decompressed products are disposable build intermediates.  Every output is
written to a temporary file, hashed while it is produced, and atomically moved
into the ignored ``extracted`` tree only after the compressed input matches the
download manifest.
"""

from __future__ import annotations

import argparse
import bz2
import gzip
import hashlib
import json
import os
from pathlib import Path
import tarfile
from typing import BinaryIO, Callable
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_FREEZE = ROOT / "data_sources" / "official" / "official-latest-20260722"


def sha256(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(8 * 1024 * 1024):
            value.update(chunk)
    return value.hexdigest()


def verified_input(freeze: Path, relative: str, records: dict[str, dict[str, object]]) -> Path:
    path = freeze / "raw" / relative
    record = records.get(relative)
    if record is None:
        raise RuntimeError(f"{relative} is absent from download_manifest.json")
    if not path.is_file() or path.stat().st_size != int(record["bytes"]):
        raise RuntimeError(f"{relative} is missing or has the wrong byte count")
    actual = sha256(path)
    if actual != record["sha256"]:
        raise RuntimeError(f"{relative} SHA-256 mismatch: {actual}")
    return path


def stream_copy(source: BinaryIO, destination: Path) -> tuple[int, str]:
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    temporary.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    size = 0
    with temporary.open("wb") as output:
        while chunk := source.read(8 * 1024 * 1024):
            output.write(chunk)
            digest.update(chunk)
            size += len(chunk)
    os.replace(temporary, destination)
    return size, digest.hexdigest()


def extract_one(
    destination: Path,
    opener: Callable[[], BinaryIO],
    force: bool,
) -> dict[str, object]:
    if destination.exists() and not force:
        return {
            "path": str(destination.relative_to(ROOT)).replace("\\", "/"),
            "bytes": destination.stat().st_size,
            "sha256": sha256(destination),
            "reused": True,
        }
    with opener() as source:
        size, digest = stream_copy(source, destination)
    print(f"{destination}: {size} bytes sha256={digest}")
    return {
        "path": str(destination.relative_to(ROOT)).replace("\\", "/"),
        "bytes": size,
        "sha256": digest,
        "reused": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freeze", type=Path, default=DEFAULT_FREEZE)
    parser.add_argument(
        "--dataset",
        action="append",
        choices=("linkedmdb", "dbpedia", "imdb", "wikimovies", "aminer"),
        default=[],
        help="extract only selected datasets; the default extracts all five",
    )
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    freeze = args.freeze.resolve()
    selected = set(args.dataset or ("linkedmdb", "dbpedia", "imdb", "wikimovies", "aminer"))
    download_manifest = json.loads((freeze / "download_manifest.json").read_text(encoding="utf-8"))
    records = {str(record["path"]): record for record in download_manifest["files"]}
    products: list[dict[str, object]] = []

    if "linkedmdb" in selected:
        archive_path = verified_input(
            freeze, "linkedmdb/linkedmdb-latest-dump.zip", records
        )
        with ZipFile(archive_path) as archive:
            bad = archive.testzip()
            if bad:
                raise RuntimeError(f"LinkedMDB ZIP CRC failed at {bad}")
            member = "linkedmdb-latest-dump.nt"
            products.append(
                {
                    "dataset": "LinkedMDB-2012",
                    "compressed_path": "linkedmdb/linkedmdb-latest-dump.zip",
                    "archive_member": member,
                    **extract_one(
                        freeze / "extracted" / "linkedmdb" / member,
                        lambda: archive.open(member),
                        args.force,
                    ),
                }
            )

    if "dbpedia" in selected:
        for source_name in (
            "mappingbased-objects_lang=en.ttl.bz2",
            "labels_lang=en.ttl.bz2",
        ):
            relative = f"dbpedia-2022.12/{source_name}"
            source_path = verified_input(freeze, relative, records)
            products.append(
                {
                    "dataset": "DBpedia-2022.12-en",
                    "compressed_path": relative,
                    **extract_one(
                        freeze / "extracted" / "dbpedia-2022.12" / source_name.removesuffix(".bz2"),
                        lambda source_path=source_path: bz2.open(source_path, "rb"),
                        args.force,
                    ),
                }
            )

    if "imdb" in selected:
        for source_name in (
            "title.basics.tsv.gz",
            "title.principals.tsv.gz",
            "name.basics.tsv.gz",
        ):
            relative = f"imdb-20260722/{source_name}"
            source_path = verified_input(freeze, relative, records)
            products.append(
                {
                    "dataset": "IMDb-daily-20260722",
                    "compressed_path": relative,
                    **extract_one(
                        freeze / "extracted" / "imdb-20260722" / source_name.removesuffix(".gz"),
                        lambda source_path=source_path: gzip.open(source_path, "rb"),
                        args.force,
                    ),
                }
            )

    if "wikimovies" in selected:
        relative = "wikimovies/wikimovies.tar.gz"
        archive_path = verified_input(freeze, relative, records)
        destination = freeze / "extracted" / "wikimovies" / "questions.tsv"
        if destination.exists() and not args.force:
            size = destination.stat().st_size
            digest = sha256(destination)
            reused = True
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            temporary = destination.with_suffix(".tsv.tmp")
            members = (
                ("train", "movieqa/questions/wiki_entities/wiki-entities_qa_train.txt"),
                ("dev", "movieqa/questions/wiki_entities/wiki-entities_qa_dev.txt"),
                ("test", "movieqa/questions/wiki_entities/wiki-entities_qa_test.txt"),
            )
            with tarfile.open(archive_path, mode="r:gz") as archive, temporary.open(
                "w", encoding="utf-8", newline="\n"
            ) as output:
                for split, member_name in members:
                    member = archive.extractfile(member_name)
                    if member is None:
                        raise RuntimeError(f"WikiMovies archive is missing {member_name}")
                    for line_number, raw_line in enumerate(member, 1):
                        line = raw_line.decode("utf-8").rstrip("\r\n")
                        question_and_index, separator, _answers = line.partition("\t")
                        _source_index, space, question = question_and_index.partition(" ")
                        if not separator or not space or not question:
                            raise RuntimeError(
                                f"malformed WikiMovies {split} row {line_number}"
                            )
                        output.write(f"{split}:{line_number}\t{question}\n")
            os.replace(temporary, destination)
            size = destination.stat().st_size
            digest = sha256(destination)
            reused = False
        products.append(
            {
                "dataset": "Meta-WikiMovies",
                "compressed_path": relative,
                "archive_members": [
                    "movieqa/questions/wiki_entities/wiki-entities_qa_train.txt",
                    "movieqa/questions/wiki_entities/wiki-entities_qa_dev.txt",
                    "movieqa/questions/wiki_entities/wiki-entities_qa_test.txt",
                ],
                "rows": 116137,
                "path": str(destination.relative_to(ROOT)).replace("\\", "/"),
                "bytes": size,
                "sha256": digest,
                "reused": reused,
            }
        )

    if "aminer" in selected:
        relative = "aminer-dblp-v18/DBLP-Citation-network-V18.zip"
        archive_path = verified_input(freeze, relative, records)
        member = "DBLP-Citation-network-V18.jsonl"
        with ZipFile(archive_path) as archive:
            if member not in archive.namelist():
                raise RuntimeError(f"AMiner ZIP is missing {member}")
            products.append(
                {
                    "dataset": "DBLP-AMiner-V18",
                    "compressed_path": relative,
                    "archive_member": member,
                    **extract_one(
                        freeze / "extracted" / "aminer-dblp-v18" / member,
                        lambda: archive.open(member),
                        args.force,
                    ),
                }
            )

    manifest_path = freeze / "extracted" / "extraction_manifest.json"
    prior: dict[str, object] = {"schema_version": 1, "freeze_id": freeze.name, "products": []}
    if manifest_path.exists():
        prior = json.loads(manifest_path.read_text(encoding="utf-8"))
    merged = {str(record["path"]): record for record in prior.get("products", [])}
    merged.update({str(record["path"]): record for record in products})
    output = {
        "schema_version": 1,
        "freeze_id": freeze.name,
        "products": [merged[key] for key in sorted(merged)],
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = manifest_path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, manifest_path)
    print(f"Manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
