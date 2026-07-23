#!/usr/bin/env python3
"""构建论文 correctness gate 使用的锁定版第三方 CPU baseline。

所有外部产物都与主 CMake 树隔离；Windows 下还会规范环境变量名大小写，
避免 MSBuild 因同时继承 ``Path`` 与 ``PATH`` 而拒绝启动。
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import shlex
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
    """返回不会破坏 Linux PATH 的子进程环境。

    Windows 环境变量名不区分大小写，MSBuild 会拒绝同时存在
    ``Path``/``PATH`` 的环境，因此只在 Windows 上去重；POSIX 必须
    原样保留大小写敏感的 ``PATH``。
    """
    if os.name != "nt":
        return dict(os.environ)
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


def is_multi_config_generator(generator: str) -> bool:
    """判断 generator 是否不使用 CMAKE_BUILD_TYPE 的多配置类型。"""
    return (
        "Visual Studio" in generator
        or generator == "Xcode"
        or "Multi-Config" in generator
    )


def configure_prefix(generator: str, architecture: str) -> list[str]:
    """构造跨平台 CMake generator 参数，避免在 Make/Ninja/Xcode 上传 ``-A``。"""
    arguments = ["-G", generator]
    if architecture and "Visual Studio" in generator:
        arguments.extend(["-A", architecture])
    if not is_multi_config_generator(generator):
        arguments.append("-DCMAKE_BUILD_TYPE=Release")
    return arguments


def run(command: list[str]) -> None:
    """在仓库根目录执行一条外部构建命令，失败时立即终止。"""
    rendered = (
        subprocess.list2cmdline(command) if os.name == "nt" else shlex.join(command)
    )
    print("+", rendered, flush=True)
    subprocess.run(command, cwd=ROOT, env=clean_environment(), check=True)


def configure_soplex(generator: str, architecture: str) -> None:
    """配置锁定版 SoPlex，作为 SCIP-Jack 的独立 LP 依赖。"""
    run(
        [
            "cmake",
            "-S",
            str(SOPLEX_SOURCE),
            "-B",
            str(SOPLEX_BUILD),
            *configure_prefix(generator, architecture),
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
            f"-DCMAKE_INSTALL_PREFIX={SOPLEX_INSTALL}",
            "-DGMP=OFF",
            "-DBOOST=OFF",
            "-DBUILD_TESTING=OFF",
        ]
    )


def build_soplex() -> None:
    """编译并安装 SoPlex 到仓库内忽略的外部构建目录。"""
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
    """使用本地 SoPlex 配置锁定的 SCIP-Jack 7.0.3。"""
    matches = sorted(SOPLEX_INSTALL.glob("lib*/cmake/soplex/soplex-config.cmake"))
    if not matches:
        raise FileNotFoundError(
            "SoPlex is not installed. Run this script with --target soplex first."
        )
    soplex_config = matches[0].parent
    run(
        [
            "cmake",
            "-S",
            str(SCIP_SOURCE),
            "-B",
            str(SCIP_BUILD),
            *configure_prefix(generator, architecture),
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
            # SCIP 7.0.3 用这个历史开关控制包括 SCIP-Jack 在内的全部应用；
            # 后续只编译 scipstp target，不构建或运行无关测试。
            "-DBUILD_TESTING=ON",
        ]
    )


def build_scip_jack() -> None:
    """只编译 SCIP-Jack 应用 target，不运行上游的其他测试。"""
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
    """配置 GPU4GST artifact 中的 CPU PrunedDP++ 独立工程。"""
    if not (BOOST_ROOT / "boost" / "heap" / "fibonacci_heap.hpp").exists():
        raise FileNotFoundError(
            "Pinned Boost headers are missing. See docs/archive/THIRD_PARTY.md."
        )
    run(
        [
            "cmake",
            "-S",
            str(OFFICIAL_PRUNEDDP_SOURCE),
            "-B",
            str(OFFICIAL_PRUNEDDP_BUILD),
            *configure_prefix(generator, architecture),
            f"-DBOOST_ROOT={BOOST_ROOT}",
            f"-DBoost_INCLUDE_DIR={BOOST_ROOT}",
            "-DBoost_NO_SYSTEM_PATHS=ON",
        ]
    )


def build_official_pruneddp() -> None:
    """编译 GPU4GST artifact 的上游 CPU 工程，只用于历史校准。"""
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
    """仅删除 ``build_external`` 下经解析验证的指定构建产物。"""
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
    """解析目标并按 SoPlex -> SCIP-Jack/作者 PrunedDP 依赖顺序构建。"""
    default_generator = (
        "Visual Studio 17 2022" if os.name == "nt" else "Unix Makefiles"
    )
    default_architecture = "x64" if os.name == "nt" else ""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--target",
        choices=("soplex", "scip-jack", "official-pruneddp", "all"),
        default="all",
    )
    parser.add_argument(
        "--generator", default=default_generator, help="CMake generator"
    )
    parser.add_argument(
        "--architecture",
        default=default_architecture,
        help="CMake platform architecture (used only by Visual Studio)",
    )
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    if args.clean:
        remove_builds(args.target)

    # SCIP-Jack 依赖本地安装的 SoPlex。把它作为传递构建依赖，保证文档中的
    # 单命令恢复流程可以从干净 checkout 直接执行。
    if args.target in ("soplex", "scip-jack", "all"):
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
