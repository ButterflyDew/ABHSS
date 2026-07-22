#!/usr/bin/env python3
"""Build the pinned third-party CPU baselines used by the paper.

The script intentionally keeps every external build out of the main CMake
tree.  It also normalizes environment-key casing because Windows/MSBuild
rejects an inherited environment containing both ``Path`` and ``PATH``.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
EXTERNAL_BUILD = ROOT / "build_external"
SOPLEX_SOURCE = ROOT / "third_party" / "soplex-5.0.2"
SOPLEX_BUILD = EXTERNAL_BUILD / "soplex-5.0.2"
SOPLEX_INSTALL = EXTERNAL_BUILD / "install" / "soplex-5.0.2"
SCIP_SOURCE = ROOT / "third_party" / "scip-jack-7.0.3"
SCIP_BUILD = EXTERNAL_BUILD / "scip-jack-7.0.3"
OFFICIAL_PRUNEDDP_SOURCE = (
    ROOT / "third_party" / "GPU4GST-sigmod" / "code" / "PrunedDP++"
)
OFFICIAL_PRUNEDDP_BUILD = EXTERNAL_BUILD / "gpu4gst-pruneddp"
BOOST_ROOT = ROOT / "third_party" / "boost_1_85_0"


def clean_environment() -> dict[str, str]:
    """Return a case-insensitively de-duplicated environment for Windows."""
    cleaned: dict[str, str] = {}
    spelling: dict[str, str] = {}
    for key, value in os.environ.items():
        folded = key.casefold()
        previous = spelling.get(folded)
        if previous is not None:
            cleaned.pop(previous, None)
        canonical = "Path" if folded == "path" else key
        spelling[folded] = canonical
        cleaned[canonical] = value
    return cleaned


def run(command: list[str]) -> None:
    print("+", subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=clean_environment(), check=True)


def configure_soplex(generator: str, architecture: str) -> None:
    run(
        [
            "cmake",
            "-S",
            str(SOPLEX_SOURCE),
            "-B",
            str(SOPLEX_BUILD),
            "-G",
            generator,
            "-A",
            architecture,
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
            f"-DCMAKE_INSTALL_PREFIX={SOPLEX_INSTALL}",
            "-DGMP=OFF",
            "-DBOOST=OFF",
            "-DBUILD_TESTING=OFF",
        ]
    )


def build_soplex() -> None:
    run(
        [
            "cmake",
            "--build",
            str(SOPLEX_BUILD),
            "--config",
            "Release",
            "--target",
            "install",
        ]
    )


def configure_scip_jack(generator: str, architecture: str) -> None:
    soplex_config = SOPLEX_INSTALL / "lib" / "cmake" / "soplex"
    if not (soplex_config / "soplex-config.cmake").exists():
        raise FileNotFoundError(
            "SoPlex is not installed. Run this script with --target soplex first."
        )
    run(
        [
            "cmake",
            "-S",
            str(SCIP_SOURCE),
            "-B",
            str(SCIP_BUILD),
            "-G",
            generator,
            "-A",
            architecture,
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
            f"-DSOPLEX_DIR={soplex_config}",
            "-DLPS=spx",
            "-DZLIB=OFF",
            "-DREADLINE=OFF",
            "-DGMP=OFF",
            "-DPAPILO=OFF",
            "-DZIMPL=OFF",
            "-DIPOPT=OFF",
            "-DSYM=none",
            # SCIP 7.0.3 places all applications, including SCIP-Jack, behind
            # this historical switch.  Building target scipstp does not run
            # or build the unrelated tests.
            "-DBUILD_TESTING=ON",
        ]
    )


def build_scip_jack() -> None:
    run(
        [
            "cmake",
            "--build",
            str(SCIP_BUILD),
            "--config",
            "Release",
            "--target",
            "scipstp",
        ]
    )


def configure_official_pruneddp(generator: str, architecture: str) -> None:
    if not (BOOST_ROOT / "boost" / "heap" / "fibonacci_heap.hpp").exists():
        raise FileNotFoundError(
            "Pinned Boost headers are missing. See third_party/README.md."
        )
    run(
        [
            "cmake",
            "-S",
            str(OFFICIAL_PRUNEDDP_SOURCE),
            "-B",
            str(OFFICIAL_PRUNEDDP_BUILD),
            "-G",
            generator,
            "-A",
            architecture,
            f"-DBOOST_ROOT={BOOST_ROOT}",
            f"-DBoost_INCLUDE_DIR={BOOST_ROOT}",
            "-DBoost_NO_SYSTEM_PATHS=ON",
        ]
    )


def build_official_pruneddp() -> None:
    run(
        [
            "cmake",
            "--build",
            str(OFFICIAL_PRUNEDDP_BUILD),
            "--config",
            "Release",
        ]
    )


def remove_builds(target: str) -> None:
    paths = {
        "soplex": [SOPLEX_BUILD, SOPLEX_INSTALL],
        "scip-jack": [SCIP_BUILD],
        "official-pruneddp": [OFFICIAL_PRUNEDDP_BUILD],
        "all": [
            SOPLEX_BUILD,
            SOPLEX_INSTALL,
            SCIP_BUILD,
            OFFICIAL_PRUNEDDP_BUILD,
        ],
    }[target]
    root = EXTERNAL_BUILD.resolve()
    for path in paths:
        resolved = path.resolve()
        if root not in resolved.parents:
            raise RuntimeError(f"Refusing to clean outside {root}: {resolved}")
        if resolved.exists():
            print(f"Removing {resolved}")
            shutil.rmtree(resolved)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--target",
        choices=("soplex", "scip-jack", "official-pruneddp", "all"),
        default="all",
    )
    parser.add_argument(
        "--generator", default="Visual Studio 17 2022", help="CMake generator"
    )
    parser.add_argument("--architecture", default="x64")
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    if args.clean:
        remove_builds(args.target)

    if args.target in ("soplex", "all"):
        configure_soplex(args.generator, args.architecture)
        build_soplex()
    if args.target in ("scip-jack", "all"):
        configure_scip_jack(args.generator, args.architecture)
        build_scip_jack()
    if args.target in ("official-pruneddp", "all"):
        configure_official_pruneddp(args.generator, args.architecture)
        build_official_pruneddp()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode) from error
