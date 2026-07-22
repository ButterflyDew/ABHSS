# Third-party exact baselines

本目录只服务于**单线程、精确 GST** 的可复现实验。版本和归档哈希由
[`experiments/environment_lock.json`](../experiments/environment_lock.json) 锁定；正式计时前必须运行：

```powershell
python tools/experiments/validate_environment.py
```

## 组件与用途

| 组件 | 冻结版本 | 本文用途 | 是否进入主性能表 |
| --- | --- | --- | --- |
| GPU4GST artifact | commit `716a19c` | 作者 2025 artifact 中的 CPU PrunedDP++；仅在整数边权且 `g<=14` 的作者查询审计中使用 | 否，附录 artifact audit |
| GroupSteinerTree | commit `09f3bda` | 作者的 Basic+ 精确 progressive 实现，`lambda=1, ratio=1` | 否，正确性/历史 exact 附录 |
| SCIP-Jack | SCIP tag `v703` | 通用 branch-and-cut GST 求解器，在 SteinLib 原始 `.stp` 上独立验证 | 否，标准实例正确性表 |
| SoPlex | tag `release-502` | SCIP-Jack 7.0.3 的开源 LP 后端 | 不单列 |
| Boost | 1.85.0 | 两个作者 artifact 的头文件依赖 | 不单列 |

主性能 baseline 是本仓库统一输入、统一内存口径的 `pruneddp_safe`。外部 artifact 的作用是验证历史实现和已知最优值，不把输入限制、计时口径或年代不同的实现混入主倍率图。

## 获取与构建

若本目录未随内部研究包提供，可按锁文件恢复 Git 源并 checkout 对应 commit。Boost 归档来自其[官方历史归档](https://archives.boost.io/release/1.85.0/source/)，解压为 `third_party/boost_1_85_0`。`GroupSteinerTree` 仓库内的 `ysgraph_20210203.zip` 需原样解压到同仓库目录。

先构建本仓库 adapter：

```powershell
cmake -S . -B build
cmake --build build --config Release --target `
  pruneddp dpbf basic_plus gpu4gst_pruneddp_artifact `
  abhss_light abhss_heavy abhss_light_no_early `
  abhss_light_no_witness abhss_heavy_forward compare_graphs
```

再构建独立外部程序：

```powershell
python tools/build_external_baselines.py --target all
```

生成位置：

- `build/Release/gpu4gst_pruneddp_artifact.exe`：不改作者核心头文件的统一 I/O adapter；
- `build/Release/basic_plus.exe`：不改作者 Basic+ 核心头文件的统一 I/O adapter；
- `build_external/gpu4gst-pruneddp/bin/Release/PrunedDP++.exe`：作者原始命令行程序，仅人工复核；
- `build_external/scip-jack-7.0.3/bin/applications/Release/scipstp.exe`：SCIP-Jack。

## 版本与许可边界

- SCIP-Jack 和 SoPlex 使用 **ZIB Academic License**，论文与 artifact 必须按许可引用并致谢。SCIP-Jack [官方页面](https://scipjack.zib.de/)显示截至 2026-05 当前版为 2.2，且需要向作者申请；本包使用可由公开 Git 历史精确恢复的 7.0.3。若投稿冻结前取得 2.2，只能把整个 SCIP-Jack 标准实例列统一重跑，不能把两个版本择优拼表。
- Boost 使用 Boost Software License 1.0。
- GPU4GST 和 GroupSteinerTree 两个作者仓库在本次冻结 commit 的根目录均未发现 repository-level license。**内部研究可以保留来源副本，但公开投稿 artifact 前不得默认拥有再分发权**；应取得作者许可，或在 artifact 中只提供 commit、下载说明、adapter 和哈希，由评测者自行获取。
- 不修改作者核心算法文件；本仓库的兼容代码仅位于 `src/baselines/`。任何后续 patch 都必须在论文中列出并重新做已知最优值校验。

## SCIP-Jack 的范围

SCIP-Jack 7.0.3 官方文档明确支持 group Steiner tree；本文只让它读取 SteinLib 原始 `.stp`，以单线程和 10,000 秒限制求解。它不读取本文展开式 query 文件，也不在百万级半真实图上与专用 DP 作端到端倍率比较。该限制避免把格式转换、通用 MIP/branch-and-cut 与专用多查询搜索混成一个含义不清的主 baseline。
