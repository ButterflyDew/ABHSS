#!/usr/bin/env python3
"""Map official keyword-query text to source-label groups in repository format."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_STOPWORDS = ROOT / "tools" / "data" / "english_stopwords.txt"
TOKEN = re.compile(r"[A-Za-z0-9]+")


def tokenize(text: str, stopwords: set[str]) -> list[str]:
    result: list[str] = []
    seen: set[str] = set()
    for match in TOKEN.finditer(text):
        token = match.group(0).lower()
        if token not in stopwords and token not in seen:
            seen.add(token)
            result.append(token)
    return result


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(8 * 1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def read_groups(dataset_dir: Path) -> tuple[dict[str, int], dict[int, list[int]], dict[int, int]]:
    token_to_group: dict[str, int] = {}
    declared_sizes: dict[int, int] = {}
    with (dataset_dir / "source_group_ids.tsv").open(encoding="utf-8", newline="") as source:
        for row in csv.DictReader(source, delimiter="\t"):
            group_id = int(row["group_id"])
            token_to_group[row["token"]] = group_id
            declared_sizes[group_id] = int(row["size"])
    groups: dict[int, list[int]] = {}
    with (dataset_dir / "candidate_groups.txt").open(encoding="utf-8") as source:
        for expected, line in enumerate(source, 1):
            prefix, separator, values = line.partition(":")
            if not separator or prefix != f"g{expected}":
                raise RuntimeError(f"candidate group line {expected} is malformed")
            members = list(map(int, values.split()))
            if len(members) != declared_sizes.get(expected):
                raise RuntimeError(f"candidate group g{expected} size disagrees with source_group_ids.tsv")
            groups[expected] = members
    if len(groups) != len(token_to_group):
        raise RuntimeError("candidate_groups.txt and source_group_ids.tsv have different group counts")
    return token_to_group, groups, declared_sizes


def read_queries(path: Path) -> list[tuple[str, str]]:
    result: list[tuple[str, str]] = []
    with path.open(encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            line = line.rstrip("\r\n")
            if not line:
                continue
            query_id, separator, text = line.partition("\t")
            if not separator or not query_id or not text:
                raise RuntimeError(f"source query line {line_number} is not ID<TAB>text")
            result.append((query_id, text))
    return result


def locate_component_auditor() -> Path:
    candidates = (
        ROOT / "build" / "Release" / "audit_query_feasibility.exe",
        ROOT / "build" / "audit_query_feasibility",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(
        "audit_query_feasibility is required to remove component-infeasible "
        "source queries; build that CMake target first"
    )


def write_pool(
    dataset_dir: Path,
    g: int,
    selected: list[dict[str, object]],
    groups: dict[int, list[int]],
) -> tuple[Path, Path]:
    query_path = dataset_dir / f"query_g{g}.txt"
    group_ids_path = dataset_dir / f"query_g{g}.group_ids.txt"
    query_temporary = query_path.with_suffix(".txt.tmp")
    group_ids_temporary = group_ids_path.with_suffix(".txt.tmp")
    with query_temporary.open("w", encoding="utf-8", newline="\n") as output:
        output.write(f"{len(selected)}\n")
        for record in selected:
            output.write(f"{g}\n")
            for group_id in record["group_ids"]:
                members = groups[int(group_id)]
                output.write(f"{len(members)} {' '.join(map(str, members))}\n")
    with group_ids_temporary.open("w", encoding="utf-8", newline="\n") as output:
        output.write("# protocol=official-source-keyword-query\n")
        output.write("# group_ids=1-based vertex_ids_in_query_file=1-based\n")
        output.write(f"# queries={len(selected)}\n")
        for record in selected:
            output.write(" ".join(map(str, record["group_ids"])))
            output.write("\n")
    os.replace(query_temporary, query_path)
    os.replace(group_ids_temporary, group_ids_path)
    return query_path, group_ids_path


def audit_component_feasibility(
    dataset_dir: Path, grouped: dict[int, list[dict[str, object]]]
) -> tuple[dict[int, set[int]], dict[str, int]]:
    executable = locate_component_auditor()
    audit_path = dataset_dir / ".source_query_component_audit.json"
    query_paths = [dataset_dir / f"query_g{g}.txt" for g in sorted(grouped)]
    completed = subprocess.run(
        [
            str(executable),
            str(dataset_dir / "graph.txt"),
            str(audit_path),
            *(str(path) for path in query_paths),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if completed.returncode not in (0, 2):
        raise RuntimeError(
            "component-feasibility audit failed: " + completed.stderr.strip()
        )
    payload = json.loads(audit_path.read_text(encoding="utf-8"))
    audit_path.unlink()
    excluded: dict[int, set[int]] = {}
    for row in payload["query_files"]:
        match = re.fullmatch(r"query_g(\d+)\.txt", Path(row["path"]).name)
        if match is None:
            raise RuntimeError(f"unexpected component-audit query path: {row['path']}")
        g = int(match.group(1))
        excluded[g] = {int(index) for index in row["infeasible_query_indices"]}
    graph_stats = {
        "connected_components": int(payload["connected_components"]),
        "largest_component_vertices": int(payload["largest_component_vertices"]),
    }
    return excluded, graph_stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("source_queries", type=Path)
    parser.add_argument("--stopwords", type=Path, default=DEFAULT_STOPWORDS)
    parser.add_argument("--minimum-g", type=int, default=1)
    parser.add_argument("--maximum-g", type=int, default=16)
    parser.add_argument(
        "--preprocessed",
        action="store_true",
        help="the source already supplies stopped query text; do not apply the local stopword list",
    )
    args = parser.parse_args()
    dataset_dir = args.dataset_dir.resolve()
    source_queries = args.source_queries.resolve()
    stopwords = set() if args.preprocessed else {
        line.strip()
        for line in args.stopwords.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.startswith("#")
    }
    token_to_group, groups, sizes = read_groups(dataset_dir)
    records: list[dict[str, object]] = []
    grouped: dict[int, list[dict[str, object]]] = {}
    for source_index, (query_id, text) in enumerate(read_queries(source_queries), 1):
        normalized = tokenize(text, stopwords)
        retained = [token for token in normalized if token in token_to_group]
        missing = [token for token in normalized if token not in token_to_group]
        group_ids = [token_to_group[token] for token in retained]
        record: dict[str, object] = {
            "source_query_index": source_index,
            "source_query_id": query_id,
            "source_text": text,
            "normalized_tokens": normalized,
            "retained_tokens": retained,
            "missing_tokens": missing,
            "group_ids": group_ids,
            "g": len(group_ids),
            "group_sizes": [sizes[group_id] for group_id in group_ids],
        }
        records.append(record)
        if args.minimum_g <= len(group_ids) <= args.maximum_g:
            grouped.setdefault(len(group_ids), []).append(record)

    mapped_count = sum(len(rows) for rows in grouped.values())
    for g, selected in sorted(grouped.items()):
        write_pool(dataset_dir, g, selected, groups)

    excluded_by_g, component_stats = audit_component_feasibility(dataset_dir, grouped)
    excluded_records: list[dict[str, object]] = []
    generated: list[dict[str, object]] = []
    for g in sorted(grouped):
        mapped = grouped[g]
        excluded_indices = excluded_by_g.get(g, set())
        selected: list[dict[str, object]] = []
        for pool_index, record in enumerate(mapped, 1):
            feasible = pool_index not in excluded_indices
            record["component_feasible"] = feasible
            record["mapped_pool_query_index"] = pool_index
            if feasible:
                selected.append(record)
            else:
                excluded_records.append(
                    {
                        "g": g,
                        "mapped_pool_query_index": pool_index,
                        "source_query_id": record["source_query_id"],
                        "source_query_index": record["source_query_index"],
                        "source_text": record["source_text"],
                        "reason": "no graph connected component intersects every retained keyword group",
                    }
                )
        query_path, group_ids_path = write_pool(dataset_dir, g, selected, groups)
        generated.append(
            {
                "g": g,
                "queries": len(selected),
                "query_path": str(query_path.relative_to(ROOT)).replace("\\", "/"),
                "query_sha256": sha256(query_path),
                "group_ids_path": str(group_ids_path.relative_to(ROOT)).replace("\\", "/"),
                "group_ids_sha256": sha256(group_ids_path),
            }
        )
        print(
            f"g={g}: mapped={len(mapped)} component-feasible={len(selected)} "
            f"excluded={len(excluded_indices)}"
        )

    manifest = {
        "schema_version": 1,
        "protocol": (
            "officially preprocessed query text -> lower-case ASCII alphanumeric tokens -> exact rdfs:label token groups"
            if args.preprocessed
            else "official source query text -> lower-case ASCII alphanumeric tokens -> frozen stopword removal -> exact rdfs:label token groups"
        ),
        "source_query_path": str(source_queries.relative_to(ROOT)).replace("\\", "/"),
        "source_query_sha256": sha256(source_queries),
        "stopword_path": str(args.stopwords.resolve().relative_to(ROOT)).replace("\\", "/"),
        "stopword_sha256": sha256(args.stopwords.resolve()),
        "source_text_is_officially_preprocessed": args.preprocessed,
        "source_queries": len(records),
        "mapped_queries_in_requested_g_range": mapped_count,
        "feasible_queries_in_requested_g_range": mapped_count - len(excluded_records),
        "component_feasibility_rule": "retain a query iff at least one graph connected component intersects every retained keyword group",
        "component_audit": component_stats,
        "component_infeasible_queries": excluded_records,
        "generated_pools": generated,
        "queries": records,
    }
    path = dataset_dir / "source_query_manifest.json"
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
