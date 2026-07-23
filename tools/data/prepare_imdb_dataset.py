#!/usr/bin/env python3
"""Rebuild and verify the frozen IMDb graph from already frozen raw files."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "data_sources" / "official" / "official-latest-20260722"
OUTPUT = ROOT / "data" / "official-latest-20260722" / "imdb-20260722"


def sha256(path: Path) -> str:
    """以 8 MiB 块计算文件 SHA-256，避免读取整个大图到内存。"""
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def locate_executable(requested: Path | None) -> Path:
    """定位显式或默认构建目录中的 IMDb C++ 转换器。"""
    candidates = []
    if requested is not None:
        candidates.append(requested if requested.is_absolute() else ROOT / requested)
    candidates.extend((
        ROOT / "build" / "Release" / "build_imdb_graph.exe",
        ROOT / "build" / "build_imdb_graph.exe",
        ROOT / "build" / "build_imdb_graph",
    ))
    for path in candidates:
        if path.is_file():
            return path.resolve()
    raise FileNotFoundError("build_imdb_graph is missing; build its CMake target first")


def main() -> int:
    """展开冻结 raw、调用 C++ 构图并核对最终接口身份。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--replace-existing", action="store_true")
    parser.add_argument(
        "--executable",
        type=Path,
        help="explicit build_imdb_graph path when using a non-default build directory",
    )
    args = parser.parse_args()
    subprocess.run(
        [sys.executable, str(ROOT / "tools" / "data" / "extract_imdb_source.py")],
        cwd=ROOT,
        check=True,
    )
    extracted = FREEZE / "extracted" / "imdb-20260722"
    command = [
        str(locate_executable(args.executable)),
        str(extracted / "title.basics.tsv"),
        str(extracted / "title.principals.tsv"),
        str(extracted / "name.basics.tsv"),
        str(OUTPUT),
        "50",
        "4800",
    ]
    if args.replace_existing:
        command.append("--replace-existing")
    subprocess.run(command, cwd=ROOT, check=True)

    manifest = json.loads((OUTPUT / "dataset_manifest.json").read_text(encoding="utf-8"))
    expected = manifest["graph"]
    graph = OUTPUT / "graph.txt"
    if graph.stat().st_size != int(expected["bytes"]) or sha256(graph) != expected["sha256"]:
        raise RuntimeError("rebuilt IMDb graph does not match the frozen interface identity")
    print("IMDb graph matches the 2026-07-22 frozen interface")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
