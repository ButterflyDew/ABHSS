#!/usr/bin/env python3
"""Convert SteinLib WRP group-Steiner encodings to the repository GST format.

WRP encodes each group by a terminal/dummy vertex joined to every member with
the same large connector cost.  Removing the terminal and its incident edges
recovers one GST group; subtracting one connector per terminal recovers the
published GST optimum from the published STP optimum.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import asdict, dataclass
import hashlib
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "data_sources" / "steinlib"
DEFAULT_OUTPUT = ROOT / "experiment_data" / "steinlib"


@dataclass
class Instance:
    name: str
    source_file: str
    source_sha256: str
    stp_vertices: int
    stp_edges: int
    gst_vertices: int
    gst_edges: int
    groups: int
    mean_group_size: float
    connector_cost_sum: float
    published_stp_optimum: float | None
    derived_gst_optimum: float | None
    converted: bool
    reason: str


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def read_published_optima(path: Path) -> dict[str, float]:
    text = path.read_text(encoding="latin-1")
    pattern = re.compile(
        r"&nbsp;(wrp[34]-\d+)</td>.*?<td[^>]*>\s*<b>(\d+)</b>",
        re.IGNORECASE | re.DOTALL,
    )
    return {name.lower(): float(value) for name, value in pattern.findall(text)}


def parse_stp(path: Path) -> tuple[int, list[tuple[int, int, float]], list[int]]:
    node_count: int | None = None
    declared_edges: int | None = None
    declared_terminals: int | None = None
    edges: list[tuple[int, int, float]] = []
    terminals: list[int] = []
    section = ""

    for raw_line in path.read_text(encoding="latin-1").splitlines():
        fields = raw_line.strip().split()
        if not fields:
            continue
        if fields[0].lower() == "section" and len(fields) >= 2:
            section = fields[1].lower()
            continue
        if fields[0].lower() == "end":
            section = ""
            continue
        if section == "graph":
            if fields[0].lower() == "nodes":
                node_count = int(fields[1])
            elif fields[0].lower() == "edges":
                declared_edges = int(fields[1])
            elif fields[0].upper() == "E":
                edges.append((int(fields[1]), int(fields[2]), float(fields[3])))
        elif section == "terminals":
            if fields[0].lower() == "terminals":
                declared_terminals = int(fields[1])
            elif fields[0].upper() == "T":
                terminals.append(int(fields[1]))

    if node_count is None or declared_edges is None or declared_terminals is None:
        raise ValueError(f"Missing Graph/Terminals header in {path}")
    if declared_edges != len(edges):
        raise ValueError(f"Edge count mismatch in {path}: {declared_edges} != {len(edges)}")
    if declared_terminals != len(terminals):
        raise ValueError(
            f"Terminal count mismatch in {path}: {declared_terminals} != {len(terminals)}"
        )
    return node_count, edges, terminals


def recover_gst(
    node_count: int,
    edges: list[tuple[int, int, float]],
    terminals: list[int],
) -> tuple[list[tuple[int, int, float]], list[list[int]], float, dict[int, int]]:
    terminal_set = set(terminals)
    groups: dict[int, list[int]] = {terminal: [] for terminal in terminals}
    connector_weights: dict[int, list[float]] = {terminal: [] for terminal in terminals}
    base_edges: list[tuple[int, int, float]] = []

    for u, v, weight in edges:
        u_terminal = u in terminal_set
        v_terminal = v in terminal_set
        if u_terminal and v_terminal:
            raise ValueError("WRP contains a terminal-to-terminal edge")
        if u_terminal or v_terminal:
            terminal = u if u_terminal else v
            member = v if u_terminal else u
            groups[terminal].append(member)
            connector_weights[terminal].append(weight)
        else:
            base_edges.append((u, v, weight))

    connector_sum = 0.0
    for terminal in terminals:
        if not groups[terminal]:
            raise ValueError(f"Terminal {terminal} has no group members")
        weights = connector_weights[terminal]
        if any(abs(weight - weights[0]) > 1e-9 for weight in weights[1:]):
            raise ValueError(f"Terminal {terminal} has nonuniform connector weights")
        connector_sum += weights[0]

    original_vertices = [vertex for vertex in range(1, node_count + 1) if vertex not in terminal_set]
    remap = {old: new for new, old in enumerate(original_vertices, 1)}
    converted_edges = [(remap[u], remap[v], weight) for u, v, weight in base_edges]
    converted_groups = [sorted({remap[member] for member in groups[t]}) for t in terminals]
    return converted_edges, converted_groups, connector_sum, remap


def format_number(value: float) -> str:
    return str(int(value)) if value.is_integer() else format(value, ".17g")


def write_instance(
    output_root: Path,
    name: str,
    vertex_count: int,
    edges: list[tuple[int, int, float]],
    groups: list[list[int]],
    metadata: dict[str, object],
) -> None:
    folder = output_root / name
    folder.mkdir(parents=True, exist_ok=True)
    with (folder / "graph.txt").open("w", encoding="utf-8", newline="\n") as graph_file:
        graph_file.write(f"{vertex_count} {len(edges)}\n")
        for u, v, weight in edges:
            graph_file.write(f"{u} {v} {format_number(weight)}\n")
    with (folder / "query.txt").open("w", encoding="utf-8", newline="\n") as query_file:
        query_file.write("1\n")
        query_file.write(f"{len(groups)}\n")
        for group in groups:
            query_file.write(f"{len(group)} {' '.join(map(str, group))}\n")
    (folder / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=SOURCE_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--min-g", type=int, default=4)
    parser.add_argument("--max-g", type=int, default=16)
    args = parser.parse_args()

    optima: dict[str, float] = {}
    for family in ("WRP3", "WRP4"):
        optima.update(read_published_optima(args.source_root / f"{family}-source.html"))

    args.output.mkdir(parents=True, exist_ok=True)
    records: list[Instance] = []
    for family in ("WRP3", "WRP4"):
        for source in sorted((args.source_root / "raw" / family).glob("*.stp")):
            name = source.stem.lower()
            node_count, stp_edges, terminals = parse_stp(source)
            edges, groups, connector_sum, remap = recover_gst(node_count, stp_edges, terminals)
            published = optima.get(name)
            derived = published - connector_sum if published is not None else None
            compatible = args.min_g <= len(groups) <= args.max_g
            reason = "converted" if compatible else f"outside supported g=[{args.min_g},{args.max_g}]"
            record = Instance(
                name=name,
                source_file=str(source.relative_to(ROOT)).replace("\\", "/"),
                source_sha256=sha256(source),
                stp_vertices=node_count,
                stp_edges=len(stp_edges),
                gst_vertices=len(remap),
                gst_edges=len(edges),
                groups=len(groups),
                mean_group_size=sum(map(len, groups)) / len(groups),
                connector_cost_sum=connector_sum,
                published_stp_optimum=published,
                derived_gst_optimum=derived,
                converted=compatible,
                reason=reason,
            )
            records.append(record)
            if compatible:
                write_instance(
                    args.output,
                    name,
                    len(remap),
                    edges,
                    groups,
                    {
                        **asdict(record),
                        "conversion": (
                            "remove each listed terminal and its incident connector edges; "
                            "the former neighbors are one GST group; remap remaining IDs in ascending order"
                        ),
                        "optimum_formula": "published_stp_optimum - connector_cost_sum",
                    },
                )

    json_records = [asdict(record) for record in records]
    (args.output / "index.json").write_text(
        json.dumps(json_records, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    with (args.output / "index.csv").open("w", encoding="utf-8", newline="") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=list(asdict(records[0]).keys()))
        writer.writeheader()
        writer.writerows(json_records)
    converted_count = sum(record.converted for record in records)
    print(f"Indexed {len(records)} WRP instances; converted {converted_count} to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
