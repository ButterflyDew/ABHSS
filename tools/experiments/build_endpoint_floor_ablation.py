#!/usr/bin/env python3
"""Build the preregistered farthest-only A1 ablation in an isolated source tree."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import os


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = ROOT / "build-ablation-no-endpoint-floor"
OLD = "return std::max(FarthestRemaining(p, vertex, continuation), p.tour.EndpointFloorAt(vertex, continuation, p.group_distance));"
NEW = "return FarthestRemaining(p, vertex, continuation);"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def run(command: list[str], cwd: Path) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    output = args.output if args.output.is_absolute() else ROOT / args.output
    source_text = (ROOT / "src" / "abhss" / "core.cpp").read_text(encoding="utf-8")
    if source_text.count(OLD) != 1 or NEW in source_text:
        raise RuntimeError("production source does not contain exactly the preregistered endpoint-floor expression")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if args.dry_run:
        print(json.dumps({"commit": commit, "old_occurrences": source_text.count(OLD), "new_occurrences": source_text.count(NEW), "output": str(output)}, indent=2))
        return 0

    metadata_path = output / "ablation_build.json"
    binary_path = output / "abhss"
    if metadata_path.exists() and binary_path.exists():
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        if metadata.get("source_commit") != commit or metadata.get("only_semantic_change", {}).get("from") != OLD or metadata.get("only_semantic_change", {}).get("to") != NEW:
            raise RuntimeError(f"existing {output} belongs to another source identity")
        if sha256_file(binary_path) != metadata.get("binary_sha256"):
            raise RuntimeError("existing ablation binary hash does not match its metadata")
        print(f"Reusing validated endpoint-floor ablation binary: {binary_path}")
        return 0
    if output.exists():
        raise RuntimeError(f"refusing to overwrite incomplete or unrecognized directory: {output}")

    staging = output.with_name(output.name + f".staging-{os.getpid()}")
    if staging.exists():
        raise RuntimeError(f"staging directory already exists: {staging}")
    staging.mkdir(parents=True)
    archive = staging / "source.tar"
    run(["git", "archive", "--format=tar", "-o", str(archive), commit], ROOT)
    source = staging / "source"
    source.mkdir()
    with tarfile.open(archive) as package:
        package.extractall(source)
    archive.unlink()

    core = source / "src" / "abhss" / "core.cpp"
    before = core.read_text(encoding="utf-8")
    if before.count(OLD) != 1 or NEW in before:
        raise RuntimeError("archived source does not match the registered one-line edit")
    after = before.replace(OLD, NEW)
    core.write_text(after, encoding="utf-8")
    if after.count(OLD) != 0 or after.count(NEW) != 1:
        raise RuntimeError("isolated source edit did not produce exactly one farthest-only expression")

    build = source / "build"
    run(["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"], ROOT)
    run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], ROOT)
    run(["ctest", "--test-dir", str(build), "--output-on-failure"], ROOT)
    built = build / "abhss"
    if not built.exists():
        raise RuntimeError("isolated build did not produce abhss")
    shutil.copy2(built, staging / "abhss")
    metadata = {
        "schema_version": 1,
        "built_at": datetime.now(timezone.utc).isoformat(),
        "source_commit": commit,
        "production_binary_sha256": sha256_file(ROOT / "build" / "abhss"),
        "binary_sha256": sha256_file(staging / "abhss"),
        "source_file": "src/abhss/core.cpp",
        "source_before_sha256": hashlib.sha256(before.encode()).hexdigest(),
        "source_after_sha256": hashlib.sha256(after.encode()).hexdigest(),
        "only_semantic_change": {"from": OLD, "to": NEW},
        "runtime_switch": False,
        "tests": "all repository CTest targets passed in the isolated source tree",
    }
    (staging / "ablation_build.json").write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    staging.rename(output)
    print(f"Built endpoint-floor ablation: {output / 'abhss'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
