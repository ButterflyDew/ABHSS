#!/usr/bin/env python3
"""Audit and manifest every query in the two published P1 workloads.

No sampling occurs here.  MonoGST+ contributes its five complete ``query.txt``
files; GPU4GST contributes the first 300 author CSV queries already converted
to repository format for each of ``g=3,5,7`` on all eight graphs.
"""

from __future__ import annotations

import csv
from collections import Counter
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "experiment_data" / "p1_published_workloads"

MONO_DATASETS = (
    ("Toronto", "Toronto-MonoGSTPlus", 46_073, 68_353, 160, "synthetic <g,f>"),
    ("MovieLens", "MovieLens-MonoGSTPlus", 62_423, 35_323_774, 160, "synthetic <g,f>"),
    ("DBLP", "DBLP-MonoGSTPlus", 2_497_782, 12_786_329, 160, "synthetic <g,f>"),
    ("LinkedMDB", "LinkedMDB-MonoGSTPlus", 1_326_784, 2_132_796, 200, "WikiMovies natural"),
    ("DBpedia", "DBpedia-MonoGSTPlus", 5_887_296, 18_338_729, 438, "DBpedia-Entity v2 natural"),
)

# Solver-independent component audit on the exact author graph/query files.
# These rows stay in P1 because the experiment intentionally uses every
# published query; exact methods must agree that their optimum is infeasible.
KNOWN_INFEASIBLE = {
    "LinkedMDB-MonoGSTPlus": [
        3, 12, 14, 27, 29, 33, 43, 46, 49, 51, 53, 54, 56, 61, 69, 74,
        76, 84, 91, 92, 115, 118, 122, 125, 131, 139, 146, 150, 153, 155,
        157, 159, 160, 163, 168, 173, 181, 182, 187, 189, 193, 194, 195,
        196, 197, 200,
    ],
    "DBpedia-MonoGSTPlus": [59, 107, 169, 174, 201, 209, 226, 287, 437],
}

GPU_PAPER_STATS = {
    "Musae": (19_109, 400_497, 13_183),
    "Twitch": (34_118, 429_113, 3_163),
    "Github": (37_700, 289_003, 4_005),
    "Youtube": (1_134_890, 2_987_624, 8_385),
    "DBLP": (2_423_455, 12_786_329, 127_726),
    "Orkut": (3_072_440, 117_185_083, 6_288_363),
    "LiveJournal": (3_997_962, 34_681_189, 664_414),
    "Reddit": (4_262_834, 12_502_767, 1_146_657),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def graph_header(path: Path) -> tuple[int, int]:
    with path.open("r", encoding="utf-8", errors="strict") as source:
        fields = source.readline().split()
    if len(fields) != 2:
        raise ValueError(f"invalid graph header: {path}")
    return int(fields[0]), int(fields[1])


def query_summary(path: Path) -> tuple[int, dict[int, int]]:
    counts: Counter[int] = Counter()
    with path.open("r", encoding="utf-8", errors="strict") as source:
        first = source.readline()
        if not first:
            raise ValueError(f"empty query file: {path}")
        declared = int(first.strip())
        for index in range(1, declared + 1):
            g_line = source.readline()
            if not g_line:
                raise ValueError(f"truncated before query {index}: {path}")
            g = int(g_line.strip())
            if g <= 0:
                raise ValueError(f"invalid g={g} in query {index}: {path}")
            counts[g] += 1
            for _ in range(g):
                group_line = source.readline()
                if not group_line:
                    raise ValueError(f"truncated in query {index}: {path}")
                size_text, separator, members = group_line.partition(" ")
                if not separator or int(size_text) != len(members.split()):
                    raise ValueError(f"malformed group in query {index}: {path}")
        if source.read(1):
            raise ValueError(f"trailing content after {declared} queries: {path}")
    return declared, dict(sorted(counts.items()))


def relative(path: Path) -> str:
    return str(path.relative_to(ROOT)).replace("\\", "/")


def dataset_record(
    *,
    dataset: str,
    source_family: str,
    graph: Path,
    paper_vertices: int,
    paper_edges: int,
    paper_groups: int | None,
    query_family: str,
    query_records: int,
    query_blocks: list[str],
    source_reference: str,
) -> dict[str, object]:
    vertices, edges = graph_header(graph)
    return {
        "dataset": dataset,
        "source_family": source_family,
        "graph_path": relative(graph.parent),
        "graph_sha256": sha256(graph),
        "graph_bytes": graph.stat().st_size,
        "vertices": vertices,
        "edges": edges,
        "paper_vertices": paper_vertices,
        "paper_edges": paper_edges,
        "paper_candidate_groups": paper_groups,
        "matches_paper_vertex_edge_counts": vertices == paper_vertices and edges == paper_edges,
        "query_family": query_family,
        "query_records": query_records,
        "query_blocks": query_blocks,
        "source_reference": source_reference,
    }


def main() -> int:
    cases: list[dict[str, object]] = []
    datasets: list[dict[str, object]] = []

    for directory, dataset, paper_n, paper_m, expected_queries, query_family in MONO_DATASETS:
        graph = ROOT / "data" / directory / "graph.txt"
        query = ROOT / "data" / directory / "query.txt"
        query_count, g_counts = query_summary(query)
        if query_count != expected_queries:
            raise ValueError(
                f"{dataset} declares {query_count} queries; expected {expected_queries}"
            )
        case = {
            "dataset": dataset,
            "source_family": "MonoGSTPlus",
            "query_block": "all",
            "graph_path": relative(graph.parent),
            "query_path": relative(query),
            "queries": query_count,
            "g_counts": {str(g): count for g, count in g_counts.items()},
            "query_sha256": sha256(query),
            "known_infeasible_query_indices": KNOWN_INFEASIBLE.get(dataset, []),
            "selection": "all author-used queries; no sampling",
        }
        cases.append(case)
        datasets.append(
            dataset_record(
                dataset=dataset,
                source_family="MonoGSTPlus",
                graph=graph,
                paper_vertices=paper_n,
                paper_edges=paper_m,
                paper_groups=None,
                query_family=query_family,
                query_records=query_count,
                query_blocks=["all"],
                source_reference="A Practical Sublinear Approximation for Group Steiner Tree (VLDB 2026 accepted manuscript)",
            )
        )
        datasets[-1]["known_infeasible_queries"] = len(
            KNOWN_INFEASIBLE.get(dataset, [])
        )

    for prefix, (paper_n, paper_m, paper_groups) in GPU_PAPER_STATS.items():
        graph_dir = ROOT / "data" / f"GPU4GST_{prefix}"
        graph = graph_dir / "graph.txt"
        dataset = f"{prefix}-GPU4GST"
        total_queries = 0
        blocks: list[str] = []
        for g in (3, 5, 7):
            query = graph_dir / f"query_author_g{g}.txt"
            query_count, g_counts = query_summary(query)
            if query_count != 300 or g_counts != {g: 300}:
                raise ValueError(f"unexpected author query block: {query}")
            total_queries += query_count
            blocks.append(f"g{g}")
            cases.append(
                {
                    "dataset": dataset,
                    "source_family": "GPU4GST",
                    "query_block": f"g{g}",
                    "graph_path": relative(graph_dir),
                    "query_path": relative(query),
                    "queries": query_count,
                    "g_counts": {str(g): query_count},
                    "query_sha256": sha256(query),
                    "known_infeasible_query_indices": [],
                    "selection": "all first-300 author CSV rows used by the paper; no sampling",
                }
            )
        datasets.append(
            dataset_record(
                dataset=dataset,
                source_family="GPU4GST",
                graph=graph,
                paper_vertices=paper_n,
                paper_edges=paper_m,
                paper_groups=paper_groups,
                query_family="author related-group queries",
                query_records=total_queries,
                query_blocks=blocks,
                source_reference="https://github.com/toziki/GPU4GST-sigmod",
            )
        )
        datasets[-1]["known_infeasible_queries"] = 0

    if len(datasets) != 13 or len(cases) != 29:
        raise AssertionError("P1 must contain 13 datasets and 29 executable blocks")
    if sum(int(row["queries"]) for row in cases) != 8_318:
        raise AssertionError("P1 must contain all 8,318 published-workload queries")
    if sum(len(row["known_infeasible_query_indices"]) for row in cases) != 55:
        raise AssertionError("P1 must retain the 55 audited infeasible natural queries")

    payload = {
        "schema_version": 1,
        "paper_role": "P1 published-workload reproduction",
        "reporting_unit": "dataset; query blocks and g are retained only for audit",
        "selection": "all queries from both papers",
        "datasets": datasets,
        "cases": cases,
        "totals": {
            "datasets": len(datasets),
            "executable_query_blocks": len(cases),
            "queries": sum(int(row["queries"]) for row in cases),
            "known_infeasible_queries_retained": 55,
        },
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / "manifest.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    for name, rows in (("datasets.csv", datasets), ("cases.csv", cases)):
        with (OUTPUT / name).open("w", encoding="utf-8", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
    print("Manifested 13 datasets, 29 query blocks and 8,318 P1 queries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
