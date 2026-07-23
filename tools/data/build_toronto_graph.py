#!/usr/bin/env python3
"""Build the current official Toronto edge-weighted GST interface.

The road graph is the complete EPSG:2952 Toronto Centreline V2 export.  POI
groups come only from fields in the official Cultural Hotspot resource.  This
script intentionally does not use the historical paper-author Toronto files.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
import csv
import hashlib
import json
import math
import os
from pathlib import Path
from statistics import median


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "data_sources" / "official" / "official-latest-20260722"
DEFAULT_CENTRELINE = FREEZE / "raw" / "toronto-current" / "centreline-version-2-2952.csv"
DEFAULT_POI = FREEZE / "raw" / "toronto-current" / "points-of-interest-2952.csv"
DEFAULT_OUTPUT = ROOT / "data" / "official-latest-20260722" / "toronto-current"
GRID_METRES = 1000.0


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(8 * 1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def atomic_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(text, encoding="utf-8", newline="\n")
    os.replace(temporary, path)


def first_and_last_coordinate(value: str) -> tuple[tuple[float, float], tuple[float, float], float]:
    geometry = json.loads(value)
    coordinates = geometry.get("coordinates")
    if geometry.get("type") == "LineString":
        lines = [coordinates]
    elif geometry.get("type") == "MultiLineString":
        lines = coordinates
    else:
        raise ValueError(f"unsupported centreline geometry {geometry.get('type')!r}")
    lines = [line for line in lines if line and len(line) >= 2]
    if not lines:
        raise ValueError("centreline geometry has no usable line")
    length = 0.0
    for line in lines:
        for left, right in zip(line, line[1:]):
            length += math.hypot(float(right[0]) - float(left[0]), float(right[1]) - float(left[1]))
    start = (float(lines[0][0][0]), float(lines[0][0][1]))
    finish = (float(lines[-1][-1][0]), float(lines[-1][-1][1]))
    if not math.isfinite(length) or length <= 0.0:
        raise ValueError("centreline geometry has non-positive length")
    return start, finish, length


def point_coordinate(value: str) -> tuple[float, float]:
    geometry = json.loads(value)
    coordinates = geometry.get("coordinates")
    if geometry.get("type") == "Point":
        point = coordinates
    elif geometry.get("type") == "MultiPoint" and coordinates:
        point = coordinates[0]
    else:
        raise ValueError(f"unsupported POI geometry {geometry.get('type')!r}")
    return float(point[0]), float(point[1])


def load_roads(path: Path) -> tuple[dict[tuple[int, int], float], dict[int, tuple[float, float]], dict[str, int]]:
    edges: dict[tuple[int, int], float] = {}
    coordinate_sum: dict[int, list[float]] = {}
    stats = defaultdict(int)
    with path.open("r", encoding="utf-8-sig", newline="") as source:
        for row in csv.DictReader(source):
            stats["source_rows"] += 1
            try:
                left = int(row["FROM_INTERSECTION_ID"])
                right = int(row["TO_INTERSECTION_ID"])
                if left <= 0 or right <= 0 or left == right:
                    raise ValueError("invalid endpoints")
                start, finish, length = first_and_last_coordinate(row["geometry"])
            except (KeyError, TypeError, ValueError, json.JSONDecodeError):
                stats["discarded_rows"] += 1
                continue
            for vertex, coordinate in ((left, start), (right, finish)):
                accumulator = coordinate_sum.setdefault(vertex, [0.0, 0.0, 0.0])
                accumulator[0] += coordinate[0]
                accumulator[1] += coordinate[1]
                accumulator[2] += 1.0
            edge = (left, right) if left < right else (right, left)
            prior = edges.get(edge)
            if prior is None:
                edges[edge] = length
            else:
                stats["duplicate_endpoint_rows"] += 1
                if length < prior:
                    edges[edge] = length
            stats["retained_rows"] += 1
    coordinates = {
        vertex: (values[0] / values[2], values[1] / values[2])
        for vertex, values in coordinate_sum.items()
    }
    stats["unique_edges"] = len(edges)
    stats["source_intersections"] = len(coordinates)
    return edges, coordinates, dict(stats)


class NearestIndex:
    def __init__(self, coordinates: list[tuple[float, float]]) -> None:
        self.coordinates = coordinates
        self.cells: dict[tuple[int, int], list[int]] = defaultdict(list)
        for vertex, (x, y) in enumerate(coordinates, 1):
            self.cells[(math.floor(x / GRID_METRES), math.floor(y / GRID_METRES))].append(vertex)

    def nearest(self, x: float, y: float) -> tuple[int, float]:
        cell_x = math.floor(x / GRID_METRES)
        cell_y = math.floor(y / GRID_METRES)
        best_vertex = 0
        best_squared = math.inf
        radius = 0
        while True:
            for gx in range(cell_x - radius, cell_x + radius + 1):
                for gy in range(cell_y - radius, cell_y + radius + 1):
                    if radius and max(abs(gx - cell_x), abs(gy - cell_y)) != radius:
                        continue
                    for vertex in self.cells.get((gx, gy), ()):
                        vx, vy = self.coordinates[vertex - 1]
                        squared = (vx - x) ** 2 + (vy - y) ** 2
                        if squared < best_squared or (squared == best_squared and vertex < best_vertex):
                            best_squared = squared
                            best_vertex = vertex
            lower = min(
                x - (cell_x - radius) * GRID_METRES,
                (cell_x + radius + 1) * GRID_METRES - x,
                y - (cell_y - radius) * GRID_METRES,
                (cell_y + radius + 1) * GRID_METRES - y,
            )
            if best_vertex and best_squared <= lower * lower:
                return best_vertex, math.sqrt(best_squared)
            radius += 1
            if radius > 1000:
                raise RuntimeError("nearest-neighbour grid search did not converge")


def normalize_label(prefix: str, value: str) -> str:
    return f"{prefix}:{' '.join(value.strip().lower().split())}"


def load_poi_groups(
    path: Path,
    nearest: NearestIndex,
) -> tuple[dict[str, set[int]], list[dict[str, object]], dict[str, object]]:
    groups: dict[str, set[int]] = defaultdict(set)
    assignments: list[dict[str, object]] = []
    distances: list[float] = []
    discarded = 0
    with path.open("r", encoding="utf-8-sig", errors="replace", newline="") as source:
        for source_row, row in enumerate(csv.DictReader(source), 1):
            try:
                x, y = point_coordinate(row["geometry"])
            except (KeyError, TypeError, ValueError, json.JSONDecodeError):
                discarded += 1
                continue
            labels: set[str] = set()
            interests = row.get("Interests", "")
            if interests and interests != "None":
                labels.update(
                    normalize_label("interest", item)
                    for item in interests.split(",")
                    if item.strip()
                )
            neighbourhood = row.get("Neighbourhood", "")
            if neighbourhood and neighbourhood != "None":
                labels.add(normalize_label("neighbourhood", neighbourhood))
            if not labels:
                discarded += 1
                continue
            vertex, distance = nearest.nearest(x, y)
            distances.append(distance)
            for label in labels:
                groups[label].add(vertex)
            assignments.append(
                {
                    "source_row": source_row,
                    "object_id": row.get("ObjectId", ""),
                    "site_name": row.get("SiteName", ""),
                    "vertex": vertex,
                    "nearest_distance_metres": distance,
                    "labels": sorted(labels),
                }
            )
    groups = {label: members for label, members in groups.items() if len(members) >= 2}
    stats: dict[str, object] = {
        "source_rows": len(assignments) + discarded,
        "mapped_rows": len(assignments),
        "discarded_rows": discarded,
        "candidate_groups_with_at_least_two_distinct_vertices": len(groups),
    }
    if distances:
        ordered = sorted(distances)
        stats["nearest_distance_metres"] = {
            "minimum": ordered[0],
            "median": median(ordered),
            "p95": ordered[min(len(ordered) - 1, math.ceil(0.95 * len(ordered)) - 1)],
            "maximum": ordered[-1],
        }
    return groups, assignments, stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--centreline", type=Path, default=DEFAULT_CENTRELINE)
    parser.add_argument("--poi", type=Path, default=DEFAULT_POI)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    centreline = args.centreline.resolve()
    poi = args.poi.resolve()
    output = args.output.resolve()

    edges, source_coordinates, road_stats = load_roads(centreline)
    source_ids = sorted(source_coordinates)
    dense = {source_id: index for index, source_id in enumerate(source_ids, 1)}
    coordinates = [source_coordinates[source_id] for source_id in source_ids]
    dense_edges = sorted(
        (min(dense[left], dense[right]), max(dense[left], dense[right]), max(1, round(length * 1000.0)))
        for (left, right), length in edges.items()
    )
    groups, assignments, poi_stats = load_poi_groups(poi, NearestIndex(coordinates))
    ordered_groups = sorted(groups.items())

    output.mkdir(parents=True, exist_ok=True)
    graph_path = output / "graph.txt"
    graph_temporary = graph_path.with_suffix(".txt.tmp")
    with graph_temporary.open("w", encoding="utf-8", newline="\n") as destination:
        destination.write(f"{len(source_ids)} {len(dense_edges)}\n")
        for left, right, length_millimetres in dense_edges:
            destination.write(f"{left} {right} {length_millimetres}\n")
    os.replace(graph_temporary, graph_path)

    group_path = output / "candidate_groups.txt"
    group_temporary = group_path.with_suffix(".txt.tmp")
    with group_temporary.open("w", encoding="utf-8", newline="\n") as destination:
        for group_id, (_label, members) in enumerate(ordered_groups, 1):
            destination.write(f"g{group_id}: {' '.join(map(str, sorted(members)))}\n")
    os.replace(group_temporary, group_path)

    source_group_path = output / "source_group_ids.tsv"
    source_group_temporary = source_group_path.with_suffix(".tsv.tmp")
    with source_group_temporary.open("w", encoding="utf-8", newline="\n") as destination:
        destination.write("group_id\ttoken\tsize\n")
        for group_id, (label, members) in enumerate(ordered_groups, 1):
            destination.write(f"{group_id}\t{label}\t{len(members)}\n")
    os.replace(source_group_temporary, source_group_path)

    vertex_path = output / "source_vertex_ids.tsv"
    vertex_temporary = vertex_path.with_suffix(".tsv.tmp")
    with vertex_temporary.open("w", encoding="utf-8", newline="\n") as destination:
        destination.write("vertex_id\tintersection_id\tx_epsg2952\ty_epsg2952\n")
        for vertex, (source_id, (x, y)) in enumerate(zip(source_ids, coordinates), 1):
            destination.write(f"{vertex}\t{source_id}\t{x:.6f}\t{y:.6f}\n")
    os.replace(vertex_temporary, vertex_path)

    assignment_path = output / "poi_assignments.jsonl"
    assignment_temporary = assignment_path.with_suffix(".jsonl.tmp")
    with assignment_temporary.open("w", encoding="utf-8", newline="\n") as destination:
        for row in assignments:
            destination.write(json.dumps(row, sort_keys=True, ensure_ascii=False) + "\n")
    os.replace(assignment_temporary, assignment_path)

    manifest = {
        "schema_version": 1,
        "dataset": "Toronto-current",
        "projection": "EPSG:2952",
        "centreline_source": {"path": str(centreline.relative_to(ROOT)).replace("\\", "/"), "sha256": sha256(centreline)},
        "poi_source": {"path": str(poi.relative_to(ROOT)).replace("\\", "/"), "sha256": sha256(poi)},
        "graph": {"vertices": len(source_ids), "edges": len(dense_edges), "path": str(graph_path.relative_to(ROOT)).replace("\\", "/"), "sha256": sha256(graph_path)},
        "candidate_groups": {"count": len(ordered_groups), "path": str(group_path.relative_to(ROOT)).replace("\\", "/"), "sha256": sha256(group_path)},
        "edge_weight_unit": "millimetres",
        "road_transform": "valid FROM/TO intersections; metric polyline length rounded to the nearest positive millimetre; duplicate endpoint pair keeps minimum positive length",
        "poi_transform": "each POI maps to its exact nearest retained intersection; groups are prefixed Interests and Neighbourhood source fields; no distance cutoff",
        "road_stats": road_stats,
        "poi_stats": poi_stats,
    }
    atomic_text(output / "build_manifest.json", json.dumps(manifest, indent=2, sort_keys=True, ensure_ascii=False) + "\n")
    print(json.dumps(manifest["graph"], sort_keys=True))
    print(json.dumps(manifest["candidate_groups"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
