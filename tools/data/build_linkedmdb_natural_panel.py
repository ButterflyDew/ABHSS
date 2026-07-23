#!/usr/bin/env python3
"""Build the fixed 200-query LinkedMDB natural panel from Meta WikiMovies.

The practical-paper budget of 200 questions is retained, but the panel is
stratified before any solver is run so that every naturally occurring g=1..12
stratum is represented.  All rare g>=9 mappings are retained.
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

from build_rdf_query_panels import (
    DEFAULT_STOPWORDS,
    locate_component_auditor,
    read_groups,
    read_queries,
    sha256,
    tokenize,
)


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DATASET = ROOT / "data" / "official-latest-20260722" / "linkedmdb"
DEFAULT_SOURCE = (
    ROOT
    / "data_sources"
    / "official"
    / "official-latest-20260722"
    / "extracted"
    / "wikimovies"
    / "questions.tsv"
)
DEFAULT_OUTPUT = ROOT / "experiment_data" / "official_latest" / "natural" / "linkedmdb"
DEFAULT_QUOTAS = {1: 20, 2: 20, 3: 20, 4: 20, 5: 20, 6: 20, 7: 20, 8: 25, 9: 26, 10: 6, 11: 2, 12: 1}


def stable_rank(seed: int, record: dict[str, object]) -> str:
    payload = f"{seed}\0{record['source_query_id']}\0{record['source_text']}".encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def write_queries(
    output_root: Path,
    selected: dict[int, list[dict[str, object]]],
    groups: dict[int, list[int]],
) -> list[Path]:
    paths: list[Path] = []
    output_root.mkdir(parents=True, exist_ok=True)
    for g, rows in sorted(selected.items()):
        query_path = output_root / f"query_g{g}.txt"
        ids_path = output_root / f"query_g{g}.group_ids.txt"
        query_temporary = query_path.with_suffix(".txt.tmp")
        ids_temporary = ids_path.with_suffix(".txt.tmp")
        with query_temporary.open("w", encoding="utf-8", newline="\n") as queries, ids_temporary.open(
            "w", encoding="utf-8", newline="\n"
        ) as identifiers:
            queries.write(f"{len(rows)}\n")
            identifiers.write("# protocol=Meta-WikiMovies-natural-stratified-200\n")
            identifiers.write("# group_ids=source LinkedMDB label groups\n")
            for row in rows:
                queries.write(f"{g}\n")
                identifiers.write(" ".join(map(str, row["group_ids"])) + "\n")
                for group_id in row["group_ids"]:
                    members = groups[int(group_id)]
                    queries.write(f"{len(members)} {' '.join(map(str, members))}\n")
        os.replace(query_temporary, query_path)
        os.replace(ids_temporary, ids_path)
        paths.append(query_path)
    return paths


def component_infeasible(
    dataset_dir: Path,
    output_root: Path,
    query_paths: list[Path],
) -> tuple[dict[int, set[int]], dict[str, int]]:
    audit_path = output_root / ".component_audit.json"
    completed = subprocess.run(
        [
            str(locate_component_auditor()),
            str(dataset_dir / "graph.txt"),
            str(audit_path),
            *(str(path) for path in query_paths),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if completed.returncode not in (0, 2):
        raise RuntimeError("component-feasibility audit failed: " + completed.stderr.strip())
    payload = json.loads(audit_path.read_text(encoding="utf-8"))
    audit_path.unlink()
    result: dict[int, set[int]] = {}
    for row in payload["query_files"]:
        match = re.fullmatch(r"query_g(\d+)\.txt", Path(row["path"]).name)
        if match is None:
            raise RuntimeError(f"unexpected audit path {row['path']}")
        result[int(match.group(1))] = set(map(int, row["infeasible_query_indices"]))
    return result, {
        "connected_components": int(payload["connected_components"]),
        "largest_component_vertices": int(payload["largest_component_vertices"]),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-dir", type=Path, default=DEFAULT_DATASET)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--stopwords", type=Path, default=DEFAULT_STOPWORDS)
    parser.add_argument("--seed", type=int, default=20260722)
    args = parser.parse_args()
    dataset_dir = args.dataset_dir.resolve()
    source_path = args.source.resolve()
    output_root = args.output.resolve()
    stopwords = {
        line.strip()
        for line in args.stopwords.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.startswith("#")
    }
    token_to_group, groups, sizes = read_groups(dataset_dir)
    source_counts: Counter[int] = Counter()
    candidates: dict[int, list[dict[str, object]]] = {g: [] for g in DEFAULT_QUOTAS}
    seen_group_sets: dict[int, set[tuple[int, ...]]] = {g: set() for g in DEFAULT_QUOTAS}
    source_queries = read_queries(source_path)
    for source_index, (query_id, text) in enumerate(source_queries, 1):
        normalized = tokenize(text, stopwords)
        retained = [token for token in normalized if token in token_to_group]
        group_ids = tuple(token_to_group[token] for token in retained)
        g = len(group_ids)
        source_counts[g] += 1
        if g not in candidates or group_ids in seen_group_sets[g]:
            continue
        seen_group_sets[g].add(group_ids)
        record: dict[str, object] = {
            "source_query_index": source_index,
            "source_query_id": query_id,
            "source_text": text,
            "normalized_tokens": normalized,
            "retained_tokens": retained,
            "group_ids": list(group_ids),
            "group_sizes": [sizes[group_id] for group_id in group_ids],
            "g": g,
        }
        record["stable_rank"] = stable_rank(args.seed, record)
        candidates[g].append(record)

    for g in candidates:
        candidates[g].sort(key=lambda row: (str(row["stable_rank"]), int(row["source_query_index"])))
        if len(candidates[g]) < DEFAULT_QUOTAS[g]:
            raise RuntimeError(
                f"g={g} has only {len(candidates[g])} unique mappings for quota {DEFAULT_QUOTAS[g]}"
            )

    cursor = {g: DEFAULT_QUOTAS[g] for g in DEFAULT_QUOTAS}
    selected = {g: rows[: DEFAULT_QUOTAS[g]] for g, rows in candidates.items()}
    excluded: list[dict[str, object]] = []
    component_stats: dict[str, int] = {}
    for _round in range(10):
        query_paths = write_queries(output_root, selected, groups)
        infeasible, component_stats = component_infeasible(dataset_dir, output_root, query_paths)
        if not any(infeasible.values()):
            break
        for g, indices in infeasible.items():
            retained_rows: list[dict[str, object]] = []
            for query_index, row in enumerate(selected[g], 1):
                if query_index in indices:
                    excluded.append(
                        {
                            "g": g,
                            "source_query_id": row["source_query_id"],
                            "source_query_index": row["source_query_index"],
                            "reason": "no connected component intersects every mapped group",
                        }
                    )
                else:
                    retained_rows.append(row)
            while len(retained_rows) < DEFAULT_QUOTAS[g]:
                if cursor[g] >= len(candidates[g]):
                    raise RuntimeError(f"exhausted component-feasible replacements for g={g}")
                retained_rows.append(candidates[g][cursor[g]])
                cursor[g] += 1
            selected[g] = retained_rows
    else:
        raise RuntimeError("component-feasibility replacement did not converge")

    query_paths = write_queries(output_root, selected, groups)
    cells: list[dict[str, object]] = []
    for query_path in query_paths:
        g = int(re.fullmatch(r"query_g(\d+)\.txt", query_path.name).group(1))
        cells.append(
            {
                "dataset": "LinkedMDB-2012",
                "graph_path": str(dataset_dir.relative_to(ROOT)).replace("\\", "/"),
                "query_path": str(query_path.relative_to(ROOT)).replace("\\", "/"),
                "query_sha256": sha256(query_path),
                "g": g,
                "queries": len(selected[g]),
                "source_rows_with_g": source_counts[g],
                "unique_mapped_group_sets_with_g": len(candidates[g]),
                "selection": "all rare g>=9 rows; stable-hash quota for g<=8; component-infeasible rows replaced without solver feedback",
            }
        )
    manifest = {
        "schema_version": 1,
        "dataset": "LinkedMDB-2012",
        "source": str(source_path.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": sha256(source_path),
        "source_queries": len(source_queries),
        "stopwords": str(args.stopwords.resolve().relative_to(ROOT)).replace("\\", "/"),
        "stopwords_sha256": sha256(args.stopwords.resolve()),
        "seed": args.seed,
        "quotas": DEFAULT_QUOTAS,
        "source_g_counts": dict(sorted(source_counts.items())),
        "unique_mapping_counts": {g: len(candidates[g]) for g in sorted(candidates)},
        "selected_queries": sum(map(len, selected.values())),
        "component_audit": component_stats,
        "component_infeasible_replacements": excluded,
        "selection_note": "The 200-query paper budget is retained. Every naturally occurring g=1..12 stratum is primary; all 35 unique g=9..12 mappings are retained, while g=1..8 use pre-run stable-hash quotas.",
        "queries": [row for g in sorted(selected) for row in selected[g]],
    }
    for name, payload in (("cells.json", cells), ("panel_manifest.json", manifest)):
        path = output_root / name
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        os.replace(temporary, path)
    print(f"Built {len(cells)} LinkedMDB cells / {manifest['selected_queries']} queries")
    print("source g counts:", dict(sorted(source_counts.items())))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
