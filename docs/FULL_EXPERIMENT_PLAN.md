# SIGMOD / VLDB 全量实验运行方案

版本：2026-07-21。目标：支持一篇关于**纯单线程精确 GST** 的 SIGMOD 或 VLDB Research Track 投稿。本方案是预注册口径；实验完成后只允许修复实现错误或基础设施故障，不允许按结果更换询问、超时、baseline 或聚合方式。

## 1. 论文应回答的问题

| RQ | 问题 | 主要证据 |
| --- | --- | --- |
| RQ1 | ABHSS 是否始终返回精确解，尤其在 `g=9..16`？ | E0 小图交叉验证；SteinLib WRP `g=11..16` 已知最优值；所有主实验逐询问一致性检查 |
| RQ2 | 固定平均组大小时，ABHSS 随 `g=4..16` 的时间、完成率和内存如何扩展？ | E1 的 `f_target=400` 严格受控 sweep |
| RQ3 | 固定 `g` 时，平均组大小 `f` 对方法的影响是什么？ | E1 的 `g=6,10,14`、`f=200,400,800,1600` sweep；E3 原协议复现 |
| RQ4 | 优势是否跨查询生成机制和图来源成立，而不是单个生成器特例？ | E2 相关组、E3 Practical 原询问、LinkedMDB/DBpedia 自然询问 |
| RQ5 | 在 baseline 大量超时时，ABHSS 能否扩展精确求解的可行区域？ | 10,000 秒完成率、PAR-2、performance profile、双向 timeout 计数 |
| RQ6 | 提前 A1、见证树和 Heavy 的 transpose/adjoint 各贡献多少？ | E5 固定消融面板 |

核心主张应写成：“在相同单线程、相同逐实例 10,000 秒限制下，ABHSS 在小组数保持竞争力，并在较大 `g` 上显著提高精确求解完成率，且在双方完成的实例上获得随 `g` 增大的加速。”只有全量结果支持时才能把“显著”具体化为“一个数量级”。

## 2. baseline 调研结论

### 2.1 必须运行

1. **[PrunedDP++](https://doi.org/10.1145/2882903.2915217)-Safe（主 baseline）**：Hash 状态、论文逐状态 MST 可行上界、原始 admissible `lb_2`，按更小 `g` reopen；矩阵名 `pruneddp_safe`。这是唯一覆盖所有浮点/整数图和 `g=4..16` 的统一精确对照。
2. **[GPU4GST 作者 CPU PrunedDP++ artifact](https://github.com/toziki/GPU4GST-sigmod)（作者代码审计）**：commit `716a19c`，只在整数边权、`g<=14` 上有效。E4 用八图作者 `g=5,7` 询问验证统一 baseline 没有靠重实现获取不公平优势。
3. **[DPBF](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/icde07steiner.pdf)（小规模 oracle）**：经典精确动态规划。只进入 example 和 WRP，不承担百万图性能比较。
4. **[Basic+ 作者实现](https://www.vldb.org/pvldb/vol14/p1137-sun.pdf)（历史精确对照）**：PVLDB 2021 artifact，`lambda=1, ratio=1`；上游实现限 `g<=14`。只进入 E0。
5. **[SCIP-Jack](https://scipjack.zib.de/)（独立通用求解器）**：直接读取 SteinLib 原 `.stp`，用 branch-and-cut 验证转换和最优值；不与专用多查询算法做百万图倍率。

ABHSS-Light 与 ABHSS-Heavy 是两个独立方法，必须分别作图和报告；不得取逐询问较快者组成一个“ABHSS-best”，也不得按数据集名、`g` 或运行中状态自动切换。

### 2.2 只作复现审计，不是精确 baseline

`pruneddp_strict` 重现论文 Algorithm 4 的 pathmax 候选传播和永久 closed 语义。它在随机小图曾通过，但在 WRP4 已产生三个可复现错误：

| 实例 | 已知最优值 | paper-pathmax 重实现 |
| --- | ---: | ---: |
| wrp4-11 | 179 | 180 |
| wrp4-15 | 405 | 407 |
| wrp4-16 | 1190 | 1213 |

因此它只能出现在 correctness/audit 表，不能计算主 speedup、完成率或“精确方法”排名。默认命令行现已改为 Safe；复现 pathmax 必须显式加 `--lb2-pathmax=on`。完整证据见 [`experiments/correctness_audit.json`](../experiments/correctness_audit.json)和 [`BASELINE.md`](BASELINE.md)。

### 2.3 不需要再加入的旧方法

- Basic、PrunedDP、PrunedDP+、BANKS-I/II、BLINKS、ENSteiner：已有文献中被 PrunedDP++/Basic+ 或现代近似方法支配，重复实现会增加复现歧义而不改变本文结论。
- D-PrunedDP++、diameter-bounded GPU 方法：目标问题多了直径约束，不是本文 GST。
- GPU/异构前沿：按研究范围不比较；硬件、并行度与本文“纯单线程 CPU 精确算法”不一致。可以在 related work 说明互补，不放同一 runtime 图。
- KeyKG+ 等索引方法：查询时间排除了巨额预处理，且不是精确算法；不是公平的主性能 baseline。

### 2.4 是否选择近似算法只比较解质量

**主文不选。** 对精确算法，正确运行时 NWeight 恒为 1；单独列近似算法的 NWeight 只能重复说明“近似解可能偏离最优”，不能回答本文的精确搜索 RQ。它还会引入完全不同的访问模型、索引预处理和代码可用性问题，容易让审稿人要求一整套效率—质量 Pareto 研究，反而削弱主线。

只有在以下条件同时成立时，才可增加一个不影响接收结论的附录：目标近似方法官方代码公开、能读取完全相同的冻结图/询问、预处理时间和空间全部计入、并预先声明报告 runtime、NWeight、最大误差和 timeout。首选应是同数据脉络中的 MonoGST+/ImprovAPP，而不是理论但不可扩展的 polylog 方法。当前 MonoGST+ 论文未公开，故不进入冻结矩阵。

## 3. 高 `g` 的 motivation 怎样写才经得住审稿

不能写“以往没有做过 `g>8`”：PrunedDP++ 附录报告过 `g=9,10`，而 GPU4GST 的精确实验主要是 `g=3..7`。未公开 Practical 论文在五图上有 `g=1..10` 询问，但其精确 ground truth 在较大 `g` 上大量不可得。

可支持的准确表述是：

> 现有系统性 CPU 精确实证主要停留在 `g<=10`；最新 GPU 精确工作把核心参数设为 `g=3..7`。然而 exact GST 仍被用作近似算法的 ground truth、离线高价值查询和系统正确性标签。本文在统一 CPU 协议下系统覆盖 `g=4..16`，并在 `g=11..16` 的标准已知最优值实例以及多来源真实图上的半合成查询中验证精确性与可行域扩展。

这里“有意义”不是声称普通用户经常输入 16 个关键词，而是：

- team formation、设施组合、规则/技能覆盖等离线任务可自然产生 9–16 个需求组；
- 近似算法论文需要 exact solver 生成可靠 NWeight 分母，已有 Practical 工作正因 `g>10` 无法得到精确值而受限；
- `g` 是参数化复杂度的主轴，若只测 `g<=8`，无法验证方法声称的较大组数优势；
- WRP 标准组实例为 `g=11..16` 提供与应用叙事无关、可由独立 solver 核验的证据。

避免的 claim：`g=16` 是所有 keyword search 的常见在线负载；所有大图 `g=16` 都能在 10,000 秒内完成；ABHSS 在每个数据集和每个 `g` 都更快。

## 4. 冻结实验矩阵

机器可读配置是 [`experiments/paper_matrix.json`](../experiments/paper_matrix.json)。当前环境共 239 个 case、8,587 个逐方法实例任务：

| Suite | 图/格 | 询问 | 方法 | 任务数 | 论文位置 |
| --- | ---: | ---: | --- | ---: | --- |
| E0_example | 1 | 1 | 10 个 native 方法 | 10 | artifact smoke |
| E0_steinlib_wrp | 11 | 11 | ABHSS、消融、Safe/Strict、DPBF、Basic+、artifact、SCIP-Jack（按兼容范围） | 113 | 正确性主表 |
| E1_controlled_g_f | 66 | 660 | Light、Heavy、Safe | 1,980 | 主文 scaling |
| E2_gpu4gst_related_panel | 68 | 680 | Light、Heavy、Safe | 2,040 | 主文跨图/高 `g` |
| E3_practical_original | 48 | 480 | Light、Heavy、Safe | 1,440 | 主文来源复现 |
| E3_keyword_natural | 14 个 `(graph,g)` | 376 | Light、Heavy、Safe | 1,128 | 主文自然询问 |
| E3_keyword_low_g_appendix | 6 个 `(graph,g)` | 262 | Light、Heavy、Safe | 786 | 附录完整性 |
| E4_gpu4gst_author_panel | 16 | 160 | Light、Heavy、Safe、作者 CPU artifact | 640 | artifact 附录 |
| E5_ablation | 9 | 90 | 两主方法 + 三消融 | 450 | 主文/附录消融 |

### 4.1 `g/f` 协调方案

- `g` 是主参数，主 sweep 严格覆盖 4–16；
- E1 的 `g` sweep 固定每条询问 `f_target=400`，消除平均组大小漂移；
- E1 的 `f` sweep 只在预注册的 `g=6,10,14` 上变化 `f`；
- E3 原始 `(g,f)` 16 格逐字复现，不能只按 `g` 混合；
- E2 和自然 E3 保留来源分布，只以 `f_real` 描述/分层，不冒充 controlled `f`。

### 4.2 GPU4GST 数据量削减

31,200 条扩展查询不进入全量。E2 每格固定 10 条，从 300 条按 `log1p(f_real)` 排名十分位各选一条；八图测 `g=4,8,12,16`，四个代表图补齐所有 `g=4..16`，共 680 条。这个选择兼顾横向覆盖和纵向曲线，且与任一方法运行结果无关。

## 5. 公平运行协议

### 5.1 硬件与软件

论文必须记录 CPU 型号、物理核/逻辑核、socket、内存容量与频率、操作系统、编译器版本、CMake、Boost、Python、SCIP-Jack/SoPlex 版本。所有本仓库算法使用 Release/O2 和相同编译器；不能给 ABHSS 开 native/PGO 而不给 baseline。

首选一台固定 CPU 服务器或同型号节点：

- 每个 solver 只有一个计算线程；RSS sampler 是测量线程，不参与算法；SCIP `parallel/maxnthreads=1`；
- 一个物理 socket 同时只跑一个 timed solver 最干净。若资源要求并行，所有方法使用相同固定 worker 数、固定 core affinity，并额外在固定 12 格上验证 `1 worker` 与目标并发的倍率稳定；
- worker 数不超过物理核数，也不超过按 pilot P95 内存估算的安全数；大图并发时需考虑每个进程都加载一份图；
- 禁止 Turbo/电源策略或后台负载在方法间系统性变化；case 内方法顺序由 case-id SHA-256 循环轮转，不能总让 ABHSS 先/后跑。

### 5.2 计时与超时

- 每条询问独立上限 `T=10,000s`，适用于所有方法和全部 suite；
- deadline 在图和完整询问文件加载、artifact 图转换完成并输出 flushed `[Ready]` 后开始；所有方法自身的组距离、下界、DP 预处理均计入；
- 图加载另有 1,800 秒基础设施 watchdog。触发时标记 `graph_load_timeout` 并修复/重跑，**不能**当作算法 timeout 或完成率数据；
- 每次收到 flushed `[Query i]` 后为下一条重置 10,000 秒。某条超时只杀当前进程、记录该 query，随后重新加载图继续下一条；
- solver crash、OOM、非法输入是 error，不可伪装成 timeout；同一二进制/输入可复现的 error 必须在论文中报告。

选择 10,000 秒而非来源论文的 500/1,000 秒，是因为本文研究区域扩展到 `g=16`，且主 baseline 可能在大图上远慢于 ABHSS。统一的更宽上限减少把“稍慢”误判为 timeout，也能观察真正的数量级差异。附录固定报告由同一运行日志派生的 `1,000s` 完成率，以便和 GPU4GST 协议对照；主结论只用预注册的 10,000 秒原始运行，绝不在不同方法上用不同上限。

### 5.3 内存

主内存指标是 `query_processing_peak_rss_overhead_mb`：从图和全部查询加载后的固定 RSS 基线中，减去该询问期间 1ms 采样的绝对 RSS peak。它排除共享原图，符合 PrunedDP++ 论文的 query-processing overhead 口径。另在数据表报告图文件/加载后 RSS，避免读者误以为总进程只用该 overhead。

SCIP-Jack 当前 runner 不提供同口径 RSS，因此它只进入正确性/时间表，不与 native 方法作内存排名。timeout 进程的 peak 若不完整，不用于 solved-memory 分布。

### 5.4 重复次数

算法确定且单次可能 10,000 秒，主全量矩阵运行一次。为了证明计时稳定，固定做三次独立 calibration：E0、E5，以及 E1 每图 `g=4,8,12,16,f=400` 的 12 格。三次使用不同 `run_id`，但相同输入/二进制；报告 paired runtime 的中位数、CV 和最大答案误差。

若同一方法/cell 的 CV 超过 5%，先排查系统负载；需要重跑时必须重跑该 calibration 的所有方法，而不是只重跑慢的一方。calibration 结果单独汇总，不能与一次性 full run 合并后给这些格额外权重。

## 6. 指标与统计规则

### 6.1 正确性门

每个双方完成的询问检查

```text
abs(w_i - w_ref) <= 1e-6 * max(1, abs(w_ref)).
```

E0 以 SteinLib 已知最优值为 `w_ref`；其他 suite 以通过 E0 的精确方法共识为参考。一旦出现 mismatch，相关性能结果全部隔离，先定位再重跑，不能仅取较小权重当“更优”。

### 6.2 主效率指标

每个 `<suite,dataset,g>` 必须同时报告：

- 总实例数、solved、timeout、error、10,000 秒完成率及 Wilson 95% CI；
- solved-only median、P90 和 query peak RSS；
- PAR-2：完成用实际时间，timeout/error 用 `2T=20,000s`；
- ABHSS 与 Safe 的逐询问配对：双方完成时的 median/geomean speedup及确定性 bootstrap 95% CI；
- `baseline timeout / ABHSS solved`、`ABHSS timeout / baseline solved`、双方 timeout 三个方向计数；
- 跨方法 performance profile，未完成实例在任何有限 ratio 下均不计为完成。

绝不只报告 solved-only 平均时间；它会因 baseline 只剩简单询问而反向奖励 timeout。双方完成上的 geomean speedup也不能单独支撑扩展性 claim，必须紧邻 timeout 方向和完成率。

### 6.3 参数图的聚合

- E1 `g` 曲线按图分别画，或先在图内对 10 条配对、再对图等权聚合；不能让 MovieLens 的边数或某图的更多自然询问自动获得更高权重；
- E1 `f` 曲线保持 `g=6/10/14` 分面；
- E2 先按 68 个 cell 等权总结，再给八图热图。不要把四个密集测量图的额外 `g` 点当作更多独立数据来压过其他图；
- E3 自然询问报告原始 query-weighted 数字，同时提供 dataset-weighted summary；两者明确标注。

## 7. 消融设计

E5 固定在 Toronto、MovieLens、DBLP-VEW21，`g=6,10,14`、`f=400`，每格 10 条：

| 目标 | 对照 |
| --- | --- |
| 提前 A1 / early future | `abhss_light` vs `abhss_light_no_early` |
| recurring witness-tree subset DP | `abhss_light` vs `abhss_light_no_witness`；初始可行上界仍保留，确保只消融 recurring witness |
| Heavy transpose/adjoint 高层闭包 | `abhss_heavy` vs `abhss_heavy_forward`；后者用完整 forward anchored lattice，其他 Heavy 机制不变 |

每一对只报告 paired 时间、peak RSS 和答案一致性。不要跨不同基础方法做“组件贡献相加”，也不要仅选择已有历史收益最大的 query。

## 8. 执行顺序与命令

### 8.1 构建与冻结验收

```powershell
cmake -S . -B build
cmake --build build --config Release --target `
  pruneddp dpbf basic_plus gpu4gst_pruneddp_artifact `
  abhss_light abhss_heavy abhss_light_no_early `
  abhss_light_no_witness abhss_heavy_forward compare_graphs
python tools/build_external_baselines.py --target all
python tools/experiments/validate_environment.py --deep-snapshot
python tools/experiments/run_experiments.py --dry-run
```

推荐阶段：E0 正确性门 → E1 固定 pilot 估算资源 → E1/E3 → E2 大图 → E5/E4/低 `g` 附录。pilot 只能决定 worker 数和调度先后，不能决定哪些询问留在论文。

### 8.2 单机/单 shard

```powershell
python tools/experiments/run_experiments.py `
  --run-id paper_main `
  --run-dir results/paper_runs/paper_main `
  --suite E0_steinlib_wrp
```

重复同一命令会根据原子 JSON record 自动续跑，不覆盖已完成任务。

### 8.3 多节点稳定分片

以 32 shard 的第 0 个为例：

```powershell
python tools/experiments/run_experiments.py `
  --run-id paper_main `
  --run-dir results/paper_runs/paper_main_shard00 `
  --shard-index 0 --shard-count 32
```

其他 worker 只改 shard index 和目录后缀，**全部保持同一个 `--run-id paper_main`**，这样逐方法记录才能正确配对。分片按 case 而非单条 query 哈希：同一 cell 的所有方法和询问留在同一 worker，保留图加载摊销和轮转顺序。

8587 个任务在全部都触发 10,000 秒的理论上界约 994 core-days；主套件约 763 core-days。实际应远低于该值，但仍应准备 16–32 个独立 worker，并按大图 peak 内存限制并发。不能为了赶 deadline 在已看到结果后缩短 baseline timeout。

### 8.4 汇总与画图

```powershell
python tools/experiments/summarize_results.py `
  --input-glob "results/paper_runs/paper_main_shard*" `
  --output results/paper_summary `
  --baseline pruneddp_safe

python tools/experiments/plot_results.py `
  --input-glob "results/paper_runs/paper_main_shard*" `
  --output results/paper_figures `
  --suite E1_controlled_g_f `
  --suite E2_gpu4gst_related_panel
```

`summarize_results.py` 在主精确方法中发现任何 weight mismatch 时返回非零；只有 `quality_mismatches.csv` 为空才能生成论文数字。矩阵把 paper-pathmax 标为 `exact_claim=false`，默认质量门排除它；专门追加 `--include-audit-methods-in-quality` 时应复现三个反例并返回非零。

## 9. 论文表图布局

主文优先顺序：

1. Table 1：数据集、来源限定 ID、`n/m`、询问类型、`g/f`、query 数、图 SHA 前缀；
2. Table 2：E0 `g=11..16` 已知最优值正确性/完成时间，明确 Strict 三个反例；
3. Figure 1：E1 `f=400` 的 runtime/PAR-2 随 `g`，log-y，Light/Heavy/Safe 分开；
4. Figure 2：全部主实例 performance profile + completion-by-`g`；
5. Figure 3：E1 固定 `g=6,10,14` 的 `f` scaling；
6. Figure 4：E2 八图 × anchor `g` 的完成率或 PAR-2 热图，旁列 timeout 方向；
7. Table 3：E3 原始/自然询问的 dataset-level speedup、完成率、内存；
8. Table/Figure 5：三组 E5 paired ablation。

附录：E4 作者 artifact、低 `g` 自然询问、逐 cell 完整表、calibration 波动、所有错误/timeout、配置和许可。不要在摘要使用只来自单个历史 q1 或双方完成子集的最大倍率。

## 10. 投稿前硬门

- [ ] `validate_environment.py` 全通过，数据/commit/hash/二进制冻结；
- [ ] E0 中 Light、Heavy、Safe、DPBF、SCIP 与全部已知最优值一致；
- [ ] 主结果 `quality_mismatches.csv` 为空，所有 error 有解释；
- [ ] 两份 DBLP 在表、CSV、图中始终使用来源限定名称；
- [ ] 所有方法同一 10,000 秒、单计算线程、Release/O2；
- [ ] 主 speedup 同时给 completion、PAR-2 和 timeout direction；
- [ ] E2 选择文件在跑实验前冻结，未根据结果改样本；
- [ ] 没有逐询问 Light/Heavy oracle switch；
- [ ] 近似算法未以不完整质量表干扰精确主线；
- [ ] GPU4GST/GroupSteinerTree 再分发许可已取得，或公开 artifact 移除其源码并提供合法获取步骤；
- [ ] SCIP-Jack 版本和 ZIB Academic License 致谢完整；
- [ ] claim 使用“系统覆盖/扩展可行域”，不声称历史上从未有人测过 `g=9,10`。
