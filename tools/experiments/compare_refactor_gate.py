#!/usr/bin/env python3
"""成对比较 ABHSS 重构前后结果，检查目标值和面板总时间非退化。"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any


def parse_weight_file(path: Path) -> list[dict[str, float]]:
    """读取一个 weights.txt；忽略批次 header，保留查询时间、权重和峰值内存。"""

    rows: list[dict[str, float]] = []
    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != 3:
            raise ValueError(f"{path}:{line_number}: expected three fields")
        rows.append(
            {
                "seconds": float(fields[0]),
                "weight": float(fields[1]),
                "peak_rss_mib": float(fields[2]),
            }
        )
    return rows


def collect_records(root: Path, method_component: str) -> dict[str, dict[str, float]]:
    """收集指定结果目录分量下的全部查询，并生成与方法名称无关的稳定键。"""

    records: dict[str, dict[str, float]] = {}
    for path in sorted(root.rglob("weights.txt")):
        relative_parts = list(path.relative_to(root).parts)
        matches = [index for index, part in enumerate(relative_parts) if part == method_component]
        if not matches:
            continue
        if len(matches) != 1:
            raise ValueError(f"ambiguous method path: {path}")
        relative_parts[matches[0]] = "<ABHSS_CONFIGURATION>"
        file_key = "/".join(relative_parts)
        for query_index, row in enumerate(parse_weight_file(path), 1):
            key = f"{file_key}#query={query_index}"
            if key in records:
                raise ValueError(f"duplicate query key: {key}")
            records[key] = row
    if not records:
        raise ValueError(f"no {method_component!r} weights.txt records under {root}")
    return records


def compare_records(
    old: dict[str, dict[str, float]],
    new: dict[str, dict[str, float]],
    weight_tolerance: float,
    max_regression: float,
) -> dict[str, Any]:
    """逐键检查目标值，再按完整面板总 query time 计算新旧比和 gate 结论。"""

    old_keys = set(old)
    new_keys = set(new)
    missing = sorted(old_keys - new_keys)
    unexpected = sorted(new_keys - old_keys)
    mismatches: list[dict[str, Any]] = []
    paired: list[dict[str, Any]] = []
    for key in sorted(old_keys & new_keys):
        old_weight = old[key]["weight"]
        new_weight = new[key]["weight"]
        same_weight = (
            old_weight == new_weight
            if old_weight == -1.0 or new_weight == -1.0
            else math.isclose(old_weight, new_weight, abs_tol=weight_tolerance, rel_tol=0.0)
        )
        if not same_weight:
            mismatches.append(
                {"key": key, "old_weight": old_weight, "new_weight": new_weight}
            )
        paired.append(
            {
                "key": key,
                "old_seconds": old[key]["seconds"],
                "new_seconds": new[key]["seconds"],
                "new_over_old": (
                    new[key]["seconds"] / old[key]["seconds"]
                    if old[key]["seconds"] > 0.0
                    else None
                ),
            }
        )

    old_total = sum(row["seconds"] for row in old.values())
    new_total = sum(row["seconds"] for row in new.values())
    ratio = new_total / old_total if old_total > 0.0 else math.inf
    passed = (
        not missing
        and not unexpected
        and not mismatches
        and ratio <= 1.0 + max_regression
    )
    return {
        "passed": passed,
        "query_count": len(paired),
        "weight_tolerance": weight_tolerance,
        "max_allowed_regression_fraction": max_regression,
        "old_total_seconds": old_total,
        "new_total_seconds": new_total,
        "new_over_old": ratio,
        "missing_queries": missing,
        "unexpected_queries": unexpected,
        "weight_mismatches": mismatches,
        "paired_queries": paired,
    }


def main() -> int:
    """解析命令行、执行成对 gate、可选写 JSON，并以退出码表达是否通过。"""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--old-root", type=Path, required=True)
    parser.add_argument("--new-root", type=Path, required=True)
    parser.add_argument("--old-method", required=True, help="旧结果路径中的方法目录名")
    parser.add_argument("--new-method", required=True, help="新结果路径中的配置目录名")
    parser.add_argument("--weight-tolerance", type=float, default=1e-6)
    parser.add_argument("--max-regression", type=float, default=0.03)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    report = compare_records(
        collect_records(args.old_root, args.old_method),
        collect_records(args.new_root, args.new_method),
        args.weight_tolerance,
        args.max_regression,
    )
    rendered = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8", newline="\n")
    print(rendered, end="")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
