# ABHSS 冻结论文实验运行手册

`experiments/paper_matrix.json` 是可执行矩阵的唯一真值。本手册覆盖 Linux/Windows 构建、输入恢复、manifest 冻结、正确性 gate、正式分片、断点续跑与汇总。实验为 P1 全作者 workload、P2 跨 `g`、S2 受控 $\langle g,f\rangle$ 和 correctness gates；旧探针、近似解质量与 GPU/异构速度不在当前矩阵。

## 1. 构建

### 1.1 Linux 服务器（推荐）

需要 CMake 3.16+、GNU Make、Python 3.10+、pthread 和支持 C++17 的 GCC/Clang。首次先跑完整仓库内门禁：

```bash
make release JOBS=16
make validate
```

`make release` 执行 Release 配置、编译当前可用 target 和 CTest。只需正式性能运行时：

```bash
make paper-binaries JOBS=16
make validate-paper-binaries
```

这两个 target 产生 `build/abhss` 与 `build/pruneddp`。矩阵中 ABHSS Base/Enhanced 指向同一 `build/abhss`，分别固定参数 `--enhancements=none` 与 `--enhancements=all`。

如需显式选择 compiler 或分离构建目录：

```bash
make release BUILD_DIR=build-gcc CMAKE=cmake JOBS=16
```

使用 `CC`/`CXX` 环境变量选定 compiler 后，不得在同一 build directory 中更换 generator/compiler。CMake 会检测 IPO/LTO 和浮点 `std::from_chars`；不支持时分别关闭 IPO 或回退 `strtod`，不会伪装支持。使用非默认构建目录时，后续命令中的 `./build/...` 应同步替换；IMDb 包装脚本另可显式传 `--executable build-gcc/build_imdb_graph`。

### 1.2 Windows

Visual Studio 2022：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
python tools/experiments/validate_environment.py
```

MinGW Makefiles：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python tools/experiments/validate_environment.py
```

Visual Studio 多配置二进制在 `build/Release`，Linux/MinGW 单配置二进制在 `build`。runner 会按 `paper_matrix.json` 中的主路径与 fallback 查找。

### 1.3 手工 smoke 与配置契约

Linux：

```bash
./build/abhss example results/manual query.txt data 1 1 --enhancements=none
./build/abhss example results/manual query.txt data 1 1 --enhancements=all
```

Windows Visual Studio 构建将上述路径换为 `build/Release/abhss.exe`。中间 correctness/ablation 配置是 `--enhancements=directed-cut`，不是第三条正式性能曲线。`--adjoint-completion=on` 且 `--directed-cut=off` 是非法配置，会在加载图之前报错。

四个仓库内 CTest 分别核验图 I/O/分量缓存、查询 I/O、历史零权 witness，以及 144 个 $2\le g\le10$ 的确定性随机实例与入口/sub-nanogap 契约。

## 2. 恢复与转换精确输入

大图、展开查询和第三方源码不默认随 Git 分发。复制或下载任何输入后，先验证哈希，不得把新下载的可变文件放到旧 freeze ID 下。

### 2.1 GPU4GST 作者 workload 与 P2 候选

按 [`data_origin/README.md`](data_origin/README.md) 恢复并验证作者 `.in`、`.g` 和 CSV，然后：

```bash
make tools JOBS=16
./build/prepare_gpu4gst data_origin data all --seed 2025 --queries 300 --min-g 5 --max-g 16
```

Windows Visual Studio 构建使用 `build/Release/prepare_gpu4gst.exe`。该步同时生成 P1 的 `g={3,5,7}` 作者查询接口与 P2 每个 `g=5..16` 的 300 条 related-group 候选；文件名/manifest 始终区分 author 与 generated。

### 2.2 MonoGST+ 作者 workload

将五个处理后作者接口放在：

```text
data/Toronto/{graph.txt,query.txt}
data/MovieLens/{graph.txt,query.txt}
data/DBLP/{graph.txt,query.txt}
data/LinkedMDB/{graph.txt,query.txt}
data/DBpedia/{graph.txt,query.txt}
```

不从最新官方站点替换这五张 P1 图，不重生成它们的查询。最终图/查询哈希在 `experiment_data/p1_published_workloads/manifest.json`。MonoGST+ 已录用手稿与 workload 在当前 freeze 下尚无公开独立获取路径；公开 artifact 前必须取得作者许可或作者认可的获取说明。

### 2.3 S2 IMDb 图

IMDb freeze 只使用 2026-07-22 取得的 `title.basics.tsv.gz`、`title.principals.tsv.gz`、`name.basics.tsv.gz`。URL、目标路径和 SHA-256 在 `data_sources/official/official-latest-20260722/download_manifest.json`：

```bash
make tools JOBS=16
python3 tools/data/prepare_imdb_dataset.py --replace-existing
```

Windows 将 `python3` 换为 `python`。不得在 `IMDb-latest-20260722` 名下重新下载 IMDb 每日“latest”；不同日期必须创建新图身份并完整重跑 S2。该数据适用 IMDb non-commercial 条款。

### 2.4 SteinLib 正确性输入

按 SteinLib 官方 WRP3/WRP4 文件恢复到 `data_sources/steinlib`，需要重建时：

```bash
python3 tools/data/convert_steinlib.py --source-root data_sources/steinlib --output experiment_data/steinlib --min-g 11 --max-g 16
```

转换后必须保留官方实例名、来源文件和已知最优值的映射。

## 3. 生成冻结 manifest 与 panel

当且仅当第 2 节所有输入已备齐时执行：

```bash
python3 tools/data/build_published_workloads.py
python3 tools/data/build_gpu_query_panels.py
python3 tools/data/generate_controlled_queries.py
python3 tools/data/build_query_feasibility_audit.py
python3 tools/experiments/validate_environment.py --deep --require-performance-binaries
```

Windows 将 `python3` 换为 `python`。开发期若图与查询哈希完全不变，可用 `build_query_feasibility_audit.py --reuse-component-scans` 缩短审计；论文最终 freeze 必须不带该快捷开关重建完整审计。`--deep` 会重新哈希多 GB 图，只在 final freeze 或原始文件变化后执行。

预期展开数：

| Family | 查询 | 计时项 | 任务 |
|---|---:|---:|---:|
| P1 MonoGST+ | 1,118 | 3 | 3,354 |
| P1 GPU4GST | 7,200 | 3 | 21,600 |
| P2 cross-`g` | 360 | 3 | 1,080 |
| S2 controlled $\langle g,f\rangle$ | 150 | 3 | 450 |
| 性能矩阵合计 | 8,828 | 3 | 26,484 |

可行性审计必须恰好保留 P1 中已知 55 条无解自然查询（LinkedMDB 46、DBpedia 9），并要求 P2、S2 与 gate 的新查询无一条不可行。

## 4. 正确性 gate

### 4.1 仓库内 gate

`make release` 已运行 CTest。再运行机器矩阵中的 tiny smoke：

```bash
python3 tools/experiments/run_experiments.py --run-id gate --run-dir results/paper_runs/gate --suite S0_smoke
```

### 4.2 SteinLib 外部 gate

S1 的 DPBF 在仓库内；Basic+ 和 SCIP-Jack 需要锁定的第三方源码。详细 commit、许可与恢复步骤在 [`docs/archive/THIRD_PARTY.md`](docs/archive/THIRD_PARTY.md)。恢复 GroupSteinerTree/Boost 后重跑主构建；恢复 SCIP-Jack/SoPlex 后，Linux 可直接：

```bash
python3 tools/build_external_baselines.py --target scip-jack
make release JOBS=16
make validate-all-binaries
python3 tools/experiments/run_experiments.py --run-id gate --run-dir results/paper_runs/gate --suite S1_steinlib_exactness_gate
```

Windows 脚本默认使用 Visual Studio 2022/x64；Linux 默认使用 Unix Makefiles/Release。如需其他 generator，用 `--generator`；`--architecture` 只对 Visual Studio 传给 CMake。Linux runner 会把 SCIP 构建目录以及 SoPlex 的 `lib`/`lib64` 安装目录同时加入 `PATH` 和 `LD_LIBRARY_PATH`。

不得在下列任一情况下开始正式计时：仓库内 CTest 失败；可用精确方法与 SteinLib 已知最优值不一致；`quality_mismatches`/`feasibility_mismatches` 非空；或 feasibility audit 与当前 matrix hash 不一致。

## 5. 正式性能运行

先查看展开结果，不启动 solver：

```bash
python3 tools/experiments/run_experiments.py --run-id paper --run-dir results/paper_runs/paper --suite P1_monogstplus_published --suite P1_gpu4gst_published --suite P2_cross_g --suite S2_controlled_gf --dry-run
```

确认数量后按 suite 运行：

```bash
python3 tools/experiments/run_experiments.py --run-id paper --run-dir results/paper_runs/paper --suite P1_monogstplus_published
python3 tools/experiments/run_experiments.py --run-id paper --run-dir results/paper_runs/paper --suite P1_gpu4gst_published
python3 tools/experiments/run_experiments.py --run-id paper --run-dir results/paper_runs/paper --suite P2_cross_g
python3 tools/experiments/run_experiments.py --run-id paper --run-dir results/paper_runs/paper --suite S2_controlled_gf
```

每条查询有独立 10,000 秒 solver deadline。图加载在 `[Ready]` 前完成，不进入逐查询 timer，另由 1,800 秒 watchdog 保护。加载包含一次连通分量建索引；它是所有本地方法共享的 I/O 成本，只作 artifact usability 指标，不得并入算法 speedup。

## 6. 稳定分片与断点续跑

多台同构机器使用相同 `shard-count` 和不同 0-based `shard-index`：

```bash
python3 tools/experiments/run_experiments.py --run-id paper --run-dir results/paper_runs/paper --suite P1_gpu4gst_published --shard-count 8 --shard-index 0
```

分配只由稳定 case hash 决定。同一 `run-dir` 中每个 `(case,method,query)` 有独立 JSON 记录，已完成 key 会跳过，因此可直接重复原命令续跑。不得在同一物理机器上并发运行多个内存带宽重的正式 shard；不得将不同 CPU/编译器的 shard 直接合并为同一时间表。

若某个纸面值需要重跑，必须重跑该预声明 cell 的全部三个计时项并保留旧记录，不得只替换不利的单个 method/query。

## 7. 定向诊断

case ID 从 `--dry-run` 获得，`--case`、`--method`、`--query-index` 都是精确过滤：

```bash
python3 tools/experiments/run_experiments.py --run-id diagnose --run-dir results/diagnose --case P2_cross_g__Musae-GPU4GST_16 --query-index 1 --method abhss_base --method pruneddp_safe
```

诊断运行不自动进入论文汇总。若 PrunedDP++ 明显更快或 ABHSS 超时，保留原数据，再同时检查图的 $n,m$、密度/分量、实现后组大小、双方状态数、上界收紧、row 密度与各 phase 时间，不得仅用“图更大”解释。

## 8. 汇总与出图

```bash
python3 tools/experiments/summarize_results.py --input results/paper_runs/paper --output results/paper_runs/paper/summary
python3 tools/experiments/plot_results.py --input results/paper_runs/paper --suite P2_cross_g --suite S2_controlled_gf --output results/paper_runs/paper/figures
```

关键产物：

- `summary_by_dataset.csv`：P1 每图每方法一行，在内部聚合所有 query blocks 和 `g`。
- `summary_by_cell.csv`：P2/S2 逐参数 cell 表。
- `paired_speedups.csv`：双方共同完成的成对加速比与 timeout 方向。
- `quality_mismatches.csv` 和 `feasibility_mismatches.csv`：使用任何性能 claim 前必须为空。

P1 只在全部查询完成时填写 `observed_total_seconds_if_all_solved`。若有 timeout/error，先报完成数，再报已完成查询时间与将每个未完成查询按 10,000 秒计的 `capped_total_seconds`；不得把部分 solved time 当作整图总时间。

## 9. 首次 Linux 正式运行必留信息

在 run directory 中保留 CPU 完整型号、物理核/逻辑核、RAM、Linux 发行版与 kernel、GCC/Clang 版本、CMake 版本、Release flags、IPO/LTO 检测结果、Git commit、`paper_matrix.json` SHA-256、可行性 audit SHA-256、开始时间与机器是否独占。三个计时项必须使用同一个仓库 commit、编译器、优化策略、计时边界和物理机器类型。
