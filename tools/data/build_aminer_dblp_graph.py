#!/usr/bin/env python3
"""Build the official AMiner V18 heterogeneous DBLP keyword-search graph.

Vertices are papers and authors.  Positive unit edges encode authorship and
in-snapshot citations.  Candidate labels are source text tokens whose realized
frequency can support the controlled f panel.  The 15.9 GB JSONL member is
streamed directly from the frozen ZIP twice and is never extracted to disk.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import time
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "data_sources" / "official" / "official-latest-20260722"
DEFAULT_ARCHIVE = FREEZE / "raw" / "aminer-dblp-v18" / "DBLP-Citation-network-V18.zip"
DEFAULT_OUTPUT = ROOT / "data" / "official-latest-20260722" / "dblp-aminer-v18"
DEFAULT_STOPWORDS = ROOT / "tools" / "data" / "english_stopwords.txt"
ARCHIVE_MEMBER = "DBLP-Citation-network-V18.jsonl"
TOKEN = re.compile(r"[A-Za-z0-9]+")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(8 * 1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def atomic_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def text_tokens(values: list[object], stopwords: set[str]) -> set[str]:
    result: set[str] = set()
    for value in values:
        if not isinstance(value, str):
            continue
        for match in TOKEN.finditer(value):
            token = match.group(0).lower()
            if len(token) >= 2 and token not in stopwords:
                result.add(token)
    return result


def paper_tokens(record: dict[str, object], stopwords: set[str]) -> set[str]:
    values: list[object] = [
        record.get("title"),
        record.get("venue"),
        record.get("doc_type"),
    ]
    keywords = record.get("keywords")
    if isinstance(keywords, list):
        values.extend(keywords)
    return text_tokens(values, stopwords)


def author_key(author: dict[str, object]) -> tuple[str | None, str]:
    source_id = author.get("id")
    if isinstance(source_id, str) and source_id.strip():
        return "id:" + source_id.strip(), "source_id"
    name = author.get("name")
    org = author.get("org")
    normalized_name = " ".join(name.lower().split()) if isinstance(name, str) else ""
    normalized_org = " ".join(org.lower().split()) if isinstance(org, str) else ""
    if not normalized_name and not normalized_org:
        return None, "missing"
    return f"fallback:{normalized_name}\x1f{normalized_org}", "name_org_fallback"


def progress(label: str, rows: int, started: float) -> None:
    elapsed = max(time.monotonic() - started, 0.001)
    print(f"[{label}] {rows:,} rows ({rows / elapsed:,.0f} rows/s)", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--stopwords", type=Path, default=DEFAULT_STOPWORDS)
    parser.add_argument("--minimum-frequency", type=int, default=100)
    parser.add_argument("--maximum-frequency", type=int, default=2400)
    parser.add_argument("--progress-rows", type=int, default=250000)
    args = parser.parse_args()
    archive_path = args.archive.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    stopwords = {
        line.strip()
        for line in args.stopwords.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.startswith("#")
    }
    cap = args.maximum_frequency + 1
    paper_ids: dict[str, int] = {}
    author_ids: dict[str, int] = {}
    label_counts: dict[str, int] = {}
    vertex_count = 0
    statistics = {
        "paper_records": 0,
        "authors_with_source_id": 0,
        "authors_with_name_org_fallback": 0,
        "authors_missing_identity": 0,
        "duplicate_authors_within_paper": 0,
        "authorship_edges": 0,
        "citation_edges": 0,
        "references_outside_snapshot": 0,
        "duplicate_references_within_paper": 0,
    }

    graph_path = output / "graph.txt"
    graph_temporary = graph_path.with_suffix(".txt.tmp")
    header_width = 20
    started = time.monotonic()
    with ZipFile(archive_path) as archive, archive.open(ARCHIVE_MEMBER) as source, graph_temporary.open(
        "w+", encoding="utf-8", newline="\n", buffering=8 * 1024 * 1024
    ) as graph:
        graph.write(f"{0:0{header_width}d} {0:0{header_width}d}\n")
        for line_number, raw_line in enumerate(source, 1):
            record = json.loads(raw_line)
            source_paper_id = record.get("id")
            if not isinstance(source_paper_id, str) or not source_paper_id:
                raise RuntimeError(f"paper row {line_number} has no string id")
            if source_paper_id in paper_ids:
                raise RuntimeError(f"duplicate paper id {source_paper_id!r} at row {line_number}")
            vertex_count += 1
            paper_vertex = vertex_count
            paper_ids[source_paper_id] = paper_vertex
            for token in paper_tokens(record, stopwords):
                label_counts[token] = min(cap, label_counts.get(token, 0) + 1)

            authors = record.get("authors")
            if not isinstance(authors, list):
                authors = []
            seen_authors: set[int] = set()
            for author in authors:
                if not isinstance(author, dict):
                    statistics["authors_missing_identity"] += 1
                    continue
                key, identity_kind = author_key(author)
                if key is None:
                    statistics["authors_missing_identity"] += 1
                    continue
                author_vertex = author_ids.get(key)
                if author_vertex is None:
                    vertex_count += 1
                    author_vertex = vertex_count
                    author_ids[key] = author_vertex
                    statistics[f"authors_with_{identity_kind}"] += 1
                    for token in text_tokens([author.get("name")], stopwords):
                        label_counts[token] = min(cap, label_counts.get(token, 0) + 1)
                if author_vertex in seen_authors:
                    statistics["duplicate_authors_within_paper"] += 1
                    continue
                seen_authors.add(author_vertex)
                left, right = sorted((paper_vertex, author_vertex))
                graph.write(f"{left} {right} 1\n")
                statistics["authorship_edges"] += 1

            statistics["paper_records"] += 1
            if line_number % args.progress_rows == 0:
                progress("pass1", line_number, started)

        eligible = {
            token
            for token, frequency in label_counts.items()
            if args.minimum_frequency <= frequency <= args.maximum_frequency
        }
        print(
            f"[pass1] vertices={vertex_count:,} papers={len(paper_ids):,} "
            f"authors={len(author_ids):,} eligible_labels={len(eligible):,}",
            flush=True,
        )
        memberships: dict[str, list[int]] = {token: [] for token in eligible}
        seen_author_labels = bytearray(vertex_count + 1)
        pass2_started = time.monotonic()
        with archive.open(ARCHIVE_MEMBER) as source2:
            for line_number, raw_line in enumerate(source2, 1):
                record = json.loads(raw_line)
                paper_vertex = paper_ids[str(record["id"])]
                for token in paper_tokens(record, stopwords):
                    members = memberships.get(token)
                    if members is not None:
                        members.append(paper_vertex)

                authors = record.get("authors")
                if isinstance(authors, list):
                    for author in authors:
                        if not isinstance(author, dict):
                            continue
                        key, _identity_kind = author_key(author)
                        if key is None:
                            continue
                        author_vertex = author_ids[key]
                        if seen_author_labels[author_vertex]:
                            continue
                        seen_author_labels[author_vertex] = 1
                        for token in text_tokens([author.get("name")], stopwords):
                            members = memberships.get(token)
                            if members is not None:
                                members.append(author_vertex)

                references = record.get("references")
                if isinstance(references, list):
                    seen_references: set[str] = set()
                    for reference in references:
                        if not isinstance(reference, str) or reference == record["id"]:
                            continue
                        if reference in seen_references:
                            statistics["duplicate_references_within_paper"] += 1
                            continue
                        seen_references.add(reference)
                        target = paper_ids.get(reference)
                        if target is None:
                            statistics["references_outside_snapshot"] += 1
                            continue
                        left, right = sorted((paper_vertex, target))
                        graph.write(f"{left} {right} 1\n")
                        statistics["citation_edges"] += 1
                if line_number % args.progress_rows == 0:
                    progress("pass2", line_number, pass2_started)

        edge_count = statistics["authorship_edges"] + statistics["citation_edges"]
        graph.seek(0)
        graph.write(f"{vertex_count:0{header_width}d} {edge_count:0{header_width}d}\n")
    os.replace(graph_temporary, graph_path)
    ordered_groups = sorted(memberships.items())
    for token, members in ordered_groups:
        expected = label_counts[token]
        if len(members) != expected:
            raise RuntimeError(
                f"membership mismatch for {token!r}: counted {expected}, emitted {len(members)}"
            )

    candidate_path = output / "candidate_groups.txt"
    candidate_temporary = candidate_path.with_suffix(".txt.tmp")
    metadata_path = output / "source_group_ids.tsv"
    metadata_temporary = metadata_path.with_suffix(".tsv.tmp")
    with candidate_temporary.open("w", encoding="utf-8", newline="\n") as candidates, metadata_temporary.open(
        "w", encoding="utf-8", newline="\n"
    ) as metadata:
        metadata.write("group_id\ttoken\tsize\n")
        for group_id, (token, members) in enumerate(ordered_groups, 1):
            candidates.write(f"g{group_id}: {' '.join(map(str, members))}\n")
            metadata.write(f"{group_id}\t{token}\t{len(members)}\n")
    os.replace(candidate_temporary, candidate_path)
    os.replace(metadata_temporary, metadata_path)

    manifest = {
        "schema_version": 1,
        "dataset": "DBLP-AMiner-V18",
        "archive": str(archive_path.relative_to(ROOT)).replace("\\", "/"),
        "archive_sha256": sha256(archive_path),
        "archive_member": ARCHIVE_MEMBER,
        "stopwords": str(args.stopwords.resolve().relative_to(ROOT)).replace("\\", "/"),
        "stopwords_sha256": sha256(args.stopwords.resolve()),
        "graph": {
            "vertices": vertex_count,
            "paper_vertices": len(paper_ids),
            "author_vertices": len(author_ids),
            "edges": statistics["authorship_edges"] + statistics["citation_edges"],
            "path": str(graph_path.relative_to(ROOT)).replace("\\", "/"),
            "sha256": sha256(graph_path),
        },
        "candidate_groups": {
            "minimum_frequency": args.minimum_frequency,
            "maximum_frequency": args.maximum_frequency,
            "count": len(ordered_groups),
            "memberships": sum(len(members) for _token, members in ordered_groups),
            "path": str(candidate_path.relative_to(ROOT)).replace("\\", "/"),
            "sha256": sha256(candidate_path),
        },
        "statistics": statistics,
        "transform": "paper+author vertices; unit authorship and in-snapshot citation edges; source text tokens deduplicated per vertex",
    }
    atomic_json(output / "build_manifest.json", manifest)
    print(json.dumps(manifest["graph"], sort_keys=True), flush=True)
    print(json.dumps(manifest["candidate_groups"], sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
