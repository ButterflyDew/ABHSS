# 已采纳优化、性能门禁与冻结证据

本卷保存最终保留或曾通过局部门禁的优化及其测量证据。历史门禁可能对应旧二进制；每节开头的证据边界和后续勘误必须与正文一起阅读。论文正式数字只以 [`../EXPERIMENT_PLAN.md`](../EXPERIMENT_PLAN.md) 指定的冻结身份与正式结果目录为准。

## 卷内目录

- [YouTube 劣势图的本方法专属轻量下界探针](#history-communication-ten-hour-ours-only-youtube-probe) — 原文件 `communication/TEN_HOUR_OURS_ONLY_YOUTUBE_PROBE.md`
- [有序组对路径生长上界探针（2026-08-09）](#history-pair-seeded-path-growth-probe-20260809) — 原文件 `PAIR_SEEDED_PATH_GROWTH_PROBE_20260809.md`
- [有序组三元组一步前瞻路径生长探针（2026-08-09）](#history-triple-seeded-path-growth-probe-20260809) — 原文件 `TRIPLE_SEEDED_PATH_GROWTH_PROBE_20260809.md`
- [三元路径生长上界的 P1 固定成本探针（2026-08-10）](#history-triple-seeded-p1-fixed-cost-probe-20260810) — 原文件 `TRIPLE_SEEDED_P1_FIXED_COST_PROBE_20260810.md`
- [三元路径生长上界的 P1 全量门禁（2026-08-11）](#history-triple-seeded-p1-full-gate-20260811) — 原文件 `TRIPLE_SEEDED_P1_FULL_GATE_20260811.md`
- [Orkut g=15 追加 q10 优化审计](#history-orkut-g15-extra-q10-optimization-audit-20260813) — 原文件 `ORKUT_G15_EXTRA_Q10_OPTIMIZATION_AUDIT_20260813.md`
- [ordinary 证书行状态合并探针（2026-08-14）](#history-packed-certificate-row-state-probe-20260814) — 原文件 `PACKED_CERTIFICATE_ROW_STATE_PROBE_20260814.md`
- [严格支配操作与减空审计（2026-08-15）](#history-dominated-operation-reduction-audit-20260815) — 原文件 `DOMINATED_OPERATION_REDUCTION_AUDIT_20260815.md`
- [单调证书拒绝前沿探针（2026-08-15）](#history-monotone-certificate-frontier-probe-20260815) — 原文件 `MONOTONE_CERTIFICATE_FRONTIER_PROBE_20260815.md`
- [A1 top-two 自适应物化门禁（2026-08-16）](#history-adaptive-a1-top-two-materialization-gate-20260816) — 原文件 `ADAPTIVE_A1_TOP_TWO_MATERIALIZATION_GATE_20260816.md`
- [A1 完整排名、精确租金因子化与 ceiling 负向门禁（2026-08-17）](#history-a1-complete-ranking-and-staged-ceiling-gate-20260817) — 原文件 `A1_COMPLETE_RANKING_AND_STAGED_CEILING_GATE_20260817.md`
- [A1 发布屏障与恒真检查减空门禁（2026-08-17）](#history-a1-publication-barrier-reduction-gate-20260817) — 原文件 `A1_PUBLICATION_BARRIER_REDUCTION_GATE_20260817.md`
- [Certificate-support 增量 subset-DP 门禁（2026-08-18）](#history-incremental-certificate-support-dp-gate-20260818) — 原文件 `INCREMENTAL_CERTIFICATE_SUPPORT_DP_GATE_20260818.md`
- [P1 `g=5/7` 历史弱项非退化抽样（2026-08-18）](#history-p1-g5-g7-history-regression-probe-20260818) — 原文件 `P1_G5_G7_HISTORY_REGRESSION_PROBE_20260818.md`
- [IMDb `g=14` 慢询问 rooted component-cover 门（2026-08-24）](#history-imdb-g14-slow-query-opt24-gate-20260824)
- [IMDb `g=14` 单位权结构证书最终门（V36，2026-08-25）](#history-imdb-g14-unit-structural-v36-gate-20260825)
- [一般加权 tour 一次缓存与 Adjoint 布局恢复门（V61，2026-08-27）](#history-weighted-tour-adjoint-v61-gate-20260827)

- [旧人类实验文档中的优化门禁与 Orkut 风险轨迹（迁移于 2026-08-20）](#history-human-experiment-plan-legacy-gates-20260820) — 原位置 `docs/EXPERIMENT_PLAN.md`

<a id="history-communication-ten-hour-ours-only-youtube-probe"></a>

## YouTube 劣势图的本方法专属轻量下界探针

> 原始记录：`communication/TEN_HOUR_OURS_ONLY_YOUTUBE_PROBE.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

本文档记录 2026-07-31 至 2026-08-01 的候选筛选。它是实验决策记录，不是正式论文实验结果；正式结果必须由冻结提交、独占机器和统一 runner 重跑。

### 1. 公平性边界

- PrunedDP++、Base 与 Enhanced 读取完全相同的图和查询，使用相同超时与资源口径。
- 本轮不把 2-core、叶剥离或度二压缩算作本文方法贡献。若未来使用图核，它必须作为三种算法共同且计时的输入预处理。
- 最终候选只修改 ABHSS 的共同 A1 下界；Base 与 Enhanced 调用同一个函数，不读取配置位。
- 候选没有图名、组数阈值、候选端点数或按运行时间切换的超参数。
- 所有后台探针使用 `nice -n 19` 与 `ionice -c 3`。跨时期总时间只作筛查；性能结论以同机交错面板为准。

### 2. 最终保留方案：endpoint-floor A1 continuation

对每个剩余组集合，tour subset DP 已经保存所有端点对的最短 Hamilton 路径。新增预处理为每个起点组求终点自由的最短路径值，再固定其中最大的起点组。A1 查询把该常量与起点组到当前顶点的距离相加后除以二，并与原 farthest 下界取最大。

该方案的新增预处理为 `O(g^2 2^g)` 时间和 `O(2^g)` 空间；A1 热路径只增加一次组距离读取。cone 与 cone 外 fallback 均调用 `AnchoredSingletonContinuation`，因此没有“强条件剪枝、弱条件解释缺项”的错误。

安全性证明、1-Lipschitz 前缀闭包和 fallback 推导见 `docs/METHOD.md` 第 9.2 节。

### 3. YouTube 全量正确性与状态规模

候选在作者 g=5、g=7 查询上分别运行 Base 与 Enhanced，每格 300 条，共 1200 条。与当前 P1 逐条比较，权值不一致数为 0。

| g | 配置 | 当前状态数 | 候选状态数 | 变化 |
|---:|---|---:|---:|---:|
| 5 | Base | 10,349,023 | 6,980,792 | -32.55% |
| 5 | Enhanced | 9,397,478 | 5,742,550 | -38.89% |
| 7 | Base | 227,501,148 | 90,926,792 | -60.03% |
| 7 | Enhanced | 213,685,968 | 70,750,782 | -66.89% |

随后按作者 g=3、g=5、g=7 的全部 900 条查询重跑候选，Base 与 Enhanced 共 1800 次求解，权值不一致数为 0。候选与历史 P1 不处于同一机器负载阶段，因此分块时间变化只用于筛查；但同一轮候选内部的总时间与冻结的 PrunedDP++ 总时间使用相同求解计时字段，可用于检查最低目标是否已越过。

| 配置 | 当前 P1 总时间 / s | 候选总时间 / s | 变化 |
|---|---:|---:|---:|
| Base | 3030.604 | 2953.212 | -2.55% |
| Enhanced | 3721.936 | 3636.521 | -2.29% |

PrunedDP++ 的 YouTube 全量总时间为 3004.472 秒，因此候选 Base 快 51.260 秒，即 `PrunedDP++ / Base = 1.0174`。旧版在 YouTube 慢 26.132 秒；endpoint-floor 首次使该图越过最低目标。这个余量不大，正式论文结果仍须在独占机器上完整重跑。

旧 P1 全量中其余 12 图原本都满足 `min(Base, Enhanced) < PrunedDP++`。最窄两图为 Toronto（12.908 秒对 16.241 秒）和 Reddit（6182.108 秒对 7294.956 秒）。本轮同机冻结样本中，Toronto 的最快配置 Base 只变化 +0.91%，Reddit 的最快配置 Enhanced 变化 -0.75%，没有显示会翻转旧有余量。因此当前证据支持 13/13 图大概率满足最低目标，但除 YouTube 外不能把抽样替代正式全量重跑。

### 4. YouTube 同机交错计时

固定 7 条有代表性的 g=5/g=7 查询，对 Base、Enhanced 各运行 3 轮；奇偶轮反转当前版与候选版顺序。共 42 个配对、84 次求解，权值不一致数为 0。候选在 29 个配对更快、13 个配对更慢。

| 范围 | 配置 | 当前总时间 / s | 候选总时间 / s | 时间变化 | 状态变化 |
|---|---|---:|---:|---:|---:|
| g=5 | Base | 22.523 | 23.002 | +2.13% | -12.06% |
| g=5 | Enhanced | 49.605 | 49.339 | -0.54% | -7.60% |
| g=7 | Base | 103.207 | 97.975 | -5.07% | -40.62% |
| g=7 | Enhanced | 105.612 | 99.227 | -6.05% | -45.49% |
| g=5,7 | Base | 125.730 | 120.977 | -3.78% | -39.82% |
| g=5,7 | Enhanced | 155.216 | 148.566 | -4.28% | -44.57% |

g=5 Base 的 2.13% 回退属于需要在正式独占重跑中继续观察的边界，不能被隐藏。总体及核心 g=7 劣势区间均改善，且状态削减远大于计时噪声。

随后用正式合入且把新增预计算降为 `O(g^2 2^g)` 的二进制，单独对 g=5 Base 的三条查询做 3 轮反序交错复测。9 个配对全部权值一致，当前版与正式候选总时间分别为 22.306 s 和 22.323 s，变化为 +0.08%；状态下降 12.06%。因此现有证据只能把该边界判断为持平，不能声称稳定加速，也没有证据支持有意义的性能退化。

### 5. 其他图

Musae、Twitch、Github 各取前 10 条作者查询并同时运行 Base/Enhanced。候选相对当前版的聚合时间变化如下。

| 图 | Base | Enhanced |
|---|---:|---:|
| Musae | +1.55% | -6.15% |
| Twitch | +0.37% | -3.88% |
| Github | -10.21% | -18.32% |

单条补充探针中，DBLP 基本持平，LinkedMDB 的 Base 从 3.02 s 降至 2.72 s，Enhanced 从 4.87 s 降至 4.82 s。DBLP、LiveJournal、Orkut、LinkedMDB 的扩大面板另行保存于 `/tmp/floor_cross_panel/runs.tsv`；`/tmp` 文件不是可复现实验产物，最终数字需进入正式 runner 后再引用。

扩大面板共 22 个当前版—候选版配对，权值不一致数为 0。DBLP、LiveJournal、Orkut、LinkedMDB 的 Base 状态分别下降 20.56%、53.24%、30.66%、26.44%；Enhanced 分别下降 23.04%、53.47%、46.90%、26.46%。单轮 Orkut Enhanced 时间曾回退 8.56%，但其旧版自身也明显偏离历史 P1，不能据此判断退化。对最异常的 Orkut g=5 第 3 条 Enhanced 查询追加 3 轮反序交错复测后，总时间从 172.757 s 降至 167.835 s（-2.85%），状态从 1,107,810 降至 553,698（-50.02%），三轮权值全部一致。该复测支持把先前异常归为系统波动，但正式独占 P1 仍是最终证据。

#### 5.1 冻结的 12 图同机样本

为消除跨日期 CPU 漂移，本轮又冻结 75 条查询，并分别以当前二进制和候选二进制运行 Base/Enhanced，共 300 次求解。两份结果各 150 行，状态迁移为 0，权值不一致为 0。下表只列 `g>3` 的 active 样本；GPU 图另有 `g=3,q=150` 无操作控制，未发现系统性候选开销。

| 图 | Base 时间变化 | Base 状态变化 | Enhanced 时间变化 | Enhanced 状态变化 |
|---|---:|---:|---:|---:|
| DBLP-GPU4GST | -0.60% | -9.62% | -0.93% | -23.39% |
| DBLP-MonoGSTPlus | +3.49% | -0.57% | -1.76% | -0.03% |
| DBpedia-MonoGSTPlus | -12.63% | -53.44% | -27.56% | -76.09% |
| Github-GPU4GST | -12.08% | -44.88% | -20.07% | -73.04% |
| LinkedMDB-MonoGSTPlus | -12.33% | -45.14% | -22.20% | -57.37% |
| LiveJournal-GPU4GST | -1.80% | -52.31% | -1.65% | -63.22% |
| MovieLens-MonoGSTPlus | +0.40% | 0.00% | +1.09% | 0.00% |
| Musae-GPU4GST | +1.38% | -24.25% | -5.86% | -55.36% |
| Orkut-GPU4GST | -4.80% | -40.58% | -2.71% | -51.12% |
| Reddit-GPU4GST | +1.96% | 15→4 | -0.75% | 15→4 |
| Toronto-MonoGSTPlus | +0.91% | 0.00% | +0.79% | 0.00% |
| Twitch-GPU4GST | -1.61% | -6.87% | -3.10% | -29.20% |

DBLP-Mono Base 是唯一需要复核的非短查询边界。对 g=5 的 q=21 与 g=8 的 q=141 做三轮反序交替后，当前版合计 119.291 秒，候选为 120.720 秒，即候选慢 1.20%，状态完全相同。该小幅固定成本应如实保留；但 DBLP-Mono 的 P1 最快配置是 Enhanced，旧全量为 3275.496 秒，远快于 PrunedDP++ 的 29049.935 秒，且候选 Enhanced 样本反而快 1.76%，不会威胁论文最低目标。

#### 5.2 P2 的 YouTube 与 DBLP 跨组数样本

P2 在 YouTube 与 DBLP-GPU4GST 上对每个 `g=5..16` 冻结一个中等规模分层查询。每格运行当前 Base、候选 Base、当前 Enhanced、候选 Enhanced 与 PrunedDP++，共 24 格、120 次求解，超时为 600 秒。权值不一致格数为 0，候选相对当前版的超时迁移数为 0。

| 图与配置 | 共同完成格数 | 候选时间变化 | 候选状态变化 |
|---|---:|---:|---:|
| DBLP Base | 10 | +2.22% | 0.00% |
| DBLP Enhanced | 12 | -0.20% | 0.00% |
| YouTube Base | 10 | -0.21% | -0.02% |
| YouTube Enhanced | 12 | -2.17% | -0.12% |

YouTube 的 12 个组数样本中，`min(候选 Base, 候选 Enhanced)` 全部快于 PrunedDP++。DBLP 仅 g=5、g=6 的低组数样本略输；g=7 至 g=16 的 10 格全部获胜或 PrunedDP++ 超时。代表性的高组数结果包括 YouTube g=16 Enhanced 245.934 秒而 PrunedDP++ 超时，以及 DBLP g=15 Enhanced 412.705 秒而 PrunedDP++ 超时。P2 没有触发“明显劣化后继续研究”的停止门。

### 6. 被否决候选

- A1 top-two exact cache：值完全相同，但 g=5 经常变慢且增加持久数组。
- 静态 group-MST/2：安全但几乎总被 farthest 支配，状态下降不足 0.1%。
- 完整 matched tour：YouTube 剪枝很强，但每个 A1 顶点扫描端点表，Musae/Twitch Base 回退 50%–90%。
- 单起点完整 tour：把热路径降为 `O(g)` 后仍使小图 Base 回退约 20%–33%。
- all-endpoint floor：YouTube 更强，但 Musae Base 聚合回退 13.7%，因此不能作为无分支默认方案。
- strict query kernel：有价值，但必须公平地同时提供给 baseline；不属于“仅本方案实施”的最终贡献。
- farthest-first 延迟 endpoint 读取：数学上与保留方案等价，但 DBLP-Mono 两条查询三轮交替中，当前版合计 122.244 秒，延迟版为 126.886 秒，慢 3.80%；状态完全相同。新增热路径分支没有抵消常数读取，已回退。

最终只保留一个方案，避免为了凑数引入第二个证据不足或需要经验开关的优化。

### 7. 正式实验门

1. 用独占提交重新构建，不复用本轮 `/tmp` 二进制。
2. 运行全部 CTest、Markdown 门禁和小图独立精确 DP 回归。
3. 重跑 YouTube g=5/g=7 全 300 条 Base/Enhanced，并与 PrunedDP++ 使用同一 runner、超时和计时口径。
4. 至少重跑 Musae、Twitch、Github 的 10 条边界面板；若 Base 的小幅回退超出重复运行噪声，不得声称“全图不退化”。
5. 论文把 endpoint-floor 写成 Base/Enhanced 的共同 A1 操作，不写成 Enhanced 开关，也不为它增加经验参数。

<a id="history-pair-seeded-path-growth-probe-20260809"></a>

## 有序组对路径生长上界探针（2026-08-09）

> 原始记录：`PAIR_SEEDED_PATH_GROWTH_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> 状态：已被同日的有序组三元组一步前瞻版本严格包含并替代；本文只保留 pair 版演进证据。

本文只保留纳入主线前的探索证据；输入均为 `experiment_data/p2_cross_g` 的正式 P2 panel，不是 GPU4GST 的 300 条源查询。数值是同机单进程探针，不替代冻结矩阵的正式复跑。

### 方案

枚举全部有序组对，以组距离 tight path 建立初始连通树，再反复接入距当前树最近的未覆盖组。边按原图 ID 去重并按真实权值计费。非负边权使累计费用单调不减，因此既有 incumbent 是无参数的安全停止条件。Base、DirectedCutOnly 和 Enhanced 调用同一实现。

### 关键结果

| 图与查询 | 配置 | 主线 time / MiB / states | 候选 time / MiB / states | 观察 |
|---|---:|---:|---:|---:|---|
| Orkut g16 q2 | Enhanced，阶段诊断 | A1 states 39,255,744 | A1 states 36,224,374 | 上界 52 降到 46；A1 状态减少 7.72% |
| Orkut g16 q4 | Enhanced | 1190.457 / 2722.559 / 26,517,767 | 1191.607 / 2722.559 / 26,517,767 | 状态与空间不变，时间 +0.10% |
| Orkut g15 q3 | Enhanced | 284.043 / 2689.770 / 30,674,269 | 286.407 / 2689.770 / 30,674,269 | 状态与空间不变，时间 +0.83% |
| Musae g15 q1 | Base | 580.134 / 514.492 / 42,558,822 | 573.826 / 512.793 / 42,426,446 | 时间 -1.09%，状态 -0.31% |
| Twitch g16 q1 | Enhanced | 144.423 / 554.281 / 40,494,123 | 142.576 / 554.281 / 40,494,123 | 候选被 incumbent 筛掉；状态与空间不变 |

Orkut g16 q2 的 A1+D2 累计候选状态为 266,528,577；扣除候选 A1 后，D2 增量为 230,304,203。历史正式诊断中的 D2 layer-only 状态为 262,798,507，二者口径不同，不能把累计值直接相除；按增量比较约减少 12.36%。

### 决策

方案具备真实可行上界证明、零权恢复安全性和无经验参数，在目标 Orkut g16 q2 上显著降低前两阶段状态，同时在边界与跨图样本上没有可重复的状态或空间退化。因此接入主线；完整收益仍须由后续冻结 P1/P2 复跑确认。Kruskal 后处理没有把候选 46 继续降低，未接入。

<a id="history-triple-seeded-path-growth-probe-20260809"></a>

## 有序组三元组一步前瞻路径生长探针（2026-08-09）

> 原始记录：`TRIPLE_SEEDED_PATH_GROWTH_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

本文记录纳入主线前的方向证据。所有 Orkut、Twitch、Musae 和 Youtube 输入均来自 `experiment_data/p2_cross_g` 的正式 P2 panel；运行是同机单进程或明确绑定两个独立核的同时 A/B，不替代冻结矩阵正式复跑。

### 方法与包含关系

对互异有序组三元组 `(i,j,k)`，先恢复第二组到第一组的真实最短种子路径，再显式接入第三组，随后恢复最近未覆盖组贪心。枚举所有第三组严格包含旧 pair-seeded 版本：旧版第一次贪心选择的组必在枚举中。全部路径按原图边 ID 去重并按真实非负边权计费；累计费用达到 incumbent 后安全停止。Base、DirectedCutOnly 与 Enhanced 调用同一函数。

### 结果

| 图与查询 | 配置 | 当前主线或 pair 版 | 三元候选 | 结论 |
|---|---|---:|---:|---|
| Orkut g16 q2 A1 gate | Enhanced | upper 46；A1 36,224,374 states | upper 43；A1 33,786,194 states | 相对 pair 再减 6.73%；相对旧主线 A1 减 13.93% |
| Orkut g15 q2 A1 gate | Enhanced | upper 37 | upper 36；A1 29,132,133 states | 跨 g 正信号，精确值 32 |
| Orkut g16 q4 完整 | Enhanced | 1191.607 s / 2722.559 MiB / 26,517,767 | 1198.809 s / 2722.555 MiB / 26,517,767 | 状态空间相同，时间 +0.60% |
| Twitch g16 q1 完整 | Enhanced | 142.731 s / 554.277 MiB / 40,494,123 | 36.101 s / 179.250 MiB / 6,950,489 | 时间 -74.7%，空间 -67.7%，状态 -82.8% |
| Twitch g15 q1 完整 | Enhanced | 历史主线 260.910 s / 661.965 MiB / 51,107,932 | 258.857 s / 661.832 MiB / 51,107,932 | 状态空间相同，时间持平 |
| Musae g15 q1 完整 | Base | pair 573.826 s / 512.793 MiB / 42,426,446 | 572.622 s / 510.316 MiB / 42,191,066 | 三项均不退化 |
| Youtube g16 q1 同时 A/B | Enhanced | 255.304 s / 537.754 MiB / 14,563,709 | 253.770 s / 537.754 MiB / 14,563,709 | 状态空间相同，时间持平 |

全部完整项最优值与当前主线一致。Orkut q4 的 0.60% 时间差没有状态或空间变化，属于额外有限枚举的边界成本；Twitch 的数量级收益与 Orkut q2 的上界/A1 收益表明该前瞻改变了有意义的搜索锥体，而不是只优化实现常数。

### 决策

该机制无经验超参数、严格包含旧候选、构造的每个上界均可展开为原图真实连通覆盖，且跨 Base/Enhanced 共用。因此替换 pair-seeded 版本进入主线。最坏时间从 pair 版的 `O(g^3(m+n))` 提升为保守 `O(g^4(m+n))`，临时空间仍为 `O(m+n+F)`；实际由 incumbent 单调筛掉大量起点。正式论文数值仍以后续冻结 P1/P2 全量复跑为准。

<a id="history-triple-seeded-p1-fixed-cost-probe-20260810"></a>

## 三元路径生长上界的 P1 固定成本探针（2026-08-10）

> 原始记录：`TRIPLE_SEEDED_P1_FIXED_COST_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> 历史记录：本文的抽样结论已由 [2026-08-11 全量门禁](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-triple-seeded-p1-full-gate-20260811)取代。特别是旧 Youtube 缺口属于后续 endpoint-floor 修改前的版本，不代表当前代码。

### 1. 问题

共同的三元一步前瞻上界会在正式状态搜索前分配图规模工作区并恢复真实 tight paths。它可能用更紧上界减少后续状态，也可能在上界没有改善时只留下固定成本。本探针只回答两个问题：

1. 它是否会破坏 P1 中“小组数不劣于 PrunedDP++”的目标；
2. 是否可以在不改变候选集合的前提下压低物理生成成本。

`g<=3` 查询由共同精确闭包直接返回，完全不调用本上界。P1 的 GPU4GST 查询中只有 `g=5,7` 会支付该成本。

### 2. 最终物理实现

论文算法仍枚举全部互异有序组三元组。实现把公共前缀按有序组对因子化：对每个 `(first, second)` 只恢复一次真实种子路径，保存其 `edge_id`，再给每个 `forced` 第三组重放同一批边。与逐三元组恢复相比：

- 候选三元组、种子终端、tight path、第三组顺序和后续贪心完全不变；
- 最终权值与精确状态答案完全一致；
- 最坏复杂度仍保守记为 `O(g^4(m+n))`；
- 只消除同一种子路径的重复恢复，不增加经验阈值或数据相关开关。

曾测试把图规模工作区改成持久 `thread_local` 缓存。它只有约 1% 的时间收益，却让 Youtube 的数组跨越预处理并与状态搜索峰值重叠，峰值 RSS 增加约 15 MiB，因此拒绝。最终实现保留查询内局部工作区，使其在主状态搜索前释放。

### 3. 探针结果

父版本为提交 `672bd253cddefaddf9da7545314f1d34638cc54f`，不含三元上界。`triple` 为最终因子化实现。父版本和 triple 固定到相邻独立核心并行运行；表中时间为同一查询块求和，空间为查询峰值最大值。PrunedDP++ 来自服务器现有 `p1_full` 的相同查询记录，仅用于判断余量，不把跨时段微小差异解释成严格加速。

| 图与配置 | 查询 | 父版本时间（s） | triple 时间（s） | 时间变化 | 状态变化 | triple 相对 PrunedDP++ |
|---|---:|---:|---:|---:|---:|---:|
| Musae Base | `g=5`，300 条 | 13.262 | 12.438 | -6.22% | -61.18% | -20.86% |
| Musae Enhanced | `g=5`，300 条 | 12.936 | 13.088 | +1.18% | -20.29% | -16.73% |
| Orkut Base | `g=5`，前 10 条 | 295.378 | 287.301 | -2.73% | -50.04% | -32.10% |
| Youtube Base | `g=7`，前 20 条 | 127.788 | 128.455 | +0.52% | -12.43% | +20.04% |
| Youtube Enhanced | `g=7`，前 20 条 | 139.084 | 141.138 | +1.48% | -4.86% | +31.89% |
| Toronto Base | 全 160 条 | 13.196 | 13.224 | +0.21% | -18.69% | -18.58% |
| Toronto Enhanced | 全 160 条 | 17.180 | 17.423 | +1.42% | -13.87% | +7.28% |

所有成对运行的答案权值一致。回归测试为 5/5 通过。

### 4. 结论与论文边界

三元上界的净影响不是统一固定开销：它在 Musae 和 Orkut 通过减少状态取得净收益，在 Youtube、Toronto Enhanced 这类上界收益较弱的查询上留下约 0.2% 至 1.5% 的净成本。该量级不会把原本有明确余量的 P1 图翻转为劣势。

当前不能声称 P1 已经逐图满足目标。Youtube 的父版本在没有三元上界时已经弱于 PrunedDP++，Toronto Enhanced 也已经略弱；三元上界只分别增加约 0.5% 至 1.5%，不是这些缺口的根因。撤掉上界不能弥补 Youtube 约 20% 至 32% 的缺口，也不能解决其状态结构问题。

按已有 13 图 P1 归档，旧版总时间为 Base 90,825 s、Enhanced 107,849 s、PrunedDP++ 232,414 s，所以全量总时间目标有很大余量。真正未满足的是“任何图上 PrunedDP++ 都不同时胜过 Base 与 Enhanced”的更严格逐图底线，当前仍需专门优化 Youtube；不能用全量总时间掩盖这一点。

最终保留三元上界，理由是：

1. 它是 Base 与 Enhanced 完全共同的真实可行上界，不破坏配置包含/替换叙事；
2. 在多个 P1 与高组查询上显著减少状态；
3. 因子化后已把可消除的重复恢复去掉；
4. 现有 P1 失败点来自后续搜索结构，而不是这项至多约 1.5% 的净成本。

后续 P1 验证必须同时报告父版本、最终版本和 PrunedDP++ 的同询问结果；Youtube 的改进不得通过图名、组数阈值或整数权特化实现。

<a id="history-triple-seeded-p1-full-gate-20260811"></a>

## 三元路径生长上界的 P1 全量门禁（2026-08-11）

> 原始记录：`TRIPLE_SEEDED_P1_FULL_GATE_20260811.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 目的与对照

共同的有序组三元组路径生长在正式状态搜索前生成真实可行上界。它会支付路径恢复与候选树维护的固定成本，也可能借助更紧的 incumbent 减少后续状态。本门禁回答：最终因子化实现是否会破坏 P1 中“小组数不劣于 PrunedDP++”的目标，以及看似能减少循环的物理改写是否真的有收益。

主对照冻结同一份代码，只用编译期探针跳过三元上界；正式源码不保留该开关。两边在 GPU4GST Youtube 上运行 P1 的全部 900 条查询，即 `g=3,5,7` 各 300 条。每对进程固定到相邻独立物理核。权值逐条一致。`g=3` 由共同精确基例闭包，不进入三元上界，因此只用于估计两核与运行时段差异。

### 2. 全量结果

| 查询块 | 启用三元上界时间（s） | 跳过三元上界时间（s） | 时间比 | 启用状态数 | 跳过状态数 | 状态比 | 启用/跳过峰值（MiB） |
|---|---:|---:|---:|---:|---:|---:|---:|
| `g=3`，300 条 | 308.070572 | 306.934922 | 1.003700 | 0 | 0 | -- | 62.340 / 62.328 |
| `g=5`，300 条 | 795.476392 | 779.948115 | 1.019909 | 5,888,677 | 6,980,792 | 0.843554 | 94.855 / 95.191 |
| `g=7`，300 条 | 1,906.895181 | 1,900.019610 | 1.003619 | 74,359,677 | 90,926,792 | 0.817797 | 127.207 / 126.297 |
| 全部 900 条 | 3,010.442145 | 2,986.902647 | 1.007881 | 80,248,354 | 97,907,584 | 0.819634 | 127.207 / 126.297 |

裸时间增加 0.788%，累计状态减少 18.037%，查询峰值只增加 0.910 MiB。用不执行三元上界的 `g=3` 时间比校准相邻核差异后，启用侧折算时间为 2,999.344660 s，相对跳过侧增加 0.417%。校准只用于风险判断，不替代最终同版本正式 P1 结果。

旧的 endpoint-floor P1 归档中，Youtube Base 为 2,953.211649 s，PrunedDP++ 为 3,004.472131 s，时间差为 51.260482 s。不同运行时段不能用于宣称微小加速，但即使采用未校准的 0.788% 作为保守固定成本，也没有吃完旧结果约 1.71% 的余量。由于 P1 预注册为按图汇总全部源查询，而不是逐个 `g` 选择胜者，现有证据支持“风险很小、最终仍需同版本全量门禁”，不支持把 Youtube 写成显著获胜。

### 3. 两个被拒绝的固定成本改写

最终因子化实现对每个有序前两组只恢复一次 seed tight path，但在枚举第三组时重放已保存的真实 `edge_id`；覆盖掩码只在候选需要分支或继续生长时扫描当前树。为确认这不是遗漏的明显优化，另测试以下改写：

1. `incremental-covered`：每次 `AddVertex` 都把 `membership[v]` 并入持久覆盖掩码，清树时归零；
2. `checkpoint`：在 seed 后保存并回滚候选树 checkpoint，避免为每个第三组重放 seed 边。

Youtube `g=5` 前 50 条做两轮 CPU10/CPU11 换位。所有版本的最优权值和 1,883,287 个状态逐条一致。按同一 CPU 比较：

| CPU | 冻结原始按需版本（s） | `incremental-covered`（s） | 变化 | `checkpoint`（s） | 变化 |
|---|---:|---:|---:|---:|---:|
| CPU10 | 162.817694 | 164.365758 | +0.951% | 163.177056 | +0.221% |
| CPU11 | 159.692819 | 162.323982 | +1.648% | 160.864737 | +0.734% |

`incremental-covered` 把一次低频按需扫描换成了每次加点都执行的 OR 与写依赖，实际增加热路径成本。`checkpoint` 虽少重放 seed 边，却增加 checkpoint 保存、回滚和更长的活动状态；两颗核上也都没有胜过原始实现。二者均拒绝，不在正式代码中保留开关、结构或探针分支。

### 4. 恢复前必要费用短路与空后缀门禁（2026-08-12）

后续审计发现，路径生长已经扫描得到当前真实树到目标组的最短距离，却仍会先恢复整条 tight path，再检查累计费用。最终实现统一检查“已付树费用 + 当前树到目标组的最短距离”是否仍严格小于 incumbent；失败时任何连通扩展都不可能产生更优候选，因此可在 DFS 前返回。种子、显式第三组、购买后的第四组和后续最近组调用同一个无参数条件。另一个独立固定成本是 Enhanced 的 H 后缀为空时，旧入口仍保留并复制没有 H 消费者的 A1 row；最终入口直接复用完整前向完成，不构造转置工作区。

冻结旧二进制与最终二进制在 CPU10 上对 Youtube `g=5` 前 50 条做两轮换序。CPU11 始终运行同一条 Orkut 长查询；该负载使绝对时间偏高，但四次运行覆盖同一查询且顺序反转，所以只用于因果不退化门禁，不进入论文时间表。

| 配置 | 旧版两轮（s） | 最终版两轮（s） | 变化 | 每轮状态数 | 权值/状态不一致 | 旧版/最终峰值（MiB） |
|---|---:|---:|---:|---:|---:|---:|
| Base | 328.459380 | 326.012930 | -0.745% | 1,883,287 | 0 / 0 | 94.105 / 92.027 |
| Enhanced | 416.977378 | 414.869083 | -0.506% | 1,580,495 | 0 / 0 | 181.949 / 181.801 |

Base 不经过空 H 后缀分支，所以其稳定收益单独证明恢复前必要条件没有固定成本回退；Enhanced 的结果证明两项修改合并后也不退化。两种配置的状态逐条相同，说明本 panel 上最终上界和主搜索完全未变，收益来自少做无效恢复/复制，而不是偶然改变状态空间。

### 5. 最终哈希二进制的 P1 隔离复跑（2026-08-12）

在固定 CPU10、没有第二个 ABHSS 求解进程竞争的条件下，最终二进制对 Youtube P1 的 900 条作者查询完整运行。注释审计后重新构建的 SHA-256 仍为 `8bbce11a3b07964db3b31bc871456a4ce42867e538b63e7bc1695935d5ddc32d`，与本轮 P1 二进制逐字节相同。冻结 PrunedDP++ 只作为既有同服务器参考；本门禁用于判断接入风险，不替代投稿前统一机器策略下的正式三方法重跑。

| 查询块 | 最终 Base 时间（s） | 冻结 PrunedDP++ 时间（s） | Base / PrunedDP++ 状态数 | Base / PrunedDP++ 峰值（MiB） | 权值不一致 |
|---|---:|---:|---:|---:|---:|
| `g=3`，300 条 | 310.273993 | 522.517926 | 0 / 35,090 | 62.285 / 98.367 | 0 |
| `g=5`，300 条 | 790.356155 | 896.138232 | 5,888,677 / 845,943 | 97.789 / 137.461 | 0 |
| `g=7`，300 条 | 1,893.455101 | 1,585.815973 | 74,359,677 / 11,569,236 | 122.809 / 408.125 | 0 |
| 全部 900 条 | 2,994.085249 | 3,004.472131 | 80,248,354 / 12,450,269 | 122.809 / 408.125 | 0 |

P1 预注册口径是每张图汇总全部源查询。最终 Base 因此以 10.386882 秒、约 0.346% 的直接余量通过 Youtube 这张最窄边界图；不能据此声称每个 `g` 都胜出，因为 `g=7` 仍慢于 PrunedDP++。三元上界在 `g=3` 精确基例中不执行；`g=5,7` 的状态数与上一轮启用三元上界的全量结果完全相同，说明恢复前必要费用短路只删除没有候选作用的 tight-path DFS，没有依靠改变搜索域越过门禁。跨运行时段的 16.356896 秒变化不作加速归因，固定成本因果结论仍以第 4 节反序 A/B 为准。

### 6. 最终决定

- 保留按有序组对因子化的三元上界，因为它在 Youtube 全量查询上以约 0.42% 的校准净成本减少约 18% 状态，并在更难查询上更可能转为净收益。
- 保留按需覆盖扫描、seed `edge_id` 重放和恢复前必要费用短路；它们均由换位实验验证，优于更复杂的增量/checkpoint 实现且不改变可改善候选。
- 不增加图名、组数、时间、row 密度或 incumbent 触发开关。三元候选集合、恢复顺序和终止合同在所有配置上相同。
- [上一轮抽样探针](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-triple-seeded-p1-fixed-cost-probe-20260810)只保留为历史记录；其中关于旧 Youtube 缺口的判断已被后续 endpoint-floor 修改和本次全量门禁取代，不能再作为当前结论引用。

<a id="history-orkut-g15-extra-q10-optimization-audit-20260813"></a>

## Orkut g=15 追加 q10 优化审计

> 原始记录：`ORKUT_G15_EXTRA_Q10_OPTIMIZATION_AUDIT_20260813.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> **状态：局部等价优化保留，旧 q10 整体门失效。** 本文记录的证书因子化、缓存与单调拒绝前沿仍是输入无关的安全工作消除；但最终长测二进制继承了缺失辅助 $H(h)$ 的精确性错误。文末 9,934.603 秒只作当前复跑风险估计，不能作为当前答案、状态或 10,000 秒门禁。修复见 [辅助半层 Adjoint 正确性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-auxiliary-half-adjoint-correctness-audit-20260815)。

> **后续状态（2026-08-19）。** 修复后的冻结生产二进制已完成 Orkut g15 q1--q10，q10 为 9,544.561 秒、精确值 54、状态数 1,459,398,194；13 图 P1 逐图底线也全部通过。本文其余时间仍按原意保留为错误旧版或中间候选的历史数据，不能替换最终表。最终身份见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

### 1. 问题定位

P2 扩样后的 Orkut `cross_g15.txt` q6--q10 使用提交 `589894be3774ff6658e1120f9a9e899a53377ab1` 的 Enhanced、每查询 10,000 秒。q6、q7、q8、q9 均完成，q10 timeout。q10 的输入侧统计为 `mean_f=309.8`、`min_f=56`、`max_f=793`；它并不是追加五条中平均组最大的查询。

诊断版 q10 的共同预处理约 255 秒，初始可行上界为 62。共同 A1 约 113 秒、产生 39,056,188 个标量。Enhanced 在 D2 第 2 行买入 residual closure；购买后把已发布 ordinary 标量从 4,600,116 降到 1,169,801，并把上界收紧到 59。到 10,000 秒仍在处理大规模 ordinary 状态，最终结果、峰值和状态数不可用。对照 q6 虽也很难，但在 7,396.080 秒完成并产生 452,486,374 个状态。

PrunedDP++ 的短诊断不能解释为上界优势：q10 完成组距离预处理后，在 65K、262K、1.05M、4.19M 状态处的 incumbent 依次为 70、67、67、67，均弱于 Enhanced 的 59。q10 的核心矛盾是 ABHSS ordinary 状态域与现有 future 强度的组合，而不是 baseline 已经找到更好的可行树。

### 2. 第一阶段：拆分因子化与阶段证书缓存

对固定 ordinary row 和顶点，全部规范拆分共享同一 future。候选先精确聚合逐顶点最小 split，再只对最终 seed 求一次 future；size 2 只有一个规范拆分，仍走单遍路径。DirectedCut 配置还把 dual、farthest、公共 A1、tour 组织成阶段缓存：较大标签在中间阶段被拒绝后，较小标签可复用已算下界并继续尚未完成的阶段。Base 保留原有 flat realization，避免为不存在的 dual 早停支付阶段分支。

该机制不改变状态域。1200 秒同核诊断中，候选完成 79 张 ordinary row，冻结对照完成 58 张；对应 row 的 `best`、状态和 primitive work 逐项相同。候选累计消除了 3,327,089,563 次重复证书求值。q6 完整结果从 7,396.080169 秒降到 4,438.446981 秒，权值仍为 36，状态仍为 452,486,374，空间为 6,019.934 MiB。

但该阶段的 q10 正式 Release 门仍跑满 10,000 秒，结果文件只有 header。故它是有效的一般物理优化，却尚不足以单独解决新增 q10。

### 3. 第二阶段：完整 directed-cut fallback 复用

residual closure 后的转置查询先以缓存全势减去已覆盖组势，得到带标准浮点误差界的 certified interval。区间下端是可采纳值，但不足以代表完整证书；若 upper endpoint 已让当前标签严格通过，所有后续更小标签也会通过，因而可结束 dual 判定。只有区间不能判定而逐组求和时，或 closure 前本来就逐组求和时，返回值才是固定 `(mask,vertex)` 的完整 directed-cut 证书。新候选让 API 同时返回 `exact` 标志：当 exact 和拒绝当前标签时，该标志允许 ordinary staged cache 仍把 dual 阶段标为完成。后续更小标签复用逐位相同的 `double` 和，不压缩精度、不改求和顺序，也不缓存前一个标签的拒绝结论。

严格 1,200 秒总墙钟门包含约 39 秒图加载、254 秒预处理、112 秒 A1 和约 220 秒 residual closure。最终完成全部 91 张 size-2 row；第一阶段同口径完成 79 张，提升约 15.2%。全部对应 row 的 `best`、标量数和 primitive work 仍逐项相同。closure 前两张大 row 的缓存拒绝分别为 24,504,909 和 26,083,169 次，全部命中已经计算过的完整 dual 和；closure 后每 row 仍通常复用约 470 万至 980 万次 exact fallback。

正式 P1 门禁使用相同算法源码、无诊断宏。最终 Base 的 300 条 Youtube g7 答案和状态与冻结版逐条一致，时间从 1,890.144916 秒降到 1,873.186241 秒（-0.90%），状态均为 74,359,677；query-level 最大 RSS overhead 从 122.863 MiB 变为 128.035 MiB（+5.172 MiB，+4.2%）。条件分配 `bound_stage` 后，最终 Base 前 60 条为 382.221554 秒、122.008 MiB、16,793,059 状态；冻结版同前缀为 385.787714 秒、120.094 MiB、相同状态。源码已经少分配约一 byte/vertex 的 Base-only 无用阶段数组，故全量最大值差异来自 1 ms RSS 采样和分配器瞬时峰；审计仍如实保留该数字，不把它声称为空间改进。Enhanced 的 exact-cache 全量结果为 2,123.472070 秒、218.508 MiB、67,570,613 状态；加入 exact 标志前同一候选为 2,119.582756 秒、217.859 MiB、相同状态，答案和状态逐条零差异，时间差为 +0.18%，按同机波动视为等价门通过而不声称加速。

该阶段的 q10 正式门最终也跑满 10,000 秒，结果文件只有 header。随后加入 ordinary tour 上包络后，q10 在 CPU2 的外层 10,070 秒门内仍未产生结果行；其中图加载约 39.25 秒。因此截至这一阶段，q10 只是擦近边界，尚未解决。

### 4. 第三阶段：无深度的最远组 oracle 等价工作消除

旧实现已经为每个顶点保存唯一的全局最远组下标。构造该下标时，每次比较都重新通过 `GroupRow` 读取当前最远组；在完整势 realization 中，argmax miss 后的剩余组扫描也反复经过已经不再需要的布局分派；H 前缀则另写了一份 included-mask 扫描。当前候选不增加任何排名缓存：预处理在一次组扫描中用局部 `farthest_value` 维护唯一 argmax；完整势 miss 直接读取已知 dense 的底层值数组，bounded realization 仍走原 oracle；D、A、H 统一调用同一个 mask 最大距离函数。

这些操作不改变缓存深度、距离值、mask、比较顺序、状态域或浮点加法。唯一 argmax 数组仍是一 byte/vertex，空间与冻结版相同；规则不读取图名、组数、权值类型、状态量或运行时间。五项 CTest 全部通过。正式 Release 短门如下：

| 门禁 | 冻结版 | 无深度候选 | 结果 |
| --- | ---: | ---: | --- |
| Orkut g15 q3，CPU2 | 379.715 s | 376.944 s | 权值 38、状态 31,051,638 均相同 |
| Orkut g15 q4，CPU2 | 426.485 s | 412.695 s | 权值 28、状态 15,061,435 均相同 |
| Orkut g15 q8，CPU2 | 281.632 s | 279.067 s | 权值 18、状态 366,330 均相同 |
| Orkut g15 q9，CPU2 | 1,961.608 s | 2,145.202 s | 权值 35、状态 135,705,582 均相同；非同时运行，不能声称时间收益 |
| Youtube P1 Enhanced 前 60 条，CPU3 | 421.829 s | 419.024 s | 答案、状态逐条相同 |
| Youtube P1 Base 前 60 条，CPU3 | 382.991 s | 351.082 s | 答案、状态逐条相同 |

候选二进制 SHA-256 为 `ee1001526e8e82dae7cd51c1f7bfa4ea33652a9af886318cd70d0b3eb931ce2f`。该版 q10 严格门最终仍在外层 10,070 秒后退出，结果文件只有 run header，没有 `[Query 10]` 完成行。CPU2 与 CPU3 是同一型号 Xeon E5-2643 v4 的两个物理 socket 核，均为单线程绑核运行；结果必须记录绑核和并发邻居，不能把跨时段差异解释为算法收益。故无深度最远组 oracle 保留为一般等价工作消除，但没有单独解决 q10。

### 5. 本轮排除方向

所有候选均在隔离构建中测试；没有修改查询、浮点精度、整数权假设或按图/按组数超参数。

- 更深的五组种子路径上界：q10 上界仍为 59，搜索轨迹不变，只增加预处理；q6 也没有状态收益。
- residual 需求顺序替换：正序、稀有组优先、常见组优先、需求量分层、未支付量优先等均不能稳定强于正式的远到近初始增长加反序 completion；部分路线直接降低固定窗口吞吐。
- tour 数据布局、half-perimeter 松弛和延迟求值：没有减少 q10 的关键状态，额外分支或内存访问使固定窗口进度持平或下降。
- indexed heap 与显式复用普通二叉堆：indexed heap 在相同工作下更慢；复用堆已被既有 `REJECTED_AND_DEFERRED_DIRECTIONS.md#history-reusable-ordinary-heap-negative-probe-20260810` 否决，本轮不重复保留实现。
- ordinary 重复 settle：在严格 1,200 秒 q10 门中，全部已完成 row 的重复 settle 均为 0。一致 A* 已自然保证首次有效弹出定型；增加 settled 检查不会减少邻接展开，诊断代码已删除。
- 固定组端点树遍历下界：已知被 directed-cut potential 完全支配，见 `REJECTED_AND_DEFERRED_DIRECTIONS.md#history-endpoint-pair-tree-bound-dominated-note-20260812`。
- 一次性互补 directed-cut 证书：正确但购买过晚、成本过高，详见 `REJECTED_AND_DEFERRED_DIRECTIONS.md#history-ephemeral-complementary-dual-negative-probe-20260813`。
- 固定两级最远组缓存：分离的一字节第二名数组在 Orkut q4 上约快 6.4%，Youtube P1 Base/Enhanced 前缀也不退化；但它仍是固定深度 top-k。既有 `REJECTED_AND_DEFERRED_DIRECTIONS.md#history-purchased-farthest-order-negative-probe-20260811` 已明确禁止用缺乏理论边界的固定 top-k 深度替代。q5/q6 分别运行约 15/29 分钟后主动停止，二者都没有结果行；源码随后删除第二名数组。该性能信号不能凌驾于预声明的论文约束。
- 全局 dual-first、仅 residual 重滤 dual-first 以及全局内联 tour 上包络：答案与状态不变，但 Youtube P1 前缀分别出现约 0.5%--2.6% 退化，均已回退；不再用 Orkut 单点波动覆盖 P1 门。

### 6. 当前决策边界

拆分因子化、阶段证书缓存、完整 dual fallback 复用与无深度的最远组 oracle 快路径均是输入无关的等价工作消除：它们不改变上界、下界、状态定义、浮点语义或渐近复杂度。前三者适合统一表述为 candidate-independent certificate factorization；后者是同一组距离 oracle 的布局专门化与唯一 argmax 线性构造。Base 继续使用同职责的 flat realization；Enhanced 只因新增 directed-cut 证书而采用 staged realization，没有产生 Base 独占逻辑操作。

当时的最终接入以三项证据为硬门：q10 的 `query_seconds` 严格小于 10,000；Orkut g15 q1--q10 的最终二进制结果全部完成；P1 Base/Enhanced 不退化。这三项后续已由文首冻结生产版通过。禁止项仍有效：不得删除 q10、延长该条专属 TL、按 `f` 或图名加开关，也不得把本节的阶段吞吐冒充完成结果。

后续把 staged cache 的 row epoch、stage 与 exact 标志合并为单一 32-bit 元数据字，属于同一证书 realization 的物理局部性优化；证明、空间账、两轮交换 CPU 的固定窗口结果及当前严格门见 `ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-packed-certificate-row-state-probe-20260814`。该严格门必须使用追加面板 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt`，不能误用图目录下含 300 条询问的 `query_g15.txt`。


### 7. 历史最终阶段：单调证书拒绝前沿

packed state、row-local 实际值前沿、all-stage 解析 cutoff 与 stage-0-on-miss 都没有同时满足严格 q10 和 P1 门。最终候选保留 row-local reuse admission，只在首次物化且证书仍处于 stage 0 时，把偶然的拒绝标签替换为由当前真实上界和已缓存可采纳下界解析出的拒绝 cutoff。实现再用原 `double` 加法与严格比较把 cutoff 向上修正到确实拒绝的首个可表示值。由非负浮点加法单调性，所有不小于该值的标签都必被原 `CanImprove` 拒绝；虚拟值不入堆、不计状态，较小 crossing 标签仍走完整证书链。该操作不使用图名、`g`、`f`、时间、内存、整数权或精度压缩。完整证明、失败路线和原始门禁见 `ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-monotone-certificate-frontier-probe-20260815`。

历史候选 Release 二进制 SHA-256 为 `a957bdcecafc486575cb7d78779dcbeda687eb0ea7283a0fc1104089dce83b86`。Youtube P1 Enhanced 全 300 条为 2,107.963 秒、218.477 MiB RSS overhead、67,570,613 状态；Base 全 300 条为 1,859.266 秒、122.453 MiB、74,359,677 状态。这些运行仍可隔离局部优化的成本方向，但 Enhanced 权值与聚合不能进入当前正式表。

旧 Orkut g15 q10 写出结果行：9,934.603 秒、返回值 54、19,776.727 MiB RSS overhead、1,654,690,262 状态；旧 q1--q10 也都产生结果。由于同一二进制不精确，当前只能继承“q10 极接近 TL”这一风险信号。当前辅助半层二进制必须重新完成十条，且旧状态数不设为相等门。

<a id="history-packed-certificate-row-state-probe-20260814"></a>

## ordinary 证书行状态合并探针（2026-08-14）

> 原始记录：`PACKED_CERTIFICATE_ROW_STATE_PROBE_20260814.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> **证据边界。** packed metadata 是仍保留的局部布局优化；本文 Enhanced 长测所用二进制则继承了缺失辅助 $H(h)$ 的精确性错误。因此固定窗口与 Youtube 时间只可说明 packed 相对同状态域对照的物理方向，不能作为当前 P1 答案或 Orkut 门禁。修复过程与最终生产版状态见 [辅助半层 Adjoint 正确性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-auxiliary-half-adjoint-correctness-audit-20260815)。

### 1. 动机与候选

Enhanced 的 ordinary 热路径按 `(mask, vertex)` 复用 staged future。原实现为每个顶点并行维护 32-bit row stamp 与 8-bit stage；诊断构建还另有 32-bit exact-dual stamp。Orkut g=15 q10 的 1,200 秒诊断窗口中，前 163 张共同 ordinary row 已发生 2,855,058,390 次证书入口调用，其中 2,600,519,242 次直接被缓存证书拒绝。这里的主要物理成本不是重新计算数学下界，而是反复随机读取多个并行元数据数组。

候选把 DirectedCut 配置的 row epoch、stage 与 exact-dual 诊断标志合并到一个 32-bit 无符号字：高位保存 row epoch，低 3 bit 保存 stage，下一 bit 保存 exact 标志。`double` 下界值仍保存在原来的 `bound_cache` 中；证书值、求和顺序、比较式、队列 key 与状态定义均不改变。Base 不使用 staged dual，继续只分配原有的单一 32-bit stamp，不进入打包状态机。

### 2. 正确性不变量

每张 ordinary row 开始时递增 epoch。仓库限制 `g <= 16`，固定锚组后 ordinary mask 数严格小于 `2^15`，因此 epoch 左移 4 bit 后远未达到 32-bit 上限。低 4 bit 可安全留给元数据，不存在一条查询内的回绕。

stage 的语义保持为“已经完成到哪个候选无关证书阶段”，不是“上一标签是否被拒绝”。若 closure interval 的安全下端拒绝当前较大标签，但没有计算 exact fallback，代码只缓存可采纳下端并保持 stage 0；以后更小标签若不能被该下端拒绝，仍会继续 dual 判定。只有以下两种情况可以结束 dual 阶段：

- 已得到逐组 `double` 精确和；
- interval 上端证明当前标签通过，而后续到达同一顶点的标签只会更小，因此也必然通过 dual。

这与合并前状态机逐分支一致。候选不压缩浮点精度，不改变逐组求和顺序，也不把 interval 下端冒充 exact 值。

### 3. 空间账

设图有 `n` 个顶点。正式构建中：

- Base：合并前后均为一个 32-bit stamp，即约 `4(n+1)` bytes；
- DirectedCut 配置：合并前为 32-bit stamp 加 8-bit stage，即约 `5(n+1)` bytes；合并后为一个 32-bit state，即约 `4(n+1)` bytes；
- 诊断构建：原来的额外 32-bit exact stamp 被 state 中的一 bit 取代。

因此候选不会增加 Base 空间，并为 DirectedCut 配置确定性减少约一 byte/vertex 的常驻 ordinary 证书元数据。收益来自更少的随机元数据流，而不是少算状态或改变算法精度。

### 4. 正确性门

正式源码重建后，五项 CTest 全部通过。配置精确回归覆盖确定性随机实例、正权唯一终端压力实例与省略半格转置实例；零权 witness 与 `(mask, vertex)` 状态计数回归也通过。候选与合并前构建在所有共同诊断 row 上的 `best`、状态数、primitive work 及各证书调用计数逐项相同。

### 5. Orkut q10 固定窗口 A/B

两轮试验均使用 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt` 的逻辑 q10，固定总墙钟 1,200 秒，并把进程与内存都绑定到 NUMA node 1。每轮同时运行候选与冻结对照，第二轮交换 CPU5/CPU7；共同预处理、A1、residual closure、上界 59 与 closure 后的 1,169,801 个 retained ordinary 标量一致。

| 轮次 | 候选 CPU | 对照 CPU | 候选完成 ordinary rows | 对照完成 ordinary rows | 候选提升 |
|---|---:|---:|---:|---:|---:|
| 1 | 7 | 5 | 179 | 163 | 9.82% |
| 2 | 5 | 7 | 180 | 165 | 9.09% |

两轮方向一致，且共同 row 的算法诊断逐项相同，因此候选通过进入长测的固定窗口门。该结果只证明相同轨迹的物理吞吐提升，不能代替完整询问结果。

### 6. 严格门结果

正确的严格门使用追加 P2 面板 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt`，而不是图目录下含 300 条查询的 `query_g15.txt`。逻辑 q10 的统计为 `g=15, mean_f=309.8, min_f=56, max_f=793`。packed 候选曾在 CPU5、NUMA node 1 本地内存上运行；外层 10,070 秒只补偿约 30 秒图加载，验收要求结果行中的单询问 `query_seconds < 10000`。该进程最终达到外层门限，结果文件 `results/probes/orkut_g15_packed_strict_cross_q10_cpu5/.../weights.txt` 只有 run header，没有查询结果行。因此 packed state 是通过 P1 门的一般局部性优化，但它没有单独解决 q10，不能把固定窗口吞吐提升写成完整询问结果。

### 7. P1 不退化门

为避免与 Orkut q10 争用内存带宽，q10 固定在 CPU5/NUMA node 1，Youtube P1 固定在 CPU4/NUMA node 0。前 60 条先在同一 CPU 和内存节点上顺序运行 packed 候选与合并前冻结对照：

| 配置 | packed 候选时间 | 冻结对照时间 | 时间变化 | 候选峰值 | 对照峰值 | 状态一致性 |
|---|---:|---:|---:|---:|---:|---|
| Enhanced | 420.904 s | 427.824 s | -1.62% | 213.688 MiB | 217.719 MiB | 15,389,926，逐条一致 |
| Base | 380.925 s | 386.378 s | -1.41% | 122.008 MiB | 119.918 MiB | 16,793,059，逐条一致 |

Base 不进入 packed state 路径；约 2.09 MiB 的单次采样峰值反向波动没有对应常驻数组或状态变化。配对时间没有退化，不能把该采样差异解释为算法空间增加。

随后只补跑第 61--300 条，与前缀合并为最终候选全量结果。Enhanced 共 300 条、2,115.979 秒、峰值 218.590 MiB、67,570,613 个状态；历史同轨迹全量为 2,119.583 秒、峰值 217.859 MiB、状态相同。Base 共 300 条、1,876.588 秒、峰值 128.586 MiB、74,359,677 个状态；历史同阶段为 1,873.190 秒、峰值 128.035 MiB、状态相同，时间差约 +0.18%。两种配置的 300 条权值与状态均逐条完全一致。

因此 P1 的答案、状态和运行时间不退化门通过；不足 1 MiB 的全量峰值差异保守记为 1 ms RSS 采样与分配器瞬时波动，不声称空间收益。DirectedCut 配置确定性减少一 byte/vertex 的 ordinary 元数据仍由第 3 节的布局账证明。


### 8. 后续状态位演化

本文件记录的 packed 候选使用低 4 bit：3 bit stage 和 1 bit exact-dual。后续 ordinary 拒绝前沿又在同一个 32-bit 字的低位加入 `rejected-seen` 与 `rejected-frontier` 两个布尔标志，row epoch 因而由左移 4 bit 改为左移 6 bit；没有恢复并行元数据数组，常驻空间账仍为每顶点 4 bytes。新增标志及最终 stage-0 分析前沿的证明和门禁见 `ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-monotone-certificate-frontier-probe-20260815`。

<a id="history-dominated-operation-reduction-audit-20260815"></a>

## 严格支配操作与减空审计（2026-08-15）

> 原始记录：`DOMINATED_OPERATION_REDUCTION_AUDIT_20260815.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

本文记录当前 ABHSS 源码的一次全链路“减成空”审计。目标不是追求源码行数最少，而是在不改变精确语义、论文主线和物理性能的前提下，删除被其他操作严格覆盖的重复工作。正式方法口径仍以 `../METHOD.md` 和 `../CODE_GUIDE.md` 为准；本文保存接受与拒绝候选的证明、实测和最终硬门状态。

> **当前勘误。** 本文最初记录的候选 E 及之后的单调证书优化本身没有引入伴随层错误，但它们继承了“从最高逻辑层直接启动 H”的不精确实现。旧 P1 Enhanced 与 Orkut g15 q1--q10 结果因此全部降级为历史性能探针。当前源码已加入辅助 $H(h)$、必要的较低双块 terminal、successor 全值读取与互补 H 完成；事故演进、反例和证明见 [辅助半层 Adjoint 正确性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-auxiliary-half-adjoint-correctness-audit-20260815)及 [Adjoint split 完备性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-adjoint-split-completeness-audit-20260817)。修复后的冻结生产二进制已经完成当前 P1 与 Orkut q1--q10 正式硬门，见第 9 节。

> **二次勘误（2026-08-17）。** 辅助半层的第一轮修复曾错误地删除全部较低直接 terminal；后续 Musae 全量审计证明该实现仍不完备：较低 H 的平衡 split 需要必要双块终端，successor 需要 ordinary 全值，非锚组二等分还需要互补 H 完成。本文第 5、6、8 节中与此冲突的减空结论只作失败方向历史记录，不再描述当前源码。现行证明与门禁见 [Adjoint split 完备性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-adjoint-split-completeness-audit-20260817)。

> **长门更新（2026-08-17）。** 修复版 Orkut g15 q10 已以 30,000 秒诊断预算自然完成，精确值 54、solver 时间 11,178.235 秒，故没有通过正式 10,000 秒硬门。该轨迹还证伪并回退了后续证书链上包络；完整数据、阶段时间与当前构建门见同一 Adjoint split 审计。

> **最终更新（2026-08-19）。** 本地 checkpoint `e9eee92` 把 Enhanced 的共同前向前缀固定为 A1，并由 H 实现逻辑层 2 到 q；其 q10 诊断时间为 9,431.057 秒。后续冻结求解器源码提交 `12d6adb` 又加入 adjoint 空域减空与 certificate-support 增量求值，并以自身诊断二进制在 9,250.911 秒完成同一 q10。无诊断正式二进制 SHA-256 `793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced` 已完成 P1 全 13 图和 Orkut g15 q1--q10；q10 为 9,544.561 秒、精确值 54、状态数 1,459,398,194。13 图均满足最快 ABHSS 配置不劣于 PrunedDP++。

### 1. 审计状态

- A--E 的局部减空仍保留；其历史回滚构建 SHA-256 `3d02cd2ec7ea8bd45db8acee5d2fc29e0232b39548e0539eaf7782bd7e46adad` 只证明这些局部改动的当时物理对照，不再标识当前源码。
- 当前修复版通过 5/5 CTest、随机精确门、SteinLib 已知最优门以及第 9 节最终性能门；正式二进制身份在第 9 节冻结。
- 接受项 A--E 与新增辅助半层减空均不读取图名、查询编号、运行时间、状态数、row 密度或经验阈值，不压缩 `double` 精度，也不假设边权为整数。
- 旧错误二进制的 Orkut `g=15` q1--q10 曾全部在 10,000 秒内结束，最紧 q10 为 9,934.603 秒；该数字只用于评估当前复跑风险，不是当前门禁结果。
- `e9eee92` 的 q10 诊断结果只保留为演进证据；最终结论使用生产 SHA 的无诊断 q1--q10 十条正式运行。
- P1 当前 Base/Enhanced 共 16,636 条，复用的冻结 PrunedDP++ 共 8,318 条；独立审计按 task key 对齐全部查询，55 个 infeasible 身份和所有可行目标值一致。最终 P1 与 Orkut 十条硬门均为 PASS。

### 2. “被支配”的判定标准

只有同时满足以下条件，才能把操作减成空：

1. **数值支配**：被删操作的结果在所有合法输入上都不可能改变当前最小值、最大下界、可行上界、队列顺序、规范代表或输出 witness。
2. **时序支配**：支配结果在同一消费点之前已经可用。一个更强但更晚才计算的证书，不能支配更早用于拒绝候选的便宜证书。
3. **副作用为空**：被删操作不承担状态首次登记、owner 移交、branch 标记、changed-arc 登记、真实边恢复、诊断计数或缓存物化职责。
4. **浮点轨迹不变**：不能用重新结合求和、降低精度、epsilon 放宽或整数化来制造“等价”。
5. **物理不退化**：对热代码，源级严格减空仍须经过同机对照。LTO、内联和代码布局可能使逻辑更少的版本实际更慢；这种候选不能进入论文实现。

允许保留少量平凡连接操作，例如边界检查、调用接口和可选诊断参数，只要它们使证明边界清楚或实测有利。它们不应被包装成新的算法步骤。

### 3. 全链路职责清单

| 区域 | 主要操作 | 支配审计结论 |
|---|---|---|
| 输入与低组闭包 | 快速读图、查询校验、零权 cover、正权 cover、至多三组精确闭包 | 都决定合法输入或共同精确基例，不能删 |
| 距离—根初始化 | Base 的 bounded 距离 realization、DirectedCut 的 complete-potential realization、共同根扫描 | 两种 realization 实现同一外层合同，但物理产物不同，不能相互删除 |
| 上界初始化 | SPT 边并集、root star、facility/primal support、三元 seeded path growth、tour/witness | 候选强弱和出现时机不同；没有一个全程支配其余全部 |
| `GroupRow` | cutoff 读取、精确 singleton 读取、精确枚举、dense/bitmap 表示 | 合同不同；只合并了重复 membership 定位 |
| 公共 A1 | seed、farthest、endpoint-floor cone、非负 fallback、图闭包、owner 移交 | Base 与 Enhanced 完全相同，不能由 dual 或 H 替换 |
| ordinary D | 规范 split 聚合、统一 future、图闭包、branch 标准化 | 都参与精确状态生成；只删除重复 accessor 和不可能有作用的容器操作 |
| future 链 | dual、farthest、A1、tour 与 staged cache | 后段可能更强，但前段能更早拒绝，故按时序保留 |
| forward A / adjoint H | 共同 A1、辅助半层 terminal、递减 H | 辅助 $H(h)$ 精确转置被省略的 $D(h)$；逻辑 H(2..q) 替换 A1 之后的前向层，不能省略半层闭包或必要的较低 terminal |
| DirectedCut | potential、cone、changed arc、residual、exact fallback、primal 恢复 | 只删除零梯度方向回写；其余操作维持对偶可行性与可行上界 |
| witness rent-or-buy | 两边各自 witness、统一零起点 rent、同一 buy 公式、同一树 DP | witness 不同，但调度合同和树 DP 共同；不能恢复无条件树 DP |
| 状态容器 | epoch、bitmap padding guard、ready、branch bitmap、settled 排序 | A1 完整发布屏障后的 singleton `ready` 检查已严格减空；其他生命周期检查或由实测要求保留，不能推广删除 |

### 4. 接受的严格减空

#### 4.1 A：删除已命中 cache 的第二次 accessor

`CanImprove` 成功返回时已经把当前 epoch 的完整 future 写入 `bound_cache[vertex]`。随后 heap key 再调用 `Bound` 或 `Prefix` 只会命中同一 cache，不可能计算新值。当前实现直接读取 cache；同时把 `staged_certificate_cache` 提到循环外，并让 exact-dual 位只在诊断构建中写入。

这不改变证书阶段、值、heap key、状态数或拒绝集合。YouTube 作者查询前 30 条的同机结果为：

| 配置 | 对照 / 秒 | 候选 A / 秒 | 变化 |
|---|---:|---:|---:|
| Base | 188.152039 | 187.907820 | -0.13% |
| Enhanced | 206.065450 | 204.751011 | -0.64% |

#### 4.2 B：用 tour 上包络跳过必然无贡献的精确求值

调用点先得到已经需要的 farthest 与 dual 最大值。若该前缀已经不小于 `tour.UpperEnvelope`，则精确 `tour.At` 不可能提高最终最大值，因而被严格支配；只有前缀低于上包络时才求精确 tour。

交叉绑核结果中 CPU5 从 206.165259 秒降到 204.657361 秒；CPU4 为 206.277161 与 206.314776 秒，属于噪声范围。候选不改变权值和状态数，且删除条件直接来自上包络证明，因此接受。

#### 4.3 C1：删除 `GroupRow touched` 中不可能产生的去重

多源 Dijkstra 仅在顶点标签由无穷首次变为有限时把顶点加入 `touched`，之后只允许严格改善；同一组内天然唯一。完整与 bounded-dense 布局不依赖顶点递增，直接使用 dense payload；ranked-bitmap 布局只排序一次以生成递增压紧 payload，不再调用结果必为空的 `unique`。

| 配置 | 对照 B / 秒 | C1 / 秒 | 变化 |
|---|---:|---:|---:|
| Base | 189.306721 | 183.410831 | -3.11% |
| Enhanced | 205.710297 | 205.451459 | -0.13% |

#### 4.4 D：把 membership 检查与精确 singleton 读取合成一次

旧路径先调用 `IsExact(v)`，再通过另一入口重复定位相同 dense/bitmap 位置。现在所有随机 singleton 消费者统一调用 `ExactValueOrInf(v)`：精确位置返回真实距离，cutoff 占位返回正无穷。`operator[]` 仅保留给需要 cutoff 下界占位的消费者，`ForEachExact` 保留给枚举消费者。

路径恢复还把当前 tight distance 缓存在顶点循环外，避免每条邻边重复 rank 定位。接口合并没有把 cutoff 当 DP seed，也不改变任何距离值。

| 配置 | 对照 C1 / 秒 | D / 秒 | 变化 |
|---|---:|---:|---:|
| Base | 182.929343 | 180.326254 | -1.42% |
| Enhanced | 207.535515 | 205.834483 | -0.82% |

#### 4.5 E：每条无向边只回写唯一可能为正的势梯度

一条无向边两端势相等时两个方向梯度均为零；不等时只有高势端到低势端的方向为正。旧实现对两向都执行 `max` 回写，零方向严格无效。初始 cone 扣减与购买后 residual 补全现在共用 `SubtractPositiveGradient`，只标记并扣除唯一正方向；恢复初始 residual 时也跳过零梯度赋值。

这保持相同减法、residual、changed-arc 集和 primal 支撑，不利用容差。

| 探针 | 对照 D / 秒 | E / 秒 | 变化 |
|---|---:|---:|---:|
| YouTube Enhanced 前 30 条 | 205.910918 | 203.704031 | -1.07% |
| Orkut g15 q3 | 288.672101 | 285.567882 | -1.08% |

#### 4.6 F：完整 D 只支配同目标 pair

正确的 H 不能从最高逻辑层 $q=h-1$ 直接启动；它必须先在辅助层 $H(h)$ 完成被省略 ordinary 半层的精确转置。更早版本曾进一步声称“较低 H 的全部直接 terminal 都被 successor 支配”，Musae q162/q295 已给出反例：若 ordinary 规范 split 的两侧都不超过 q，而任一侧加入当前目标后都会越过 H 的物理上界，则必要双块 terminal 是唯一入口；successor 还必须读取新增 ordinary 块的全部精确值，不能只读 branch。故“删除全部较低 terminal”不是接受项，相关源码已经恢复。

当前只保留一个逐值严格支配：对固定 H 目标 S，若完整 ordinary $D(Q)$ 已发布，其中 $Q$ 是 S 的补集，则 $D(Q,v)$ 已是所有同根 split seed 经相同图闭包后的精确最小值；同目标任意 pair 都是两棵可行 rooted 子树的和，不可能小于 $D(Q,v)$。因此该目标只装载单块 $D(Q)$，不再枚举 pair。若完整 $D(Q)$ 不存在，所有证明所需的双块 terminal 仍完整保留。这个删减只读 row 的已证明发布域，不读取图名、查询统计或运行表现。

#### 4.7 G1--G7：当前辅助半层版本中的严格等价减空

这一组改动不增加下界或上界，只删除由当前生命周期和有序容器合同严格蕴含的重复工作。

1. **证书升级后的 ordinary 稳定重滤原地压紧。** 旧实现为每张 row 重新分配 branch bitmap、重算全部 branch 数和最小值。新实现保持顶点顺序，用 read/write 游标原地搬移仍存活项；branch 位随项搬移，branch 数只减去被删项。只有被删值与旧最小值精确相等时才重扫剩余 payload，否则旧最小值仍由未删除项实现。若一项未删，row 完全不写。删除谓词、剩余 payload 和 padding 位逐项相同。
2. **D/A/H 初始标签统一线性建堆。** `touched` 中每个顶点只出现一次，且 key、distance、vertex 三元组已经确定。`BuildInitialQueue` 把完全相同的节点交给标准线性 heapify；`QueueNode` 以 vertex 作末级比较，形成全序，所以与逐项 `push` 的 pop 轨迹一致，只删除建堆的重复对数调整。
3. **只在层序已证明处删除 ready 检查。** ordinary split 的两侧都是真子集，已经由较低层发布；平衡补集只在更低层或同层更小编号时消费；forward A 的锚定侧是真子集或隐式空侧，ordinary 侧在整个 A 阶段前已经完成；H successor 严格位于已完成的更高层。普通 forward A 若从未产生标签，会省去 ready 写入，但严格 size 层序已证明该 mask 被处理，后继只把空 payload 读作无穷。ordinary、提前 A1 owner 交接、H 和其他跨阶段边界仍显式保留 `ready`，没有把普通 row 的“已发布空”与“未生成”混写。
4. **转置不变量移出 64 顶点块。** ordinary 按完整层发布，因此一个规范 representative 的 availability 等价于整层 availability；可转置 mask 在进入顶点块前按原数值升序筛一次。pair 与 submask 两种等价枚举的工作量选择中，一旦累计 pair work 已严格超过 submask work，后续非负增量不可能改变选择，立即停止计数。`Update` 的目标 popcount 由三个调用点的 cover 条件逻辑蕴含，但删除它在偶数 g=14 的四轮 Release 门中合计回退 0.494%，因此作为局部域合同保留，见第 5 节。
5. **H 边界只生成 successor 子掩码。** 旧循环扫描整个子集格，再用 `mask & ~successor` 拒绝绝大多数 mask。新循环按相同数值升序生成 successor 的全部子掩码，并保留相同的低层域条件；所有被省略 mask 都必定在旧谓词处失败。H seed 中的 successor 由当前 mask 加非空 outside block 得到，严格位于已完成高层，因而删除第二次生命周期读取。
6. **二分工作量用位宽直接计算。** 非空长度的旧循环结果严格等于其二进制位宽，空表仍定义为 1。编译器位扫描只替换计数循环，不改变交集算法选择式。
7. **缺少最小单块 cover 时跳过整段直接扫描。** 单块 terminal 的 cover 区间从 $k-h$ 开始，ordinary 发布域按块大小向下闭合。若这一最小层的 representative 不可用，则更大的完整 D 也全部不可用；旧逐值循环的 cover 条件必定逐项失败。当前实现只跳过这段严格空扫描，双块 terminal、successor、互补 H 和 `Update` 域合同全部保留。判断只读取层域 availability，不直接按奇偶、图名、查询编号或计时分派。

普通 probe 现在只保留稀疏阶段事件；逐候选证书计数需要显式 `GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS`。这项拆分只消除探针构建的观测开销，不属于论文算法步骤。共同 A1 的 lazy/顺序物化则是相同精确视图的物理调度，不是严格删状态；其证明、直接分支覆盖和小门见 [A1 top-two 自适应物化门禁](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-adaptive-a1-top-two-materialization-gate-20260816)。

#### 4.8 A1 tail 的精确物理候选（不计入严格支配）

1. **精确 mask-rent 因子化。** top-two 购买后，同一 remaining mask 的逐 bit 租金只重复相加由 row 长度决定的整数。子集递推表逐项等于原和式，不改变累计 rent 或排名购买点；tail 查询足够多时，它把重复加法换成表读取。但若购买后几乎没有 tail 查询，预构造 $2^k$ 项可能净增工作，所以它不是所有输入上的严格支配，只能按物理缓存接受并经过 P1 门禁。
2. **一次性物化的冷机器码边界。** `MaterializeAllTopTwo` 与 `MaterializeRankedTail` 每条查询各至多执行一次；非内联边界只阻止 IPO 把冷购买代码并入逐状态 `Future`。它不是新开关，也不删除数学操作；两轮 Musae 小门分别为候选/对照 0.990564 和 0.979960。

完整 byte 排名、租金表和冷边界都是同一精确 A1 视图的物理 representation，不是严格删状态或新的算法证书；是否最终保留必须由完整 P1 与 Orkut 硬门共同决定。无条件和购买后 staged second-rank ceiling 均已实测回退。证明、复杂度、P1/q10 窗口和负结果见 [A1 完整排名、精确租金因子化与 ceiling 负向门禁](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-a1-complete-ranking-and-staged-ceiling-gate-20260817)。

最终空扫描候选的 production SHA-256 为 `96a4add04c2a731f76c1c61bd7762760ee2e8eb11f0bfab7c2e3abfcf25554c3`，diagnostic SHA-256 为 `5c81392125b8b003ec681e679599f9caf202befb0f6f1caf53e2f65b885992c9`；两种构建均通过 5/5 CTest，包括 5000 个随机、500 个正权唯一终端和 160 个辅助半格实例。Musae g7 全 300 条四轮合计为候选 89.201 秒、对照 89.072 秒，变化 +0.145%；奇数 g15 十条两轮为候选 334.142 秒、对照 334.836 秒，变化 -0.207%；偶数 g14 十条两轮为 +0.046%。三组答案、状态与空间逐项一致，故只将它判为端到端中性、transpose 局部减空，不包装成算法贡献。`e9eee92` 的 q10 以 9,431.057 秒返回精确值 54；冻结求解器源码提交 `12d6adb` 的诊断 q10 又以 9,250.911 秒通过。其最终生产 SHA 的完整 P1 与 q1--q10 十条正式运行现已完成，见第 9 节。

#### 4.9 A1 发布屏障后的恒真检查

1. **singleton ready。** A1 重启循环只有在全部 singleton row（含空 payload）写入 `ready` 后才初始化只读 future；发布屏障之后的重复检查不再承担合法性、fallback、购买或 owner 职责。删除后 Musae 两轮几何比为 1.000479，Orkut q10 900 秒两轮均把对照事件序列严格延长 2 个事件，内存不增。
2. **ranked buy 正性。** A1 域推出 $g\ge4$、 $k\ge3$、 $n\ge1$，故 `ranked_buy_work` 严格为正。热路径只比较 rent 是否达到 buy。Musae 两轮几何比为 0.998610；Orkut q10 两轮分别延长 5 和 8 个事件，内存不增。

两项的代码、证明、SHA 与完整交换轮见 [A1 发布屏障与恒真检查减空门禁](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-a1-publication-barrier-reduction-gate-20260817)。它们删除共同 A1 realization 的重复检查，不是新证书或可调开关。

### 5. 已证明逻辑可删、但因物理回归而拒绝

#### 5.1 H--J：热边界检查和可选计数接口

H 同时删除 bitmap padding guard、`GroupRow` 边界、`RecoverPrimal` 可选工作量指针、A1 pop cutoff 与路径恢复根检查。I 恢复 A1 cutoff 和根检查。J1 再恢复 `RecoverPrimal(..., long long* work=nullptr)` 后，Enhanced 恢复；J2 则尝试只恢复 bitmap 检查，但再次明显回归。

| 版本 | Base / 秒 | Enhanced / 秒 | 结论 |
|---|---:|---:|---|
| E 同轮对照 | 181.086371 | 203.580140 | 接受基线 |
| H | 178.755346 | 205.342039 | Enhanced 回归，拒绝 |
| I | 179.731421 | 204.552449 | Enhanced 回归，拒绝 |
| J1 | 179.017812 | 202.590997 | 说明可选计数接口影响 LTO 布局 |
| J2 | 181.273388 | 207.432285 | 明显回归，拒绝 |

可选 `work` 只用于隔离探针计数，论文调用传空，不是算法步骤。保留它和短边界检查是物理实现选择，不能在论文中包装成额外优化。

#### 5.2 L--M：按 primal bitmap 枚举 witness 边

`BuildDualWitness` 当前扫描原图边数组并检查 primal bitmap。L 改为逐 64-bit word 枚举置位，理论工作由全边扫描变为 word 扫描加 witness 边数，而且保持 edge-id 顺序。5 个 CTest 全部通过，但 LTO 改变了相邻 ordinary 热函数的机器码形状，Enhanced 的 30 条中有 28 条变慢。M 用 `noinline` 隔离该冷边界仍未恢复。

| 版本 | Base / 秒 | Enhanced / 秒 | 结论 |
|---|---:|---:|---|
| E 同轮对照 | 181.086371 | 203.580140 | 接受基线 |
| L：bitmap 置位枚举 | 179.051621 | 205.879566 | Enhanced +1.13%，拒绝 |
| M：L + noinline | 181.441349 | 206.206884 | Enhanced +1.29%，拒绝 |

因此“渐近工作更少”不足以进入当前论文二进制。若未来要重试，必须隔离翻译单元并重新跑完整 P1，而不能直接恢复 L。

#### 5.3 N--Q：D/H pop cutoff 与 forward 空行 ready 补写

ordinary D 和 adjoint H 在单张 row 的队列闭包内不更新 `best`，所以出堆时的第二次 `key < best` 在逻辑上由入队证书蕴含；forward A 与提前 A1 会在队列内收紧 incumbent，不能使用这一结论。普通 forward A 的零标签行也可由严格层序与空 payload 直接表示，无需显式补写 ready。候选全部保持权值与状态，但删除 D/H pop cutoff 的组合在 Musae 回归 0.68%，删除 H cutoff 并补 ready 在 Orkut q3 回归 0.67%；单独逐行和批量补 ready 在 Musae 分别回归 1.50% 与 1.41%。当前源码已经全部恢复，详细交换绑核证据见 [Queue pop 与 forward 空行发布负向探针](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-queue-pop-and-empty-ready-negative-probe-20260816)。

#### 5.4 R--S：staged cache 延迟发布与冻结配置分派

新 epoch 的临时 cache 零写会被首次 dual 结果覆盖，查询内 frozen config 判断也可由入口模板分派一次；两者在源码语义上均可减空。当时前者在 Musae 两轮均值回归 0.66%、Orkut q3 回归 0.42%，模板分派也回归 1.01%，故都恢复。ordinary 内核后来发生实质改写后按原文的重试条件复查：模板分派的 Base 两轮合计快 0.39%，Enhanced 与一个后来删除的额外分支联合时合计慢 0.13%，交换核方向反转，判定为无可辨认退化；当前仅恢复同源模板的入口分派，epoch/cache 延迟发布仍拒绝。详细证据见 [Staged certificate cache 物理减写与冻结分派负向探针](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-staged-cache-physical-reduction-negative-probe-20260816)。

#### 5.5 T--U：公共 A1 必然存在与 ordinary size-1 扫描

共同精确基例已经闭包全部 $g\le3$ 查询，因此进入指数递推的查询必含 A1，ordinary 也不需要 size-1 row。删除 nullable A1 分支并跳过 size-1 扫描的合并候选虽减少 305 字节 text，Musae 全 300 条两轮均值仍回归 1.58%；单独跳过扫描在 Musae 持平，但 Orkut g15 q3 两轮均值回归 0.72%。全部正确性、权值和状态证据一致，源码已恢复。详细证明与交换绑核结果见 [公共 A1 必然存在与 ordinary size-1 扫描负向探针](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-mandatory-a1-and-size1-scan-negative-probe-20260816)。

#### 5.6 A1 top-two 哨兵与 Future 入口域检查

两个候选都能由当前调用链证明恒真/恒假，但删后发生稳定物理回归：top-two `>=0` / `!=255` 守卫的 Musae 两轮几何回归 0.9028%；`Future` 入口 `first.empty() || !remaining` 的两轮几何回归 1.6241%。权值与状态逐项一致，说明回归来自机器码布局而非算法差异。两项均已恢复；没有使用 NOP、强制对齐或按编译器特调来掩盖回归。

#### 5.7 Adjoint `Update` 目标域复查

direct 单块、排序 pair 和互补 submask 三个调用点都先把 cover 限制到合法区间，因此 `Update` 内再次读取目标 popcount 并拒绝越界在逻辑上恒不触发。删除该检查的候选通过两种构建各 5/5 CTest，答案、状态和空间也逐项不变，但无诊断 Release 在 Musae g14 十条查询的四轮交换绑核中合计由 410.412 秒增至 412.439 秒，回退 0.494%。恢复短检查后，同一偶数域两轮只差 +0.046%。当前把它作为 lambda 的局部输入合同保留；这属于已测得更优的平凡连接操作，论文不把它写成剪枝或独立优化。

#### 5.8 Proper-submask 补集与 Adjoint 同层 `ready`（2026-08-18 复查）

本轮又找到两类输入无关的逻辑蕴含，并分别隔离测试。

1. `CertificateSupportDpCache` 与 `BuildPrimalFacilityUpper` 都从 `(mask - 1) & mask` 开始枚举非空真子集 `left`。因此 `right = mask ^ left` 必非空，原条件中的 `!right` 恒假。
2. `SolveHighAdjoint` 在固定 size 内按 mask 递增生成并发布 H row。互补完成只在 `complement < mask` 且两者同层时执行，所以 `backward[complement].ready` 恒真；空 payload 也会在发布点显式写 `ready = true`。

四个候选均通过 5/5 CTest；Musae g7 全 300 条的两轮交换绑核中，权值与 `(mask,v)` 状态逐查询完全一致。无诊断 Release 的两轮求解时间如下：

| 候选 | Base 候选 / 对照秒 | Base 变化 | Enhanced 候选 / 对照秒 | Enhanced 变化 |
|---|---:|---:|---:|---:|
| 两处 proper-submask 同时删除 | 93.925 / 92.005 | +2.09% | 46.425 / 46.169 | +0.56% |
| 仅 certificate-support 删除 | 93.916 / 92.334 | +1.71% | 45.933 / 44.673 | +2.82% |
| 仅 primal-facility 删除 | 92.468 / 91.556 | +1.00% | 44.672 / 44.525 | +0.33% |
| 仅删除 Adjoint 同层 `ready` | 92.415 / 90.320 | +2.32% | 44.511 / 44.449 | +0.14% |

发生方向在交换 CPU 后没有形成可接受的双配置非退化证据；其中 support-only 的 text 段虽减少 32 字节，仍明显更慢，Adjoint 候选的 text 段反而增加 8 字节。所有候选均已恢复，正式二进制重新逐字节等于 SHA-256 `793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced`。这些检查按第 2 节的物理准则作为平凡局部合同保留，不写成论文剪枝；由于短门已经失败，没有冒险运行 Orkut q10 长门。

同轮还复查了 `MarkAllMasksDirty` 的初始化清零、component-cover 后的零上界判断，以及 certificate-support 顶点集合的两次构造。前两项属于冷路径局部状态合同，删除收益至多一次线性写或一次比较；后一项分别服务购买工作量和缓存所有权，合并会延长顶点向量生命周期并改变空间。它们都不构成值得继续扩张代码和长门的严格物理支配候选。

### 6. 其他拒绝项与原因

| 候选 | 逻辑判断 | 实测或证明结论 |
|---|---|---|
| 组度量对角项跳过 | 对角距离为零，数值上冗余 | YouTube Enhanced 由 205.583234 增至 207.923509 秒，约 +1.14%；拒绝 |
| 空定义域早返回、singleton/common-value 特判 | 多数分支由上游计划蕴含 | 热布局回归；不再叠加零收益分支 |
| callback 透传已知值 | 可省一次读取 | 调用边界与内联变化使整体回归 |
| A1 延迟读取 endpoint | 只在候选存活后读取 | DBLP 样本约慢 3.8%；拒绝 |
| 全局 dual-first 或全局内联 tour envelope | 更强证书先算看似更好 | P1 出现约 0.5%--2.6% 回归；保留逐阶段便宜先行 |
| ordinary endpoint 预筛 | 可提前拒绝部分顶点 | 样本约慢 0.67%；拒绝 |
| fixed endpoint-pair tree bound | 是可采纳下界 | 已被同消费点的 directed cut 逐值支配，且实测无新增拒绝；不保留 |
| 固定 top-k second-farthest | 可能增强 farthest | 缺少与剩余集合容量对应的理论边界，属于无意义超参数；禁止 |
| 无条件或购买后 second-rank ceiling | 已有下界可能逐值支配 A1 tail | 无条件版本阻断排名 rent 且 P1 回归；staged 版本的 q10 归一化进度弱于无 ceiling 完整排名；全部回退 |
| `StrengthenLower` 合并 API | 数学上等价于调用点取 max | 扩大热代码后 Musae 几何回归约 1.31%；恢复精确 `Future` API |
| ranked-tail locator | 可把购买后的最后一次二分也减空 | q10 归一化进度更差且增加约 273 MiB；只保留 byte bit 次序 |
| farthest 已有下界 ceiling | 已有下界达到全局最大组距离时可跳过扫描 | 无新增剪枝，且削弱同轮 A1-only 归一化进度；恢复精确 farthest 接口 |
| `sort+unique(settled)` 去重删除 | 很多路径看似不会重复 settle | 在全部启发式与浮点并列下未证明唯一；排序又是 row 契约，保留 |

### 7. 不能误判为支配的有效操作

- **便宜 farthest 与较晚的 A1/tour/dual**：后者即使数值更强，也不能删除更早且便宜的拒绝。
- **staged certificate 的各阶段**：每一级都可能在支付下一阶段成本前拒绝；它们是时序链，不是重复求同一值。
- **证书升级后的 refilter**：新 incumbent 或更强 future 会使已经生成的 row 出现过期存活项，重滤不是重复闭包。
- **triple 与 quad path growth**：triple 在购买前提供共同早期上界；quad 只在购买后追加一层，候选集合和时机不同。
- **SPT、root-star、path-growth、facility/primal 与 witness 上界**：任何一个在某些实例更强，不代表在全部实例、全部时间点支配其余。
- **Bootstrap SPT 上界**：除了初始 incumbent，还决定 bounded 多源 Dijkstra 的安全 cutoff；后续 root-star 更优也不能追溯删除其初始化职责。
- **root-path union**：同时提供可行上界、根候选和 witness 输入，不是 Base 独占的额外方法步骤。
- **A1 owner 标记**：`anchored[mask].ready` 在公共前向入口区分“提前物化并等待移动的 A1”与“尚未生成的普通 A”，还决定是否重复累计状态。即使当前层计划令前者恰为 size 1，也不能让消费者改用层号猜测生产者；该一次性读取是所有权合同，不是第二次 A1 计算。
- **隐式 A(0) 计划位**：`complete_implicit_anchor` 由完整正层定义域为空推出，显式指定最终完成职责；保留它可避免内核从 `last_size`、低层前缀或经验组数条件反推模式。该查询级布尔值不是新的算法分支。
- **配置边界检查**：命令行和公开求解 API 分别拒绝未知增强位及 adjoint-only 组合。二者保护不同调用边界，不进入 D/A/H 递推，也不应作为论文操作计数。
- **A1 pop cutoff、恢复根检查和 bitmap padding guard**：前置不变量通常蕴含它们，但实测删除损害机器码或丧失局部断言边界，按平凡连接操作保留。
- **局部域连接检查**：proper-submask 的非空补集、Adjoint `Update` 目标域及互补 H 的 `ready` 可由当前调用链部分推出，但第 5.7、5.8 节的交换绑核证明删除会物理退化；它们作为短输入合同保留，不包装成剪枝。
- **`BuildDualWitness` 全边扫描**：L/M 已证明源级减空但物理退化，当前保留。

### 8. 论文可写性核验

当前接受项可统一描述为“在相同精确递推和证书链内，避免重复 realization”：

1. 已物化 cache 直接消费；
2. 已有上包络证明无贡献时不求精确 tour；
3. 利用 Dijkstra 首次触及唯一性，不做空去重；
4. singleton membership 与读取合并为一个有类型含义的接口；
5. 势梯度只回写真正非零的方向；
6. 辅助 H 半层建立精确基例；只有已有完整 $D(Q)$ 时才不再枚举被其逐值支配的同目标 pair，缺少完整 D 的必要较低 terminal 全部保留；
7. 最小单块 cover 层不可用时，由 ordinary 发布域的向下闭合证明整个单块扫描为空，但双块 terminal 和 successor 不变；
8. ordinary refilter 在原容器中稳定压缩，并在同一次扫描中维护最小值和存活分支数，不再复制相同候选或重复扫描；
9. D、A、H 的初始标签集合已经完整且有序，直接线性建堆，不再逐项执行等价插入；
10. 只有严格层序已经证明生产者完成的消费点才删除 `ready` 读取，仍承担真实生命周期职责的检查全部保留；
11. successor 直接枚举目标补集的子掩码，不再遍历全部 mask 后拒绝不可能的候选；
12. 非空掩码的二进制长度用等价 bit width 直接得到，空掩码仍按原定义计作 1；
13. A1 发布屏障之后不再重复检查 singleton `ready`，并由 A1 的定义域直接推出 ranked-buy 工作量为正。

第 8--13 项是第 4.7、4.9 节已经逐项证明并通过物理门禁的精确稀疏实现，不是六项新的算法贡献。论文中应将它们合并写成“相同精确递推的稀疏 realization”，只突出第 1--7 项中对方法主线有解释价值的支配关系。

它们没有引入第三种方法、图特化、经验分派或近似数值语义。A1 完整排名、租金表和非内联边界只属于经过实测选择的物理布局，不进入上述严格支配清单，也不应写成独立算法贡献。Base 与 Enhanced 的论文关系仍是：共同执行精确主干；Enhanced 在相同 A1 和 ordinary 基础上增加 DirectedCut，并用结构同职责的 H realization 替换高层 A realization。接受项不会让 Base 获得 Enhanced 不包含的算法职责。

### 9. 最终硬门

旧错误二进制的历史冻结结果如下；这里只作为运行时间风险估计，不证明当前答案、状态或门禁：

| Orkut g15 查询 | 时间 / 秒 |
|---:|---:|
| q1 | 2270.861729 |
| q2 | 2523.203538 |
| q3 | 281.276293 |
| q4 | 315.547937 |
| q5 | 4568.638367 |
| q6 | 3847.347995 |
| q7 | 3451.310315 |
| q8 | 204.234148 |
| q9 | 1465.793602 |
| q10 | 9934.602789 |

冻结求解器源码提交 `12d6adb` 的正式二进制 SHA-256 为 `793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced`，PrunedDP++ SHA-256 为 `4c1d3599f03da6073d368a6a83fcbd31ea0a625f9ba90892b22b0b239eb42bf2`，矩阵 SHA-256 为 `aed5db1ed83d134f5882f4c9d4544bf26549e9ee5a5392d74c3066b96c273162`。当前 ABHSS 的 16,636 条记录与冻结 baseline 的 8,318 条记录来自同一 `test-PowerEdge-R630`、同一矩阵和 10,000 秒 TL；baseline 代码与二进制未变，因此复用其既有完整运行是公平且可核验的。独立审计确认 8,318 个三方法任务身份、55 个 infeasible 状态和全部可行权值一致。

P1 按图聚合如下。`min/P` 只验收每图的 `min(Base, Enhanced) <= PrunedDP++`，不构造逐查询 oracle 配置。

| 图 | Base / 秒 | Enhanced / 秒 | PrunedDP++ / 秒 | `min/P` |
|---|---:|---:|---:|---:|
| DBLP-GPU4GST | 8,035.299 | 9,694.387 | 13,683.019 | 0.5872 |
| DBLP-MonoGSTPlus | 3,766.660 | 3,160.483 | 29,049.935 | 0.1088 |
| DBpedia-MonoGSTPlus | 8,386.674 | 7,593.174 | 20,288.408 | 0.3743 |
| Github-GPU4GST | 90.900 | 76.562 | 338.436 | 0.2262 |
| LinkedMDB-MonoGSTPlus | 611.421 | 491.172 | 6,612.577 | 0.0743 |
| LiveJournal-GPU4GST | 14,080.989 | 24,257.869 | 27,535.664 | 0.5114 |
| MovieLens-MonoGSTPlus | 1,090.124 | 869.355 | 3,041.379 | 0.2858 |
| Musae-GPU4GST | 61.146 | 38.497 | 186.901 | 0.2060 |
| Orkut-GPU4GST | 30,744.397 | 45,405.709 | 120,913.770 | 0.2543 |
| Reddit-GPU4GST | 6,246.078 | 6,150.121 | 7,294.956 | 0.8431 |
| Toronto-MonoGSTPlus | 10.374 | 16.916 | 16.241 | 0.6387 |
| Twitch-GPU4GST | 77.349 | 81.723 | 448.326 | 0.1725 |
| Youtube-GPU4GST | 2,804.511 | 3,603.514 | 3,004.472 | 0.9334 |

结果为 13/13 PASS；最紧的 YouTube 仍有 6.66% 聚合时间优势。正式 Orkut `g=15` 十条如下，均为当前生产二进制的无诊断记录：

| 查询 | CPU | 权值 | 秒 | 峰值 MiB | `(mask,v)` states |
|---:|---:|---:|---:|---:|---:|
| q1 | 4 | 21 | 1,988.313 | 3,270.918 | 95,833,201 |
| q2 | 4 | 32 | 1,725.783 | 3,762.445 | 162,834,196 |
| q3 | 5 | 38 | 274.595 | 2,690.477 | 30,571,329 |
| q4 | 5 | 28 | 291.363 | 2,995.301 | 12,586,184 |
| q5 | 5 | 32 | 4,135.305 | 6,341.828 | 475,605,896 |
| q6 | 5 | 36 | 2,033.287 | 5,990.352 | 201,117,345 |
| q7 | 5 | 38 | 2,303.644 | 5,990.352 | 265,320,212 |
| q8 | 5 | 18 | 210.787 | 5,990.352 | 394,894 |
| q9 | 5 | 35 | 1,070.936 | 5,990.352 | 96,019,704 |
| q10 | 4 | 54 | 9,544.561 | 18,318.238 | 1,459,398,194 |

q10 保持精确权值 54，状态数是当前辅助半层状态域的真实计数；它比 10,000 秒 TL 少 455.439 秒。旧错误版的 1,654,690,262 states 只保留在历史表，不是相等门。至此性能硬门与正确性硬门均通过；CTest、Markdown、环境和源码—文档检查的最终仓库结果记录在 [`../../experiments/correctness_audit.json`](../../experiments/correctness_audit.json)。

#### 9.1 本轮冻结后的分级复测

本轮完整 P1 与 Orkut q1--q10 是冻结生产二进制的最终全量参考。此后不把全量当作每个局部清理的默认 gate：

1. 只改文档、注释，或重建后生产二进制逐字节相同时，不运行性能实验；
2. 单点、已给出输入无关等价证明且只影响一个物理热阶段的小删除，不再重跑全量 P1，也不重跑 Orkut q1--q9；只运行本轮结束后按固定规则冻结的 P1 高风险哨兵与 Orkut `g=15` q10；
3. 哨兵已经一次冻结：13 图各取最终最快 ABHSS 配置中耗时最大的一条，加入历史反向项 Orkut `g=7` q175 Enhanced，再加入 Musae `g=5` q1--q100 Enhanced 固定成本块；选择只读冻结结果，不读取候选结果；
4. 哨兵的权值、可行性和 `(mask,v)` 状态必须逐项不变。候选时间只有明确不高于冻结参考才接受；方向接近或交换 CPU 后不稳定时，结论是“不足以接受删除”，而不是用容差放行；
5. q10 仍使用单查询 10,000 秒绝对硬门。可把候选 q10 与 P1 哨兵固定到两个已经校准的独立物理核并行运行，但不能在同核叠加求解器；
6. 只有删除跨越多个热阶段，或改变状态定义域、递推依赖、发布生命周期、证书可采纳性及上界见证链时，才重新考虑完整 P1；触发条件按语义覆盖面判断，不按源码行数或数据集表现设置经验参数。

CPU 5 标准哨兵共 114/114 成功：14 条跨图/历史风险项合计 816.198581 秒；Musae 固定块 100 条合计 4.199400 秒、峰值 10.312 MiB、累计 76,770 states。全部 114 条的任务身份、权值、可行性和状态数与正式 P1 逐项一致。具体 task key 和参考值冻结在 [`../../experiments/correctness_audit.json`](../../experiments/correctness_audit.json) 的 `final_dominated_operation_gate.post_freeze_sentinel`。

这套小门只用于后续开发中的“无可识别退化”决策，不能改写成新的 P1 全量证据。论文正式时间仍对应本轮冻结二进制；若后续候选改变了该二进制，文档必须同时保留冻结结果身份和候选哨兵身份。本轮严格支配审计、最终全量门和后续分级复测基线至此全部完成。

<a id="history-monotone-certificate-frontier-probe-20260815"></a>

## 单调证书拒绝前沿探针（2026-08-15）

> 原始记录：`MONOTONE_CERTIFICATE_FRONTIER_PROBE_20260815.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> **状态：局部优化保留，旧整体验证已失效。** 本文证明的单调拒绝前沿仍是安全的等价工作消除；但用于长测的二进制继承了从逻辑层直接启动 H 的精确性错误。下述 Youtube Base 轨迹仍可说明该局部优化没有进入 Base 路径，旧 Orkut 时间只能作为性能历史，不能证明当前 Enhanced 的答案或 10,000 秒门。修复见 [辅助半层 Adjoint 正确性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-auxiliary-half-adjoint-correctness-audit-20260815)。

### 1. 问题与论文边界

Enhanced 的 ordinary row 可能对同一顶点看到多个候选标签；已有真实或虚拟 `distance` 只让更小标签继续，但正确性不要求这些候选按大小到达。staged future 已经把固定 `(mask, vertex)` 的可采纳下界缓存在 `bound_cache`，但当 directed-cut 的 certified interval 下端反复拒绝标签、而 exact fallback 尚未完成时，旧实现仍需对每个标签重新进入 `CanImprove`、读取状态并执行同一浮点比较。Orkut g=15 q10 的瓶颈正集中在这种 stage 0 缓存拒绝；Youtube P1 几乎没有该事件。

本轮只考虑输入无关、精确且能进入论文主线的等价工作消除。明确排除按图名、组数、组大小、运行时间或状态量切换的规则，也排除整数权、浮点精度压缩、近似比较和固定深度 top-k 缓存。

### 2. 最终候选：stage-0 分析拒绝前沿

设当前真实可行上界为 `U`，固定 row 和顶点的已缓存可采纳下界为 `L(v)`，ordinary 候选的真实已付代价为 `x`。原代码唯一的接纳条件是：

```math
x+L(v)<U.
```

因此同一 `U` 与 `L(v)` 下，所有满足原始浮点谓词 `!(x + L(v) < U)` 的标签形成一个向上的拒绝区间。当前候选不改变这个谓词，而是在 row 已经按原规则确认值得物化前沿时执行一次：

1. 从 `max(0, U - L(v))` 开始；
2. 若用原来的浮点加法顺序检查后仍可改善，就用 `nextafter` 向上移动到首个被原谓词拒绝的可表示值；
3. 把该值作为虚拟 `distance[v]`，而不是保存本次偶然遇到的较大拒绝标签。

后续不小于该前沿的标签由已有 `next >= distance[v]` 支配，因而不再进入证书状态机。若后来出现更小且能通过完整证书链的标签，原有 crossing 分支会清除前沿位、把顶点加入 `touched`，随后写入真实距离并正常入堆。虚拟值本身从不入堆、不进入 settled row、不计为 `(mask, vertex)` 状态；row 结束时由独立的 `rejected` 列表恢复为无穷。

候选只在 stage 0 使用分析 cutoff。这里的 stage 0 具有算法语义：directed-cut certified interval 尚未完成 exact/upper determination；它不是由 `g`、`f`、图名或经验阈值划出的类别。stage 1--4 已经完成 dual 职责，继续保存实际拒绝值即可。这样只把 q10 中占主导、Youtube P1 中不存在的未完成 interval 复用转成支配快路，而不让所有普通拒绝都支付 eager cutoff 成本。

诊断构建给出了直接结构证据。q10 前 50 张 completed row 中，stage 0 有 229,590,946 次缓存拒绝和 108,724,980 次前沿物化，stage 1 为 17,705,449/8,012,631，stage 4 只有 41/22。Youtube P1 前 30 条中 stage 0 为 0/0，stage 1 为 8,822,861/1,458,067，stage 3 和 stage 4 仅为 529/195 与 21,730/7,203。因此 stage-0 分析前沿针对的是证书生命周期差异：它覆盖 q10 的主导重复，却不会在 P1 主路径增加 cutoff 物化。

### 3. 正确性

#### 3.1 固定证书下的安全性

对非负有限 `double`，固定 `L(v)` 时，浮点加法关于非负的 `x` 单调。构造得到的 cutoff `T(v)` 明确满足与原实现逐位相同的拒绝谓词：

```math
!(T(v)+L(v)<U).
```

所以任意可表示的 `x >= T(v)` 也必被同一谓词拒绝。前沿只提前跳过原实现必然返回 `false` 的 `CanImprove` 调用，不新增剪枝结论。

#### 3.2 上下界更新后的安全性

一条查询内 `best` 只会下降，可采纳 future 在同一 row 内只会保持或加强。上界下降或下界增大都会扩大拒绝集合，因此旧前沿最多变弱，不会变成错误剪枝。若一个更小标签不被前沿支配，它仍逐阶段执行原证书链；interval 下端没有被冒充为 exact dual。

#### 3.3 状态与输出不变

前沿物化只发生在 `distance[v]` 仍为无穷且当前标签已经被原证书拒绝时。该虚拟值不会进入初始堆或邻接松弛堆。真正可接纳的 crossing 标签会先登记 `touched` 并清除前沿位，再由调用者写入真实距离。因此最终 row、branch bit、队列 key、状态计数、答案及浮点求和顺序均不改变。

### 4. 成本与复杂度

每个实际物化的 stage-0 前沿只增加一次减法、一次非负截断和必要的相邻浮点值修正；固定 binary64 精度下这是常数工作。 在仓库使用的 IEEE round-to-nearest 下，若重组加法仍落在 `best` 下方，向上一个相邻值已经越过实数阈值；源码保留 `while` 是为了直接以原谓词自证，而不是按数据迭代或设置次数参数。前沿更新继续走原有的一次赋值快路。没有新增按顶点数组、图预处理或查询参数，最坏时间、空间复杂度和 Base 路径均不变。

源码同时删除了从未被读取的 `active_frontiers` 计数。该删除不改变任何分支，只避免在 crossing 热路径维护无消费者的整数状态。

### 5. 候选筛选与失败路线

在分析 cutoff 之前，先测试过只把“本次实际被拒绝的标签值”写入虚拟 `distance`。完全 eager 的做法虽然能减少 q10 的重复证书入口，却让 Youtube P1 前缀出现约 0.5%--1.5% 的反向变化；在证书出口重复判断、把前沿并入 split 工作区或改变 crossing 更新位置也没有形成稳定双端收益。当前 row-local admission 因而只在一张 row 已经观察到缓存拒绝复用后才启动前沿：首个顶点用重复拒绝支付准入证据，此后该 row 的其他顶点可直接物化。它是一次写入/清理成本与至少一次已观察复用之间的确定性 break-even，不读取任何查询参数。最终候选保留这条 P1 友好的准入规则，只把 stage 0 首次物化的值从偶然观测标签加强为同一证书解析出的完整拒绝区间下端。

| 候选 | Orkut q10 信号 | Youtube P1 信号 | 决策 |
| --- | --- | --- | --- |
| 所有 stage eager cutoff | 900 秒完成 173 row，对照 146 row；共同 row 完全一致 | 未形成可接受的全量门 | 拒绝：范围过宽 |
| row-global all-stage cutoff | q10 有正向信号 | 同 CPU 为 `+0.62%` | 拒绝 |
| pure per-vertex all-stage cutoff | q10 有正向信号 | 两轮合计 `+1.33%` | 拒绝 |
| stage-0 cutoff 在每次 miss 时判断 | 600 秒完成 27 row，对照 26 row | 同 CPU 约 `+0.44%` 至 `+0.7%` | 拒绝：重复阶段分支抵消收益 |
| stage-0 cutoff 在首次物化时一次完成 | 600 秒完成 55 row，对照 26 row | 进入全量门 | 最终候选 |
| 抽成 noinline helper | 600 秒只完成 46 row | 首轮 30 条由 205.514 秒变为 208.504 秒 | 拒绝：调用与布局成本 |

所有失败候选都没有改变答案或状态，但不能用 Orkut 单点收益覆盖 P1 退化。失败实现不进入正式源码；原始目录在本文件记录关键数字后删除。

### 6. 旧错误二进制上的历史门禁

#### 6.1 固定窗口与共同轨迹

最终候选在 Orkut g15 q10 的 600 秒诊断窗口完成 55 张 ordinary row；同口径 row-local 对照完成 26 张，stage-0-on-miss 候选完成 27 张。共同 row 的答案、状态、branch 与证书阶段诊断一致。提升来自更多标签被既有拒绝区间直接支配，不是更改状态域。

#### 6.2 Youtube P1 全量

最终候选在 CPU4、NUMA node 0 完成 Youtube P1 Enhanced 全 300 条：

| 构建 | 总时间 | 峰值 RSS overhead | `(mask, vertex)` 状态 |
| --- | ---: | ---: | ---: |
| 最终候选 | 2,107.963 秒 | 218.477 MiB | 67,570,613 |
| tour-envelope 历史同轨迹 | 2,117.447 秒 | 218.332 MiB | 67,570,613 |
| progressive-final 历史同轨迹 | 2,119.583 秒 | 217.859 MiB | 67,570,613 |

候选与两份历史结果的 300 条答案、状态逐条零差异；时间约改善 0.45%--0.55%。不足 1 MiB 的 RSS 差异按 1 ms 采样波动处理，不声称空间改善。Base 不进入 DirectedCut staged 分支，目标差异没有改变 Base 执行语句。

最终候选随后在同一 CPU4、NUMA node 0 完成 Base 全 300 条：1,859.266 秒、122.453 MiB、74,359,677 状态。同 CPU 冻结证据由 60 条前缀与 240 条尾段组成，合计 1,876.588 秒、128.586 MiB、相同状态；300 条权值和状态逐条零差异，候选时间改善 0.92%。此前顺序运行的前 30 条 A/B 也给出同一方向：最终候选 186.090 秒、冻结 row-local 对照 187.848 秒，均为 8,090,874 状态且权值逐条一致。因此 Base 的完整路径和机器码布局门均不退化；RSS 只按采样值如实登记，不据此声称算法空间改善。

用于这些历史长测的候选二进制 SHA-256 为 `a957bdcecafc486575cb7d78779dcbeda687eb0ea7283a0fc1104089dce83b86`。它不再是当前 release 哈希。

#### 6.3 Orkut g15 历史直接结果

以下结果由当时的候选二进制直接产生。后来发现其独立精确门没有覆盖辅助半层缺失的固定反例，且该二进制在 P1 的 LiveJournal、Orkut 多条查询高报。因此表中时间与资源只用于估计当前修复版风险；权值、状态和“十条通过”不能继承为当前结论。

| 查询 | 查询时间 | 最优值 | 峰值 RSS overhead | 状态数 |
| --- | ---: | ---: | ---: | ---: |
| q1 | 2,270.862 秒 | 21 | 3,270.934 MiB | 105,902,192 |
| q2 | 2,523.204 秒 | 32 | 3,609.332 MiB | 261,581,592 |
| q3 | 281.276 秒 | 38 | 2,690.473 MiB | 31,051,638 |
| q4 | 315.548 秒 | 28 | 2,995.387 MiB | 15,061,435 |
| q5 | 4,568.638 秒 | 32 | 7,130.719 MiB | 560,869,444 |
| q6 | 3,847.348 秒 | 36 | 6,019.355 MiB | 452,486,374 |
| q7 | 3,451.310 秒 | 38 | 4,815.082 MiB | 364,962,795 |
| q8 | 204.234 秒 | 18 | 2,690.305 MiB | 366,330 |
| q9 | 1,465.794 秒 | 35 | 3,538.766 MiB | 135,705,582 |
| q10 | 9,934.603 秒 | 54 | 19,776.727 MiB | 1,654,690,262 |

旧 q10 写出结果行 `query_seconds=9934.602789`，外层进程另含 30.351 秒图加载。该值说明当前复跑只有约 65 秒历史余量，风险很高；十条旧结果的总查询时间 28,862.817 秒与总状态数 3,582,677,644 均不得写入当前正式结果。

### 7. 当时的接入门及其缺口

只有同时满足以下条件才把候选合入当前工作树：

1. 当时二进制的 Orkut g15 q10 写出结果且 `query_seconds < 10000`；
2. 当时二进制的 q1--q10 都有直接结果；
3. Youtube P1 全量和 Base 静态路径均不退化；
4. 五项 CTest、源码/文档矛盾检查和 GitHub Markdown 检查全部通过。

前两项当时被误称为精确整体门，因为第五项“辅助半层固定反例”尚不存在。单调拒绝前沿这项局部优化满足了物理接入条件并仍保留；“最终候选满足四项整体条件”的旧结论已被辅助半层反例推翻。当前整体接入必须重新通过五项 CTest、P1 权值与聚合门、以及 Orkut g15 q1--q10 的当前二进制门。

<a id="history-adaptive-a1-top-two-materialization-gate-20260816"></a>

## A1 top-two 自适应物化门禁（2026-08-16）

> 原始记录：`ADAPTIVE_A1_TOP_TWO_MATERIALIZATION_GATE_20260816.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> **后续状态（2026-08-19）。** 本文是中间 checkpoint 的局部证据；当时尚未完成的生产版 Orkut q1--q10 与 13 图 P1 后续均已通过。本文的 900 秒进度仍不能冒充最终时间，最终结果见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

### 1. 状态与边界

本记录审计共同 A1 future 的物理查找方式。候选在该时点可以进入本地 checkpoint，但不构成最终性能结论：当时完整 P1 尚未在该生产二进制上复跑，Orkut g15 q10 也尚未在 10,000 秒内完成。不得把本文的 900 秒进度领先改写成“q10 已过线”；后续最终结论只使用文首生产版记录。

候选只改变 top-two 精确视图的构造顺序：

- 少量新顶点仍逐 singleton row 二分，并把最大/次大 bit 与原 `double` payload 下标写入已有缓存。
- 已支付的首次查询工作达到由 row 形状计算的购买点后，按顶点和 bit 递增顺序扫描全部 row，在同一缓存中填满完全相同的 bit、locator 与 fallback 标志。
- Base、DirectedCutOnly 与 Enhanced 调用同一个 `InitializeLookupPlan`、`Future` 和 `MaterializeAllTopTwo`；代码不读取增强位、图名、查询编号、计时或经验组数阈值。
- 没有压缩浮点精度，没有新增近似值，没有删状态，也没有增加永久 `double[n]` 表。

正式定义、购买式和内存边界见 `../METHOD.md` 第 9.4、9.5 与 14.1 节；代码入口见 `../CODE_GUIDE.md`。

### 2. 为什么数值与搜索轨迹不变

对每个固定顶点，两条路径都按相同的 singleton bit 升序读取同一值。row 命中时复制原 `row.value` 与 32-bit 下标；缺项时调用同一个 `FallbackValue`。最大和次大只用严格 `>` 更新，所以并列选择也一致。

购买只改变“逐顶点二分”与“按 row 游标顺扫”两种枚举次序。它不改变 A1 row、future 最大值、ordinary 接纳谓词、堆节点、状态计数或 incumbent。新增生产回归 `CheckAnchoredSingletonMaterializationEquivalence` 构造 64 点、4 个 dense singleton row；其结构购买点小于顶点数，因而必然进入顺序物化。测试对全部 64 个顶点和 15 个非空 remaining mask 与独立逐 bit 最大值逐项比较，并断言购买路径确实发生。

### 3. 被拒绝的中间实现

以下实现均未保留：

- 无条件 eager 物化或额外保存两张 dense `double` 表：小查询必付全图工作，且放大 RSS。
- 在每个 singleton bit 内维护细粒度 rent：它把共同 top-two 比较和二分差分混在一起，Musae g7 Base 出现约 1.7% 回归。
- 在同一次首次查询中重复判断或重复记 rent：属于纯冗余热分支。
- 以运行秒数、图名、查询编号或固定 g 阈值决定购买：不能形成论文可解释的统一规则。

当前版本只在一个顶点第一次构造 top-two 后递减一次剩余购买计数；fallback 成本不计入既付 rent，避免因可选成本的乐观估计而过早购买。

### 4. 正确性门

| 门禁 | 结果 |
|---|---|
| 仓库 CTest | 5/5 通过 |
| 独立随机真值 | 5000 个 g=2..10 随机实例，Base、DirectedCutOnly、Enhanced 全部匹配独立 subset DP |
| 正权唯一终端压力 | 500 个 g=6..10 Enhanced 实例全部匹配 |
| 辅助半格压力 | 160 个 g=7..16 Enhanced 实例全部匹配 |
| A1 购买分支直接覆盖 | 64 点乘 15 个 remaining mask 全部逐项一致，且 `lookup_materialized=true` |
| 零权边与状态口径 | `abhss_zero_weight_regression`、`mask_vertex_state_regression` 通过 |
| SteinLib 已知真值 | Base 与 Enhanced 各 11/11；值序列均为 361、237、497、250、422、208、179、798、290、405、1190 |
| GitHub Markdown | `validate_markdown.py` 通过全部 87 个 Markdown 文件 |

生产 SteinLib 证据目录：

- `results/probes/a1material_production_v2_steinlib_base_20260816`
- `results/probes/a1material_production_v2_steinlib_enhanced_20260816`

两项 SteinLib gate 为并行正确性核验，其运行时间不能作为论文性能数。

### 5. 非退化探针

#### 5.1 Musae g7 Base 全 300 条

| 启动顺序 | 候选总时间 / s | 对照总时间 / s | 候选 / 对照 | 两边状态数 | 候选 / 对照峰值 MiB |
|---|---:|---:|---:|---:|---:|
| 候选先 | 46.684363 | 46.712625 | 0.9994 | 6,872,062 | 4.551 / 4.531 |
| 对照先 | 46.479372 | 46.694294 | 0.9954 | 6,872,062 | 4.535 / 4.500 |

300 条权值逐项一致；该图未达到顺序物化购买点，因此结果主要证明最终 countdown 热路径没有重现早期细粒度记账回归。

#### 5.2 Orkut g15 q3 完成对

| 版本 | 时间 / s | 最优值 | 状态数 | 峰值 MiB |
|---|---:|---:|---:|---:|
| 候选，CPU 4 | 292.941140 | 38 | 31,191,511 | 2690.480 |
| 对照，CPU 5 | 293.053563 | 38 | 31,191,511 | 2689.539 |

该查询未购买顺序物化；时间差为 -0.04%，状态与权值完全相同。

#### 5.3 Orkut g15 q10 的 900 秒同跑

最终 countdown 候选固定 CPU 5，对照固定 CPU 4。候选在 0.676516 秒内顺序物化 14 张 A1 row、39,056,188 个标量，并在超时前完成 208 张 ordinary row；对照完成 206 张。两边已完成对应 row 的 `best`、状态标量和 row work 一致。候选终点 watchdog RSS 为 10061.40 MiB，对照为 10005.63 MiB，但候选多推进两张大 row，不能把两个不同进度的终点 RSS 解释为固定缓存开销。

这只说明候选方向没有在 q10 前 900 秒退化并带来约两张 row 的进度收益。它没有证明总耗时小于 10000 秒；当前有效完整版本的 q10 记录仍是 10000 秒超时。

证据目录：

- `results/probes/a1material_round2_candidate_cpu5`
- `results/probes/a1material_round2_control_cpu4`
- `results/probes/a1material_p1_musae_g7_countdown_cpu4`
- `results/probes/a1material_p1_musae_g7_countdown_control_first_cpu4`
- `results/probes/a1material_q3_candidate_cpu4`
- `results/probes/a1material_q3_control_cpu5`

### 6. 结论

候选满足“共同操作、精确值不变、无经验超参数、论文可解释”的 checkpoint 条件，并消除了大图 dense A1 上反复随机二分的一部分物理工作。它不是解决 Orkut 状态爆炸的算法性剪枝，单独收益不足以宣告最终目标完成。下一阶段仍需在生产二进制上完成 q10 的谨慎长门，并继续审查 ordinary/adjoint 中可被严格支配而减成空的操作；最终接受条件仍是完整 P1 不劣且 Orkut g15 q1--q10 每条小于 10000 秒。

<a id="history-a1-complete-ranking-and-staged-ceiling-gate-20260817"></a>

## A1 完整排名、精确租金因子化与 ceiling 负向门禁（2026-08-17）

> 原始记录：`A1_COMPLETE_RANKING_AND_STAGED_CEILING_GATE_20260817.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> **后续状态（2026-08-19）。** 本文保留实现与失败候选的局部门禁不变；当时尚未完成的生产版性能门后续已经通过：Orkut g15 q1--q10 全部低于 10,000 秒，13 图 P1 均满足最快 ABHSS 配置不劣于 PrunedDP++。最终值与哈希见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

### 1. 当前结论与发布边界

本记录接续 [A1 top-two 自适应物化门禁](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-adaptive-a1-top-two-materialization-gate-20260816)。前一阶段只在 `first/second/cached_locator_pair` 中切换 lazy 二分和 top-two 顺序物化；本阶段处理 top-two 都已被 remaining mask 排除时的 tail 查找。

当前源码候选保留三项共同物理 realization：

1. top-two 已购买后，按实际支付的逐 bit 二分工作累计第二级 rent；达到由 row 形状确定的 buy 后，为每个顶点物化 top-two 之外全部 bit 的完整 byte 排名；
2. top-two 购买时以 $O(2^k)$ 子集递推精确因子化各 remaining mask 的整数 rent，后续热路径只读一个表项；
3. 两个全图物化函数保持一次性冷机器码边界，不被 IPO 并入逐状态 `Future`。

三者都不读取配置、图名、查询编号、时间、状态数、row 密度或固定组数阈值。Base、DirectedCutOnly 与 Enhanced 调用同一 `AnchoredSingletonFuture::Future`。排名没有压缩 `double`，也不是固定 top-k。
`Future` 始终返回“row 内真实 A1、row 外非负 fallback”统一视图的精确最大值；无条件和 staged second-rank ceiling 均已因 P1/归一化进度证据回退。

保留实现此前已通过两种构建各 5/5 CTest；本轮又加入全部 mask 的租金表直接断言。冷边界 pre-reduction 二进制已通过 Musae g7 Base 全 300 条两轮小门，但 Orkut g15 q10 在 10001.768655 秒被 watchdog 终止且没有写出权值。后续发布屏障 ready 减空与 ranked-buy 恒正减空已通过独立交换轮，详见 [A1 发布屏障与恒真检查减空门禁](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-a1-publication-barrier-reduction-gate-20260817)。加入 Adjoint split 完备性修复后的当时完整 q10 在 11,178.235 秒返回精确值 54，仍未通过 10,000 秒硬门；该历史构建当时也尚未复跑完整 P1，不能据此提前宣称总目标完成。后续生产版状态以文首更新为准。

### 2. 完整排名的结构购买

令非锚组数为 $k$，tail 长度为 $t=k-2$。top-two 顺序物化使用前一文档记录的 $B_{\mathrm{scan}}$。top-two 已存在后，一次旧式 tail 查询实际为 remaining 中每个 singleton row 支付一次二分和一次合并；代码只累计这些已经发生的工作。

第二级购买成本为：

```math
B_{\mathrm{rank}}=B_{\mathrm{scan}}+
n\left(\frac{t(t-1)}{2}+t\right).
```

第一项保守覆盖再次顺扫全部 singleton row 与缺项 fallback，第二项覆盖每个顶点的稳定插入排序和 byte 写入。购买后每个顶点保存全部 $t$ 个 tail bit 的非增次序。查询只线性扫描 byte，找到第一个仍在 remaining 中的 bit，再通过原 `Value` 路径读取一次精确 row double 或同一非负 fallback。

因此：

- 不存在需要调参的排名深度；
- 不保存近似值、float、量化整数或复制的 double；
- 最坏额外 tail 容量是 $t(n+1)$ 字节；
- 连同两个 bit byte、两个压入 64-bit 的 locator 和精确租金表，总查找缓存最坏为 $(k+8)(n+1)+4\cdot2^k$ 字节；
- ordinary 结束后全部查找缓存释放，标准 A1 row 本身按原所有权移交。

### 3. 精确租金因子化与冷购买边界

top-two 已物化后，一次 tail 查询的租金只由 remaining mask 和各 singleton row 长度决定。代码在同一次购买中构造整数表 $C$：

```math
C[0]=0,\qquad C[M]=C[M\setminus\{j\}]+b_j+1,
\quad j=\mathrm{lsb}(M).
```

对子集大小归纳可得 $C[M]=\sum_{i\in M}(b_i+1)$，所以一次表读取与旧逐 bit 累加严格相等；累计 rent、排名购买调用点和全部浮点读取均不变。代价是一次 $O(2^k)$ 整数递推和 $4\cdot2^k$ 字节，且只在 top-two 已购买时发生。

`MaterializeAllTopTwo` 与 `MaterializeRankedTail` 在一条查询中各至多运行一次。把它们设为跨编译器非内联函数，只把一次性购买代码留在冷边界，避免 Release IPO 扩大逐状态 `Future`；它不增加算法分支，也不改变执行语句。当前 `Future` 在全部生命周期都返回统一 row/fallback 视图的精确最大值，不接收既有下界。

### 4. 正确性直接覆盖

`CheckAnchoredSingletonMaterializationEquivalence` 使用 64 个顶点和 4 张 dense singleton row：

1. 遍历全部 64 个顶点和 15 个非空 remaining mask，把 lazy、top-two 与 ranked-tail 返回值逐项对照独立逐 bit 最大值；
2. 断言 top-two 与 ranked tail 两级购买都实际发生，且购买前后 `Future` 都返回同一精确值；
3. 遍历全部 mask，独立按 bit 累加 `BinarySearchCost(row[bit].size())+1`，逐项对照精确租金表；
4. 排名只保存 bit 次序，最终值始终从原 row double 或同一非负 fallback 读取。

该测试只覆盖物理视图和购买计数等价；完整精确性仍由随机 subset DP、零权、状态计数和辅助半层测试共同承担。

### 5. 当前性能证据

#### 5.1 Musae g7 Base 全 300 条

只加入完整 byte 排名时，四轮串行候选/干净 checkpoint 为：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 候选 / 对照 |
|---|---:|---:|---:|
| 1 | 46.234701 | 46.549594 | 0.993235 |
| 2 | 47.337311 | 46.464792 | 1.018778 |
| 3 | 46.791805 | 45.540309 | 1.027481 |
| 4 | 46.617773 | 47.129987 | 0.989132 |

四轮几何比为 1.007024，方向混杂，不能作为“小组不退化”的干净证据。

加入精确租金表后、尚未隔离冷路径时：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 候选 / 对照 |
|---|---:|---:|---:|
| 1 | 45.571224 | 46.412085 | 0.981883 |
| 2 | 46.221529 | 46.076770 | 1.003142 |

将两个一次性物化函数移出 `Future` 的 IPO 热边界后：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 候选 / 对照 |
|---|---:|---:|---:|
| 1 | 46.308637 | 46.749759 | 0.990564 |
| 2 | 45.720185 | 46.655134 | 0.979960 |

两轮均为正向；全部 300 条权值/状态向量一致，状态总数均为 6,872,062。它只通过 P1 小门，仍不能替代最终 13 图全量 P1。

#### 5.2 Orkut g15 q10

针对同一 q10 的固定窗口完成 row 数如下；对照均为干净 checkpoint：

| 候选版本与窗口 | 候选 / 对照完成 row | 归一化进度 |
|---|---:|---:|
| 旧完整 byte 排名，900 秒 | 219 / 210 | +4.29% |
| 旧完整 byte 排名，3600 秒 | 2228 / 1934 | +15.20% |
| 精确租金表仍内联，900 秒 | 207 / 206 | +0.49% |
| 精确租金表 + 冷物化边界，900 秒 | 213 / 209 | +1.91% |
| 冷边界 + 发布后 ready 减空，交换轮 1 | 216 / 214 | +0.93% |
| 冷边界 + 发布后 ready 减空，交换轮 2 | 219 / 218 | +0.46% |
| 再减 ranked-buy 恒正检查，交换轮 1 | 220 / 216 | +1.85% |
| 再减 ranked-buy 恒正检查，交换轮 2 | 221 / 214 | +3.27% |

四组共同 ordinary 前缀的 `(layer,layer_work)` 均逐项一致；最后一组的排名购买租金仍为 389,006,522，物化约 0.914 秒，候选与对照 watchdog RSS 均约 10,061 MiB。900 秒内排名接近窗口末尾才购买，因此旧 3600 秒的长期增益仍是重要但间接的证据。

“完整排名 + 精确租金表 + 冷边界”的 pre-reduction 生产二进制已在 10,000 秒门超时。后续 combined 诊断版另删除 A1 发布屏障后的恒真 ready 检查与 ranked buy 恒正检查，并补齐 Adjoint split；它在独占 CPU 5 上以 11,178.235 秒返回精确权值 54，查询峰值 21,337.590 MiB、watchdog 总 RSS 峰值 27,729.191 MiB、累计状态 1,761,794,764。该结果证明 combined 版本仍未过线；它不能被 900 秒完成 row 的局部领先改写成成功。

### 6. 本轮拒绝项

#### 6.1 无条件 second-rank ceiling

把 ceiling 放在 ranked tail 是否存在的判断之前，q10 900 秒归一化进度约领先干净 checkpoint 6.24%，但它阻止 tail rent 累计，900 秒内没有再购买完整排名。更重要的是，该候选 SHA 在 q10 背景负载下串行运行 Musae 两轮：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 变化 |
|---|---:|---:|---:|
| 候选后接对照 | 47.707738 | 46.870516 | +1.79% |
| 对照后接候选 | 47.580648 | 46.231797 | +2.92% |

结果向量与状态完全一致，说明回归来自 tail 很短时额外 second 读取/比较的固定成本。该放置已拒绝。

#### 6.2 购买后 staged second-rank ceiling

把相同判断延后到完整排名已购买后，不会阻断 tail rent，但两轮 900 秒交换绑核只得到以下 ordinary row 进度：

| 启动轮 | 候选 / 对照 |
|---|---:|
| 候选 CPU4、对照 CPU5 | 210 / 203 |
| 候选 CPU5、对照 CPU4 | 214 / 206 |

两轮共同前缀的 `(layer,layer_work)` 完全相同，几何进度优势约 3.67%，弱于同阶段无 ceiling 完整排名的 219/210（约 4.29%）。该判断仍扩大热接口并在短 tail 上付固定比较，故同样回退；正式方法不含任何 second-rank ceiling。

#### 6.3 把 max 合并移入 A1 API

将 `Future` 重命名为 `StrengthenLower` 并直接返回合并后的下界，在语义上等价且表面更易读，但改变 LTO 热代码形状。两轮同时交换绑核 Musae 为：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 变化 |
|---|---:|---:|---:|
| CPU4 / CPU5 | 46.790243 | 45.783691 | +2.20% |
| CPU5 / CPU4 | 46.532016 | 46.330567 | +0.43% |

几何归一约回归 1.31%，故恢复“`Future` 返回精确的统一 A1 视图候选、调用点统一取 max”。不能为了 API 外观保留实测退化。

#### 6.4 farthest 已有下界 ceiling

候选在 dual 下界达到全局最大组距离时跳过剩余组扫描，并把全局最大值作为 tour 的安全上包络。900 秒进度为 219/206，较同轮 A1-only 的 221/207 归一化优势略降；共同 206 张 ordinary row 的 `(layer,layer_work)` 完全一致，RSS 也几乎相同。它没有产生新剪枝，还让 tour 接口从精确 farthest 扩为上包络，已完全回退。

#### 6.5 tail locator 与固定 top-k

为 ranked tail 每项额外保存精确 locator 的候选，在 q10 3600 秒完成 2294/2014 张 row；byte-only 同轮为 2228/1934，归一化后 locator 反而较慢，并额外增加约 273 MiB。完整 byte 排名只保存 bit 后按需执行一次精确读取更优。

固定 top-k second-farthest 仍禁止：它既不是完整次序，也没有从 remaining 集合推出统一深度，会变成无意义超参数。

#### 6.6 旧完整排名单独硬门

尚未加入精确租金表与冷物化边界的完整 byte tail 生产版，在 q10 的 10000 秒门禁中于 10001.7688 秒被终止，未产生最终权值，人工采样 RSS 约 26 GiB。这证明旧版本未完成最终目标；它不能代表当前候选，也不能被写成成功。

#### 6.7 后续证书链上包络

候选在 top-two 已购买后读取全 singleton 视图最大值，并用全组最远距离构造 tour 上包络；若当前 directed-cut 下界同时覆盖两者，就直接把 farthest/A1/tour 三个后续阶段标为完成。该判断在数学上安全，也不改变浮点值。

完整 q10 证明它没有性能价值：候选与可比对照的 ordinary 各层 row、scalar 和 `layer_work` 完全相同，普通阶段却从 6,455.520 秒增加到 6,524.740 秒，增加 69.220 秒（1.07%）。也就是说，它只用额外 helper 调用、分支和缓存读取替换原本更便宜的后续证书求值，没有带来一项状态或图闭包工作削减。源码已删除 `LaterCertificatesDominated`、`FullUpperEnvelope`、对应诊断计数和正式文档口径；后续不得只凭理论支配关系重新加入。

### 7. 后续硬门

1. 清理后源码单核运行 Orkut g15 q10 的 10,000 秒硬门；当前已知正确完整轨迹为 11,178.235 秒，尚未通过。
2. 清理后源码完整运行 Orkut g15 q1--q10，逐条小于 10,000 秒且 q10 最优值为 54。
3. 当前源码完整 P1 按图聚合，13 图中每图最快 ABHSS 配置不劣于 PrunedDP++。
4. 重建两种构建并通过 CTest、Markdown 渲染检查、复杂度/RSS/二进制 SHA 和代码—文档矛盾检查。
5. 全部门禁通过后只做本地提交；本轮不得上传远程。

<a id="history-a1-publication-barrier-reduction-gate-20260817"></a>

## A1 发布屏障与恒真检查减空门禁（2026-08-17）

> 原始记录：`A1_PUBLICATION_BARRIER_REDUCTION_GATE_20260817.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> **后续状态（2026-08-19）。** 本文两项严格减空继续保留；当时尚未执行的生产版 Orkut q1--q10 与 13 图 P1 后续均已通过。最终值与哈希见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

### 1. 结论

本轮只研究公共 A1 已经完成发布之后的重复合法性检查，不增加下界、上界、状态、配置或数据相关开关。

当前源码接受两项减空：

1. A1 构建屏障之后，删除只读 future 视图中对 singleton `row[bit].ready` 的重复检查；
2. 第二级 ranked-tail 购买点删除恒真的 `ranked_buy_work > 0`，只比较已付 rent 是否达到 buy。

两项都作用于 Base、DirectedCutOnly 与 Enhanced 共用的 `AnchoredSingletonFuture`，不读取图名、查询编号、组数经验阈值、墙钟、状态数或 enhancement 位。数值、浮点读取顺序、购买公式、row payload 和 `(mask,v)` 状态数不变。

本记录写入时的 combined production SHA-256 为 `57d2afa0f7098cec9baff15fd8088695c006edf72adccdb1c5668f4ba2a4b505`；diagnostics SHA-256 为 `37204d7f4c80d654a5ae58082ca907675ef001038467680d7458b416dd142095`。该时点的 Orkut g15 q10 10,000 秒硬门尚未运行，所以本文件后续的局部结果不能改写成当时已经通过；最终生产版状态以文首更新为准。

### 2. singleton row 发布屏障

`BuildReusableAnchoredSingletonLayer` 的外层循环只可能有两种结果：

- witness-tree DP 收紧 incumbent：当前工作 row 被清空，已构造的部分 singleton 集合整体丢弃，从第一个 bit 重新开始；
- 没有重启：内层按全部 singleton bit 完整结束，每张 row 都在 payload 写入后设置 `ready=true`，然后才初始化 future 查找计划并返回。

payload 可以为空，但空 payload 与“未发布”不同；前者同样已经设置 `ready=true`，并通过统一 cone 外 fallback 返回安全值。因此一旦 `InitializeLookupPlan` 可见，全部 singleton row 必然已经发布。只读阶段反复执行 `if (!row[bit].ready) continue` 不可能改变：

- buy/rent；
- row 与 fallback 的选择；
- top-two 或完整排名；
- 返回下界；
- owner 移交与状态计数。

当前源码只删除 A1 发布屏障之后的这些检查。ordinary、H、forward owner 交接及其他存在“未生成/已发布空”区别的生命周期仍保留各自 `ready` 语义。

### 3. ranked buy 严格为正

A1 只在逻辑正层域包含 A1 时构造。由

```math
q=\left\lfloor\frac{g}{2}\right\rfloor-1
```

以及 `q>=1` 可得 `g>=4`，故非锚 bit 数 `k=g-1>=3`。可行输入又有 `n>=1`。第一层购买工作对每个 singleton bit 至少加入 `2n`，所以：

```math
B_{\mathrm{scan}}>0,\qquad
B_{\mathrm{rank}}=
B_{\mathrm{scan}}+
n\left(\frac{(k-2)(k-3)}{2}+k-2\right)>0.
```

因此进入第二级 tail rent 路径时，`ranked_buy_work > 0` 恒真。删除它不改变购买点；实际条件仍是 `ranked_rent_work >= ranked_buy_work`。

### 4. 正确性与构建门

production 与 diagnostics 构建均通过 5/5 CTest：

- 快速图读入结构；
- 查询读入校验；
- 零权 witness；
- 配置精确性；
- `(mask,v)` 状态计数。

A1 直接回归遍历 64 个顶点与全部 15 个非空 remaining mask，对照独立逐 bit 最大值，并强制经过 lazy、top-two、ranked-tail 与精确 mask-rent 表。两项减空不改变该测试的值、购买路径或状态向量。

### 5. 接受项性能门

#### 5.1 删除发布后 ready 检查

Musae g7 Base 全 300 条，两轮交换 CPU4/CPU5：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 候选 / 对照 |
|---|---:|---:|---:|
| 1 | 46.128891 | 45.566126 | 1.012351 |
| 2 | 45.825665 | 46.347212 | 0.988747 |

几何比为 1.000479，属于中性；两轮各 300 条权值、状态逐项相同，状态总数均为 6,872,062。

Orkut g15 q10 diagnostics 900 秒交换轮：

| 轮次 | 候选 ordinary row | 对照 ordinary row | 候选 / 对照总事件 |
|---|---:|---:|---:|
| 1 | 216 | 214 | 279 / 277 |
| 2 | 219 | 218 | 283 / 281 |

忽略观测耗时字段后，对照事件序列在两轮都是候选事件序列的严格前缀；峰值 RSS 同为约 10,061 MiB。

#### 5.2 删除 ranked buy 恒正检查

Musae g7 Base 全 300 条交换轮：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 候选 / 对照 |
|---|---:|---:|---:|
| 1 | 46.746360 | 46.412513 | 1.007193 |
| 2 | 46.118568 | 46.579688 | 0.990100 |

几何比为 0.998610；全部权值、状态逐项一致。

Orkut g15 q10 diagnostics 900 秒交换轮：

| 轮次 | 候选 ordinary row / witness buy | 对照 ordinary row / witness buy | 候选 / 对照总事件 |
|---|---:|---:|---:|
| 1 | 220 / 48 | 216 / 47 | 284 / 279 |
| 2 | 221 / 48 | 214 / 47 | 285 / 277 |

两轮对照事件序列同样是候选严格前缀；峰值 RSS 无增长。

### 6. 回退项

#### 6.1 top-two 哨兵守卫

A1 域内 `k>=3` 且 future 值非负，因此从 -1 初始化的 first/second 在扫描前两个 bit 后必有效。删除写入处与命中处的 `>=0` / `!=255` 守卫在语义上成立，但 Musae 两轮分别为：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 比值 |
|---|---:|---:|---:|
| 1 | 46.306288 | 45.871324 | 1.009482 |
| 2 | 46.110555 | 45.718554 | 1.008574 |

几何回归 0.9028%，交换核后方向不变，故完整回退。不能用源代码分支更少代替物理门禁。

#### 6.2 Future 入口域检查

生产调用确实只来自已初始化 A1，并且 ordinary mask 是非锚 full mask 的真子集；但删除 `first.empty() || !remaining` 后，Musae 两轮为：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 比值 |
|---|---:|---:|---:|
| 1 | 46.504723 | 45.614214 | 1.019523 |
| 2 | 46.359855 | 45.766229 | 1.012971 |

几何回归 1.6241%，故恢复该平凡边界检查。没有加入 NOP、强制对齐或按编译器分派等难以形成论文主线的机器特调。

### 7. 10000 秒硬门状态

在上述两项严格减空之前，“完整排名 + 精确租金表 + 冷物化边界”的 production 二进制 `d3aee9cf453327762a974ba332701f8c3ea388bf6328d6d7562bb8f1508c8ee0` 对 Orkut g15 q10 运行到 watchdog 10001.768655 秒后 timeout，没有写出权值；终止前人工 RSS 采样约 26.2 GiB。

该超时只否定 pre-reduction 二进制，不能证明当前 combined 版本失败。下一步必须对当前 SHA 单核运行同一 q10：

- timeout 为 10000 秒；
- 只有写出精确权值 54；
- 且 solver 时间严格小于 10000 秒；

才算通过。该段所要求的后续 q1--q10 全门和 13 图 P1 现已由文首冻结生产版完成。

<a id="history-incremental-certificate-support-dp-gate-20260818"></a>

## Certificate-support 增量 subset-DP 门禁（2026-08-18）

> 原始记录：`INCREMENTAL_CERTIFICATE_SUPPORT_DP_GATE_20260818.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 目的与边界

本轮只优化 Enhanced/DirectedCut 在 residual closure 之后反复购买的 certificate-support evaluator。它不改 ordinary、A/H、directed-cut potential、rent-or-buy 公式、购买位置、refilter 条件、浮点精度或最终精确性口径。Base 不产生 certificate support，也不执行每张 ordinary row 的 support 通知。

对照源码为提交 `6593b0de3c15f4deb11b6c9e5597c8ce49f3dd08`。由该源码独立重建的正式二进制 SHA-256 为 `96a4add04c2a731f76c1c61bd7762760ee2e8eb11f0bfab7c2e3abfcf25554c3`，与保存的历史正式二进制逐字节相同；q10 实际使用的历史诊断二进制 SHA-256 为 `5c81392125b8b003ec681e679599f9caf202befb0f6f1caf53e2f65b885992c9`。q10 增量诊断候选 SHA-256 为 `7be8af050899a81c56498d00cc1450c02c8249b54a648cac7e91a20836b75a30`。随后只把 support-mask 通知从共同 `Account` 接口移到 `BuildOrdinaryRowsImpl<true>` 的编译期 DirectedCut 分支，最终正式候选 SHA-256 为 `793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced`。该移动不改变 Enhanced 的通知时点或求值语义，只保证 Base 热循环不承担新增判断。

为未来超长询问给 `ordinary_row/layer` 增加累计秒数后，最终诊断二进制 SHA-256 为 `e4dd07df4bca170d505cad10ad5fe8931929b8de1bd120b3fef599fee916122e`；正式二进制哈希仍逐字节保持 `793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced`，证明诊断计时路径在正式构建中被完全消除。

### 2. 等价改写与正确性

旧 evaluator 每次购买都重新构造 support 顶点、Floyd metric 和全部 mask 的 subset DP。新 `CertificateSupportDpCache` 第一次仍执行完全相同的全量递推，之后保留固定 support 的 metric 和 DP。两次购买间若新发布的 ordinary mask 集为 $P$，脏域为：

```math
\mathcal U(P)=\{X\mid \exists M\in P,\ M\subseteq X\}.
```

新 $D(M)$ 只改变 $M$ 的 direct seed。对任意 $X\notin\mathcal U(P)$，其 direct seed不变；任一 split 子 mask 若受影响，就会包含某个 $M$，进而推出 $X$ 也包含 $M$，矛盾。按 mask 基数递增归纳，非脏状态的 split、merge 和固定 metric closure 均不变；只按原顺序重算脏超集即可得到与全量 evaluator 逐项相同的表。

以下生命周期事件会主动放弃增量假设：

- incumbent 收紧后的 destructive ordinary refilter 调用 `Reset`，下一次购买全表重建；
- residual closure 刷新 support 时销毁旧缓存并构造新缓存；
- 第一次 support 购买全表重建。

调度器继续使用完整 $B_{\mathrm{sup}}$，所以 rent、buy、购买序号与 refilter 时点不变。独立回归测试在合成 support 上分批发布多组 ordinary row，每批后把持久缓存结果与新建 stateless 全量 evaluator 比较；再删除已有 row 值并 `Reset` 后复比。全套 5/5 CTest 还包含 5,000 个确定性随机精确 oracle 实例和既有高组数反例。

### 3. Orkut `g=15` q10 自然完成结果

输入为 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt` 的 q10，固定 CPU 5，单进程运行。历史与候选分别保存在 `results/paper_runs/final_6593b0d_orkut_g15_q10_diag_cpu5` 和 `results/paper_runs/incremental_support_q10_full_diag_cpu5`。两边都开启同一层级的稀疏诊断；历史 timeout 为 30,000 秒，候选为 86,400 秒，均自然完成而未触发截断。候选完成时间为 9250.911428 秒。

| 指标 | 历史对照 | 增量候选 | 差值或比值 |
|---|---:|---:|---:|
| solver 秒 | 9395.877740 | 9250.911428 | -144.966312 |
| 端到端 old/new | 1.000000 | 0.984571 | 1.015670 倍加速 |
| 最优权值 | 54 | 54 | 相同 |
| `(mask,v)` 状态 | 1,459,398,194 | 1,459,398,194 | 相同 |
| 查询峰值 MiB | 18,305.211 | 18,318.242 | +13.031 |
| watchdog RSS MiB | 24,696.816 | 24,709.785 | +12.969 |
| 距 10,000 秒余量 | 604.122 | 749.089 | +144.967 |

阶段时间如下。相邻长跑存在正常系统波动，adjoint 可作为未修改阶段的环境对照。

| 阶段 | 历史秒 | 候选秒 | 说明 |
|---|---:|---:|---|
| prepare | 188.074 | 185.288 | 未修改 |
| A1 | 84.797 | 84.500 | 未修改 |
| ordinary | 6665.830 | 6529.610 | 包含 support 购买与 refilter |
| low anchor | 9.134 | 8.877 | 未修改 |
| adjoint transpose | 160.814 | 159.378 | 未修改环境对照 |
| adjoint 总计 | 2447.990 | 2442.580 | 未修改环境对照 |

两版 ordinary 均发布 6461 行、1,091,601,442 个标量；refilter 都在第 218 次购买把 `best` 从 59 收紧到 58，删除 180,686,498 个标量，留下 1428 行和 458,071,592 个标量。refilter 时间为 144.827 与 144.776 秒，说明搜索轨迹与主要固定成本一致。

### 4. Support 购买细分

两版均购买 witness/support 496 次，最初两次树 witness 购买相同；其后 494 次为 support evaluator。候选的 494 次中，2 次全量、492 次增量。

| 指标 | 数值 |
|---|---:|
| 旧版全量等价 mask 总数 | 8,093,202 |
| 候选实际重算 mask 总数 | 1,374,856 |
| 实际/全量 | 16.9878% |
| 增量最小重算 mask | 480 |
| 第一四分位 | 1,404 |
| 中位数 | 2,240 |
| 第三四分位 | 3,597 |
| 最大值 | 10,496 |
| 平均值 | 2,727.825 |
| 发布 ordinary mask | 6,454 |
| 接纳为 direct seed 的 mask | 6,445 |

9 个发布与接纳差来自两次全量重建前待处理的 row；全量路径直接读取全部当前 ordinary，因此没有遗漏。全部 support buy（含 refilter）从 400.633 秒降到 205.545 秒；扣除约 144.8 秒相同 refilter 后，evaluator 本体约从 255.806 秒降到 60.769 秒，即约 4.21 倍。端到端只快 1.57%，因为 90% 以上时间仍在 ordinary 主搜索与 adjoint，而不是该 evaluator。

### 5. P1 非退化门

| 面板 | 历史秒 | 候选秒 | 结论 |
|---|---:|---:|---|
| Enhanced Musae `g=7`, 全 300 条 | 22.195592 | 22.021389 | 候选快 0.79%，答案与状态逐条相同 |
| Enhanced Orkut `g=7`, q175，两轮合计 | 392.950553 | 391.618166 | 候选快 0.34%，答案与状态相同 |
| Enhanced Orkut `g=7`, q175，最终二进制 | 197.194998 | 197.392606 | 候选慢 0.10%，中性波动；内存少 0.387 MiB |
| Base Musae `g=7`, 全 300 条，最终热路径 | 46.614704 | 46.083246 | 候选快 1.15%，状态相同 |
| Base Orkut `g=7`, q175，最终热路径 | 165.347131 | 166.651139 | 候选慢 0.79%，状态与内存相同 |

Base 两个方向正负翻转，且最终模板实例在编译期没有 `PublishOrdinaryMask`，不能把该波动归因于算法新增工作。P1 门只支持“无可识别退化”，不支持宣称 Base 加速。

### 6. 长询问必须保留的中间信息

诊断构建在运行中逐事件写日志，而不是等查询结束后才汇总。下一次允许自然超过 timeout 的探索长跑必须保存：

- `prepare_start/end`、`singleton_anchor_start/end`、`ordinary_start/end`、逐 `ordinary_layer`；
- 每张 `ordinary_row` 和每层 `ordinary_layer` 从 ordinary 开始的累计秒数及工作量；
- 每次 `witness_buy` 的序号、当前 `best`、秒数、已发布 rows/scalars 与 paid rent；
- 每次 `support_dp_full/incremental` 的 `evaluation`、`published_masks`、`activated_masks`、`recomputed_masks`；
- 每次 `witness_refilter` 的触发序号、前后 `best`、删除标量数与耗时；
- `adjoint_transpose`、逐 `adjoint_layer` 与 `adjoint_end`；
- 最终 solver time、weight、query peak RSS、watchdog RSS 与 `(mask,v)` 状态数；
- 输入路径和 SHA-256、源码提交、正式/诊断二进制 SHA-256、CPU 绑核与同时运行任务。

这些量足以区分“support evaluator 优化无效”“ordinary 状态爆炸”“refilter 固定成本”“adjoint 后缀昂贵”和“机器并发波动”。正式论文计时仍使用无诊断二进制；诊断日志只服务机制分析。

### 7. 被否决的复杂化

- 没有为 direct ordinary seed 再保存第二张 mask-vertex 表：ordinary row 已是唯一真值，复制只增加内存和同步风险。
- 没有让 refilter 返回精细 changed-mask 集：q10 只在一次 refilter 后多做约 0.4 秒全量 support 重建，不值得扩张 destructive 接口。
- 没有按 dirty 比率动态调整 buy：这会改变购买位置并引入经验参数，破坏干净对照。
- 没有按图名、 $g$、时间或内存选择路径，也没有降低浮点精度。
- 没有修改 ordinary 深度或 adjoint 递推；它们是 q10 剩余时间的主要来源，需要独立证明和独立门禁。

<a id="history-p1-g5-g7-history-regression-probe-20260818"></a>

## P1 `g=5/7` 历史弱项非退化抽样（2026-08-18）

> 原始记录：`P1_G5_G7_HISTORY_REGRESSION_PROBE_20260818.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 对照与协议

- 本次核验的求解器源码版本：`12d6adb9bd4e5e90731627a3a8b85696fd5c4acb`，正式二进制 SHA-256 为 `793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced`。
- 历史版本：最新完整且位于已知 Adjoint 正确性事故之前的 P1 ABHSS 全量结果 `p1_dual_full_ours_672bd253cdde_20260802`，提交 `672bd253cddefaddf9da7545314f1d34638cc54f`；本次直接复用与该全量元数据哈希完全一致的二进制 `bbab71a3e000c736ba087adaf739e6fcb3816758680df1c389859a876c8fce98`。
- PrunedDP++ 历史参考：`results/paper_runs/p1_full`，与上述 ABHSS 全量使用同一 P1 矩阵。
- 两个版本同时运行，第一轮为当前 CPU 4、历史 CPU 5，第二轮交换 CPU；严格保持两个单线程 solver，不启动其他实验。
- 每个版本均运行相同的 Base 和 Enhanced。下表时间为交换核两轮的算术平均，空间为两轮峰值的较大者。
- 单条 timeout 为 1,000 秒；所有选中历史 ABHSS 查询均远低于该值。

### 选样

选样不是随机抽容易项，而是从历史全量中优先覆盖三类风险：Base/Enhanced 都输给 PrunedDP++、Enhanced 明显慢于 Base、绝对耗时较高。

- `g=5`：DBLP q109、LiveJournal q296、Orkut q115/q116、Reddit q140、YouTube q266。
- `g=7`：DBLP q92、LiveJournal q24、Orkut q165/q175、Reddit q255、YouTube q65/q94。

### 正确性与完整性 gate

- 四个运行目录各有 26 条记录，共 104 条，全部为 `ok`。
- 四轮任务键完全相同。
- 当前版与历史版的最优权值逐查询、逐配置完全相同。
- 当前版、历史版与 PrunedDP++ 历史结果的最优权值逐项完全相同。

### 聚合结果

| `g` | 配置 | 查询数 | 当前秒 | 历史秒 | 当前/历史 | 当前峰值 MiB | 历史峰值 MiB |
|---:|---|---:|---:|---:|---:|---:|---:|
| 5 | Base | 6 | 99.598 | 110.544 | 0.9010 | 298.7 | 301.8 |
| 5 | Enhanced | 6 | 194.336 | 197.731 | 0.9828 | 2,201.3 | 2,201.2 |
| 7 | Base | 7 | 250.726 | 281.244 | 0.8915 | 497.1 | 525.9 |
| 7 | Enhanced | 7 | 375.488 | 373.701 | 1.0048 | 2,660.5 | 2,293.9 |

Base 的 13 条查询全部更快；Enhanced `g=5` 的 6 条也全部更快。Enhanced `g=7` 中 6/7 条持平或更快，但 Orkut q175 在两颗核上都稳定变慢，导致该组 7 条合计慢 0.48%。

### 逐查询结果

| `g` | 图 | q | Base 当前/历史秒 | Base 比值 | Enhanced 当前/历史秒 | Enhanced 比值 | Enhanced 当前/历史 MiB | PrunedDP++ 历史秒 |
|---:|---|---:|---:|---:|---:|---:|---:|---:|
| 5 | DBLP | 109 | 11.011 / 11.884 | 0.927 | 11.381 / 11.519 | 0.988 | 484.4 / 484.5 | 7.148 |
| 5 | LiveJournal | 296 | 0.653 / 0.677 | 0.964 | 32.562 / 33.004 | 0.987 | 966.7 / 966.6 | 22.145 |
| 5 | Orkut | 115 | 22.170 / 23.477 | 0.944 | 79.095 / 80.453 | 0.983 | 2,201.3 / 2,201.2 | 41.449 |
| 5 | Orkut | 116 | 49.828 / 57.042 | 0.874 | 55.539 / 56.787 | 0.978 | 2,200.0 / 2,199.1 | 34.229 |
| 5 | Reddit | 140 | 10.947 / 12.249 | 0.894 | 10.494 / 10.650 | 0.985 | 688.2 / 688.4 | 8.094 |
| 5 | YouTube | 266 | 4.988 / 5.215 | 0.957 | 5.265 / 5.318 | 0.990 | 178.6 / 178.6 | 2.993 |
| 7 | DBLP | 92 | 14.640 / 20.619 | 0.710 | 17.889 / 20.232 | 0.884 | 561.2 / 561.2 | 14.916 |
| 7 | LiveJournal | 24 | 3.878 / 4.900 | 0.791 | 50.338 / 51.309 | 0.981 | 1,093.6 / 1,093.7 | 43.446 |
| 7 | Orkut | 165 | 34.702 / 38.456 | 0.902 | 77.718 / 79.128 | 0.982 | 2,252.3 / 2,252.3 | 52.994 |
| 7 | Orkut | 175 | 167.036 / 183.848 | 0.909 | 196.356 / 189.730 | 1.035 | 2,660.5 / 2,293.9 | 884.553 |
| 7 | Reddit | 255 | 15.738 / 17.594 | 0.894 | 17.264 / 17.253 | 1.001 | 818.5 / 818.5 | 11.516 |
| 7 | YouTube | 65 | 8.024 / 8.708 | 0.921 | 7.712 / 7.781 | 0.991 | 213.1 / 213.1 | 4.207 |
| 7 | YouTube | 94 | 6.708 / 7.117 | 0.943 | 8.211 / 8.269 | 0.993 | 213.9 / 214.0 | 4.237 |

Orkut `g=7` q175 的 Enhanced 原始时间为：当前 CPU 4/5 分别 195.998/196.714 秒，历史 CPU 5/4 分别 189.832/189.628 秒。方向在交换核后不变，因此不能归为单核噪声。

### 结论边界

1. 当前 Base 在这批历史弱项上没有时间或空间退化，且有约 9%--11% 的合计时间改善。
2. 当前 Enhanced `g=5` 没有退化；其历史上已有的 LiveJournal/Orkut 弱项仍存在，但没有因当前版本进一步恶化。
3. 当前 Enhanced `g=7` 没有普遍退化，但不能宣称逐查询完全不退化：Orkut q175 慢 3.5%，且峰值空间增加 366.6 MiB（约 16.0%）。由于它是高耗时查询，7 条样本合计出现 0.48% 的轻微时间回退。
4. 这不是最后一条增量 support-DP 提交单独引入的回退。仓库既有 `6593b0d` 到 `12d6adb` 直接门禁在同一 q175 上为 197.195/197.393 秒，峰值空间反而少 0.387 MiB；本次差异来自 `672bd25` 到当前版本之间更大的 Enhanced、正确性与完成阶段重构范围。
5. 该抽样足以否定“当前版在 P1 `g=5/7` 有普遍性能退化”，但不足以替代当前版本的 8,318 条全量 P1。后续完整 P1 已完成并以 13/13 图通过逐图底线，正式结论见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

<a id="history-human-experiment-plan-legacy-gates-20260820"></a>

## 旧人类实验文档中的优化门禁与 Orkut 风险轨迹（迁移于 2026-08-20）

> 以下内容从旧版 `docs/EXPERIMENT_PLAN.md` 原样迁入。它记录旧二进制、方向探针和结构重构门，不定义当前正式矩阵；当前实验身份与正式结果只看主文档。

### 4.4 Orkut `g=15` 扩样与 10,000 秒可运行性门

追加 q6--q10 后，提交 `589894be3774ff6658e1120f9a9e899a53377ab1` 的正式 Enhanced 首轮只完成 q6--q9，q10 在 10,000 秒内没有结果行。该失败只作为优化动机；查询没有被删除，TL 没有延长，也没有按 `mean_f`、图名或状态量增加开关。

完成输入无关的证书因子化、打包 staged cache、tour 支配跳过、最远组 oracle 等价快路和单调拒绝前沿后，旧二进制 SHA-256 `a957bdcecafc486575cb7d78779dcbeda687eb0ea7283a0fc1104089dce83b86` 曾让十条都产生结果行。然而后续 P1 权值审计发现该二进制从最高逻辑层直接启动 H，漏掉被省略 $D(h)$ 的图闭包；LiveJournal 与 Orkut 共九条作者查询高报。所以下表只保存旧运行的时间和资源规模，用于评估当前修复版风险，不是正式精确结果或当前门禁。

| 查询 | tranche | size stratum | 实际 `mean_f` | 旧秒数 | 旧查询峰值 MiB | 旧 `(mask,v)` states | 旧返回值 |
|---|---:|---:|---:|---:|---:|---:|---:|
| q1 | 1 | 3 | 430.067 | 2,270.862 | 3,270.934 | 105,902,192 | 21 |
| q2 | 1 | 5 | 980.733 | 2,523.204 | 3,609.332 | 261,581,592 | 32 |
| q3 | 1 | 4 | 587.000 | 281.276 | 2,690.473 | 31,051,638 | 38 |
| q4 | 1 | 1 | 249.733 | 315.548 | 2,995.387 | 15,061,435 | 28 |
| q5 | 1 | 2 | 343.067 | 4,568.638 | 7,130.719 | 560,869,444 | 32 |
| q6 | 2 | 2 | 338.867 | 3,847.348 | 6,019.355 | 452,486,374 | 36 |
| q7 | 2 | 4 | 662.667 | 3,451.310 | 4,815.082 | 364,962,795 | 38 |
| q8 | 2 | 3 | 464.400 | 204.234 | 2,690.305 | 366,330 | 18 |
| q9 | 2 | 5 | 685.067 | 1,465.794 | 3,538.766 | 135,705,582 | 35 |
| q10 | 2 | 1 | 309.800 | 9,934.603 | 19,776.727 | 1,654,690,262 | 54 |

旧十条总查询时间为 28,862.817 秒、总状态数为 3,582,677,644；q10 为 9,934.603 秒，只剩约 65.4 秒余量。后续审计先补入辅助 $H(h)$，又由 Musae q162/q295 发现“只播种辅助层、successor 只读 branch”仍不完备。修复版已改为必要单/双块终端、全值 successor 与互补 H 平衡完成，并在 Musae $g=7$ 全 300 条上与 Base 逐条一致。

第一版完备性修复后的诊断构建曾在独占 CPU 5、30,000 秒外层预算下自然完成 Orkut q10：精确权值为 54，solver 时间 11,178.235 秒，超过正式 10,000 秒 TL；查询峰值为 21,337.590 MiB，watchdog 总 RSS 峰值为 27,729.191 MiB，累计首次发现状态为 1,761,794,764。其阶段时间为预处理 189.940 秒、共同 A1 85.956 秒、ordinary 6,524.740 秒、低层 A 2,040.340 秒、adjoint 2,337.210 秒。该构建还包含后来回退的证书链候选，只保留为历史风险对照。

最小共同 A1 checkpoint `e9eee92` 令逻辑层 $2,\ldots,q$ 全由已经证明等价的 H realization 承担。其诊断二进制 SHA-256 `067620fbac9b16a661b74d2c1071a1ea15648f01034a893f5de7e06b09ab11d2` 在同一 Orkut q10 上得到精确权值 54，solver 时间为 9,431.057 秒，低于正式 TL 568.943 秒；查询峰值 18,305.023 MiB，watchdog 总 RSS 峰值 24,696.602 MiB，累计状态 1,459,398,194。相对上述历史完备版分别减少 15.63% 时间、14.21% 查询峰值和 17.16% 状态。该 checkpoint 的 ordinary 为 6,710.250 秒，A1 移交后的共同前向结算只有 8.766 秒；代价是最终上界从旧路径的 H6 延后到 H2 才由 56 收紧为 54。该对照的旧二进制含后来回退的代码，不能把每个 wall-time 差值都归因于层边界；交换绑核的 Musae $g=14$ 对照则逐条保持答案并把状态减少 12.82%，两轮时间分别改善 11.33% 和 13.71%。后续最终工作树又加入 adjoint 空域扫描减除与 certificate-support 增量求值；最终生产二进制不能继承该 checkpoint 的时间，但已经用自身二进制完成 q10 单条硬门，见第 7.6 节。详细演进见 [最小共同 A1 与 q10 门禁](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-minimal-forward-a1-adjoint-gate-20260818)。

`mean_f` 仍不是难度的单调解释变量：q10 属于最低 size stratum，却远慢于组更大的 q8；因此不能按平均组大小删除 q10 或设置经验超参数。正式复跑对每条查询使用固定 10,000 秒，并以冻结机器、当前 commit、当前二进制哈希和新结果行为准。为了保留完整轨迹，冻结求解器源码提交 `12d6adb` 的诊断构建曾对 q10 使用大于正式 TL 的 86,400 秒外层预算，但该查询在 9,250.911 秒自然完成，结果行本身满足 `solver_seconds < 10000`；精确权值 54，状态数 1,459,398,194。外层诊断预算不得改写成正式 TL。此后同一源码的无诊断正式二进制完成了 q1--q10 固定十条，其中 q10 为 9,544.561 秒、权值 54、状态数 1,459,398,194，十条全部低于 10,000 秒。13 图 P1 全量也已按冻结矩阵完成并通过逐图底线，最终表与身份见第 7.10 节。事故演进与正确性修复见 [辅助半层 Adjoint 审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-auxiliary-half-adjoint-correctness-audit-20260815)和 [Adjoint split 完备性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-adjoint-split-completeness-audit-20260817)。

### 7.2 历史严格公共 A1 接入门

本段冻结的是“所有配置使用严格共同 A1”的上一阶段证据，已被第 7.3 节的共同 witness 调度门取代，但不能删除，因为它记录了 dual-A1 等被否决方案。旧版 SHA-256 为 `a8a7c0c60ed2db729c149723332334309cd296931ea8f364f3f2f35fd42fb611`，计时候选为 `c15b98614e88bb15cde1c8d192b5bd8410a61b6b98c53baa76c727dab6854c63`；仅补充注释后的验证二进制为 `af18f0d74b5ba2f00b4640c6b4920a585110317a6487b232e104665a8bb79c82`。时间只含 solver query timer，排除图/查询加载；短中面板使用交错重复的总时中位数，大图方向以固定查询补充 RSS 和状态数。A1-active 重复面板的预登记门为 `new/old <= 1.03`。

| 面板 | 旧版秒 | 新版秒 | new/old | 状态/RSS 要点 | 结论 |
|---|---:|---:|---:|---|---|
| Musae `g=10`, q1--q5 | 5.665100 | 5.077327 | 0.896247 | 新版状态分别为 192539、258437、588247、5481、930440 | 通过，约快 10.4% |
| Musae `g=14`, q1 | 9.676265 | 9.651949 | 0.997487 | 2066026 -> 2188274 states | 通过，基本持平 |
| YouTube `g=10`, q1 | 12.554718 | 12.758588 | 1.016239 | 12065 -> 417065 states；扩张公开保留 | 通过，低于 1.03 |
| Orkut `g=6`, q1 | 87.609767 | 86.055657 | 0.982261 | 11415 -> 109705 states；2210.891 -> 2211.719 MiB | 大图方向通过 |
| Musae `g=16`, q1 | 219.745747 | 168.639290 | 0.767429 | 31.098M -> 19.790M states；455.449 -> 306.414 MiB | 高组数压力方向通过 |
| Reddit `g=10`, q1--q2 | 28.242423 | 29.377420 | 1.040188 | 两版均为 0 states，在 A1/D 前闭合 | 负对照；公开但不归因于 A1 |

该历史候选通过 5/5 CTest、144 个随机精确实例，状态累计为 Base/DirectedCutOnly/Enhanced `2772/913/913`；SteinLib Base 与 Enhanced 均 11/11 命中已知最优值，总 solver 时间分别为 13.115069 s 和 3.627557 s。YouTube 说明公共 A1 可能增加发现状态但总时仍在门内，Musae `g=16` 说明它也可能通过更早收紧后续搜索显著减少状态；因此论文不能把“状态必然减少”写成普遍性质。Reddit 的两条查询根本未执行 A1，其波动只能作为预处理/机器噪声风险，既不能拿来证明 A1 退化，也不能从原始记录中删除。

### 7.3 共同 witness rent-or-buy 门

当前修改消除了上一阶段残留的不对称：旧二进制中 Base 在预处理末无条件运行一次 root-path witness DP，Enhanced 则不无条件运行 dual-primal witness DP。当前方案要求两边预处理只构造各自 witness，随后统一从 `rent=0` 开始；公共 A1 与 ordinary $D$ 连续累计 queue-pop/edge-relax 工作，达到同一公式才调用同一个 `EvaluateWitnessTree`：

```math
B_{\mathrm{wit}}(T,k)
=
\lvert V(T)\rvert\left(
\frac{3^k-1}{2}+3^k
\right).
```

冻结旧二进制 SHA-256 为 `af18f0d74b5ba2f00b4640c6b4920a585110317a6487b232e104665a8bb79c82`。大图、SteinLib 与第一轮重复计时使用的候选为 `6c94736cdbadf1860798e178d733bbaa90b4c7d871d4be969f298665091686a7`；随后只修改了中文注释和文档，没有改变可执行语句。为避免依赖这一判断，亚秒面板交替 7 次、中等面板交替 3 次，已直接在最终二进制 `85acbdca1618ea59b2c7bfa30400eeb318298c1651e744b5f4c28576452222bf` 上重新计时。表中比较固定配置的 q1--q5 solver-only 总时中位数；预登记门仍为 `new/old <= 1.03`。

| 重复面板 | 配置 | 旧版中位秒 | 新版中位秒 | new/old | 状态结论 |
|---|---|---:|---:|---:|---|
| Musae `g=6`, q1--q5 | Base | 0.307200 | 0.307538 | 1.001100 | 五条均逐项相同 |
| Musae `g=6`, q1--q5 | Enhanced | 0.578834 | 0.582641 | 1.006577 | 五条均逐项相同 |
| Musae `g=10`, q1--q5 | Base | 12.820206 | 12.605179 | 0.983227 | 仅 q3 为 1,781,121 -> 1,781,521；后续 D 购买修订点改变 |
| Musae `g=10`, q1--q5 | Enhanced | 2.124140 | 2.131375 | 1.003406 | 五条均逐项相同 |

大图与高组数只作为固定查询方向检查，不冒充重复中位数：

| 方向面板 | 配置 | 旧版秒 | 新版秒 | new/old | 状态 old -> new | 权值 |
|---|---|---:|---:|---:|---:|---:|
| YouTube `g=10`, q1 | Base | 19.364107 | 19.609395 | 1.012667 | 942,973 -> 942,973 | 48 |
| YouTube `g=10`, q1 | Enhanced | 13.922364 | 13.894155 | 0.997974 | 341,307 -> 341,307 | 48 |
| Orkut `g=6`, q1 | Base | 19.157284 | 18.379818 | 0.959417 | 174,343 -> 174,343 | 11 |
| Orkut `g=6`, q1 | Enhanced | 73.805967 | 70.973760 | 0.961626 | 66,987 -> 66,987 | 11 |
| Musae `g=14`, q1 | Base | 94.121125 | 91.655003 | 0.973798 | 6,581,139 -> 6,605,541 | 414 |
| Musae `g=14`, q1 | Enhanced | 35.728035 | 35.289030 | 0.987713 | 6,223,600 -> 6,177,805 | 414 |

正确性门为 5/5 CTest；144 个独立全子集 DP 实例全部匹配，最终状态累计 Base/DirectedCutOnly/Enhanced 为 `2815/913/913`；SteinLib 11 个 $11\le g\le16$ 实例两种配置均 11/11 命中已知最优值，总 solver 时间为 9.823664 s 与 3.723920 s。实现过程中有两个必须保留的反例：只在 D 累计 rent 会让 Orkut Base 从 174,343 扩到 617,937 个状态；在 A1 行之间购买但保留旧行会混合两套 cutoff，并使 Musae `g=10` q1 从正确的 291 错为 426。最终方案只在购买真正收紧上界时丢弃部分 A1 pass、用新固定 cutoff 整轮重启；被丢弃的工作支付 rent，但不计为已发布逻辑状态。

逐次观测、RSS、输入哈希、被否决变体和二进制身份见 [`experiments/abhss_configuration_refactor_gate.json`](../../experiments/abhss_configuration_refactor_gate.json) 的 `common_witness_rent_or_buy_gate_20260725`。该门只证明结构修改不退化，不替代 P1/P2/S2 正式实验。

### 7.4 距离—根初始化重构门

这一历史门使 `PrepareProblem` 不再显式执行 Base-only 的 `BuildCanonicalSptUpper`。对进入非平凡距离预处理的查询，所有配置只调用一次 `BuildDistanceRootInitialization`，并接收同一个 `DistanceRootInitialization{group_distance, root, upper}`。BootstrappedBounded 把规范 SPT 边并集封装为自身的 cutoff bootstrap；若规范终端在非连通图中没有共同分量，它允许临时无穷 cutoff，并由不截断的多源距离与共同 root-star 在已知公共分量中取得有限上界。CompletePotential 把全图距离扩展封装为自身的表示成本。两者返回前都执行同一个 root-star 扫描；在该门对应的版本中，返回后共同进入 root-path-union。后续第 7.5 节增加了全部配置共享的 $g\le3$ 精确闭包：当前代码在进入配置替换职责前让三个配置统一运行 BootstrappedBounded root-star 基例，只有 $g>3$ 才按 profile 选择距离 realization 并进入共同 root-path-union。这两层组织都符合“共同操作或同职责替换”，又避免强迫 Enhanced 支付不被低组基例消费的完整势。

性能旧版为上一节最终二进制 `85acbdca1618ea59b2c7bfa30400eeb318298c1651e744b5f4c28576452222bf`，计时候选为 `4b7a8ad917aefdd7218ccb0af01409208670ba0b23e32ac37a7e03f9d179570e`。加入合同回归测试、中文注释并最终重建后的发布二进制为 `d93d44ec97ea19c49b9fb2c96b833bded549559a2c8ea1eab3351c6b5a50c319`；它与计时候选的可执行 `.text` 节逐字节相同，SHA-256 均为 `20473d68886e07607c5dd1efa3c2bc2e385832df75f1f9877948bd78508f0f33`，因此计时对应当前 solver 指令。短面板按旧/新顺序交替，比较五条固定查询的 solver-only 总时中位数；所有权值与 `(mask,v)` 状态向量逐项相同：

| 重复面板 | 配置 | 对数 | 旧版中位秒 | 新版中位秒 | new/old |
|---|---|---:|---:|---:|---:|
| Musae `g=6`, q1--q5 | Base | 7 | 0.348237 | 0.332611 | 0.955128 |
| Musae `g=6`, q1--q5 | Enhanced | 7 | 0.582339 | 0.578981 | 0.994234 |
| Musae `g=10`, q1--q5 | Base | 3 | 13.094906 | 13.206400 | 1.008514 |
| Musae `g=10`, q1--q5 | Enhanced | 3 | 2.287082 | 2.199319 | 0.961627 |

大图固定查询用于检查初始化固定成本，没有冒充重复中位数：

| 方向面板 | 配置 | 旧版秒 | 新版秒 | new/old | 状态 old/new | 权值 |
|---|---|---:|---:|---:|---:|---:|
| YouTube `g=10`, q1 | Base | 22.161634 | 19.850138 | 0.895698 | 942,973 / 942,973 | 48 |
| YouTube `g=10`, q1 | Enhanced | 14.065857 | 13.913244 | 0.989150 | 341,307 / 341,307 | 48 |
| Orkut `g=6`, q1 | Base | 18.020921 | 18.027482 | 1.000364 | 174,343 / 174,343 | 11 |
| Orkut `g=6`, q1 | Enhanced | 71.457513 | 71.452179 | 0.999925 | 66,987 / 66,987 | 11 |

SteinLib 的 11 个 `g=11..16` 实例中，Base 与 Enhanced 仍均为 11/11 命中已知最优值，状态向量逐实例相同。Base 单轮总时为 9.364713 -> 9.131532 秒。Enhanced 首轮方向值 1.040754 超过 1.03，因总时仅约 3.6 秒而触发五对交替复测；旧/新中位数为 3.560374 / 3.643549 秒，最终比值 1.023361，低于门限。该门的最大中位数比值因此为 1.023361，结论为通过；它证明当前代码组织没有可检测的性能退化，不声称重命名本身产生加速。

完整逐次时间、RSS、状态、输入身份和二进制身份冻结在 [`experiments/abhss_configuration_refactor_gate.json`](../../experiments/abhss_configuration_refactor_gate.json) 的 `distance_root_initialization_gate_20260725`。本地 `.tmp_canonical_spt_refactor_20260725` 仅为 Git 忽略的原始运行目录。

### 7.5 三项 P1 优化的本地方向探针

2026-07-29 在远端最新 `main` 提交 `762c9431069a5bbd7b523e4919f32d4a103ff415` 上冻结旧二进制，并在同一台 Windows/MinGW 机器上探测三项改动：全部配置共享的 $g\le3$ root-star 精确闭包、正权 cover 快路径与安全工作区复用、DirectedCut 的截断 potential cone。该探针沿用本仓库配置重构门的 `candidate/main <= 1.03` 本地噪声阈值。它不是正式 Linux 服务器 gate：本机绝对时间已经与旧服务器记录明显不同，表中比值只能用来拒绝明显退化、决定是否值得进入全量复跑，不能进入论文主表。

| 固定面板 | 配置 | 查询数 | main 秒 | 候选秒 | candidate/main | 权值与状态 |
|---|---|---:|---:|---:|---:|---|
| YouTube author `g=3`, q1--q5 | Base | 5 | 3.686119 | 3.240606 | 0.879138 | 逐条相同，状态均为 0 |
| YouTube author `g=3`, q1--q5 | Enhanced | 5 | 13.220844 | 3.199866 | 0.242032 | 逐条相同，状态均为 0 |
| YouTube author `g=5`, q1--q5 | Base | 5 | 23.914524 | 23.224964 | 0.971166 | 逐条相同 |
| YouTube author `g=5`, q1--q5 | Enhanced | 5 | 25.268008 | 23.722211 | 0.938824 | 逐条相同 |
| YouTube author `g=7`, q1--q3 | Base | 3 | 24.437060 | 23.565183 | 0.964322 | 逐条相同 |
| YouTube author `g=7`, q1--q3 | Enhanced | 3 | 29.503628 | 28.355726 | 0.961093 | 逐条相同 |
| Musae author `g=5`, q1--q100 | Base | 100 | 6.062077 | 6.026033 | 0.994054 | 逐条相同 |
| Musae author `g=5`, q1--q100 | Enhanced | 100 | 7.072846 | 6.532474 | 0.923599 | 逐条相同 |
| Orkut generated `g=6`, q1 | Base | 1 | 32.166481 | 32.841080 | 1.020972 | 相同，111,044 states；同时段各两次中位数 |
| Orkut generated `g=6`, q1 | Enhanced | 1 | 85.414253 | 84.168028 | 0.985410 | 相同，109,705 states |

探针中曾出现一个必须公开保留的被否决中间态：cone 代码继续留在类内可内联后，Orkut Base 三次交替的中位数从 30.660223 秒增到 32.161013 秒，比值 1.048951，尽管权值和状态不变。原因不是 Base 执行了 cone，而是 Release IPO 把增大的 Enhanced 冷分支并入共同预处理，污染了 Base 指令布局。最终代码给一次性 dual 构造增加跨编译器非内联冷边界；随后 Orkut Base/Enhanced 都回到表中的非退化方向。这个边界不增加或替换任何论文逻辑操作，只限制机器码布局。

表中所有候选值均来自 SHA-256 为 `93a5582baecfc38c7683fd6b44c34f0d4a31b1ab05c1f309cc9506933b8cbd9e` 的同一计时二进制。最终审阅没有再改变可执行源码；以完全相同的源文件重链接并重跑 5/5 CTest 后，Windows/MinGW LTO 产物 SHA-256 为 `e2db08db82f7d7f47ff9db14898dd242dcc39ffd59d537615a684490c2897160`，因此记录同时保存计时产物和最终验证产物身份，而不把重链接哈希冒充原计时文件。除 Orkut Base 外均为单次本地方向值；Orkut Base 首次结果贴近门限，因此按候选/main/候选/main 在同一时段各运行两次，表中为两边两样本中位数，两个配对比值分别为 1.028865 和 1.013128。其 1.020972 只表示没有超过既定 1.03 本地噪声门，不能宣称加速。`g=3` 的 Base 与 Enhanced 逐项执行同一个 bounded root-star 包，二者总时间只相差约 1.3%，且每条查询均为 0 个主状态；Enhanced 相对 main 的大幅下降来自不再为已经由共同数学基例闭合的查询构造完整势、dual、witness 与 facility，而不是逐查询关闭增强。单次差异只能视为本机噪声，不能宣称配置间在该基例上存在速度差异。

最终候选通过 Release 全构建、5/5 CTest、显式 $g=2,3$ 共同闭包回归、逐弧 residual 复算和 15/15 GitHub Markdown 校验。完整机器信息、二进制/源文件 SHA-256、RSS 和被否决中间态记录在 [`p1_local_optimization_probe_20260729.json`](../../experiments/p1_local_optimization_probe_20260729.json)。该段当时要求的 Linux 13 图、8,318 条 P1 全量已由第 7.10 节冻结版完成；不能继续把本地探针写成当前状态。

### 7.6 Certificate-support 增量求值门

Orkut P2 `g=15` 新增面板 q10 是当前已知最重的自然完成查询之一。旧版提交 `6593b0d` 的诊断二进制在固定 CPU 5 上用 9395.877740 秒完成；保持调度、状态搜索和浮点语义不变，仅把同一 support 上反复全量重建的 subset DP 改为脏超集增量求值后，诊断候选用 9250.911428 秒完成，节省 144.966312 秒，且权值 54 与 1,459,398,194 个 `(mask,v)` 状态完全一致。查询内存峰值从 18,305.211 MiB 增到 18,318.242 MiB，即增加 13.031 MiB；这是持久保存 $O(2^k s)$ DP 表的预期代价。该查询没有超过 10,000 秒，余量为 749.089 秒。两次长跑都开启了稀疏诊断，故这里只作为同诊断口径的机制门禁，不替代无诊断正式 P2 计时。

494 次 support 购买中有 2 次因首次购买或 refilter 全量重建，492 次走增量路径。旧版等价地处理 8,093,202 个 mask，新版实际重算 1,374,856 个，即 16.988%；剔除相同的约 144.8 秒 ordinary refilter 后，support evaluator 本体约由 255.806 秒降到 60.769 秒。整条查询只获得 1.57% 加速，是因为 ordinary 搜索仍占约 6529.6 秒、adjoint 仍占约 2442.6 秒；不能把局部 evaluator 的 4.21 倍写成端到端加速。

P1 非退化门覆盖 Musae `g=7` 全 300 条和 Orkut `g=7` 固定 q175。Enhanced 的 Musae 总时为 22.195592 -> 22.021389 秒；Orkut 两轮合计为 392.950553 -> 391.618166 秒。最终生产二进制再次运行 Orkut q175 得到 197.194998 -> 197.392606 秒，即 0.10% 的中性波动；权值、状态数逐项相同，查询峰值还少 0.387 MiB。Base 在编译期不含 support-mask 通知；Musae 与 Orkut 的正负约 1% 顺序波动没有稳定方向，答案、状态和内存不变。该门因此只证明新增缓存对 P1 无可识别退化、对重 support 购买查询确有端到端收益，不替代正式 P1/P2 汇总。

诊断构建在长查询运行途中逐次输出 `support_dp_full` 或 `support_dp_incremental`，并记录 `evaluation`、`published_masks`、`activated_masks`、`recomputed_masks`；`ordinary_row` 与 `ordinary_layer` 还记录从 ordinary 开始的累计秒数，`witness_buy`、`witness_refilter` 和 `adjoint_layer` 事件同时给出当前 `best`、行数、标量数、工作量与局部阶段秒数。下一次自然完成长跑必须保留这些日志，不能只留下最终 `weights.txt`。完整二进制身份、阶段分解、正确性论证和被否决复杂化见 [`archive/ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-incremental-certificate-support-dp-gate-20260818`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-incremental-certificate-support-dp-gate-20260818)。

### 7.7 当前版相对最近有效历史 P1 的 `g=5/7` 风险抽样

第 7.6 节只比较 `6593b0d` 与 `12d6adb`，不能推广为当前版相对较早完整 P1 的逐查询无退化。为覆盖更大的重构跨度，当前正式二进制与最新一轮位于已知 Adjoint 正确性事故之前、且完整跑完 P1 的 `672bd25` 二进制做了两轮交换绑核抽样；13 条查询优先选取双方曾输给 PrunedDP++、Enhanced 明显慢于 Base 或绝对耗时较高的历史弱项。104 条运行记录全部完成，当前/历史 Base/Enhanced 与 PrunedDP++ 的目标值逐项一致。

| `g` | 配置 | 查询数 | 当前秒 | 历史秒 | 当前/历史 | 当前峰值 MiB | 历史峰值 MiB |
|---:|---|---:|---:|---:|---:|---:|---:|
| 5 | Base | 6 | 99.598 | 110.544 | 0.9010 | 298.7 | 301.8 |
| 5 | Enhanced | 6 | 194.336 | 197.731 | 0.9828 | 2,201.3 | 2,201.2 |
| 7 | Base | 7 | 250.726 | 281.244 | 0.8915 | 497.1 | 525.9 |
| 7 | Enhanced | 7 | 375.488 | 373.701 | 1.0048 | 2,660.5 | 2,293.9 |

Base 13 条全部更快，Enhanced `g=5` 六条全部更快，Enhanced `g=7` 有 6/7 条持平或更快。唯一稳定反向项是 Orkut `g=7` q175：Enhanced 由 189.730 秒增至 196.356 秒，增加 3.5%；峰值由 2,293.9 MiB 增至 2,660.5 MiB。交换 CPU 后方向不变，因此不能归为单核噪声；但 `6593b0d -> 12d6adb` 的直接门在同一查询上只有 0.10% 时间波动且空间略降，说明该差异来自 `672bd25` 以来更大的正确性与阶段重构范围，不是增量 support-DP 单独造成。这个定向风险样本否定了“当前版在 P1 `g=5/7` 普遍退化”，不能证明逐查询无退化，也不能替代 8,318 条正式 P1。完整协议与逐查询表见 [P1 `g=5/7` 历史弱项抽样](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-p1-g5-g7-history-regression-probe-20260818)。


<a id="history-imdb-g14-slow-query-opt24-gate-20260824"></a>

## IMDb `g=14` 慢询问 rooted component-cover 门（2026-08-24）

### 1. 目标、输入与证据边界

本轮只针对已经自然完成、且历史 Enhanced solver 时间超过 10,000 秒的 IMDb 受控 `g=14` 查询。输入固定为 `data/official-latest-20260722/imdb-20260722` 与 `experiment_data/s1_controlled_gf/imdb_latest_20260722/g14_f*.txt`，不修改图、查询、组数或平均组大小。性能运行固定到独立物理核，外层 `timeout 10060` 只为图加载和结果落盘留余量；计入“压入门内”必须有完整结果行且其中 solver time 小于 10,000 秒。诊断前缀、被外层终止的空结果文件和根据状态增长作出的预测均不计作完成。

历史面板共 11 条，原始时间为 11,388--77,844 秒，状态量为 15.01 亿--143.90 亿，查询峰值为 43.94--187.79 GiB。`mean_f` 与难度并不单调：最慢项来自 `f=800`，两个 `f=200` 项也分别需要 28,804 和 46,487 秒。因此本轮没有按 `f` 或历史耗时设置算法分支。

### 2. 保留的结构优化

> **现行作用域勘误（2026-08-26）。** 本节保存的是 V21 当时“Base 与 Enhanced 共用 generic rooted-cover”的历史实现和门禁。当前生产版只在配置开启 DirectedCut 且全边严格等于 1 时物化 rooted-entry；Base 与一般权图既不构造每顶点根分量掩码，也不消费这项 future。下列历史时间与正确性证据仍用于解释方案演进，但“共同 future”不得复制到当前方法章节。

先收缩零权连通分量。令 `c(M)` 为覆盖剩余组集合 `M` 至少需要的终端分量数，令 `Gamma(v)` 为当前根的零权分量免费命中的组，令 `w+` 为全图最小正边权。当前共同 future 使用：

```math
L_{\mathrm{cc}}^r(v,M)
=
\max\left\{
\max\{0,c(M)-1\}w_+,
c(M\setminus\Gamma(v))w_+
\right\}.
```

第一项连接所选终端分量，第二项把当前根分量作为额外连通责任。对第二项，`M` 中未被 `Gamma(v)` 免费命中的每个被选分量都不同于根分量，把这些分量接到根分量至少各跨一条正权边；故两项都不超过真实补全费用。它不使用图名、 `g/f` 分段、状态数或墙钟。

实现先为全部组子集计算 cover；只有查询确实进入指数搜索时才按顶点物化 16-bit 根分量组掩码。该下界进入 Base、DirectedCutOnly 与 Enhanced 共用的 ordinary、forward 和 adjoint future，不进入 A1 cone：A1 的 row 外缺项仍由其自身 1-Lipschitz continuation 解释。单位权图中的真实补全费用为整数，代码只对可采纳下界取精确 `ceil`；状态距离仍保存和比较原始 `double`，没有精度压缩。非单位小数权的 cover 乘法使用向下修正，防止浮点乘法向上舍入破坏可采纳性。

#### V14 底座中的单位权精确 key 队列

> **现行勘误（2026-08-25）。** 下段 1-Lipschitz 论证只描述 V14 当时的 future 集。后续保留的 first-hit/nonterminal rooted-entry 组合界一般不一致，沿一条边的 key 可以下降超过 1。现行 `UnitKeyQueue` 的正确性依赖每次插入执行 `current=min(current,key)` 并回退到更小桶，不依赖一致性；一般复杂度必须显式计入回退后的重复桶头扫描，不能继续声称每张 row 必为线性桶界。历史计时仍有效，但该旧证明不得复制进当前方法章节。

> **现行配置勘误（2026-08-26）。** V14 的 Base/Enhanced 共同整数桶只属于该历史底座。当前生产版仅为 strict-unit DirectedCut ordinary 选择 `UnitKeyQueue`；Base 即使在单位权图上也使用二叉堆。下段隔离时间保留为历史实现证据，不能描述当前配置边界。

V14 已包含一个与 rooted cover 正交的共同物理 realization：加载器逐边确认全图边权恰为 1 后，ordinary 状态、真实可行上界和费用格闭合后的 future 都是精确整数；rooted-cover、farthest、tour、公共 A1 与 directed-cut 各项又都在单位边上满足 1-Lipschitz，取最大和 `ceil` 后仍保持。因此沿边 A* key 不下降，`UnitKeyQueue` 可用 $0,1,\ldots,U-1$ 的一桶一 key 单调实现抽象最小队列；状态值仍是原 `double`，插入时保留防御性指针回退。它没有浮点量化、桶宽、图名或 $g/f$ 分支，Base 与 Enhanced 共用同一规则；非单位权图继续使用二叉堆。此前否决的 65 桶 double radix 是任意浮点 key 的另一种容器，不能与这一精确整数费用格实现混写。

V9 对 V8 的隔离完整运行保持逐查询权值和状态数不变：IMDb `g=14,f=400,q1` 从 3424.328014 秒降至 3330.504066 秒（约 2.74%），Orkut 作者 `g=7,q175` 从 199.421834 秒降至 198.812577 秒（约 0.31%），峰值空间基本不变。该证据只支持单位权容器的常数收益；最终候选仍须由当前 5/5 CTest 与 P1 门共同验收，不能用这两条计时替代理论正确性。

版本演进中，V16 只加入全部子集的未定根 cover，V19 将上述 rooted 形式接入 ordinary/forward，V21 再接入 adjoint 并完成生命周期减空与安全乘法。V19 对 `f400,q4/q5` 的直接 V14 对照说明主要收益不是后续无关优化：q4 从 9,280.211 秒降到 6,893.245 秒，q5 从 4,685.688 秒降到 3,647.704 秒。

### 3. 11 条历史慢询问的硬门结果

| `f` | 查询 | 历史秒 | 当前已验证秒 | 权值 | 当前状态 | 判定 |
|---:|---:|---:|---:|---:|---:|---|
| 400 | q5 | 11,388.481 | 3,647.704 | 17 | 755,449,904 | V19 完成，3.12 倍 |
| 400 | q4 | 17,836.391 | 6,893.245 | 17 | 1,926,557,530 | V19 完成，2.59 倍 |
| 800 | q1 | 18,761.846 | 2,866.864 | 15 | 885,901,805 | V19 完成，6.54 倍 |
| 800 | q3 | 20,545.682 | 3,729.975 | 15 | 1,147,493,956 | V19 完成，5.51 倍 |
| 400 | q2 | 22,178.548 | 9,308.023 | 18 | 2,233,769,210 | V19 完成，2.38 倍 |
| 200 | q3 | 28,803.599 | >10,000 | 20 | -- | V19、V22 均由外层门终止，不计完成 |
| 800 | q2 | 31,581.095 | 6,741.756 | 15 | 2,198,178,033 | V21 完成，4.68 倍 |
| 1,600 | q2 | 34,312.333 | >10,000 | 14 | -- | V21 外层门终止，不计完成 |
| 800 | q4 | 40,705.411 | 未做最终门 | 16 | -- | 保留历史难例 |
| 200 | q1 | 46,487.487 | 未做最终门 | 20 | -- | 保留历史难例 |
| 800 | q5 | 77,843.622 | >10,000 | 16 | -- | V22 外层门终止，不计完成 |

因此 V21 已严格完成 6/11 条，即在没有把预测或前缀冒充结果的口径下达到多数。该结论是本轮“多数慢询问压到 10,000 秒内”的完成度，不等价于全部慢询问已经解决。

后续 V22 曾尝试在四元路径不改善时，以“公共 root-path-union 与 closure primal 的原图边并”替换旧 witness，并仅在其符号 support-DP 工作量不超过 witness-tree DP 时启用。IMDb `f=800,q5` 与 `f=200,q3` 分别绑定 CPU5/CPU4 运行，均在外层 10,060 秒以退出码 124 结束，没有写出完整结果行；终止前 RSS 约为 50.4 GiB 和 34.7 GiB。该候选未增加任何完成项，已从源码和正式方法文档删除，详见否决档案。V21 的 6/11 不受这次失败探针影响。

### 4. 两类困难不能混写

`f800,q5` 的历史轨迹先以 18 开始，closure facility 只得到 17，最优值 16 到 ordinary 后层才出现；它属于“优质可行上界出现过晚”。相反，`f1600,q2` 在预处理结束已经得到最终最优值 14，随后 ordinary 仍用约 23,396 秒、adjoint 又用约 10,196 秒；它属于“已经找到最优解，但安全下界仍不足以证明没有更优解”。前者应改进真实上界消费者，后者需要更强的可采纳状态下界或支配规则。仅减少 witness 求值常数不可能同时解决两者。

状态和内存是第二层放大器。历史最慢项实际发现 143.90 亿个 `(mask,v)`，峰值达到 187.79 GiB；哈希、分配和内存带宽会进一步放大状态爆炸。但常数工程优化不能替代对证明状态数的削减，也不能据此引入按数据选择的超参数。

### 5. P1 与正确性门

V22 回退后，用最终重建的 V21 二进制重新执行 Musae 作者 `g=5/7` 的 Base/Enhanced 全 300 条面板。与既有 V21 共 1,200 次逐查询权值、状态完全一致；相对 V14 的四格总时比分别为 0.9990、1.0110、1.0291 和 1.0228，全部不超过预登记的 1.03 非退化门。对应总时为 11.340421、12.664423、60.281578 和 22.693092 秒。

最终大图门仍采用历史弱项 Orkut 作者 `g=7,q175`：重建 V21 为 203.588613 秒、权值 33、2,666.289 MiB、11,598,909 状态；相对 V14 时间比为 1.0240，相对先前 V21 为 1.0104，状态不增加，仍在 1.03 门内。

最终 V21 Release 构建通过 5/5 CTest；其中配置回归包含独立全子集 DP、零权图、配置新增/替换合同、support-DP 全量/增量逐项一致和单位权整数闭包。性能门不能替代理论证明：rooted component-cover 的安全性责任是上述正边计数，真实上界的安全性仍由每个有限候选可展开到原图边保证。

本节冻结 V21 的 6/11 与最终 P1/CTest 证据。后续若接受新的 closure-support 消费者，必须追加其完整慢例、P1、5/5 CTest 和理论审计结果；不得直接覆盖本表或把仍在运行的结果写成已完成。

<a id="history-imdb-g14-unit-structural-v36-gate-20260825"></a>

## IMDb `g=14` 单位权结构证书最终门（V36，2026-08-25）

> **现行配置勘误（2026-08-26）。** 本节的 11/11 数字来自 V36，保留其完整历史身份。当前生产版把 rooted-entry、精确整数队列和反序 packing 统一归入 strict-unit DirectedCut 的安全新增；Base 不再构造或消费前两项。V36 的完成时间仍证明这些结构在 Enhanced 困难查询上的价值，但不能证明当前 Base 采用同一 realization。

### 1. 冻结身份与选择边界

本节追加 V21 之后的最终候选证据，不覆盖上一节的版本演进。11 条输入仍是 V23 之前按“历史 Enhanced `solver_seconds > 10,000`”一次冻结的完整集合：`f200` q1/q3，`f400` q2/q4/q5，`f800` q1--q5，`f1600` q2。候选实现、消融计划和本表结果均不得据当前快慢重新删项。

11 条性能结果全部来自同一个 Release 二进制，完整 SHA-256 为 `137a8d1d74da491e84d0bfa3e554861834bc2efb010f0818e1c1a8de000a03ca`。性能二进制与最终独立重建二进制逐字节相同；两者 `.text` SHA-256 均为 `d0705c27d4c77e30efdc87d8b8945601fc232786927b15b681a4798e11f8bcce`。每条任务固定到 CPU 4 或 CPU 5，最多同时运行两个单线程进程；外层 `timeout 10060` 只容纳图加载与结果落盘，表中验收仍严格要求结果行的查询时间小于 10,000 秒。

### 2. 最终保留机制及其论文边界

最终候选保留四类与主线一致的操作。

1. 对所有边权逐条精确等于 1 的图，共同 rooted-entry future 把必须选择的候选终端数与三种进入责任中的最大值相加：首次命中前的内部点、到剩余组路径上不可避免的非候选顶点数，以及候选诱导分量的非候选邻接 block-cover。候选终端与三种进入责任分别计互斥顶点类别；三种进入责任之间可能复用，所以只能取最大、不能相加。
2. 单位权补全费用属于整数格，因此仅对已经证明可采纳的实数下界取精确上整。ordinary 的抽象最小队列按精确整数 key 分桶，但 rooted-entry 组合界不要求一致；每次插入更小 key 都显式回退桶头。真实状态值仍是原 `double`，没有量化或降低精度。
3. 主 residual closure 购买后，继续发生的 ordinary 结构工作可以按静态成本购买一份独立的反组序初始 packing。两份 residual 各自容量可行，future 只取两者最大，不相加，也不逐组混合。只有逐项复核为有限、非负、可由 16-bit 无损表示的整数势才使用紧凑布局；任一值不满足即整表回退 `double`。
4. Adjoint 的物理转置用稀疏事件桶替换旧 64 顶点块扫描；每张有序 ordinary row 只挂下一个真实 payload，消费后再挂后继。顶点内仍按 `(reduced,mask)` 排序，候选集合和并列顺序不变。tour 的紧凑端点布局及逐完整端点项提前拒绝同样只消除已经由可采纳部分值拒绝的剩余求值；未拒绝时返回与完整扫描相同的值。

这些操作不读取图名、查询编号、目标 `f`、观测状态数或墙钟，也不以经验 `g` 阈值切换。唯一数据特化是加载器验证得到的“全图严格单位权”数学不变量。V35 曾尝试延迟购买非候选 0-1 深度，但它在代表性难例上先放入更多状态、后续重滤无法追回已做工作，已经回退并记录在[否决档案](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-unit-nonterminal-depth-scheduler-negative-20260825)。

### 3. 11/11 同二进制硬门结果

| `f` | 查询 | 原始历史秒 | V36 秒 | 加速比 | 权值 | 峰值 MiB | V36 `(mask,v)` states |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 200 | q1 | 46,487.487 | 7,985.788 | 5.82 | 20 | 30,622.855 | 2,125,653,711 |
| 200 | q3 | 28,803.599 | 1,575.061 | 18.29 | 20 | 11,312.207 | 382,628,052 |
| 400 | q2 | 22,178.548 | 847.874 | 26.16 | 18 | 10,108.164 | 188,230,094 |
| 400 | q4 | 17,836.391 | 734.149 | 24.30 | 17 | 10,278.309 | 191,301,633 |
| 400 | q5 | 11,388.481 | 429.254 | 26.53 | 17 | 7,086.602 | 118,213,342 |
| 800 | q1 | 18,761.846 | 451.830 | 41.52 | 15 | 7,703.934 | 155,360,143 |
| 800 | q2 | 31,581.095 | 895.435 | 35.27 | 15 | 10,875.477 | 256,772,118 |
| 800 | q3 | 20,545.682 | 514.595 | 39.93 | 15 | 8,113.449 | 202,115,939 |
| 800 | q4 | 40,705.411 | 857.868 | 47.45 | 16 | 11,048.391 | 226,298,237 |
| 800 | q5 | 77,843.622 | 1,465.845 | 53.10 | 16 | 11,708.184 | 439,078,459 |
| 1,600 | q2 | 34,312.333 | 790.055 | 43.43 | 14 | 11,654.766 | 264,224,189 |

11/11 均自然完成且结果行严格小于 10,000 秒；最慢项为 `f200,q1` 的 7,985.788 秒。总查询时间为 16,547.754 秒，累计实际状态为 4,549,875,917，单查询峰值为 30,622.855 MiB。11 条返回权值与历史精确答案逐项一致；最小加速比仍为 5.82，最大为 53.10。

### 4. 正确性与证据边界

最终独立 Release 构建通过 5/5 CTest。配置精确性门包含 5,000 个随机小图、500 个正权互异单终端实例、160 个 `g=7..16` Adjoint 转置压力实例、256 个单位权三配置端到端实例，以及 128 个单位权实例上主 packing、反序 packing 和 rooted-entry 对独立精确 rooted subset DP 的逐状态比较。测试只能防止实现偏离，理论安全性仍分别依赖候选/非候选顶点互斥计数、整数费用格闭合、两份 residual 独立可行和事件桶逐 payload 双射。

本节的 11 条是历史冻结困难面板，也是消融 C 的生产列；它不是完整 S2，更不是当前二进制的完整 P1/P2 正式重跑。旧提交 `12d6adb`、二进制 `793d4e...` 的完整 P1 与 Orkut q1--q10 仍是单独的历史冻结论文证据。V36 必须以固定 P1 哨兵和 Orkut `g=15,q10` 完成开发非退化门；若论文最终冻结 V36，所有正式主表必须明确记录当前二进制身份，不能把旧全量记录伪装成 V36 结果。


<a id="history-weighted-tour-adjoint-v61-gate-20260827"></a>

## 一般加权 tour 一次缓存与 Adjoint 布局恢复门（V61，2026-08-27）

### 1. 退化根因与证据边界

本轮目标不是发明新的下界，而是消除一次代码重构造成的物理退化。GPU4GST Orkut 接口并非单位权图；原始 `graph.txt` 含 89、57 等整数权重，加载器对该图给出 `all_edges_unit_weight=false`。因此 strict-unit rooted-entry、整数桶和整数闭包均不适用于该图。此前把 Orkut 当成单位权而尝试基于 rooted-entry 拒绝数启停结构，诊断计数恒为 0 的原因是“不适用”，不是现有下界支配；该临时候选已完整回退。

重构后的 `TourLowerBound::AtUntilRejected` 允许一般加权标签在当前部分 tour 已足以拒绝时返回 `exact=false`。ordinary 和 adjoint 的阶段状态因而保持未完成；同一 `(mask,v)` 后续出现更小标签时，会从这个未完成阶段再次扫描 tour 端点。该行为在 strict-unit 中能配合整数补全闭包节省工作，但一般加权图没有这项职责，反复重入会把同一候选无关证书支付多次。另一个差异是 Adjoint 一度把历史 64 顶点块统一替换为事件桶；小 P1 查询中影响只有数秒，高组 q10 的 adjoint 历史诊断却占 2,442.58 秒，布局常数会被放大。

V59 只恢复 weighted ordinary 的完整 tour 一次缓存。它在 Orkut `g=7,q175` 得到 199.916952 秒、权值 33、11,598,909 states，但 `g=15,q10` 仍在外层 10,070 秒退出 124，峰值 25,290,600 KiB 且没有结果行。这证明 ordinary 修复正确但不足，不能把 q175 的短门外推成 q10 完成。

### 2. V61 保留实现

V61 在查询入口只依据加载器逐边验证的精确边权不变量实例化 adjoint 模板，不读取图名、查询编号、`g`、组大小、row 数、状态量或墙钟。

1. 一般加权 ordinary 第一次到达 tour 阶段即调用完整 `TourLowerBound::At`，把精确 tour 最大值写入 row-epoch cache 并结束阶段。strict-unit 继续逐完整端点项早拒绝；若 tour 尚未完成，后续更小标签从缓存前缀恢复。
2. 一般加权 transposed terminal 和递减 H prefix 同样首次完整计算 tour；递减 H 对固定 `(mask,v)` 只保存一份完整 farthest/dual/packing/tour 最大值。strict-unit 保留 rooted-entry、整数闭包和可恢复的部分 tour。
3. `BuildTransposedTerminals` 只保留一个 `ProcessVertex` 数学处理：subset 势、排序、单/双块枚举、prefix 和 terminal 输出均共用。strict-unit 用稀疏事件桶聚集 payload；一般加权图用 64 顶点块聚集同一 payload。两条路径都按递增顶点调用公共处理，并在顶点内按 `(reduced,mask)` 排序。
4. weighted 模板在编译期不生成 `RootedEntryCoverLower` 与整数闭包调用；这些函数在一般加权图上本来分别返回 0 和恒等值。删除恒空调用不删除数学证书。

这里没有浮点压缩或新阈值。完整 tour 是原可采纳 tour 证书的精确值，必不弱于提前结束时已计算的部分最大值；把它更早缓存只能减少重复求值，不能放过原本应拒绝的标签。事件桶与块聚集交给 `ProcessVertex` 的 payload 多重集相同，排序后浮点加法和并列次序相同。物理布局可能改变不同顶点收紧 incumbent 的时刻，从而使“实际发现 states”略有差异，但不会改变递推候选或最终精确权值。

### 3. 硬门结果

V61 Release 二进制 SHA-256 为 `f3b99791518882572577a2c9e35c10bec259dfc1de2db401b52229a502d00e46`。所有任务单线程固定 CPU 5；外层 timeout 只覆盖加载与落盘，q10 验收仍看结果行内 `query_seconds < 10000`。

| 门禁 | 秒 | 权值 | 查询峰值 MiB | `(mask,v)` states | 判定 |
|---|---:|---:|---:|---:|---|
| Orkut `g=15,q10` Enhanced | 9,560.341419 | 54 | 18,309.305 | 1,459,398,194 | 通过 10,000 秒硬门 |
| Orkut `g=7,q175` Enhanced | 198.644568 | 33 | 2,660.766 | 11,598,909 | 回到历史快区间 |
| IMDb `g=14,f=400,q2` Enhanced | 845.422852 | 18 | 10,108.762 | 188,230,094 | strict-unit 路径不退化 |

q10 外层墙钟为 2:39:53，`/usr/bin/time` 最大 RSS 为 25,293,744 KiB；结果权值与状态数和冻结生产版逐项一致。IMDb 参考为 845.268881 秒、同权值和同 states，V61 的 0.02% 差异属于同机波动。V61 还通过仓库 5/5 CTest：5,000 个随机精确对照、500 个正权压力实例、160 个 `g=7..16` omitted-half 转置实例、零权 witness、I/O 和状态计数全部通过。

P1 开发哨兵按 V42 的 15 次冻结调用重放，共 114 条。14 个跨图/历史难例总时间从 831.208884 秒变为 830.016360 秒，比例 0.998565；Musae `g=5` q1--q100 从 4.272498 秒变为 4.289315 秒，比例 1.003936。114 条权值全部一致，113 条 states 一致。唯一例外是 Reddit `g=7,q220` Enhanced：states 从 94 变为 101，时间从 21.394980 秒变为 22.237817 秒；更早的冻结快版 `793d4e...` 在同一任务上也是 101 states。该 7-state 差异来自物理遍历下 incumbent 更新时间，不写成状态严格支配；总体 P1 时间门仍通过。

### 4. 论文边界与后续冻结

V61 的 q10、IMDb 和 P1 哨兵是当前候选的开发非退化证据，不替代旧提交 `12d6adb` 的完整 P1/P2 正式表，也不表示 V61 已重跑全部矩阵。若以 V61 冻结论文 artifact，应在同一二进制身份下运行剩余正式 campaign；在此之前只能声称“关键 P1/P2/strict-unit 门通过”。失败的 V59/V60 目录不得混入正式汇总，V60 的 q10 在发现残留 weighted adjoint 问题后于 ordinary 前段主动终止，status 143，不是 timeout 或完成结果。
