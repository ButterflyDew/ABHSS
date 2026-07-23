#!/usr/bin/env python3
"""Rebuild the official 2026-07-22 paper-data freeze end to end.

Large products are deliberately not committed.  This supervisor invokes the
small, versioned converters in the only supported order and leaves every
identity decision to the tracked catalog/manifests.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
FREEZE_ID = "official-latest-20260722"
FREEZE = ROOT / "data_sources" / "official" / FREEZE_ID
EXTRACTED = FREEZE / "extracted"
WORK = FREEZE / "work"
OUTPUT = ROOT / "data" / FREEZE_ID
STOPWORDS = ROOT / "tools" / "data" / "english_stopwords.txt"
SNAP = (
    ("SNAP-Wikipedia-2018", "snap-wikipedia-2018"),
    ("SNAP-Twitch-2018", "snap-twitch-2018"),
    ("SNAP-GitHub-2019", "snap-github-2019"),
    ("SNAP-YouTube", "snap-youtube"),
    ("SNAP-Orkut", "snap-orkut"),
    ("SNAP-LiveJournal", "snap-livejournal"),
)
RELATED = tuple(slug for _dataset, slug in SNAP) + ("movielens-32m", "toronto-current")


def binary(name: str) -> Path:
    candidates = (
        ROOT / "build" / "Release" / f"{name}.exe",
        ROOT / "build" / name,
        ROOT / "build" / f"{name}.exe",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise RuntimeError(
        f"missing {name}; configure a Release build and compile the data-builder targets first"
    )


def run(arguments: list[str | Path]) -> None:
    command = [str(argument) for argument in arguments]
    print("+ " + subprocess.list2cmdline(command), flush=True)
    completed = subprocess.run(command, cwd=ROOT, env=os.environ.copy())
    if completed.returncode:
        raise RuntimeError(f"command failed with exit code {completed.returncode}")


def download() -> None:
    run(
        [
            sys.executable,
            ROOT / "tools" / "data" / "download_official_sources.py",
            "--rehash",
        ]
    )


def extract() -> None:
    run([sys.executable, ROOT / "tools" / "data" / "extract_movielens32m.py"])
    run([sys.executable, ROOT / "tools" / "data" / "extract_extended_sources.py"])


def build_interfaces(jobs: int) -> None:
    normalize = [
        sys.executable,
        ROOT / "tools" / "data" / "prepare_official_snap.py",
    ]
    for dataset, _slug in SNAP:
        normalize.extend(["--dataset", dataset])
    run(normalize)

    jaccard = binary("build_jaccard_graph")
    for _dataset, slug in SNAP:
        graph = OUTPUT / slug / "graph.txt"
        if not graph.exists():
            run([jaccard, WORK / slug / "edges.txt", graph, "--threads", str(jobs)])
        else:
            print(f"= reuse {graph.relative_to(ROOT)}", flush=True)

    movielens = OUTPUT / "movielens-32m"
    if not (movielens / "graph.txt").exists():
        run(
            [
                binary("build_movielens_graph"),
                EXTRACTED / "movielens-32m" / "ratings.csv",
                EXTRACTED / "movielens-32m" / "movies.csv",
                movielens / "graph.txt",
                movielens / "candidate_groups.txt",
            ]
        )
    else:
        print(f"= reuse {(movielens / 'graph.txt').relative_to(ROOT)}", flush=True)

    rdf_builder = binary("build_rdf_graph")
    linkedmdb = OUTPUT / "linkedmdb"
    linked_source = EXTRACTED / "linkedmdb" / "linkedmdb-latest-dump.nt"
    if not (linkedmdb / "graph.txt").exists():
        run([rdf_builder, linked_source, linked_source, linkedmdb, STOPWORDS])
    else:
        print(f"= reuse {(linkedmdb / 'graph.txt').relative_to(ROOT)}", flush=True)

    dbpedia = OUTPUT / "dbpedia-2022.12-en"
    if not (dbpedia / "graph.txt").exists():
        run(
            [
                rdf_builder,
                EXTRACTED / "dbpedia-2022.12" / "mappingbased-objects_lang=en.ttl",
                EXTRACTED / "dbpedia-2022.12" / "labels_lang=en.ttl",
                dbpedia,
                STOPWORDS,
                FREEZE / "raw" / "dbpedia-entity-v2" / "queries-v2_stopped.txt",
            ]
        )
    else:
        print(f"= reuse {(dbpedia / 'graph.txt').relative_to(ROOT)}", flush=True)

    imdb = OUTPUT / "imdb-20260722"
    if not (imdb / "graph.txt").exists():
        run(
            [
                binary("build_imdb_graph"),
                EXTRACTED / "imdb-20260722" / "title.basics.tsv",
                EXTRACTED / "imdb-20260722" / "title.principals.tsv",
                EXTRACTED / "imdb-20260722" / "name.basics.tsv",
                imdb,
                "50",
                "4800",
            ]
        )
    else:
        print(f"= reuse {(imdb / 'graph.txt').relative_to(ROOT)}", flush=True)

    dblp = OUTPUT / "dblp-aminer-v18"
    if not (dblp / "graph.txt").exists():
        run(
            [
                binary("build_aminer_dblp_graph"),
                EXTRACTED / "aminer-dblp-v18" / "DBLP-Citation-network-V18.jsonl",
                dblp,
                STOPWORDS,
                "50",
                "4800",
            ]
        )
    else:
        print(f"= reuse {(dblp / 'graph.txt').relative_to(ROOT)}", flush=True)

    toronto = OUTPUT / "toronto-current"
    if not (toronto / "graph.txt").exists():
        run([sys.executable, ROOT / "tools" / "data" / "build_toronto_graph.py"])
    else:
        print(f"= reuse {(toronto / 'graph.txt').relative_to(ROOT)}", flush=True)


def build_queries() -> None:
    related_builder = binary("prepare_gpu4gst")
    for slug in RELATED:
        dataset_dir = OUTPUT / slug
        run(
            [
                related_builder,
                "--generic",
                slug,
                dataset_dir / "graph.txt",
                dataset_dir / "candidate_groups.txt",
                dataset_dir,
                "--seed",
                "20260722",
                "--queries",
                "300",
                "--min-g",
                "4",
                "--max-g",
                "16",
            ]
        )

    run(
        [
            sys.executable,
            ROOT / "tools" / "data" / "build_rdf_query_panels.py",
            OUTPUT / "dbpedia-2022.12-en",
            FREEZE / "raw" / "dbpedia-entity-v2" / "queries-v2_stopped.txt",
            "--preprocessed",
        ]
    )
    label_generator = ROOT / "tools" / "data" / "generate_label_frequency_queries.py"
    run(
        [
            sys.executable,
            label_generator,
            "--dataset-dir",
            OUTPUT / "dblp-aminer-v18",
            "--output",
            ROOT / "experiment_data" / "official_latest" / "controlled_labels" / "dblp",
            "--dataset-name",
            "DBLP-AMiner-V18",
            "--file-prefix",
            "dblp",
            "--queries-per-cell",
            "10",
            "--seed",
            "20260722",
        ]
    )
    run(
        [
            sys.executable,
            label_generator,
            "--dataset-dir",
            OUTPUT / "imdb-20260722",
            "--output",
            ROOT / "experiment_data" / "official_latest" / "controlled_labels" / "imdb",
            "--dataset-name",
            "IMDb-daily-20260722",
            "--file-prefix",
            "imdb",
            "--queries-per-cell",
            "10",
            "--seed",
            "20260722",
        ]
    )
    run([sys.executable, ROOT / "tools" / "data" / "build_official_query_panels.py"])
    run([sys.executable, ROOT / "tools" / "data" / "build_linkedmdb_natural_panel.py"])


def finalize() -> None:
    finalizer = ROOT / "tools" / "data" / "finalize_official_dataset.py"
    for directory in sorted(path for path in OUTPUT.iterdir() if path.is_dir()):
        run([sys.executable, finalizer, directory])
    run([sys.executable, ROOT / "tools" / "data" / "build_official_snapshot.py"])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--stage",
        action="append",
        choices=("download", "extract", "interfaces", "queries", "finalize"),
        help="repeat to run selected stages; default runs all stages in dependency order",
    )
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    args = parser.parse_args()
    selected = set(args.stage or ("download", "extract", "interfaces", "queries", "finalize"))
    for name, action in (
        ("download", download),
        ("extract", extract),
        ("interfaces", lambda: build_interfaces(args.jobs)),
        ("queries", build_queries),
        ("finalize", finalize),
    ):
        if name in selected:
            print(f"\n== {name} ==", flush=True)
            action()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
