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

使用 `CC`/`CXX` 环境变量选定 compiler 后，不得在同一 build directory 中更换 generator/compiler。CMake 会检测 IPO/LTO 和浮点 `std::from_chars`；不支持时分别关闭 IPO 或回退 `strtod`，不会伪装支持。使用非默认构建目录时，后续命令中的 `./build/...` 应同步替换。

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

五个仓库内 CTest 分别核验图 I/O/分量缓存、查询 I/O、历史零权 witness、5,000 个 $2\le g\le10$ 的确定性随机实例、500 个正权互异单终端压力实例、160 个 $g=7,\ldots,16$ 的 omitted-half transpose 实例、真值 5.75 的 12 点辅助半层固定反例、全部受支持组数的 adjoint split 结构覆盖、互补半格域、稀疏 row 全值交集三种遍历、入口/sub-nanogap 契约，以及 ABHSS/PrunedDP++ 的实际 `(mask,v)` 状态计数。

## 2. 恢复与转换精确输入

大图、展开查询和第三方源码不默认随 Git 分发。复制或下载任何输入后，先验证哈希，不得把新下载的可变文件放到旧 freeze ID 下。

### 2.1 GPU4GST 作者 workload 与 P2 候选

按 [`data_origin/README.md`](data_origin/README.md) 恢复并验证作者 `.in`、`.g` 和 CSV，然后：

```bash
make tools JOBS=16
./build/prepare_gpu4gst data_origin data all --seed 2025 --queries 300 --min-g 5 --max-g 15
```

Windows Visual Studio 构建使用 `build/Release/prepare_gpu4gst.exe`。该步同时生成 P1 的 `g={3,5,7}` 作者查询接口与 P2 每个 `g=5..15` 的 300 条 related-group 候选；文件名/manifest 始终区分 author 与 generated。

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

### 2.3 S2 受控查询

S2 不再构建或使用 IMDb 图，而是逐字节复用 P1 的两个 MonoGST+ 作者接口：

```text
data/DBLP/graph.txt
data/Toronto/graph.txt
```

生成前必须确认二者仍与 `experiment_data/p1_published_workloads/manifest.json` 中的 `DBLP-MonoGSTPlus` 和 `Toronto-MonoGSTPlus` 图哈希一致。随后运行：

```bash
python3 tools/data/generate_controlled_queries.py
```

该命令固定生成 `g={3,6,9,12,15}`、`f={200,400,800,1600,3200}`、每格 10 条、base seed `20260827` 的 500 条查询；每个图和 `g` 延续一条随机流，并按“组大小、该组成员”的参考调用顺序抽样，同时重建 `cells.json`、`cells.csv` 与 `queries.json`。每个查询文件第一行是查询数；每条查询先写一行 `g`，再写 `g` 行 `组大小 顶点1 ... 顶点k`。顶点使用 `1..n`，组内无重复，组间允许重叠。展开后的 `.txt` 被 Git 忽略；新服务器可以重新执行同一命令，或连同整个 `experiment_data/s1_controlled_gf` 查询目录一起传输。不得在 S2 名义下换图、换 seed、重采样或按 solver 结果筛选。旧 IMDb 获取定义只保留为历史来源证据，不进入当前 P1/P2/S2。

`g=15` 的 10 个查询文件在 2026-08-27 已生成并完成 Enhanced 无 TL 资源运行；2026-08-28 扩展网格时，生成器按独立的 `(graph,g)` RNG 流重建全部文件，逐字节哈希确认这 10 个文件未变，原 100 条结果也原地保留。`g={3,6,9,12}` 是后加的 400 条，不得把五层网格表述为在 `g=15` 运行前预登记。

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
| P2 cross-`g` | 660 | 3 | 1,980 |
| S2 controlled `<g,f>` | 500 | 3 | 1,500 |
| 性能矩阵合计 | 9,478 | 3 | 28,434 |

可行性审计必须恰好保留 P1 中已知 55 条无解自然查询（LinkedMDB 46、DBpedia 9），并要求 P2、S2 与 gate 的新查询无一条不可行。

## 4. 正确性 gate

### 4.1 仓库内 gate

`make release` 已运行 CTest。再运行机器矩阵中的 tiny smoke：

```bash
python3 tools/experiments/run_experiments.py --run-id gate --run-dir results/paper_runs/gate --suite S0_smoke
```

### 4.2 SteinLib 外部 gate

S1 的 DPBF 在仓库内；Basic+ 和 SCIP-Jack 需要锁定的第三方源码。详细 commit、许可与恢复步骤在 [`docs/archive/DATA_BASELINES_AND_ARTIFACTS.md#history-third-party`](docs/archive/DATA_BASELINES_AND_ARTIFACTS.md#history-third-party)。恢复 GroupSteinerTree/Boost 后重跑主构建；恢复 SCIP-Jack/SoPlex 后，Linux 可直接：

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

P2 的 q1--q5 是原 panel，q6--q10 是本次追加 tranche。若旧五条已经在同一机器和二进制上完成，可只运行新增部分；为避免旧 run metadata 所记录的查询文件哈希与十条版 panel 冲突，使用新的 run-id/run-dir：

```bash
python3 tools/experiments/run_experiments.py --run-id p2_extra --run-dir results/paper_runs/p2_extra --suite P2_cross_g --query-index 6 --query-index 7 --query-index 8 --query-index 9 --query-index 10
```

汇总成十条/格的正式结果时必须同时保留旧 run directory、新增 tranche run directory、各自 commit 与机器环境记录；不得覆盖或重新编号旧 q1--q5。

每条查询有独立 3,600 秒 solver deadline。图加载在 `[Ready]` 前完成，不进入逐查询 timer，另由 1,800 秒 watchdog 保护。加载包含一次连通分量建索引；它是所有本地方法共享的 I/O 成本，只作 artifact usability 指标，不得并入算法 speedup。

每个 native `weights.txt` 查询记录固定为四列：

```text
query_seconds weight query_peak_rss_mib mask_vertex_states
```

末列是本次查询首次进入主状态存储的状态项数。PrunedDP++ 统计 Hash/Dense `StateStore` 中实际 `present` 的 `(mask,v)` 项，reopen 不重复；ABHSS 统计 $D$、 $A$、 $H$ 行中首次接纳的项，并把状态族视为键的一部分，因此不同状态族中数值相同的 `(mask,v)` 分别计数。只要 A1 逻辑层存在，所有 ABHSS 配置都在 ordinary 前生成并计数一次；同一 A1 row 转交公共前向行时不重复。两边均排除组距离、route/tour、dual 势读取、队列过期副本、重复松弛和只更新完整解的 full-mask 候选。平凡/前置闭合查询可为 0；尚无统一口径的 correctness-only adapter 写 `-1`。console 的 `[Query]` 行末同步输出 `mask_vertex_states=<整数>`，supervisor 将正式方法的非负值写入每任务 JSON。

## 6. 稳定分片与断点续跑

多台同构机器使用相同 `shard-count` 和不同 0-based `shard-index`：

```bash
python3 tools/experiments/run_experiments.py --run-id paper --run-dir results/paper_runs/paper --suite P1_gpu4gst_published --shard-count 8 --shard-index 0
```

分配只由稳定 case hash 决定。同一 `run-dir` 中每个 `(case,method,query)` 有独立 JSON 记录，已完成 key 会跳过，因此可直接重复原命令续跑。不得在同一物理机器上并发运行多个内存带宽重的正式 shard；不得将不同 CPU/编译器的 shard 直接合并为同一时间表。

本轮最终 campaign 固定使用已校准的 CPU 4/5。先执行只读身份审计和队列展开；确认输出为 40 个 Enhanced cell job、400 个新 Enhanced 任务、24,954 个复用 P1 任务、660 个复用 P2 Enhanced 任务和 100 个复用 S2 `g=15` Enhanced 任务后，再启动后台会话：

```bash
python3 tools/experiments/run_parallel_campaign.py prepare
tmux new-session -d -s abhss-final -c "$PWD" 'python3 tools/experiments/run_parallel_campaign.py run > results/paper_runs/final_3600s_campaign_793d4e_20260828/scheduler.log 2>&1'
python3 tools/experiments/run_parallel_campaign.py status
```

`prepare` 验证生产二进制哈希、CPU 物理核、当前 P1/P2/S2 task key、查询路径和历史记录全集，并只在身份完全相同时复用完整 P1、全部 660 条 P2 Enhanced 与原 100 条 S2 `g=15` Enhanced。它在新目录生成 25,714 条只读历史记录的正式视图：按统一 3,600 秒口径把 P1 PrunedDP++ 的 2 条、P2 Enhanced 的 3 条和 S2 `g=15` Enhanced 的 6 条原始长完成记为 timeout，但不覆盖原始精确结果。`run` 先按 `g` 递增、同一 `g` 内按确定性估计从快到慢调度新增 `g={3,6,9,12}` 的 400 条 Enhanced；真实 timeout 正常落盘并继续，不触发 campaign 级硬停止，也不据此更换查询。

Enhanced 全部得到完成或 timeout 记录后，P2 的 Base/PrunedDP++ 按 `g` 递增运行。每格先跑覆盖五个组大小层的 q1--q5；仅当 5/5 都真实达到 3,600 秒且 Enhanced 对应 5/5 均在 3,600 秒内完成，才把当前 q6--q10 与同图同方法更大 `g` 写为 `not_run_likely_timeout` 而不启动。S2 按同一思想但不跨 `f` 外推：固定图、方法、`f`、`g` 的 10/10 全部真实 timeout，且 Enhanced 同十条均在 TL 内完成时，才跳过同图同方法同 `f` 的更大 `g`。这些预测停止项不是正式 timeout，不进入完成数、PAR-2 或时间总和；清单保留全部 task key 与真实 timeout 证据，论文需要完整点时再补跑。最后执行冻结的最小消融；endpoint-floor 变体由隔离构建器创建并通过完整 CTest。精确协议见 `experiments/final_campaign_plan.json` 和 `docs/EXPERIMENT_PLAN.md` 第 8.5 节。

调度器持有排他锁且每个 task key 独立落盘；后台会话中断后重复同一 `run` 命令即可恢复。换机器或换 CPU 前必须重新校准并修改机器计划，不能直接沿用 CPU 4/5 的正式时间口径。

若某个纸面值需要重跑，必须重跑该预声明 cell 的全部三个计时项并保留旧记录，不得只替换不利的单个 method/query。

## 7. 定向诊断

case ID 从 `--dry-run` 获得，`--case`、`--method`、`--query-index` 都是精确过滤：

```bash
python3 tools/experiments/run_experiments.py --run-id diagnose --run-dir results/diagnose --case P2_cross_g__Musae-GPU4GST_15 --query-index 1 --method abhss_base --method pruneddp_safe
```

诊断运行不自动进入论文汇总。若 PrunedDP++ 明显更快或 ABHSS 超时，保留原数据，再同时检查图的 $n,m$、密度/分量、实现后组大小、双方状态数、上界收紧、row 密度与各 phase 时间，不得仅用“图更大”解释。

对可能越过正式 TL 的单条长询问，可在单独 probe 构建与新 run directory 中追加 `--probe-diagnostics`，并把外层诊断预算设得足以跑完整轨迹。该预算只用于定位，不能替换矩阵中的正式 3,600 秒 TL。runner 会把 `[ProbeDiag]` 行解析进任务 JSON 的 `probe_diagnostics`：`prepare_end`、`singleton_anchor_end`、`ordinary_layer` 和 `adjoint_transpose` 给出阶段边界；`adjoint_layer` 同时给出该 H 层的秒数、row/scalar 数与当前上界；`adjoint_complementary_half_upper` 表示互补辅助半格严格收紧上界。最终还必须保留 solver time、查询峰值 RSS、watchdog 峰值 RSS、返回值和状态数，不能用中途 row 数冒充完成结果。

## 8. 汇总与出图

```bash
python3 tools/experiments/summarize_results.py --input results/paper_runs/final_3600s_campaign_793d4e_20260828/historical_formal_records.jsonl --input results/paper_runs/final_3600s_campaign_793d4e_20260828/workers/cpu4 --input results/paper_runs/final_3600s_campaign_793d4e_20260828/workers/cpu5 --output results/paper_runs/final_3600s_campaign_793d4e_20260828/summary
python3 tools/experiments/plot_results.py --input results/paper_runs/final_3600s_campaign_793d4e_20260828/historical_formal_records.jsonl --input results/paper_runs/final_3600s_campaign_793d4e_20260828/workers/cpu4 --input results/paper_runs/final_3600s_campaign_793d4e_20260828/workers/cpu5 --suite P2_cross_g --suite S2_controlled_gf --output results/paper_runs/final_3600s_campaign_793d4e_20260828/figures
```

`p2_likely_timeout_not_run.jsonl` 和 `s2_likely_timeout_not_run.jsonl` 不作为 `--input`；它们只登记未运行项，不能混入正式分母。消融结果位于独立的 `ablation_workers`，按消融面板另行汇总。

关键产物：

- `summary_by_dataset.csv`：P1 每图每方法一行，在内部聚合所有 query blocks 和 `g`。
- `summary_by_cell.csv`：P2/S2 逐参数 cell 表。
- `paired_speedups.csv`：双方共同完成的成对加速比与 timeout 方向。
- `quality_mismatches.csv` 和 `feasibility_mismatches.csv`：使用任何性能 claim 前必须为空。

P1 只在全部查询完成时填写 `observed_total_seconds_if_all_solved`。若有 timeout/error，先报完成数，再报已完成查询时间与将每个未完成查询按 3,600 秒计的 `capped_total_seconds`；不得把部分 solved time 当作整图总时间。

`summary_by_cell.csv` 另给完成查询的平均/中位/p90 `mask_vertex_states`；`summary_by_dataset.csv` 给已完成查询的状态总数，并且仅在全图查询全部完成且计数都可用时填写全 workload 状态总数。`paired_speedups.csv` 的 `baseline_over_contender` 状态倍率只使用双方均完成且状态数都为正的配对；倍率大于 1 表示 ABHSS 发现的主状态更少。timeout 进程没有最终计数，不能以已运行时间或内存反推，也不能把完成子集的状态倍率宣称为全查询倍率。

## 9. 首次 Linux 正式运行必留信息

在 run directory 中保留 CPU 完整型号、物理核/逻辑核、RAM、Linux 发行版与 kernel、GCC/Clang 版本、CMake 版本、Release flags、IPO/LTO 检测结果、Git commit、`paper_matrix.json` SHA-256、可行性 audit SHA-256、开始时间与机器是否独占。三个计时项必须使用同一个仓库 commit、编译器、优化策略、计时边界和物理机器类型。
