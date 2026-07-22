# Build, run, resume, and summarize

## 1. 构建

正式计时只使用 Release/O2：

```powershell
cmake -S . -B build
cmake --build build --config Release --target `
  pruneddp dpbf basic_plus gpu4gst_pruneddp_artifact `
  abhss_light abhss_heavy abhss_light_no_early `
  abhss_light_no_witness abhss_heavy_forward compare_graphs
```

构建 SCIP-Jack、SoPlex 和作者原命令行 PrunedDP++：

```powershell
python tools/build_external_baselines.py --target all
```

第三方版本和许可见 [`third_party/README.md`](third_party/README.md)。

## 2. 长跑前验收

```powershell
python tools/experiments/validate_environment.py
python tools/experiments/run_experiments.py --dry-run
```

验收检查锁定 commit/归档哈希、两份 DBLP 身份、面板数量、所有 case 输入和二进制。当前期望为 239 cases / 8,587 tasks。投稿冻结前再运行一次 `python tools/experiments/validate_environment.py --deep-snapshot`，逐字节复核约 14GB 的全部快照文件。

## 3. 直接运行一个求解器

```text
<exe> [graph_selector] [result_root] [query_selector]
      [data_root] [query_begin] [query_limit] [method options]
```

`graph_selector` 可为 `data` 下的图名或图目录；`query_selector` 可为图目录内名称，也可为显式询问文件路径。示例：

```powershell
.\build\Release\abhss_light.exe `
  data\Toronto result experiment_data\controlled\Toronto\controlled_g10_f400.txt `
  data 1 10

.\build\Release\pruneddp.exe `
  data\Toronto result experiment_data\controlled\Toronto\controlled_g10_f400.txt `
  data 1 10 --state-storage=hash --mst-upper=on --lb2-pathmax=off
```

PrunedDP++ 选项：

```text
--state-storage=hash|dense  (default: hash)
--mst-upper=on|off          (default: on)
--lb2-pathmax=on|off        (default: off, safe exact reopen mode)
```

`--lb2-pathmax=on` 只用于 paper-pathmax 复现审计；它在 WRP4 已有三个非最优反例，不能用于主精确表。

## 4. 统一实验 supervisor

机器可读矩阵为 [`experiments/paper_matrix.json`](experiments/paper_matrix.json)。运行一个 suite：

```powershell
python tools/experiments/run_experiments.py `
  --run-id paper_e0 `
  --run-dir results/paper_runs/paper_e0 `
  --suite E0_steinlib_wrp
```

可重复使用 `--suite`、`--case`、`--method` 精确筛选。相同命令自动跳过已有原子 record 并续跑。默认行为：

- 每条 query 独立 10,000 秒；deadline 从 solver 输出 `[Ready]` 后开始；
- 图加载有独立 1,800 秒基础设施 watchdog，不计为 solver timeout；
- 一条 query 超时只杀当前进程，记录后重新加载图继续下一条；
- case 内方法顺序按 SHA-256 稳定循环轮转；
- run metadata 保存配置和每个二进制 SHA-256。

## 5. 多机分片

第 `i` 个 worker：

```powershell
python tools/experiments/run_experiments.py `
  --run-id paper_main `
  --run-dir results/paper_runs/paper_main_shard00 `
  --shard-index 0 --shard-count 32
```

所有 shard 必须使用相同 `run-id`、不同 `run-dir`。分片以 case 为单位，保证同 cell 的所有方法/询问在同一 worker；修改 worker 数会改变分配，因此一次正式 run 不能中途改变 `shard-count`。

## 6. 汇总与画图

```powershell
python tools/experiments/summarize_results.py `
  --input-glob "results/paper_runs/paper_main_shard*" `
  --output results/paper_summary `
  --baseline pruneddp_safe

python tools/experiments/plot_results.py `
  --input-glob "results/paper_runs/paper_main_shard*" `
  --output results/paper_figures `
  --suite E1_controlled_g_f --suite E2_gpu4gst_related_panel
```

汇总输出：

- `summary_by_cell.csv`：10,000 秒主完成率、派生 1,000 秒完成率及 Wilson CI、median/P90/geomean solved time、PAR-2、peak RSS；
- `paired_speedups.csv`：双方完成的配对 speedup及 bootstrap CI、三个 timeout 方向；
- `quality_mismatches.csv`：答案差异；非空时程序返回失败；
- PDF/PNG performance profile、completion-by-`g`、paired-speedup-by-`g`。

默认质量门排除矩阵中 `exact_claim=false` 的 paper-pathmax audit。要专门复现其三个已知反例，追加 `--include-audit-methods-in-quality`；该审计输出预期非零，不能与主结果目录混用。

三次 calibration 与一次 full run 应分别汇总，不能合并后让 calibration 格获得额外权重。

## 7. 原生结果与 supervisor record

原生 solver 仍追加：

```text
<result_root>/<graph>/<method>/<query_subdir>/weights.txt
```

每行是：

```text
time_seconds best_weight query_processing_peak_rss_overhead_mb
```

论文分析以 supervisor 的 `records/*.json` 为准；它额外保存 query index、`g/f_real`、状态、deadline、命令、log、run id 和来源 case，避免错误读取 `weights.txt` 的旧批次。

完整执行顺序、重复次数、表图和投稿硬门见 [`docs/FULL_EXPERIMENT_PLAN.md`](docs/FULL_EXPERIMENT_PLAN.md)。
## 8. 投稿前 15 小时探针

完整实验前可运行一次固定抽样的可行性探针：

```powershell
python tools/experiments/run_probe_15h.py --hours 15
```

它使用独立探针二进制和 180/600 秒短截止，正式 `paper_matrix.json` 的 10,000 秒设置不变。抽样、进度、逐询问记录及最终原因分析都写入 `results/paper_runs/probe15h_*`。完整口径见 [`docs/PROBE_15H_PLAN.md`](docs/PROBE_15H_PLAN.md)。

控制器结束后必须独立验收（把目录替换为实际 run）：

```powershell
python tools/experiments/audit_probe_run.py `
  --run-dir results/paper_runs/probe15h_YYYYMMDD_HHMMSS
```

验收会重算固定 seed 排程并核对配置/二进制哈希、逐记录 task key、日志、精确权重与实际墙钟预算；结果写入 `probe_audit.json`。运行中只做前缀检查时可加 `--allow-running --no-write`，该模式不能替代最终验收。
