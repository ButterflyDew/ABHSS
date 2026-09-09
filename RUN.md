# ABHSS 冻结论文实验运行手册

`experiments/paper_matrix.json` 是可执行矩阵的唯一真值。本手册覆盖 Linux/Windows 构建、输入恢复、manifest 冻结、正确性 gate、正式分片、断点续跑与汇总。实验为 P1 全作者 workload、P2 跨 `g`、S2 受控 $\langle g,f\rangle$ 和 correctness gates；旧探针、近似解质量与 GPU/异构速度不在当前矩阵。

## 1. 构建

### 1.1 Linux 服务器（推荐）

需要 CMake 3.16+、GNU Make、Python 3.10+、pthread 和支持 C++17 的 GCC/Clang；生成 PDF/PNG 图还需要 Python Matplotlib，但求解、结果物化和 CSV 汇总不依赖它。首次先跑完整仓库内门禁：

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

本轮正式结果固定使用已校准的 CPU 4/5。`prepare` 验证生产二进制哈希、两个物理核、当前 P1/P2/S2 task key、查询路径和历史记录全集；只有身份完全一致时才复用完整 P1、全部 660 条 P2 Enhanced 与原 100 条 S2 $g=15$ Enhanced。它生成 25,714 条只读历史正式视图，并按统一 3,600 秒口径非破坏性截断 11 条原始长完成。启动与查看状态的入口为：

```bash
python3 tools/experiments/run_parallel_campaign.py prepare
tmux new-session -d -s abhss-final -c "$PWD" "python3 tools/experiments/run_parallel_campaign.py run"
python3 tools/experiments/run_parallel_campaign.py status
```

[`experiments/final_campaign_plan.json`](experiments/final_campaign_plan.json) 以 schema 2 保留 2026-08-28 源 campaign 的旧 frontier 内容并显式标记为 superseded；顶层 `current_formal_policy` 绑定完成账本，旧规则只作历史时间线。该次运行曾依据 q1--q5 或较小 $g$ 留下 75 个 `not_run_likely_timeout` 项；真实回填发现 Orkut/PrunedDP++ 同格 q6 timeout 后 q7 仅需 85.932 秒等反例。当前 runner 已删除这种可执行停止逻辑：Enhanced、Base 与 PrunedDP++ 的每个缺失 task key 都独立运行到 `ok` 或真实 3,600 秒 timeout，旧预测文件即使存在也只在 `status` 中显示为 ignored historical artifacts。2026-09-07 已完成的 75 条 backfill 由 `formal_backfill_map()` 逐项核对历史 task-key 集合、矩阵、三方法二进制、TL、非诊断模式和真实状态，并作为只读已完成记录参与恢复判断；重复 `run` 不会再次执行它们。`status` 将 `current_formal` 与 `historical_source_campaign_state` 分开显示，后者的旧停止字段不表示当前政策。

调度器按 $g$ 递增，并在同一 $g$ 内用不读取当前结果的确定性估计从快到慢排队；两个 worker 动态占用 CPU 4/5。真实 timeout 正常落盘并继续，不触发 campaign 级停止，也不换查询。父进程持有排他锁，每个 task key 原子落盘；中断后重复同一 `run` 命令只恢复缺少真实记录的任务。预测器研究必须位于独立 probe 目录、完整运行查询且不进入正式 ledger；在没有足够证据前不得把 shadow 触发用于调度。

最小消融最后运行；endpoint-floor 变体由隔离构建器创建并先通过完整 CTest。换机器或换 CPU 前必须重新校准，不能直接合并时间。若某个纸面值需要重跑，必须重跑该预声明 cell 的全部三个计时项并保留旧记录，不得只替换不利的单个 method/query。

## 7. 定向诊断

case ID 从 `--dry-run` 获得，`--case`、`--method`、`--query-index` 都是精确过滤：

```bash
python3 tools/experiments/run_experiments.py --run-id diagnose --run-dir results/diagnose --case P2_cross_g__Musae-GPU4GST_15 --query-index 1 --method abhss_base --method pruneddp_safe
```

诊断运行不自动进入论文汇总。若 PrunedDP++ 明显更快或 ABHSS 超时，保留原数据，再同时检查图的 $n,m$、密度/分量、实现后组大小、双方状态数、上界收紧、row 密度与各 phase 时间，不得仅用“图更大”解释。

对可能越过正式 TL 的单条长询问，可在单独 probe 构建与新 run directory 中追加 `--probe-diagnostics`，并把外层诊断预算设得足以跑完整轨迹。该预算只用于定位，不能替换矩阵中的正式 3,600 秒 TL。runner 会把 `[ProbeDiag]` 行解析进任务 JSON 的 `probe_diagnostics`：`prepare_end`、`singleton_anchor_end`、`ordinary_layer` 和 `adjoint_transpose` 给出阶段边界；`adjoint_layer` 同时给出该 H 层的秒数、row/scalar 数与当前上界；`adjoint_complementary_half_upper` 表示互补辅助半格严格收紧上界。最终还必须保留 solver time、查询峰值 RSS、watchdog 峰值 RSS、返回值和状态数，不能用中途 row 数冒充完成结果。

## 8. 汇总与出图

完整回填结束时，本轮冻结 finalizer 已生成规范化 ledger 与 manifest 快照；manifest 包含当时的生成时间，后续复核不得重跑 finalizer 覆盖该审计锚点。仓库跟踪的通用 materializer 应在独立目录重建账本，并与冻结 `records.jsonl` 逐字节比较。下面命令中的三个 source 标签、记录数、回填 provenance 标记、矩阵哈希和三种方法的二进制哈希都是验收合同，不得省略：

```bash
python3 tools/experiments/materialize_formal_results.py \
  --source historical_formal=results/paper_runs/final_3600s_campaign_793d4e_20260828/historical_formal_records.jsonl \
  --source source_campaign_workers=results/paper_runs/final_3600s_campaign_793d4e_20260828/workers \
  --source backfill_workers=results/paper_runs/final_3600s_backfill_793d4e_20260907/workers \
  --expect-source-count historical_formal=25714 \
  --expect-source-count source_campaign_workers=2645 \
  --expect-source-count backfill_workers=75 \
  --mark-source backfill_workers=formal_backfill_of_exploratory_prediction \
  --expected-matrix-sha 12dbac04c42bd14619b11332623b64dba8c6db3356062058967bef91047142ab \
  --expected-binary-sha abhss_base=793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced \
  --expected-binary-sha abhss_enhanced=793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced \
  --expected-binary-sha pruneddp_safe=4c1d3599f03da6073d368a6a83fcbd31ea0a625f9ba90892b22b0b239eb42bf2 \
  --expected-records 28434 \
  --run-id final_3600s_complete_793d4e \
  --output-dir results/paper_runs/final_3600s_complete_793d4e_generic_check
cmp results/paper_runs/final_3600s_complete_793d4e/records.jsonl results/paper_runs/final_3600s_complete_793d4e_generic_check/records.jsonl
```

通用 materializer 要求每个目录型 source 都带有 worker `run_metadata.json`，其中矩阵、逐查询 TL 和命令行声明的三方法二进制哈希必须逐项精确匹配；历史正式视图以单一 JSONL 输入，并由冻结 finalizer 的逐字节比较锁定。它还会从冻结查询文件逐条重算并核对 `g`、`min_f`、`max_f` 和 `mean_f`；查询编号、组数、最小/最大组大小及状态数必须是非布尔整数，均值必须是有限数。任一应为整数的字段被字符串或浮点数替换，或者任一结构值与查询文件不符时，必须在写出规范账本前失败。

只有 `cmp` 成功后，正式汇总才读取规范化的 28,434 条 ledger：

```bash
python3 tools/experiments/summarize_results.py --input results/paper_runs/final_3600s_complete_793d4e/records.jsonl --output results/paper_runs/final_3600s_complete_793d4e/summary
python3 tools/experiments/plot_results.py --input results/paper_runs/final_3600s_complete_793d4e/records.jsonl --suite P2_cross_g --suite S2_controlled_gf --output results/paper_runs/final_3600s_complete_793d4e/figures
```

消融报告必须把规范化正式账本中的 Base/Enhanced 与三个隔离变体按同一 45 条查询联结，不能只汇总 135 条新增记录：

```bash
python3 tools/experiments/report_ablation.py \
  --formal-input results/paper_runs/final_3600s_complete_793d4e/records.jsonl \
  --ablation-input results/paper_runs/final_3600s_campaign_793d4e_20260828/ablation_workers/cpu4 \
  --ablation-input results/paper_runs/final_3600s_campaign_793d4e_20260828/ablation_workers/cpu5 \
  --output results/paper_runs/final_3600s_complete_793d4e/ablation \
  --figures
```

该入口默认要求唯一正式输入为 materializer 生成的 `records.jsonl`。相邻 `manifest.json` 中的记录数、账本 SHA-256、矩阵/二进制身份、三类来源计数、suite-method 计数和四项完整性计数必须全部闭合；脚本还逐条核对 3,600 秒时限及完成/真实 timeout 字段合同。仅有 28,434 行但没有清单不能进入论文报告。两个 `--ablation-input` 还必须各自带有 `run_metadata.json`；报告器会锁定消融 run id、矩阵 SHA-256、逐查询 TL、非诊断运行方式、生产 ABHSS 二进制和唯一无 endpoint-floor 二进制，任一身份不符即停止。脚本只选择预登记面板中的 90 条生产记录，再与 135 条变体记录组成 45 查询 × 5 配置的 225 条消融账本，并要求同一查询的五个配置具有相同的组大小元数据。输出同时包含逐 cell 资源表、四个有向机制比较、总体比较和 PDF/PNG；超时进入五条固定分母的 PAR-2，成对 speedup 只使用双方完成项并另列 timeout 方向。

旧 `p2_likely_timeout_not_run.jsonl`、`s2_likely_timeout_not_run.jsonl`、RSS/state shadow 事件和诊断重放结果都不是 `--input`。规范化前必须证明 task key 恰好覆盖矩阵、无重复、状态仅为 `ok`/`timeout`，且 P2/S2 每个 cell 每方法恰好十条。消融结果位于独立 `ablation_workers`，按消融面板另行汇总。

关键产物：

- `summary_by_dataset.csv`：P1 每图每方法一行，在内部聚合所有 query blocks 和 `g`。
- `summary_by_cell.csv`：P2/S2 逐参数 cell 表，包含完成查询的内存记录数与峰值，以及该 cell 实际 `mean_f` 的最小值和最大值。
- `paired_speedups.csv`：双方共同完成的成对加速比与 timeout 方向。
- `p2_summary_by_tranche.csv`：按 `(dataset,g)` 分开报告 q1--q5 与 q6--q10 两个五查询 tranche；`p2_tranche_audit.json` 核对固定 size strata、查询索引、方法全集和记录完整性。
- `quality_mismatches.csv` 和 `feasibility_mismatches.csv`：使用任何性能 claim 前必须为空。

绘图程序分别生成每个 suite 的 performance profile、六个 P2 数据集的 completion/PAR-2/共同完成加速比分面，以及两个 S2 数据集各五个 `g` 下随 `f` 变化的对应分面；每张图同时输出 PDF 和 PNG。分面不得跨数据集聚合。Performance profile 的横轴覆盖全部已完成查询的有限时间比，不作任意数值截断；timeout 视为无穷并保留在分母中，因此曲线最右端等于该方法的完成率。

P1 只在全部查询完成时填写 `observed_total_seconds_if_all_solved`。若有 timeout/error，先报完成数，再报已完成查询时间与将每个未完成查询按 3,600 秒计的 `capped_total_seconds`；不得把部分 solved time 当作整图总时间。

`summary_by_cell.csv` 另给完成查询的平均/中位/p90 `mask_vertex_states`；`summary_by_dataset.csv` 给已完成查询的状态总数，并且仅在全图查询全部完成且计数都可用时填写全 workload 状态总数。`paired_speedups.csv` 的 `baseline_over_contender` 状态倍率只使用双方均完成且状态数都为正的配对；倍率大于 1 表示 ABHSS 发现的主状态更少。timeout 进程没有最终计数，不能以已运行时间或内存反推，也不能把完成子集的状态倍率宣称为全查询倍率。

## 9. 首次 Linux 正式运行必留信息

在 run directory 中保留 CPU 完整型号、物理核/逻辑核、RAM、Linux 发行版与 kernel、GCC/Clang 版本、CMake 版本、Release flags、IPO/LTO 检测结果、Git commit、`paper_matrix.json` SHA-256、可行性 audit SHA-256、开始时间与机器是否独占。三个计时项必须使用同一个仓库 commit、编译器、优化策略、计时边界和物理机器类型。

当前 runner 对未来记录在 `[Ready]` 或上一条查询完成后立即捕获 `started_at`。提交 42a06a1 及其更早的历史 native 记录是在组装最终 JSON 时才填写该字段，几乎等于 `finished_at`；这些历史记录的算法时间仍由 `solver_seconds` 和 `watchdog_wall_seconds` 正确给出，任何分析都不得用旧 `started_at` 反推开始时间。
