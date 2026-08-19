# 已否决与暂缓方向

本卷保存负结果、被现有结构支配的方案、尚未达到采用门槛的候选和仅用于静态调研的设想。除非重新给出完整正确性证明并通过当前门禁，这些内容均不得描述为现行算法，也不得悄然恢复。保留它们是为了避免后续重复试错。

## 卷内目录

- [P1 图的结构核与多尺度分量下界统计](#history-communication-cross-graph-structural-bound-survey) — 原文件 `communication/CROSS_GRAPH_STRUCTURAL_BOUND_SURVEY.md`
- [YouTube 弱图五小时公平优化探针](#history-communication-five-hour-fair-youtube-optimization-probe) — 原文件 `communication/FIVE_HOUR_FAIR_YOUTUBE_OPTIMIZATION_PROBE.md`
- [Youtube 类稀疏图的结构下界静态分析](#history-communication-youtube-structural-lower-bound-analysis) — 原文件 `communication/YOUTUBE_STRUCTURAL_LOWER_BOUND_ANALYSIS.md`
- [Directed-cut 替代顺序负面探针](#history-dual-order-negative-probe-20260807) — 原文件 `DUAL_ORDER_NEGATIVE_PROBE_20260807.md`
- [H 在线 star 上界负面探针](#history-hstar-negative-probe-20260807) — 原文件 `HSTAR_NEGATIVE_PROBE_20260807.md`
- [Orkut 高组数状态爆炸：未采用探针（2026-08-07）](#history-state-explosion-negative-probes-20260807) — 原文件 `STATE_EXPLOSION_NEGATIVE_PROBES_20260807.md`
- [Remaining-aware A2 排名方案负结果（2026-08-08）](#history-remaining-a2-ranking-negative-probe-20260808) — 原文件 `REMAINING_A2_RANKING_NEGATIVE_PROBE_20260808.md`
- [Adjoint 生命周期压缩负结果（2026-08-09）](#history-adjoint-lifecycle-negative-probe-20260809) — 原文件 `ADJOINT_LIFECYCLE_NEGATIVE_PROBE_20260809.md`
- [强制桥并集下界负面探针（2026-08-09）](#history-bridge-union-bound-negative-probe-20260809) — 原文件 `BRIDGE_UNION_BOUND_NEGATIVE_PROBE_20260809.md`
- [互补掩码相邻调度负面探针（2026-08-09）](#history-complement-pair-scheduling-negative-probe-20260809) — 原文件 `COMPLEMENT_PAIR_SCHEDULING_NEGATIVE_PROBE_20260809.md`
- [一致代表组树上界负结果（2026-08-09）](#history-consistent-representative-upper-negative-probe-20260809) — 原文件 `CONSISTENT_REPRESENTATIVE_UPPER_NEGATIVE_PROBE_20260809.md`
- [高组数 component-cover 与组重叠负结果（2026-08-09）](#history-high-g-component-and-overlap-negative-probe-20260809) — 原文件 `HIGH_G_COMPONENT_AND_OVERLAP_NEGATIVE_PROBE_20260809.md`
- [查询度量外围锚负面探针（2026-08-09）](#history-metric-peripheral-anchor-negative-probe-20260809) — 原文件 `METRIC_PERIPHERAL_ANCHOR_NEGATIVE_PROBE_20260809.md`
- [Ordinary row 重滤负结果（2026-08-09）](#history-ordinary-refilter-negative-probe-20260809) — 原文件 `ORDINARY_REFILTER_NEGATIVE_PROBE_20260809.md`
- [非必要叶剥离负面探针（2026-08-09）](#history-redundant-leaf-pruning-negative-probe-20260809) — 原文件 `REDUNDANT_LEAF_PRUNING_NEGATIVE_PROBE_20260809.md`
- [Rooted endpoint-floor 负结果（2026-08-09）](#history-rooted-endpoint-floor-negative-probe-20260809) — 原文件 `ROOTED_ENDPOINT_FLOOR_NEGATIVE_PROBE_20260809.md`
- [Ordinary row 物理布局负结果（2026-08-09）](#history-row-layout-negative-probe-20260809) — 原文件 `ROW_LAYOUT_NEGATIVE_PROBE_20260809.md`
- [上包络筛选 A1 tour 负结果（2026-08-09）](#history-screened-a1-tour-negative-probe-20260809) — 原文件 `SCREENED_A1_TOUR_NEGATIVE_PROBE_20260809.md`
- [三块完成感知调度负面探针（2026-08-09）](#history-triple-completion-scheduling-negative-probe-20260809) — 原文件 `TRIPLE_COMPLETION_SCHEDULING_NEGATIVE_PROBE_20260809.md`
- [A1 seed-support 永久锚负面探针（2026-08-10）](#history-a1-seed-support-anchor-negative-probe-20260810) — 原文件 `A1_SEED_SUPPORT_ANCHOR_NEGATIVE_PROBE_20260810.md`
- [因果低层 block future 负面探针（2026-08-10）](#history-causal-block-future-negative-probe-20260810) — 原文件 `CAUSAL_BLOCK_FUTURE_NEGATIVE_PROBE_20260810.md`
- [子集零权分量覆盖 future 的支配性负结果（2026-08-10）](#history-component-cover-future-dominated-probe-20260810) — 原文件 `COMPONENT_COVER_FUTURE_DOMINATED_PROBE_20260810.md`
- [代价引导规范拆分 pivot 的中性探针（2026-08-10）](#history-cost-guided-canonical-pivot-neutral-probe-20260810) — 原文件 `COST_GUIDED_CANONICAL_PIVOT_NEUTRAL_PROBE_20260810.md`
- [Ordinary D2 支撑选锚负面探针（2026-08-10）](#history-d2-support-anchor-negative-probe-20260810) — 原文件 `D2_SUPPORT_ANCHOR_NEGATIVE_PROBE_20260810.md`
- [Double radix queue 负面探针（2026-08-10）](#history-double-radix-queue-negative-probe-20260810) — 原文件 `DOUBLE_RADIX_QUEUE_NEGATIVE_PROBE_20260810.md`
- [Dual primal 支撑并集负面探针（2026-08-10）](#history-dual-primal-support-union-negative-probe-20260810) — 原文件 `DUAL_PRIMAL_SUPPORT_UNION_NEGATIVE_PROBE_20260810.md`
- [完整 dual 根可行性早筛负面探针（2026-08-10）](#history-full-potential-root-screen-negative-probe-20260810) — 原文件 `FULL_POTENTIAL_ROOT_SCREEN_NEGATIVE_PROBE_20260810.md`
- [单组删枝重插上界负结果（2026-08-10）](#history-group-reinsert-upper-negative-probe-20260810) — 原文件 `GROUP_REINSERT_UPPER_NEGATIVE_PROBE_20260810.md`
- [Residual closure 增量购买价混合探针（2026-08-10）](#history-incremental-closure-buy-mixed-probe-20260810) — 原文件 `INCREMENTAL_CLOSURE_BUY_MIXED_PROBE_20260810.md`
- [`U/2` ordinary 边生长负面探针（2026-08-10）](#history-incumbent-half-edge-growth-negative-probe-20260810) — 原文件 `INCUMBENT_HALF_EDGE_GROWTH_NEGATIVE_PROBE_20260810.md`
- [Orkut `g=15,q5` 精确 oracle 上界负面探针（2026-08-10）](#history-oracle-optimal-upper-p2-negative-probe-20260810) — 原文件 `ORACLE_OPTIMAL_UPPER_P2_NEGATIVE_PROBE_20260810.md`
- [ordinary row 有序位图物化负面探针（2026-08-10）](#history-ordered-row-bitmap-negative-probe-20260810) — 原文件 `ORDERED_ROW_BITMAP_NEGATIVE_PROBE_20260810.md`
- [Ordinary pair-partition 上界负面探针（2026-08-10）](#history-ordinary-pair-partition-upper-negative-probe-20260810) — 原文件 `ORDINARY_PAIR_PARTITION_UPPER_NEGATIVE_PROBE_20260810.md`
- [原图设施度量上界负面探针（2026-08-10）](#history-original-graph-facility-upper-negative-probe-20260810) — 原文件 `ORIGINAL_GRAPH_FACILITY_UPPER_NEGATIVE_PROBE_20260810.md`
- [路径生长证书复用为 witness 的负结果（2026-08-10）](#history-path-growth-witness-reuse-negative-probe-20260810) — 原文件 `PATH_GROWTH_WITNESS_REUSE_NEGATIVE_PROBE_20260810.md`
- [Residual 永久锚优先顺序负面探针（2026-08-10）](#history-residual-anchor-first-order-negative-probe-20260810) — 原文件 `RESIDUAL_ANCHOR_FIRST_ORDER_NEGATIVE_PROBE_20260810.md`
- [Residual closure 同序完成探针（2026-08-10）](#history-residual-closure-same-order-p2-probe-20260810) — 原文件 `RESIDUAL_CLOSURE_SAME_ORDER_P2_PROBE_20260810.md`
- [Residual 双顺序证书在补集布局下的复核负面探针（2026-08-10）](#history-residual-dual-order-ensemble-complement-retest-negative-probe-20260810) — 原文件 `RESIDUAL_DUAL_ORDER_ENSEMBLE_COMPLEMENT_RETEST_NEGATIVE_PROBE_20260810.md`
- [Residual 双顺序证书负面探针（2026-08-10）](#history-residual-dual-order-ensemble-negative-probe-20260810) — 原文件 `RESIDUAL_DUAL_ORDER_ENSEMBLE_NEGATIVE_PROBE_20260810.md`
- [Residual least-paid-first 顺序负面探针（2026-08-10）](#history-residual-least-paid-order-negative-probe-20260810) — 原文件 `RESIDUAL_LEAST_PAID_ORDER_NEGATIVE_PROBE_20260810.md`
- [Residual single-demand radius 负面探针（2026-08-10）](#history-residual-single-demand-radius-negative-probe-20260810) — 原文件 `RESIDUAL_SINGLE_DEMAND_RADIUS_NEGATIVE_PROBE_20260810.md`
- [Ordinary 可复用二叉堆负面探针（2026-08-10）](#history-reusable-ordinary-heap-negative-probe-20260810) — 原文件 `REUSABLE_ORDINARY_HEAP_NEGATIVE_PROBE_20260810.md`
- [Rooted component-cover future 负结果（2026-08-10）](#history-rooted-component-cover-future-negative-probe-20260810) — 原文件 `ROOTED_COMPONENT_COVER_FUTURE_NEGATIVE_PROBE_20260810.md`
- [Tour endpoint 预筛选负面探针（2026-08-10）](#history-tour-endpoint-screen-negative-probe-20260810) — 原文件 `TOUR_ENDPOINT_SCREEN_NEGATIVE_PROBE_20260810.md`
- [三元路径真实树支撑复用负面探针（2026-08-10）](#history-triple-path-support-reuse-negative-probe-20260810) — 原文件 `TRIPLE_PATH_SUPPORT_REUSE_NEGATIVE_PROBE_20260810.md`
- [锚终端 rooted 全局下界负向探针（2026-08-11）](#history-anchor-rooted-global-lower-negative-probe-20260811) — 原文件 `ANCHOR_ROOTED_GLOBAL_LOWER_NEGATIVE_PROBE_20260811.md`
- [Branch bitmap 置位枚举：负向探针](#history-branch-word-enumeration-negative-probe-20260811) — 原文件 `BRANCH_WORD_ENUMERATION_NEGATIVE_PROBE_20260811.md`
- [Directed-cut reduced partition screen：负向探针](#history-dual-reduced-partition-screen-negative-probe-20260811) — 原文件 `DUAL_REDUCED_PARTITION_SCREEN_NEGATIVE_PROBE_20260811.md`
- [最高 ordinary 层直接融合到 adjoint 的静态否定（2026-08-11）](#history-fused-top-ordinary-static-negative-20260811) — 原文件 `FUSED_TOP_ORDINARY_STATIC_NEGATIVE_20260811.md`
- [独立 witness ensemble 负向探针（2026-08-11）](#history-independent-witness-ensemble-negative-probe-20260811) — 原文件 `INDEPENDENT_WITNESS_ENSEMBLE_NEGATIVE_PROBE_20260811.md`
- [closure 后完整最远组顺序负向探针（2026-08-11）](#history-purchased-farthest-order-negative-probe-20260811) — 原文件 `PURCHASED_FARTHEST_ORDER_NEGATIVE_PROBE_20260811.md`
- [Purchased vertex-major group-distance layout：负向探针](#history-purchased-vertex-group-distance-negative-probe-20260811) — 原文件 `PURCHASED_VERTEX_GROUP_DISTANCE_NEGATIVE_PROBE_20260811.md`
- [固定组端点树遍历下界：支配性排除记录](#history-endpoint-pair-tree-bound-dominated-note-20260812) — 原文件 `ENDPOINT_PAIR_TREE_BOUND_DOMINATED_NOTE_20260812.md`
- [省略两层 ordinary 与无条件三块终端：被否决的 Orkut 探针（2026-08-12）](#history-omitted-two-ordinary-layers-negative-probe-20260812) — 原文件 `OMITTED_TWO_ORDINARY_LAYERS_NEGATIVE_PROBE_20260812.md`
- [一次性互补 directed-cut 证书：负结果记录](#history-ephemeral-complementary-dual-negative-probe-20260813) — 原文件 `EPHEMERAL_COMPLEMENTARY_DUAL_NEGATIVE_PROBE_20260813.md`
- [动态最大 residual 边际顺序负结果](#history-lazy-maximum-residual-marginal-negative-probe-20260814) — 原文件 `LAZY_MAXIMUM_RESIDUAL_MARGINAL_NEGATIVE_PROBE_20260814.md`
- [[负结果] 提前 Quad 与辅助 H pair 分桶](#history-early-quad-and-half-pair-negative-probe-20260815) — 原文件 `EARLY_QUAD_AND_HALF_PAIR_NEGATIVE_PROBE_20260815.md`
- [[负结果] ordinary 半层直接复用为最高 H](#history-shared-ordinary-half-negative-probe-20260815) — 原文件 `SHARED_ORDINARY_HALF_NEGATIVE_PROBE_20260815.md`
- [[负结果] cached dual 区间复用与 witness 输入相关调度](#history-cached-dual-and-witness-relevance-negative-probe-20260816) — 原文件 `CACHED_DUAL_AND_WITNESS_RELEVANCE_NEGATIVE_PROBE_20260816.md`
- [公共 A1 必然存在与 ordinary size-1 扫描负向探针](#history-mandatory-a1-and-size1-scan-negative-probe-20260816) — 原文件 `MANDATORY_A1_AND_SIZE1_SCAN_NEGATIVE_PROBE_20260816.md`
- [Queue pop 与 forward 空行发布负向探针](#history-queue-pop-and-empty-ready-negative-probe-20260816) — 原文件 `QUEUE_POP_AND_EMPTY_READY_NEGATIVE_PROBE_20260816.md`
- [Staged certificate cache 物理减写与冻结分派负向探针](#history-staged-cache-physical-reduction-negative-probe-20260816) — 原文件 `STAGED_CACHE_PHYSICAL_REDUCTION_NEGATIVE_PROBE_20260816.md`
- [Directed-cut 延迟精确购买负向探针（2026-08-17）](#history-lazy-exact-dual-purchase-negative-probe-20260817) — 原文件 `LAZY_EXACT_DUAL_PURCHASE_NEGATIVE_PROBE_20260817.md`

<a id="history-communication-cross-graph-structural-bound-survey"></a>

## P1 图的结构核与多尺度分量下界统计

> 原始记录：`communication/CROSS_GRAPH_STRUCTURAL_BOUND_SURVEY.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

本文是候选优化的只读调研，不表示已经修改正式算法。所有统计器均位于 `/tmp`，以 `nice -n 19` 和 `ionice -c 3` 单核运行；没有写入数据或实验结果。

### 1. 查询相关结构核的图级上限

查询相关结构核保留 2-core，以及所有候选终端从 fringe 通向 core 的必要路径。表中的“可剥比例”是全图 2-core 外顶点比例，只是潜在上限；若图有多个连通分量，还必须结合查询所在分量解释。

| P1 图 | 点数 | 边数 | 2-core 比例 | 可剥比例 | 桥比例 | 初步判断 |
|---|---:|---:|---:|---:|---:|---|
| Toronto | 46,073 | 68,353 | 87.65% | 12.35% | 8.46% | 中低 |
| MovieLens | 62,423 | 35,323,774 | 43.43% | 56.57% | 约 0% | 全图比例被大量其他分量放大，主稠密分量内很低 |
| DBLP / GPU-DBLP | 2,497,782 | 12,786,329 | 86.54% | 13.46% | 1.80% | 中等 |
| LinkedMDB | 1,326,784 | 2,132,796 | 49.45% | 50.55% | 28.70% | 高 |
| DBpedia | 5,887,296 | 18,338,729 | 67.49% | 32.51% | 10.17% | 高 |
| Musae | 19,109 | 400,497 | 95.48% | 4.52% | 0.22% | 很低，适合非退化门禁 |
| Twitch | 34,118 | 429,113 | 91.77% | 8.23% | 0.65% | 低 |
| Github | 37,700 | 289,003 | 86.11% | 13.89% | 1.81% | 中低 |
| Youtube | 1,134,890 | 2,987,624 | 41.43% | 58.57% | 22.33% | 很高 |
| LiveJournal | 3,997,962 | 34,681,189 | 79.52% | 20.48% | 2.37% | 中高 |
| Reddit | 4,262,834 | 12,502,767 | 49.23% | 50.77% | 17.25% | 很高 |
| Orkut | 3,072,441 | 117,185,083 | 未扫描 | 未扫描 | 未扫描 | 平均度约 76，推测很低；待正式 Orkut 任务结束后核验 |

Youtube 作者查询有 90.98%--93.61% 的终端位于 2-core，所以其约六成 fringe 中绝大多数对单条查询无关。Toronto 每组平均跨越很多桥分量，纯桥强制条件为零；Github 的纯桥条件在 $g=7$ 上命中约 25.7%，但尚未证明超过现有 tour。总体上，结构核对 Youtube、LinkedMDB、Reddit、DBpedia 最值得尝试，对 Musae/Twitch 应接近无操作。

### 2. GPU 图的单尺度 component-cover

对阈值 $t$ 收缩所有权重严格小于 $t$ 的边。令 $c_t$ 为覆盖全部查询组至少需要的连通分量数，则 $t(c_t-1)$ 是简单全局下界。下表给出扫描阈值集合中的最高平均值；它尚未扣除现有 `tour`，因此是强度上限而不是预期新增剪枝。

| 图 | 最小正边权 | $g=3$ | $g=5$ | $g=7$ | 相对现有 min-edge 判断 |
|---|---:|---:|---:|---:|---|
| Musae | 1 | 102.8 | 176.8 | 263.5 | 很大新增潜力 |
| Twitch | 30 | 127.3 | 236.8 | 368.3 | 有新增潜力，但现有下界本来已强 |
| Github | 19 | 134.5 | 266.3 | 393.5 | 较大新增潜力 |
| Youtube | 1 | 9.1 | 9.8 | 18.8 | 中等，重点看宽状态中的新增淘汰 |
| GPU-DBLP | 1 | 94.3 | 169.5 | 255.2 | 很大新增潜力 |
| LiveJournal | 1 | 1.9 | 3.7 | 6.2 | 很弱 |
| Reddit | 100 | 约 174 | 约 305 | 约 474 | 基本已被当前 min-edge component-cover 得到 |

Reddit 所有边权均为 100，所以较大的绝对值不是新能力。Twitch/Github 边权下界本身较高，也必须和当前 `ComponentCover` 比较。真正最值得做动态探针的是 GPU-DBLP、Musae、Github、Youtube；LiveJournal 可作为“弱但不退化”对照。

### 3. 多尺度版本的正确使用

若选择少量递增阈值 $t_1<\cdots<t_L$，可用区间右端的分量需求构造安全下矩形积分：

```math
L_{\mathrm{scale}}(v,M)=\sum_{j=1}^{L-1}(t_{j+1}-t_j)c_{t_{j+1}}(v,M).
```

每条权重为 $w$ 的树边只对 $t\leq w$ 的层级贡献，积分总量不超过其真实权重，因此不同尺度可以按上述方式累计，不能直接把若干 $t(c_t-1)$ 相加。

建议先做单尺度运行时探针：在现有 farthest/tour/dual 之后计算候选值，只记录它是否成为新的最大下界、是否独立淘汰候选，不实际改变搜索。若 GPU-DBLP、Github、Youtube 中有明显新增命中，再实现 3--4 层版本。

### 4. 整体效果预期

两项优化覆盖互补图族：

- 结构核依赖拓扑 fringe，最可能帮助 Youtube、LinkedMDB、Reddit、DBpedia；
- 多尺度 component-cover 依赖低权子图的连通层次，最可能帮助 GPU-DBLP、Musae、Github、Youtube；
- Youtube 同时命中两类，因而仍是首要开发图；
- Musae/Twitch 的结构核几乎无作用，适合验证公共实现不会因固定预处理退化；
- LiveJournal 的结构核可能有中等收益，但多尺度下界很弱；
- Reddit 的结构核可能很强，但多尺度绝对值主要重复现有 min-edge 下界；
- Orkut 预期两者都弱，应在当前正式任务结束后只做一次低优先级核验。

### 5. 推荐实验顺序

1. 查询相关结构核：Youtube、LinkedMDB、Reddit、DBpedia；负对照 Musae、Twitch。
2. 单尺度 component-cover 只记录不剪枝：Youtube、GPU-DBLP、Github、Musae；负对照 LiveJournal、Reddit。
3. 只有“超过现有最大下界”和“新增淘汰”显著时，才进入真实剪枝实验。
4. 真实剪枝先验证 Base/Enhanced 权值、状态计数与公共调用路径完全一致，再比较时间和内存。
5. Orkut 等当前 P1 结束后再补结构统计，不与正式运行争抢 2.1 GiB 图文件的 IO。

<a id="history-communication-five-hour-fair-youtube-optimization-probe"></a>

## YouTube 弱图五小时公平优化探针

> 原始记录：`communication/FIVE_HOUR_FAIR_YOUTUBE_OPTIMIZATION_PROBE.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 结论

本轮唯一同时通过公平性、精确性和跨图价值审查的主候选是“严格查询核”：对当前查询的全部候选顶点做保护，反复删除非候选且度数不超过 1 的顶点，再压缩非候选二度顶点。压缩二度顶点时，用一条权重为两条原边权重之和的新边连接其两个邻点；允许保留平行边。该变换不改变任一可行 GST 的最优权值。

该预处理必须对 ABHSS Base、ABHSS Enhanced 和仓库内 `pruneddp_safe` 完全相同地执行，并把查询核的计算与压缩耗时逐查询计入三种方法。不能只给本文方法使用，不能从运行结果选择是否启用，不能根据图名、组数或人工阈值切换，也不能把图文件写出和再次读入的原型开销混入最终算法时间。

本轮没有找到第二个已经成熟的算法方案。直接把现有 tour completion 加入公共 A1 cone 的候选虽能大幅减少状态，但在固定面板上产生错误答案，已被否决。后续可以独立研究只改变实现、不改变下界值的 A1 farthest top-two 缓存，但在通过端到端 gate 前不能写入论文结论。

### 2. 公平实验协议

- 正式 baseline 是仓库内 `pruneddp_safe`，配置保持 P1 的 hash state storage、MST upper bound 开启、LB2 pathmax 关闭。
- Base、Enhanced、PrunedDP++ 使用同一压缩图和同一重编号查询。
- 核构造从图和查询已加载后开始计时；标记候选、叶剥离、二度压缩和紧凑重编号均计入。临时图写盘和第二次图加载仅为原型接口，不计入算法时间，正式实现应在内存中完成。
- 所有探针使用单进程、`nice -n 19` 和 `ionice -c 3`。正在运行的 Orkut P1 保持默认优先级；探针期间 CPU 与内存 pressure 均为 0。
- YouTube 样本沿用此前确定的固定索引，并覆盖已知弱查询，不按候选结果重新挑选。
- 每个变体必须先比对最优权值，再比较时间、峰值额外 RSS 和 `(mask,v)` 状态数。

### 3. 为什么严格查询核正确

查询中任一候选顶点都被保护。非候选叶子不可能是最优树中必须保留的端点；若可行树包含它及其唯一关联边，删除该叶子不会失去任何组覆盖，并且非负边权保证代价不增加。因此可反复剥离。

对非候选二度顶点，任何使用它的简单连接只能同时使用它的两条关联边。将这两条边替换为连接两个邻点、权重等于两边权重之和的新边，保持连接关系与代价；反向展开新边即可恢复原图解。若两个邻点相同则形成无用闭环，可直接删除。平行边可以保留，不影响精确算法正确性。

两类规则都只读取图结构和当前查询候选集合，不读取图名、组数分段、历史运行时间或方法身份，因而没有无意义超参数，也不存在 per-query method oracle。

### 4. 图规模变化

| 图与查询 | 原点数 | 原边数 | 核点数 | 核边数 | 保留点比例 | 保留边比例 | 原型构造秒数 |
|---|---:|---:|---:|---:|---:|---:|---:|
| YouTube g7 q49 | 1,134,890 | 2,987,624 | 300,546 | 2,099,755 | 26.48% | 70.28% | 0.966 |
| LinkedMDB q1 | 1,326,784 | 2,132,796 | 279,695 | 800,866 | 21.08% | 37.55% | 0.352 |
| Musae g7 q1 | 19,109 | 400,497 | 17,307 | 397,983 | 90.57% | 99.37% | 0.157 |

YouTube 在只做叶剥离后仍有 168,853 个二度顶点，占叶核的 35.91%；LinkedMDB 为 57.47%；Musae 仅为 5.16%。这解释了方案为何重点改善 YouTube 和 LinkedMDB，同时也说明当前基于有序映射合并平行边的原型在 Musae 上不划算。正式实现应线性保留平行边，避免这项不必要成本。

### 5. YouTube 固定样本结果

下表时间已经给每一种方法加上同一查询的核构造时间。单位为秒。

| g | 查询 | Base | Enhanced | PrunedDP++ | 最快方法 |
|---:|---:|---:|---:|---:|---|
| 5 | 59 | 1.519 | 2.625 | 1.965 | Base |
| 5 | 96 | 1.798 | 2.587 | 1.870 | Base |
| 5 | 266 | 2.885 | 2.577 | 1.910 | PrunedDP++ |
| 7 | 49 | 4.802 | 4.313 | 4.315 | Enhanced，近似持平 |
| 7 | 107 | 5.068 | 4.392 | 5.215 | Enhanced |
| 7 | 241 | 4.026 | 3.165 | 3.417 | Enhanced |
| 7 | 265 | 4.624 | 4.106 | 3.523 | PrunedDP++ |
| 合计 | 7 条 | 24.72 | 23.76 | 22.22 | PrunedDP++ |

相同七条查询在原图上的合计约为 Base 39.00 秒、Enhanced 48.86 秒、PrunedDP++ 33.17 秒。严格核后，PrunedDP++ 相对 Enhanced 的优势从约 47% 缩小到约 7%，相对 Base 的优势从约 18% 缩小到约 11%。因此该方案明显缓解 YouTube 劣势，且不是只加速本文方法；但当前证据尚不足以声称 YouTube 聚合已经反超 baseline。

代表性内存结果也有改善。YouTube q49 的查询峰值额外 RSS 为 Base 43.5 MiB、Enhanced 77.1 MiB、PrunedDP++ 56.4 MiB；原图中 Enhanced 的对应内存明显更高。状态数仍显示 PrunedDP++ 的 DP 状态更少，说明后续若要彻底反超，应继续优化公共 A1，而不是声称图约简已经解决全部算法差距。

### 6. 跨图结果

计入核构造后，LinkedMDB q1 的 Base、Enhanced、PrunedDP++ 分别约为 0.936、1.349、0.911 秒；Base 与 baseline 基本持平，并且绝对时间优于只做叶剥离的旧原型。Musae q1 分别约为 0.403、0.220、0.385 秒；Enhanced 仍最快，但三种方法都被 0.157 秒的原型构造成本拖累。

Musae 结果不能用经验阈值掩盖。正确处理是优化共同构造器：压缩后直接保留平行边，使用线性容器，不运行有序映射去重。若优化后仍有小图固定开销，则论文应诚实报告，不能根据图名关闭方案。

### 7. 被否决的候选

#### 7.1 直接将 tour completion 加入 A1 cone

该候选令公共 A1 的 continuation 取现有 farthest 与 tour 的较大值。它在 YouTube g7 q49 上曾把 Enhanced 状态从约 141.8 万降至 4,362，并降低时间，因此表面上很有吸引力。

固定面板随后发现明确错误：YouTube g5 q59 的正确答案为 23，候选 Base 返回 24；q96 的正确答案为 30，候选 Base 返回 32；另有若干查询也出现 Base 或 Enhanced 权值不一致。原因是 A1 cone/fallback 的现有证明依赖 farthest 的路径一致性和 bounded `GroupRow` 的精确性条件，不能把 ordinary 阶段可采纳的 tour 值未经重新证明直接移入 A1。该候选必须永久标记为 rejected，不能因性能好而恢复。

建议把 q59、q96、q181、q61、q91、q121、q211 加入大图答案 gate。现有五个小型 CTest 均通过却未发现这些反例，说明回归集需要补强。

#### 7.2 其他否决项

- facility residual CSR：增加构造与访存成本，YouTube 多个查询退化。
- 跳过 facility attribution：YouTube无收益，Musae 状态数近乎翻倍。
- ordinary dual-last：状态不变且时间略退化。
- 无参数 Kruskal component-cover：代表查询状态不变，增加预处理和内存。
- 将 PrunedDP++ 的第二 LB2 项补入 ABHSS：代表查询状态不变并增加时间。

这些结果共同说明 YouTube 的主要瓶颈是公共 A1 的大规模状态，而不是后续 facility、dual 或 ordinary 阶段。

### 8. 建议落地顺序

1. 先把严格查询核实现为三种方法共用的内存预处理模块，保留平行边并线性构造；统一记录 `kernel_seconds`、核前后点边数。
2. 在 correctness gate 中对压缩图与原图逐查询比对三种方法权值，并加入本轮发现的 YouTube 反例。
3. 对 P1 全查询重新运行三种方法；baseline 也必须重新跑，不能沿用未约简的旧记录。
4. 只有当全量结果确认跨图平均不退化后，才把严格核并入论文主配置。
5. 第二方向只研究数值等价的 A1 farthest 缓存，例如每顶点保存非锚组距离的最大与次大值，使“排除当前 singleton 后的最大值”成为常数时间读取。它不得改变 cone、fallback 或任何下界数值；完成实现、反例 gate 和全量性能测试前不列为最终方案。

### 9. GitHub 与后续 LLM 注意事项

- 本文不使用多行 LaTeX 公式，避免 GitHub 数学渲染差异。
- 不要把直接 tour-A1 候选重新描述为安全优化；它已有确定错误答案。
- 不要只给 ABHSS 使用查询核，也不要复用旧 PrunedDP++ 时间与新 ABHSS 时间比较。
- 不要添加图名、组数或“压缩比例达到某阈值才启用”的经验开关。
- 原型文件写盘只是实验手段，正式计时边界应是内存中的共同变换。

<a id="history-communication-youtube-structural-lower-bound-analysis"></a>

## Youtube 类稀疏图的结构下界静态分析

> 原始记录：`communication/YOUTUBE_STRUCTURAL_LOWER_BOUND_ANALYSIS.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

本文只讨论候选方向，不表示已经进入正式方法。目标是在不观察图名、不改变精确性、且图级预处理接近线性的前提下，为 Youtube 一类稀疏、重尾、带大量树状边缘的图提供更强剪枝。

### 1. 已有下界排除的重复方向

当前 `TourLowerBound` 已在组间距离矩阵上预计算 Hamilton path，并把当前顶点连接到路径两端后除以二。因此，常见的三终端半周长、组间最近邻和简单 MST 型度量下界大多已被它覆盖或非常接近，不值得再增加一套相似的指数级表。

真正缺少的是：度量下界只看到“点到组、组到组的最短距离”，没有显式累计稀疏图中多条同时不可绕过的结构边。

### 2. 首选候选：桥骨架并集下界

#### 2.1 图级结构

对无向图做一次 Tarjan 桥分解，把每个 2-edge-connected component 收缩为一点，所有桥形成一棵树或森林，记为桥树。原图上的任意可行 GST 若需要连接桥两侧的顶点，就必须购买该桥；不存在另一条原图路径可以替代它。

图级预处理复杂度为：

```math
O(n+m)\text{ 时间},\qquad O(n+m)\text{ 空间}.
```

这项预处理与查询、组数和 Base/Enhanced 配置无关，可以在图加载后只做一次。

#### 2.2 查询上的强制桥

固定当前状态顶点 $v$ 和尚未覆盖的组集合 $M$。在桥树上删除一条桥 $e$ 后得到两侧。若 $v$ 在一侧，并且某个 $i\in M$ 的全部候选顶点都在另一侧，则任何完成解都必须包含 $e$。

令 $F(v,M)$ 为满足上述条件的桥集合，则定义：

```math
L_{\mathrm{bridge}}(v,M)=\sum_{e\in F(v,M)}w(e).
```

该式不是把每个组的距离相加，而是对所有组要求的桥取并集后只计一次。因此共享前缀不会重复收费，不同桥分支却可以安全累加。

#### 2.3 正确性

对任意 $e\in F(v,M)$，至少存在一个未覆盖组，其所有候选都位于 $e$ 的另一侧。任何从 $v$ 出发并覆盖该组的连通子图都必须跨越该割；由于 $e$ 是桥，跨越该割只能使用 $e$ 本身。因此每条 $e\in F(v,M)$ 都属于任意可行完成树。桥彼此是不同原边，故其边权之和不超过任意完成树代价。

所以：

```math
L_{\mathrm{bridge}}(v,M)\leq \mathrm{OPT}(v,M).
```

最终 future 可以安全取：

```math
L(v,M)=\max\{L_{\mathrm{far}},L_{\mathrm{tour}},L_{\mathrm{bridge}},L_{\mathrm{dual}}\}.
```

#### 2.4 为什么可能特别适合 Youtube

Youtube 的度数中位数只有 1，平均度约 5.27，具有巨型核心和大量树状边缘。对处在不同叶枝或长链中的小终端组，`farthest` 只能取一条最远路径，桥下界则能累计多个不同分支上的不可绕过边。组越小，候选越不容易跨越多个桥分量，强制桥越多；这恰好对应 Youtube 上“小组导致状态更多”的困难区域。

它对高连通图仍然安全，只是桥树退化成很少的结点，下界接近零且预处理后可立即判定为无效。

#### 2.5 不能直接采用的朴素实现

为每个顶点、每个 mask 保存桥下界需要 $O(n2^g)$ 空间，不可接受。为每次状态沿桥树扫描也不可接受。

建议采用“按 row 购买”的离线实现：

1. 每条桥的两种方向各保存一个组 mask，表示哪些组的全部候选都在该方向一侧。
2. 对某个 ordinary/anchored row，其 remaining mask 固定。
3. 当该 row 的候选数尚少时只使用现有下界，累计 rent。
4. 当 rent 达到一次桥树 reroot 的确定性工作量时，线性扫描桥树，为该 remaining mask 计算所有桥分量的精确 `bridge` 值。
5. 同一 row 后续按顶点所属桥分量 $O(1)$ 查询。

一次购买为 $O(B)$，其中 $B$ 是桥树结点数；空间为 $O(B)$ scratch，并在 row 完成后复用。Youtube 的宽 row 才会购买，状态很少的查询不会承担整树扫描。这与 witness 的 rent-or-buy 思路一致，不需要图名、经验性 $g$ 阈值或运行时间反馈。

### 3. 更轻的伴随优化：查询相关 2-core kernel

对图做 2-core peeling。core 外的每个连通部分都是附着在 core 上的树。对一条查询，只保留：

- 整个 2-core；
- 每个查询终端到 core 的唯一树路径；
- 若某个连通分量本身没有 core，则保留连接查询终端的最小树子树。

任何正权最优解都不会进入不含查询终端的 pendant branch，因为进入后只能沿同一桥返回，删除这段往返严格不增代价。因此 D/A/H 图闭包可以跳过 kernel 外顶点。

这严格说是搜索域剪枝而不是新下界，但它与桥分解共享图级信息，且可能比桥下界本身更直接地消除 Youtube 的无效叶枝状态。必须保证 group-distance 作为下界 oracle 的语义不受影响：可以保留全图距离构造，只限制真正产生 DP 状态的闭包；更激进的距离限制需要另行证明。

### 4. 次选候选：多尺度 component-cover 下界

当前 `ComponentCover` 只利用零权连通分量和最小正边权。可把它推广到若干权值阈值 $t$：只保留权重小于 $t$ 的边并收缩连通分量。若当前顶点所在分量已经覆盖一部分组，覆盖其余组至少还需要 $c_t(v,M)$ 个其他分量，则任何完成树至少购买同样数量、且每条权重不小于 $t$ 的跨分量边：

```math
L_t(v,M)=t\,c_t(v,M).
```

取若干阈值的最大值始终安全。进一步在所有权值区间上积分，可得到更强的 Kruskal 型下界：

```math
L_{\mathrm{multi}}(v,M)=\int_0^{\infty}c_t(v,M)\,dt.
```

它能累计非桥但必须跨越的“昂贵连通层级”，理论上比桥下界更普适。不过精确维护每个阈值、顶点和 mask 的 component-cover 代价较重。Youtube 边权只有 1--100，且正式查询 $g\leq7$ 时可能可行；对 $g=16$ 则必须使用确定性工作预算或少量阈值。因此它适合作为第二阶段研究，不应先于桥骨架方案。

### 5. 建议的静态优先级

1. 先统计每张 P1 图的桥数、2-core 外顶点比例、查询终端落在 fringe 的比例，以及每条查询的强制桥并集权重；这只需分析器，不改求解器。
2. 若 Youtube 的 fringe/桥占比明显而其他图较低，实现公共的桥分解和按 row rent-or-buy 下界。
3. 同时尝试只限制 DP 闭包的查询相关 2-core kernel，并单独验证权值与状态数。
4. 只有桥下界命中率不足时，再研究多尺度 component-cover。

### 6. 必须设置的验收指标

除了总时间和状态数，还应记录：

- 图的桥数、桥树结点数、2-core 外顶点数；
- 每个 row 是否购买桥下界、购买前 rent、购买成本；
- `bridge` 比现有 `max(farthest,tour,dual)` 更大的候选次数；
- 仅由 `bridge` 新淘汰的候选数；
- 每次购买减少的 D/A/H 状态与增加的预处理时间；
- Base 和 Enhanced 分别的变化，且两者调用同一个桥下界实现。

首要停止条件是：若 Youtube 查询的强制桥总权普遍被 `tour` 覆盖，或绝大多数终端和状态都位于同一个无桥核心，则不应把该方向加入正式算法，应转向多尺度 component-cover 或 facility 支撑图优化。

<a id="history-dual-order-negative-probe-20260807"></a>

## Directed-cut 替代顺序负面探针

> 原始记录：`DUAL_ORDER_NEGATIVE_PROBE_20260807.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

当前实现按候选根到组的距离递减执行 directed-cut 对偶增长。本探针在不改变对偶可行性的前提下比较两个确定性替代极点：距离递增的反向顺序，以及最远、最近交替的平衡顺序。两者均位于临时源码副本，没有修改主工作树。

### 正确性

两个候选均通过 144 个确定性随机图、g=2..10 的三配置精确性对照。反向顺序的 Base/DirectedCutOnly/Enhanced 状态总量为 2814/815/818；交替顺序为 2814/913/913。

### Orkut g16 q4 哨兵

| 版本 | 运行状态 | 已用时间 | 原版完成时间 | 结论 |
|---|---|---:|---:|---|
| 当前递减顺序 | 完成 | 1194.775808 秒 | 1194.775808 秒 | 冻结参照 |
| 反向顺序 | 主动终止 | 超过 1800 秒 | 1194.775808 秒 | 至少退化 50% |
| 远近交替 | 主动终止 | 超过 1800 秒 | 1194.775808 秒 | 至少退化 50% |

两个替代顺序在容易询问上都不能及时完成，且运行期 RSS 持续高于约 7.5 GiB。由于程序只在询问完成时输出累计状态，主动终止项没有最终状态数；但它们已经违反“性能不退化”的首要门槛，不值得继续消耗 g15/g16 长尾时间。

### 论文判断

对偶增长顺序虽然不影响证书合法性，却显著影响证书强度。当前从远到近的顺序在该实例上明显优于两个自然替代极点。仅替换顺序缺少稳定优势，不应成为论文方法。未来若研究多证书 envelope，必须保留当前证书并证明额外预处理在小图上可摊销；本探针不支持直接替换。

### 清理

候选进程已终止，临时源码、构建目录和输出已删除。原始日志也已删除，只保留本文中的结论与汇总数值。

<a id="history-hstar-negative-probe-20260807"></a>

## H 在线 star 上界负面探针

> 原始记录：`HSTAR_NEGATIVE_PROBE_20260807.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 结论

该候选不进入正式实现。它在 Enhanced 的高层 H 状态中，把已经读取的组距离相加，尝试更早构造真实 star 可行上界。候选通过随机图精确性、零权 witness 和状态计数回归，但在两条 Orkut 探针上均未减少任何 `(mask,v)` 状态。

### 隔离条件

- 日期：2026-08-07。
- 候选固定在 CPU 10、NUMA node 0。
- 进程使用 `nice=19` 和 idle I/O priority。
- 当时正式 Orkut g16 q5 固定运行于 CPU 1，主要内存位于 NUMA node 1。
- 候选二进制采用与正式版本相同的 Release/O2/IPO 构建策略。

### 结果

| 查询 | 版本 | 时间（秒） | 峰值空间（MiB） | 状态数 | 最优值 |
|---|---|---:|---:|---:|---:|
| Orkut g16 q4 | 原版 | 1194.775808 | 2722.359 | 26517767 | 20 |
| Orkut g16 q4 | H-star | 1174.988531 | 2722.559 | 26517767 | 20 |
| Orkut g15 q1 | 原版 | 10241.959915 | 3758.242 | 253636180 | 21 |
| Orkut g15 q1 | H-star | 9850.369299 | 3781.691 | 253636180 | 21 |

### 判定依据

两条查询的状态数逐项完全相同。时间分别变化约 -1.7% 和 -3.8%，但只有单次、非同时 A/B 运行，且正式 q5 在后台运行，不能把该差异解释为稳定加速。候选还增加一个按顶点分配的 `double` 缓存，g15 q1 的峰值空间略升。因此它没有解决目标中的状态爆炸，不值得继续运行 g16 长尾询问。

### 清理范围

候选对 `src/abhss/adjoint.cpp` 的修改已经撤销；专用构建、`/tmp` 结果和原始输出均已删除，仓库只保留本文结论。

<a id="history-state-explosion-negative-probes-20260807"></a>

## Orkut 高组数状态爆炸：未采用探针（2026-08-07）

> 原始记录：`STATE_EXPLOSION_NEGATIVE_PROBES_20260807.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

本文只归档没有进入当前方法的方向，防止后续重复试验。数值来自同一服务器、单查询、单线程 Enhanced 探针；它们不是论文正式结果。失败实现及原始大输出已经或将在记录完成后删除。

### 1. 诊断基线与根因

Orkut `g=16, q2` 的完整旧结果为：196032.136 秒、105862.977 MiB、9044653910 个状态，最优值为 39。预处理只得到上界 52；A1 用时 85.673 秒并产生 39255744 个标量。`g=15, q2` 的完整旧结果为：25635.596 秒、15342.992 MiB、1264668489 个状态，最优值为 32；预处理上界为 37。

仅用于诊断的 oracle 实验把已知最优值作为 incumbent 注入，但从未作为候选实现：

- `g=15, q2`：A1 标量从 30280155 降到 22975585，下降 24.1%；首个 ordinary row 从 1658095 降到 586972，下降 64.6%。
- `g=16, q2`：A1 标量从 39255744 降到 28776723，下降 26.7%；A1 时间从 85.673 秒降到 60.671 秒。

这证明松的早期可行上界是爆炸的重要原因之一，但不证明任何可实现上界算法有效。

### 2. Primal-recentered dual envelope

#### 假设

先构造原根的 directed-cut 证书，再从现有 primal witness 中选择最小化“到所有组最大距离”的确定性中心，构造第二份合法证书。搜索下界取两份证书的最大值。两份证书各自可采纳，因此最大值仍可采纳；方案不读取图名、组数阈值、时间或边权类型。

#### 结果

完整的 Orkut `g=16, q4`：

| 版本 | 时间（秒） | 峰值空间（MiB） | 状态数 |
|---|---:|---:|---:|
| 原版 Enhanced | 1194.776 | 2722.359 | 26517767 |
| recentered dual | 1170.314 | 2722.555 | 26517767 |

状态数逐项完全相同；约 2% 的时间差属于运行波动，不能解释成算法收益。

Orkut `g=16, q2` 在完整 gate 判负后停止：预处理上界从 52 改进到 50，但预处理从 223.606 秒增加到 308.007 秒；A1 标量仅从 39255744 降到 38440433（2.08%），A1 时间为 82.972 秒。ordinary 多次购买 witness DP 后上界仍为 50；停止时已产生 138263490 个 ordinary 标量。

#### 结论

第二份证书增加一次昂贵的 residual 构造和一份组势，却没有剪掉 q4 的任何实际状态；在真正困难的 q2 上也只带来很弱的 A1 收益。即使提前释放第一份 residual 能降低临时峰值，也无法改变零状态收益，因此整个方向淘汰。

### 3. 其他已淘汰的轻量下界或调度

| 方向 | Orkut `g=16, q4` 结果或诊断 | 淘汰原因 |
|---|---|---|
| 把 exact D(mask,v) 投影为 H prefix | 26509383 状态，减少 0.032% | 收益远低于新增逻辑复杂度 |
| A1 top-two 投影为 H prefix | 26434112 状态，减少 0.315%；时间约 1156.49 秒 | 正确但收益过弱，不作为独立论文机制 |
| 组度量 MST floor | 26517746 状态，只减少 21 | 基本不命中 |
| MST floor 与 A1 投影叠加 | 26434091 状态，仅比 A1 再少 21 | 没有互补收益 |
| 在线 H boundary settlement | 状态数不变；与 A1 叠加也不再减少状态 | 只是改变执行顺序，没有增强证书 |
| H row 顺序与 seed boundary priority | 状态数不变 | 调度不改变搜索域；不应引入经验顺序参数 |
| A1 star completion | `g=15/16, q2` 初始上界均不改善，A1 反而变慢 | A1 部分树太弱，组距离和严重重复计价 |
| 提前 exact A2 可行完成 | 完整 `g=16,q4` 为 1034.246 秒、24105261 状态，较原版 1194.776 秒、26517767 状态有局部正收益；但 `g=16,q2` 在 D2 后仅构造 exact A2 就耗时 1148.92 秒、物化 302166542 个 A 标量，上界仍为 52 | 该上界数学安全且不含经验参数，但强查询的固定构造成本与 payload 已达到一次普通 gate 的量级，且没有解决目标 q2 的早期 incumbent；收益不稳健，淘汰 |
| D2 最小权匹配可行完成 | `g=16,q4` 为 1166.454 秒、26517767 状态；`g=15,q1` 为 10050.135 秒、253636180 状态，状态均与原版逐项相同 | 匹配只改变要尝试的真实 rooted-D2 分区，未产生更强 incumbent；零状态收益不足以支持额外 DP 与恢复逻辑 |
| ordinary 首 settled 数值和 completion | 1156.510 秒、2722.555 MiB、26517767 状态，与原版状态完全相同 | 只把部分树值与剩余组距离相加，无法去除共享路径的重复计价 |
| ordinary 首 settled 真实 row 恢复 + 路径并集 Kruskal | 1212.351 秒、2722.555 MiB、26517767 状态；`g=16,q2` 前 25 张 size-2 row 也未把上界 52 收紧 | 反向恢复避免了全状态 backpointer，但首 A* 状态不是高质量 primal witness；零状态收益且增加恢复时间 |
| terminal medoid reroot/root-path union | `g=16, q2` 上界仍为 52，小回归状态 913 增至 1256 | 代表点中心不对应共享路径结构 |
| 最近组贪心路径增长 | `g=15, q2` 仍为 37；`g=16, q2` 最好只到 48 | 离最优值 32/39 仍太远 |
| 代表根路径并集后做 Kruskal | `g=16,q2` 上界仍为 52；`g=15,q2` 仍为 37；`g=16,q4` 仍为 21 | 去环不能弥补所选根和最短路径本身缺少共享结构 |
| 枚举查询中全部终端作为根，再对路径并集做 Kruskal | `g=16,q2` 上界仍为 52；`g=15,q2` 仍为 37 | 即使覆盖全部 5401/14711 个候选终端也无改善，说明瓶颈不是根样本不足，而是根到各组的独立最短路模型 |
| 每张 ordinary row 的全状态 star-completion argmin | `g=15,q2` 前 25 张 row、37498377 标量后仍为 37；`g=16,q2` 前 25 张 row、62613842 标量后仍为 52 | 比“首 settled”覆盖更强但仍零上界收益，并额外支付 `O(g Z_D)` 扫描；独立距离和的重复计价是结构性瓶颈 |
| pair-DP guided witness roots | `g=15,q2` 的 91 张 pair row 提供 55 个不同根，best 仍为 37；`g=16,q2` 的 105 张 row 提供 66 个根，best 仍为 52 | DP 中心替代终端根仍无收益；瓶颈是固定根独立最短路 witness 模型，而不是根来源 |
| 最小切片真实部分树补全 | `g=15,q2` 完成全部 91 张 pair row，并继续完成 17 张 size-3 row，上界仍为 37；`g=16,q2` 在 105 张 pair row 中把上界 52 收紧到 51，进入第一张 size-3 row 后未继续 | pair 层能复现 PrunedDP++ 的正信号，但只使 `g=16` pair 层标量从 262798507 降至 260218423（0.98%）；对每张高层 row 恢复全部最小值状态的额外工作增长快，当前收益不足以抵消复杂度与开销 |
| 提前 adjoint primal completion（D2/D3/D4） | D2 版在 `g=16,q4` 的首个 H11 已产生 639499 个状态，运行 9.7 分钟后仍剩 9 层；D3 版完整 gate 额外产生 583889 个状态、约 2 分钟，但 incumbent 已由原 witness 调度提前得到 20，因而没有上界收益；`g=15/16,q2` 的 D3 强探针在 D3 中段分别保持上界 37/51，尚未到提前 H 扫描即按实验调度要求停止 | D2 固定开销不可接受；D3 在便宜 gate 上已有约 10% 时间开销却未改善 incumbent，强探针在触发前也没有出现新的正信号；D4 触发更晚且被 D3 结构性覆盖。当前证据不足以承担重复 H 计算与额外实现复杂度，三版均不采用 |
| 固定终端 metric-closure witness | 从当前 dual-primal witness 为每组确定一个已命中的真实代表终端，在这些固定终端的最短路度量闭包上求 MST，展开回原图、Kruskal 去环并删除非代表终端叶子；通过配置精确性与零权边回归。`g=16,q4` 为 1174.018 秒、3094.645 MiB、26517767 状态，状态数与原版完全相同且空间增加约 372 MiB；仅预处理的 `g=15,q2` 与 `g=16,q2` 上界仍分别为 37 和 52 | 该构造是安全、确定性且可解释的可行上界，但固定当前 witness 的终端后，只改连线方式无法解决真正的组终端选择问题；强查询零上界收益，因此不进入正式方法 |
| 稀有组永久锚（minimum-cardinality anchor） | 用候选顶点数最少的组替换共同根处最远组作为所有配置共用的永久锚；通过配置精确性与零权边回归。`g=15,q2` 的完整 D2 状态从 124697954 增至 130785369（+4.88%），上界仍为 37。`g=16,q2` 的 D2 状态从 262798507 降至 261405811（-0.53%），并提前把上界 52 收紧到 50；D3 前缀收益约 2%。但完整 `g=16,q4` 为 1595.518 秒、2722.559 MiB、30910094 状态，对原版 1194.776 秒、2722.359 MiB、26517767 状态分别退化 33.5%、约 0%、16.6% | 规则结构清楚且无超参数，但不同锚会显著改变 A/H 稀疏锥体；它只对单个强查询带来小收益，却让普通 gate 明显退化，不满足论文方法的稳健性与性能不退化底线，因此淘汰 |

H star、dual 组顺序和 H 调度的独立细节分别见同目录下的 `REJECTED_AND_DEFERRED_DIRECTIONS.md#history-hstar-negative-probe-20260807`、`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-dual-order-negative-probe-20260807` 与 `METHOD_EVOLUTION_AND_CORRECTNESS.md#history-h-scheduling-probe-20260807`。

“最小切片真实部分树补全”不读取图名、`g` 阈值或时间预算：它在每张 ordinary row 中选择取得该 row 精确最小值的状态，递归恢复真实 DP 部分树，再连接尚未覆盖的组并在真实边上运行 Kruskal。它通过了 144 个配置精确性回归和零权边回归，但上述强查询结果不足以支持采用。实现与大体积临时输出已删除，只保留本节结论，避免后续重复试验。

### 4. PrunedDP++ MST completion 对照诊断

隔离诊断表明，PrunedDP++ 的早期 incumbent 并非来自单个初始根，而是在 settled rooted state 上恢复真实部分树、拼接剩余组最短路并做 Kruskal。Orkut `g=15,q2` 在前 1264 次 completion 中由 singleton state 把上界依次收紧到 60、45、43、41、39，但扩展到 4194304 个状态仍未优于 ABHSS 的 37。Orkut `g=16,q2` 的第 1332 个 completion 首次由 `mask=16392`、`mask_size=2`、`state_cost=1` 的 pair state 把上界从 55 收紧到 51，优于 ABHSS 的初始 52。该证据把后续方向限定为真实低层部分树恢复，而不是继续枚举根或叠加距离和。

### 5. 约束与后续方向

后续候选必须同时满足：

1. 使用真实可行树收紧上界，或使用有明确可采纳证明的下界；不能使用 oracle。
2. 不按图名、查询编号、组数经验阈值或运行时间选择算法。
3. 不依赖整数权、桶宽或人为超参数。
4. Base 与 Enhanced 的公共 DP 操作必须共用；Enhanced 只能安全新增证书或以同职责结构替换 Base realization。
5. 先通过确定性小图精确性门禁，再以 Orkut `g=16, q4` 检查完整时间、空间和状态，最后才进入 `q2`/`g=15` 强询问。

<a id="history-remaining-a2-ranking-negative-probe-20260808"></a>

## Remaining-aware A2 排名方案负结果（2026-08-08）

> 原始记录：`REMAINING_A2_RANKING_NEGATIVE_PROBE_20260808.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 方案

在 D2 后提前构造最终流程本来需要的 exact A2，并按每个顶点的 A1 值对全部非锚组排序。对每个 ordinary 状态，从当前 `remaining` 中取排序最高的两组，并把对应 exact A2 值作为可采纳 future。

该规则虽不含图名、g 阈值或经验参数，但其原始正确性论证是错误的。Rooted A2 给出“从根重新连接两个组”的绝对树成本，而 ordinary 状态已经拥有一棵 D 部分树；真正需要下界的是在允许复用既有 D 边时的增量成本。A2 可能重复计算已经支付的边，因而可以大于合法增量，不能作为可采纳 future。

### 实现

每个顶点按需构造一次完整 A1 组序，并用每组 4 bit 压入一个 64-bit 字。每次 future 查询扫描该字，直到找到两个仍在 remaining 中的组，再查询一张 A2 稀疏 row。

### Gate 结果

- 数据：GPU4GST Orkut，P2 g=16，query 4，Enhanced。
- 完整 CTest：5/5 通过，但这些小回归没有覆盖到上述重复计价反例，不能证明该下界正确。
- SteinLib 反例：`wrp3-16` 的已知最优值为 208，该错误 A2 下界使 Base 返回 236，因此方向首先因精确性失败而淘汰。
- 运行环境：起初与 g16 query 2 并发，发现显著内存带宽争用；随后暂停另一任务，让本 gate 独占带宽。
- 终止时查询已运行约 14 分 15 秒，仍未输出第一条 `ordinary_layer`；其中最后约 3 分 40 秒为独占带宽运行。
- 对照固定 top-two A2 的完整 query 时间为 1040.911 秒。新方案在尚未形成可比较的 ordinary 整层状态前，已消耗接近该对照的完整时间。

### 结论

该方向在数学上不安全，性能也明显退化。停止继续运行，不合入主线；以后只能把 A2 当作真实可行树上界，或重新证明一个扣除既有部分树复用后的增量证书，不能直接放入 lower-bound max stack。

<a id="history-adjoint-lifecycle-negative-probe-20260809"></a>

## Adjoint 生命周期压缩负结果（2026-08-09）

> 原始记录：`ADJOINT_LIFECYCLE_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

候选不改变搜索空间和状态转移，只缩短 Enhanced 中 ordinary 行与 adjoint terminal 向量的存活期：在 64 顶点转置块读取 ordinary 行的同时，把该行原地压缩为后续 H 阶段唯一需要的 canonical branches；每个 terminal 向量被其唯一 H row 消费后立即释放。扩展版本还按静态依赖 DAG 计算 H row 的剩余消费者数，并在最后一次消费后释放对应行。上述规则均不读取图名、查询编号、组数阈值、运行时间或边权类型。

### 正确性

转置仍在覆盖某段内的原值被覆盖前按值复制；原地写指针始终不超过读指针，因而不会覆盖尚未读取的状态。压缩保持顶点递增顺序，并只保留后续 `ForEachBackwardBranchSum` 访问的 canonical branches。terminal 向量在唯一消费者完成读取后才释放。最终精简候选通过 5/5 CTest、144 个确定性随机精确性配置、零权边回归，以及 11 个 `g=11..16` SteinLib 已知最优实例。

### 结果

以下均为同一服务器上的单查询、单线程方向探针，不是论文正式结果：

| 图与查询 | 主版本时间 / MiB / states | 流式压缩时间 / MiB / states | 结论 |
|---|---|---|---|
| Twitch `g=15`, q1 | 262.749 / 662.191 / 51,107,932 | 260.798 / 618.508 / 51,107,932 | 时间持平，峰值空间下降 6.6% |
| Twitch `g=16`, q1 | 144.472 / 554.285 / 40,494,123 | 144.768 / 554.840 / 40,494,123 | 无收益 |
| Youtube `g=16`, q1 | 254.087 / 537.758 / 14,563,709 | 252.162 / 534.973 / 14,563,709 | 仅约 0.5% 空间差异 |
| Musae `g=16`, q1 | 272.445 / 656.453 / 49,093,585 | 272.615 / 654.992 / 49,093,585 | 仅约 0.2% 空间差异 |
| Orkut `g=16`, q4 | 1,194.776 / 2,722.359 / 26,517,767 | 运行中峰值已达到原量级 | 最终峰值不可能改善 |
| GPU4GST_DBLP `g=15`, q3 | 2,601.141 / 2,498.043 / 158,655,552 | 2,549.908 / 2,498.043 / 158,655,552 | 空间和状态完全无收益 |

静态 H consumer 引用计数扩展也没有独立收益：Twitch、Musae、Youtube、Reddit、DBLP 和 Orkut 的峰值均与不含该扩展的版本相同或仅有测量噪声级差异。

### 结论

该方向不减少状态发现数，只改变部分 payload 的释放时刻。它仅在 Twitch `g=15` 的特定峰值重叠关系上有可见空间收益，在更关键的 Orkut 和 DBLP 爆炸实例上完全无效，因此不具备跨图稳定性，也不能解释为解决高组数状态爆炸的论文贡献。流式压缩、terminal 提前释放和 H consumer 引用计数均不合入主线；实现、构建与原始探针输出删除，只保留本结论防止重复探索。

<a id="history-bridge-union-bound-negative-probe-20260809"></a>

## 强制桥并集下界负面探针（2026-08-09）

> 原始记录：`BRIDGE_UNION_BOUND_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

候选对无向图做迭代 Tarjan 桥分解。对固定当前分量与 remaining groups，若删除某桥后某个剩余组的全部候选都在另一侧，则该桥是任何完成树的必付边；所有必付桥取并集且每条只计一次，得到安全下界。桥树重根可在线性时间内计算固定 remaining mask 在全部分量上的值，ordinary row 采用确定性 rent-or-buy：累计工作达到一次重根扫描后，丢弃未发布的首遍并用桥界重建。

该机制不读取图名、组数阈值、运行时间或整数权，Base 与 Enhanced 可共用同一证书。完整五项 CTest 通过。

### 正确 P2 门禁

- 数据：`experiment_data/p2_cross_g/GPU4GST_Youtube/cross_g16.txt`，q1，Enhanced。
- 主实现历史结果：254.087 秒，537.758 MiB，14,563,709 个状态，权值 64。
- 桥候选结果：256.034 秒，575.801 MiB，14,563,709 个状态，权值 64。

状态数逐项完全相同，时间没有可归因收益，峰值空间增加约 38 MiB。

### 结论

Youtube 虽有大量桥和 fringe，但该 P2 高组数查询中的强制桥并集没有超过现有 farthest、tour 与 directed-cut envelope 形成新增淘汰。按照既有结构分析预先规定的停止条件，该方向不进入 Orkut 长查询，也不合入主代码。候选源码、构建和原始输出删除，只保留本文档。

<a id="history-complement-pair-scheduling-negative-probe-20260809"></a>

## 互补掩码相邻调度负面探针（2026-08-09）

> 原始记录：`COMPLEMENT_PAIR_SCHEDULING_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选与正确门禁

当非锚组数为偶数且 ordinary DP 到达半层时，把互补的两个 mask 相邻执行，希望第一行完成后第二行更早通过两块闭合产生完整解。它只改变同层顺序，不改变状态定义、转移或界。

候选通过完整五项 CTest。性能门禁使用正确的 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt` 第 3 条询问，Enhanced：

| 版本 | 秒 | MiB | 状态 |
| --- | ---: | ---: | ---: |
| 同机主实现 | 284.043 | 2689.770 | 30,674,269 |
| 互补相邻 | 288.913 | 2689.371 | 30,669,394 |

状态只减少 4,875（0.016%），时间反而增加约 1.7%，最优值均为 38。

### 结论

互补行相邻只能极弱地改变 incumbent 出现时刻，效应远低于论文机制门槛。该方向不进入主代码；候选源码、构建和原始输出删除，只保留本文档。

<a id="history-consistent-representative-upper-negative-probe-20260809"></a>

## 一致代表组树上界负结果（2026-08-09）

> 原始记录：`CONSISTENT_REPRESENTATIVE_UPPER_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

此前固定终端 metric-closure 先从现有 witness 选定每组代表，再改变连接方式，无法解决组终端选择。本轮反过来固定组级树拓扑，并在该树上联合优化代表终端。

对组树中 child group 到 parent group 的消息，以 child 的每个候选终端及其子树代价作为多源 Dijkstra 初值。消息在 parent 的每个候选终端处取值；同一 parent 候选要累加全部 children 消息，因此一个组与多个邻组连接时被强制使用同一个真实代表。自底向上消息传递对固定组树拓扑精确求得最小路径连接和，所选原图最短路径并集连通并覆盖全部组，故该值是真实可行上界。

测试了两种无参数拓扑：组间松弛度量的 MST，以及精确最短 Hamilton path。两者都不依赖整数权、图名、运行时间或经验阈值；额外代价为约 $g-1$ 轮带初值多源 Dijkstra。隔离原型通过 5/5 CTest、随机配置精确性与零权边回归。

### Orkut `g=16,q2` 预处理 gate

当前 ABHSS 初始真实上界为 52，已知精确值为 39。两个候选在输出新上界后立即停止，没有进入长时间主搜索：

| 固定组拓扑 | 一致代表上界 | 是否收紧 52 |
|---|---:|---|
| group-metric MST | 67 | 否 |
| shortest group-Hamilton path | 68 | 否 |

两者在双进程运行下约 3–4 分钟才完成额外消息传递，所得值反而明显弱于现有 dual-primal/root-path witness。

### 真实路径恢复 gate

后续原型保存每条消息对 parent 候选选择的 child 代表，自根向下回溯全部代表；再恢复每条组树边对应的原图最短路，按 edge id 去重并运行 Kruskal。恢复后得到的是真实可行树权，而不是重复计价的连接和。

| 查询 | 当前上界 | MST 恢复树 | path 恢复树 |
|---|---:|---:|---:|
| Orkut `g=16,q2` | 52 | 54 | 65 |
| Orkut `g=15,q2` | 37 | 55 | 54 |

MST 在 g16 上因共享边从连接和 67 降到 54，证明恢复 gate 必不可少，但仍未改善 52；g15 的两个恢复值更远弱于 37。固定组级树拓扑即使联合选择一致代表并回收共享边，也不能表达目标实例中的多级 Steiner junction，因此该方向正式淘汰。候选源码、构建和原始输出删除，只保留本结论。

<a id="history-high-g-component-and-overlap-negative-probe-20260809"></a>

## 高组数 component-cover 与组重叠负结果（2026-08-09）

> 原始记录：`HIGH_G_COMPONENT_AND_OVERLAP_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 上界尺度 component-cover

候选令当前真实上界为 $U$，在阈值 $\tau=U/(g-1)$ 下收缩所有权重严格小于 $\tau$ 的边。对状态根所在分量免费计入其覆盖组，再精确求覆盖 remaining groups 至少还需选择的分量数 $c$；连接这些额外分量至少需要 $c$ 条权重不小于 $\tau$ 的边，因此 $c\tau$ 是可采纳下界。该定义不依赖整数权、图名或经验阈值。

Orkut `g=16,q2` 的初始上界为 52， $\tau=52/15$；阈值子图已有 1,078,511 个分量，但其中一个分量同时包含全部 16 组。Orkut `g=15,q2` 的初始上界为 37， $\tau=37/14$；同样已有一个分量包含全部 15 组。因此任意根最多只需再选择一个含全部组的分量，平均证书分别只有约 1.33 和 1.39。

进一步扫描权重 1、2、3 后的连通断点仍得到同一结论：即使只收缩权重 1 的边，也已经存在同时包含全部查询组的连通分量；状态根在该分量外时证书至多为一个阈值，位于其内时为 0。该强度远低于现有 farthest、tour 和 directed-cut，不值得接入热路径。单尺度及全断点 envelope 对目标 Orkut 查询均关闭。

### 2. 根顶点组重叠 dominance

若顶点 $v$ 同时属于额外组，则 rooted 状态 `(S,v)` 可以零代价提升到覆盖并集的状态；这是严格 dominance。但目标查询的静态上限很低：

| 查询 | 组成员总数 | 不同顶点数 | 属于至少两组的顶点 | 额外 membership |
|---|---:|---:|---:|---:|
| Orkut `g=16,q2` | 5,401 | 5,226 | 166 | 175 |
| Orkut `g=15,q2` | 14,711 | 14,514 | 190 | 197 |

重叠只涉及约 3% 的终端顶点，而所有有颜色顶点占 307 万顶点图的比例也很低。该归约可以作为通用规范化，但没有证据支持它能解释或解决十亿级状态爆炸；为避免增加每状态 mask payload 和跨层 promotion 复杂度，本轮不实现。

### 3. 清理

隔离分析器、构建和 `/tmp` 原始输出在本结论核验后删除；主算法源码未因这两个方向改变。

<a id="history-metric-peripheral-anchor-negative-probe-20260809"></a>

## 查询度量外围锚负面探针（2026-08-09）

> 原始记录：`METRIC_PERIPHERAL_ANCHOR_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 想法

主实现把离初始化根最远的组设为永久锚。候选改为选择组间最短路度量中总距离最大的外围组，希望把最难连接的一侧移出指数 mask，并提高部分状态的已付代价。规则完全由查询度量决定，不使用图名、运行统计或经验阈值；Base 与 Enhanced 的后续内核不变。

### 正确 P2 同机门禁

以下运行均使用 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt`，主实现与候选分别固定在核 10 和核 11 同时运行。

| 询问 | 主实现：秒 / MiB / 状态 | 外围锚：秒 / MiB / 状态 | 状态变化 |
| --- | ---: | ---: | ---: |
| `g=15`, q3 | 284.043 / 2689.770 / 30,674,269 | 292.215 / 2689.395 / 30,609,347 | -0.21% |
| `g=15`, q4 | 372.838 / 2690.145 / 15,592,307 | 424.881 / 2690.520 / 16,960,270 | +8.77% |

两项最优值分别保持 38 和 28，完整五项 CTest 通过。

### 结论

组度量外围性不能稳定预测哪个永久锚会缩小锚定/普通状态域。q3 的微小下降不足以抵消时间退化，q4 又同时显著增加状态和时间，因此该选择规则不具备论文所需的稳健性。候选不合入主代码；实现、构建和原始探针输出删除，只保留本文档。

<a id="history-ordinary-refilter-negative-probe-20260809"></a>

## Ordinary row 重滤负结果（2026-08-09）

> 原始记录：`ORDINARY_REFILTER_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

候选在一次完整 ordinary 或 anchored row 严格收紧真实 incumbent 后，重新扫描已经发布的 ordinary rows。对每个精确状态，候选重新计算当前仍存活的 directed-cut、最远剩余组、A1 cone（缓存尚存时）和 tour 下界；若状态值与下界之和已经不能严格改善 incumbent，就压紧该 row 的顶点、数值和 branch bitmap。该机制不读取图名、查询编号、组数阈值、运行时间或边权类型，并保持 Base 与 Enhanced 共用同一实现。

### 正确性

删除条件仅使用真实可行上界和可采纳下界，等号处也可安全删除，因为当前 incumbent 已经对应一棵等值可行树。候选通过仓库 5/5 CTest、144 个确定性随机精确性配置、零权边回归和 11 个 `g=11..16` SteinLib 已知最优实例。

### 结果

以下均为同一服务器上的单查询、单线程方向探针，不是论文正式结果：

| 图与查询 | 配置 | 主版本时间 / MiB / states | 候选时间 / MiB / states | 结论 |
|---|---|---|---|---|
| Twitch `g=15`, q1 | Enhanced | 260.910 / 661.965 / 51,107,932 | 250.856 / 504.277 / 51,103,946 | 峰值空间下降 23.8%，累计状态只下降 0.008% |
| Orkut `g=16`, q4 | Enhanced | 1,194.776 / 2,722.359 / 26,517,767 | 1,170.246 / 2,722.559 / 26,517,767 | 状态和空间均无收益；约 2% 时间差视为波动 |
| Musae `g=15`, q1 | Base | 577.475 / 514.441 / 42,558,822 | 652.850 / 417.691 / 42,558,822 | 空间下降 18.8%，但时间退化 13.1% |

所有实例的最优值均与主版本一致。`mask_vertex_states` 统计首次发现的状态，因此删除旧 payload 不一定降低累计计数；Twitch 的主要收益确实来自存活 payload，而不是缩小搜索域。

### 结论

按每次 incumbent 改善触发全表扫描会用 `O(r g Z_D)` 的额外工作换取不稳定的存活空间下降，其中 `r` 是触发次数。Musae 在状态数完全相同时出现明确时间退化，违反正式方法的性能不退化底线。因此淘汰“每次严格改善即重滤”的实现及其 H-boundary 触发扩展，不合入主线。

随后单独实现了更窄的公共阶段边界版：Base 与 Enhanced 都只在 ordinary 完成后、A1 future 尚存时执行一次相同回收，把额外工作限制为 `O(g Z_D)`。该版本同样通过 5/5 CTest，但 Musae Base `g=15,q1` 为 620.430 秒、505.672 MiB、42,558,822 states；相对主线仍慢 7.4%，空间只下降 1.7%，累计状态完全相同。因此“一次性阶段边界回收”也淘汰，ordinary 全表重滤方向至此关闭。

<a id="history-redundant-leaf-pruning-negative-probe-20260809"></a>

## 非必要叶剥离负面探针（2026-08-09）

> 原始记录：`REDUNDANT_LEAF_PRUNING_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 方案

对路径生长得到的真实连通可行子图，递归删除不承担任何组最后一次命中的叶顶点及其唯一关联边。删除过程保持至少一个已选顶点命中每个组，并保持剩余非空核心连通；规则无图名、组数阈值、整数权假设或经验参数。候选通过 5/5 CTest。

### Gate

- 输入：`experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g16.txt` 第 2 条，Enhanced。
- 当前 pair-growth 上界为 46；叶剥离后仍为 46。
- 由于目标精确值为 39 且预处理上界没有任何改善，按预声明 gate 未进入 A1/ordinary 长尾。

### 结论与清理

目标路径并集已经没有可删除的非必要叶；额外扫描只增加工作区与线性后处理，不能缓解状态爆炸。该方向淘汰，隔离源码、构建和原始输出删除，只保留本文。

<a id="history-rooted-endpoint-floor-negative-probe-20260809"></a>

## Rooted endpoint-floor 负结果（2026-08-09）

> 原始记录：`ROOTED_ENDPOINT_FLOOR_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

现有 A1 endpoint-floor 固定一个 remaining-group 起点 $a$，使用从 $a$ 出发、终点自由且访问全部 remaining groups 的最短组度量路径下界 $P_a(R)$。候选再加入根到 remaining groups 的最近距离：

```math
L_{\mathrm{rooted}}(v,R)=
\frac{d_a(v)+P_a(R)+\min_{i\in R}d_i(v)}{2}.
```

这是 PrunedDP++ 第二类 tour 下界的常数时间松弛。树倍增与 shortcut 证明给出可采纳性；两个顶点距离项各乘二分之一，因此该函数仍为 1-Lipschitz，可同时用于 A1 cone 和 cone 外 fallback。方案不依赖整数权、图名、组数阈值或经验参数。

隔离实现比较了两种物理 realization：朴素扫描 remaining groups 求最近距离；以及为每个顶点保存前三近组的 12-bit 排名。A1 continuation 恰好只排除永久锚组和当前 singleton 组，故前三近组中必有一个仍在 remaining，top-three 与朴素扫描逐状态严格等值。两者均通过 5/5 CTest、随机配置精确性和零权边回归。

### Orkut 高组数 gate

数据为 Orkut `g=16,q4` Enhanced，主版本与候选结果如下：

| 版本 | 时间（秒） | 查询峰值（MiB） | states | 最优值 |
|---|---:|---:|---:|---:|
| 主版本 | 1,194.776 | 2,722.359 | 26,517,767 | 20 |
| rooted endpoint-floor | 1,189.288 | 2,722.559 | 26,517,056 | 20 |

候选只减少 711 个状态，约为 0.0027%；约 0.5% 的时间差属于单次并发运行波动，空间没有改善。top-three 与朴素版状态严格相同，因此在朴素 gate 判负后停止未完成的 top-three 运行。

### ordinary 扩展为何不运行完整 gate

完整 ordinary future 已计算 `TourLowerBound::At`。对固定起点 $a$：

```math
\min_b\{P_{ab}+d_b(v)\}
\ge
\min_b P_{ab}+\min_i d_i(v).
```

因此 rooted endpoint-floor 始终不超过现有完整 tour，下放到 ordinary/A/H 不可能增加任何剪枝，只会多做最近组查询。该扩展由严格支配关系直接淘汰，不需要用长实验重复证明零状态收益。

### 结论与清理

该证书数学安全、实现轻量，但在目标高组数实例上几乎完全被现有 farthest/tour/dual 组合覆盖，不能作为论文贡献，也不能解决状态爆炸。候选源码、构建和原始输出删除，只保留本结论。

<a id="history-row-layout-negative-probe-20260809"></a>

## Ordinary row 物理布局负结果（2026-08-09）

> 原始记录：`ROW_LAYOUT_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

候选让 ordinary `Row` 按精确字节成本在稀疏 `(vertex,value)` payload 与按顶点连续的 dense value payload 之间选择，并在 incumbent 严格改善时重新过滤已发布 row。选择公式只比较两种表示的实际字节数，不读取图名、查询编号、组数阈值或边权类型。

### 正确性

候选通过仓库 5/5 CTest、144 个确定性随机配置精确性实例、零权边回归，以及 22 个 SteinLib 已知最优实例。探针期间还修复了 sparse `IsBranchVertex` successor 路径的定位错误；该修复属于候选目录，最终若主线仍存在同类代码必须独立复核，不能因本候选整体淘汰而遗漏。

### 结果

以下均为同一服务器上的单线程方向探针，不是论文正式结果：

| 图与查询 | 配置 | 主版本时间 / MiB | 候选时间 / MiB | 状态结论 |
|---|---|---:|---:|---|
| Twitch `g=15`, q1 | Enhanced | 260.910 / 661.965 | 255.632 / 504.711 | 51,107,932 / 51,103,946，差异可忽略 |
| Orkut `g=15`, q1 | Enhanced | 10,241.960 / 3,758.242 | 10,227.231 / 3,225.754 | 两边均为 253,636,180 |
| Orkut `g=16`, q4 | Enhanced | 1,194.776 / 2,722.359 | 固定版重复为 1,176.912 / 2,722.559 | 两边均为 26,517,767 |
| Musae `g=15`, q1 | Base | 577.475 / 514.441 | 663.120 / 418.066 | 两边均为 42,558,822 |

Orkut `g=16`, q2 的相同 ordinary payload 在 sparse 表示中占 3,186,432,348 B，在 dense 表示中占 2,621,177,160 B，字节数下降 17.74%；两次进程高水位均值约下降 4.85%。但平衡顺序重复的平均时间由 1,508.924 秒增至 1,525.941 秒（+1.13%），未显示稳定时间收益。

### 结论

物理布局能够降低部分高组数查询的 payload 或 RSS，但没有减少搜索状态，并使 Musae Base 在相同状态数下明确慢 14.83%。这不是可接受的论文级稳健优化，也不能用 Enhanced 的空间收益掩盖 Base 退化。因此整个 adaptive dense layout 方向淘汰，不合入主线；最终清理其候选源码、构建目录和大体积原始输出，只保留本文档。

后续允许保留的是与物理编码无关的语义生命周期回收：只有当状态已被可采纳界证明无用，或其最后一个消费者已经结束时才删除 payload。该方向必须另行通过 Base/Enhanced 正确性与性能 gate。

<a id="history-screened-a1-tour-negative-probe-20260809"></a>

## 上包络筛选 A1 tour 负结果（2026-08-09）

> 原始记录：`SCREENED_A1_TOUR_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 动机与候选

此前无条件在每个 A1 候选上计算 all-endpoint floor 或完整 matched tour，会让 Musae/Twitch Base 明显退化。本轮先用常数时间上包络判断强证书是否可能达到当前 cutoff；只有可能时才支付强证书。筛选不是经验阈值，也不读取图名、运行时间或状态规模。

对 all-endpoint floor，令 $F(v,R)$ 为 remaining groups 的最远距离，令 $P_{\max}(R)$ 为终点自由路径 floor 的最大值，则：

```math
L_{\mathrm{all}}(v,R)
\le
\frac{F(v,R)+P_{\max}(R)}{2}.
```

对完整 matched tour，固定任一起点并选择路径长度最小的终点，可得：

```math
L_{\mathrm{tour}}(v,R)
\le
F(v,R)+\frac{P_{\max}(R)}{2}.
```

若 `partial + upper < incumbent`，真实强证书必然也不能拒绝当前候选，因此只用原 endpoint-floor；否则计算强证书。A1 cone 外 fallback 始终读取完整强证书，所以没有削弱其下界合同。两个候选均通过 5/5 CTest、随机配置精确性和零权边回归。

### Orkut `g=16,q4` Enhanced gate

| 版本 | 时间（秒） | 查询峰值（MiB） | states | 最优值 |
|---|---:|---:|---:|---:|
| 主版本 | 1,194.776 | 2,722.359 | 26,517,767 | 20 |
| screened all-endpoint | 1,191.908 | 2,730.559 | 26,517,767 | 20 |
| screened matched-tour | 1,183.157 | 2,722.559 | 26,466,994 | 20 |

all-endpoint 没有减少任何状态，并为平坦 endpoint 表增加约 8 MiB。matched-tour 减少 50,773 个状态，仅约 0.19%；单次并发运行的约 1% 时间差不能视为稳定加速，也远不足以改变 g15/g16 的数量级爆炸。

### 结论与清理

严格上包络确实避免了无条件强证书的灾难性固定成本，但目标实例上的新增剪枝仍过弱。继续做负对照面板只会证明一个不具备论文效应量的机制没有严重退化，不能让它成为有效贡献。因此两个候选均不合入主线；源码、构建和原始输出删除，只保留本结论。

<a id="history-triple-completion-scheduling-negative-probe-20260809"></a>

## 三块完成感知调度负面探针（2026-08-09）

> 原始记录：`TRIPLE_COMPLETION_SCHEDULING_NEGATIVE_PROBE_20260809.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

ordinary DP 到达三块闭合首次可能发生的层时，按互补三元组聚集相关 mask，希望依赖更早同时 ready 并提前收紧 incumbent。顺序由补集关系唯一确定，不改变状态、转移、界或配置语义。完整五项 CTest 通过。

### 正确 P2 门禁

| 数据 | 主实现：秒 / MiB / 状态 | 三块调度：秒 / MiB / 状态 |
| --- | ---: | ---: |
| Orkut `g=15`, q3 | 284.043 / 2689.770 / 30,674,269 | 288.443 / 2690.332 / 30,674,269 |
| Orkut `g=16`, q4 | 1190.457 / 2722.559 / 26,517,767 | 1186.129 / 2722.559 / 26,517,767 |

输入分别为正确的 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt` 与 `cross_g16.txt`。两项最优值一致，状态数逐项完全相同；时间变化方向相反且均很小。

### 结论

把相关 mask 调度得更近没有改变完整上界出现的有效时点，也没有缩小搜索域。该方向不具论文效应量，不进入主代码；候选源码、构建和原始输出删除，只保留本文档。

<a id="history-a1-seed-support-anchor-negative-probe-20260810"></a>

## A1 seed-support 永久锚负面探针（2026-08-10）

> 原始记录：`A1_SEED_SUPPORT_ANCHOR_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

永久锚的选择会改变共同 A1 与后续半格状态域。候选没有再使用组大小、外围性或图名启发式，而是对每个合法锚 `a` 和每个其他组 `i` 精确扫描 A1 的初始 seed 条件：只有 `d_a(v)+d_i(v)` 加共同的 farthest/endpoint-floor continuation 仍严格小于真实 incumbent 时才计数。选择总 seed 支撑最小的锚；并列时保留当前“初始化根最远组”的规范锚。

该目标直接对应算法第一指数层，不使用运行时间、组数阈值、整数权或经验参数；正确性不依赖锚的身份。代价是在完整距离图上额外执行 `O(g^2 n)` 次距离检查。隔离实现通过仓库 5/5 CTest，包括零权和随机精确性回归。

### 2. Orkut `g=15` 评分门

只运行正式 P2 q2 与最难 q5 的全部 15 个锚评分；得到评分后立即停止，不把半截运行冒充完整查询结果。

| 查询 | 当前规范锚及 seed | 最小 seed 锚及 seed | 变化 |
|---|---:|---:|---:|
| q2 | 组 1：28,189,557 | 组 9：27,335,364 | -3.03% |
| q5 | 组 8：18,389,171 | 组 8：18,389,171 | 0 |

q5 的当前规范锚已经是全部候选中的严格最优者；次优组 9 为 18,738,499，仍高 1.90%。因此候选在真正目标查询上只增加全锚扫描，不改变 A1、ordinary 或后续状态域。q2 有小幅结构信号，但它不能补偿 q5 的零收益，也不足以支持对全部 P1/P2 查询增加固定评分成本。

### 3. 决策与清理

候选淘汰。全锚评分、诊断输出和选择宏从源码删除，隔离构建与原始输出删除，只保留本文。后续不再通过扩大 anchor 候选枚举解决 q5；它的规范锚已经通过直接状态域代理验证为最优，瓶颈位于锚确定后的 lower certificate 与 ordinary 闭包。

<a id="history-causal-block-future-negative-probe-20260810"></a>

## 因果低层 block future 负面探针（2026-08-10）

> 原始记录：`CAUSAL_BLOCK_FUTURE_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选与安全边界

构造 ordinary 第 `s` 层时，全部 `s-1` 层已经完成。候选对每个当前 mask，在剩余非锚组中选择组度量 Hamilton-path 下界最大的 `s-1` 组 block。当前 row 的图闭包完全结束后，若该 block 的旧 row 在同一顶点保存精确值 `D(B,v)`，并且 `D(S,v)+D(B,v)` 已不能严格改善 incumbent，才从当前 payload 删除该状态。

`B` 与 `S` 不相交，任何从状态 `(S,v)` 完成全部组的 DP continuation 都必须覆盖 `B`，其新增代价不小于精确 rooted 值 `D(B,v)`。过滤发生在当前 Dijkstra 闭包结束之后，所以不把稀疏 row 缺项变成不一致 queue heuristic；旧 row 缺失该顶点时直接不使用证书。选择规则没有图名、组数阈值、整数权假设或经验候选数。候选与 tour local gather 组合后通过仓库 5/5 CTest。

### 2. Orkut `g=15,q3` 完整 A/B

候选和仅含 gather 的冻结对照固定到 CPU 10、11；两边答案一致且均为 38。

| 指标 | gather 对照 | causal-block + gather | 变化 |
|---|---:|---:|---:|
| 查询时间 | 281.543196 s | 283.871657 s | +0.83% |
| 峰值空间 | 2689.574 MiB | 2690.227 MiB | +0.653 MiB |
| `(mask,v)` 状态 | 30,674,269 | 30,673,883 | -386（-0.0013%） |
| ordinary 末 payload | 907,586 | 903,864 | -3,722（-0.41%） |

状态收益极弱，逐状态对旧 row 的二分读取反而形成净时间退化。q3 完整门已经足以判负，因此没有把候选带入需要先等待整个 size-2 层的 q5 长探针；两进程误入下一条 q4 的共同预处理后立即停止，未把它记录为结果。

### 3. 决策与清理

该方向淘汰。隔离宏、`UnrootedFloor` 选择接口、构建目录及未完成输出全部删除，只保留本文。后续不得把“选择更多 block”当作直接修复，因为单 block 已经几乎不命中，增加 block 只会增加随机 row 查找；若重新研究低层状态复用，必须先提出无需逐状态稀疏二分的新物理结构和更强命中证据。

<a id="history-component-cover-future-dominated-probe-20260810"></a>

## 子集零权分量覆盖 future 的支配性负结果（2026-08-10）

> 原始记录：`COMPONENT_COVER_FUTURE_DOMINATED_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选与证明

现有公共预处理已经在零权连通分量超图上计算全部 mask 的精确 set-cover DP，但主线只保留完整组集的全局下界。候选额外保留每个剩余组集 R 的最小覆盖分量数 c(R)。任何覆盖 R 的连通树在零权缩点图中至少包含 c(R) 个组承载分量，因而至少使用 c(R)-1 条正权边。若全图最小正边权为 w+，则 (c(R)-1)w+ 是对任意根都可采纳、且关于顶点为常数的 future。

该证书不依赖整数权、图名、组数阈值或经验参数。它只复用已经支付的 set-cover DP，额外空间为 O(2^g) 个单字节，热路径为一次 mask 查表。候选同时接入公共 A1、ordinary、forward 和对应的 adjoint prefix；5/5 回归测试全部通过。

### 实验

Orkut g=15,q3 的候选完整结果为 284.907 秒、权值 38、2,690.289 MiB、30,674,269 个状态。状态数与冻结对照完全相同；对照随后输出的 ordinary 逐 row 工作量也与候选逐项一致，说明该证书被现有 farthest、tour 或 directed-cut 证书支配。

目标 Orkut g=15,q5 的 1,200 秒硬门更早给出相同结论：候选 A1 仍为 25,188,706 个状态，第一批 ordinary 累计仍为 4,235,986 个状态；已经完成的 row 工作量依次为 135,290,444、151,467,469、144,272,605、138,631,509、150,754,666 和 169,395,640，与冻结版逐项相同。候选没有改变目标查询的搜索锥体。

### 决策

该方向因被现有证书支配而拒绝。虽然证明简单且预处理轻量，但它既不能缓解 q5 状态爆炸，也没有跨查询收益，不应为了增加论文名词而保留一次无效查表。候选源码、隔离构建和原始探针目录删除，只保留本文结论。

<a id="history-cost-guided-canonical-pivot-neutral-probe-20260810"></a>

## 代价引导规范拆分 pivot 的中性探针（2026-08-10）

> 原始记录：`COST_GUIDED_CANONICAL_PIVOT_NEUTRAL_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

普通状态的每个非平凡无序拆分只需枚举一次。固定任意一个 pivot，并要求 accumulator 一侧包含它，都会得到同一组无序拆分。本候选不再固定最低 bit，而是根据已经完成的稀疏 row 长度、branch 数和现有交集内核的确定性工作估计，在 mask 的全部 bit 中选择估计代价最小的 pivot。它不改变状态语义、候选拆分、上界、下界或经验参数。

### 结果

完整 Orkut g=15,q3 交换核心实验中，候选与固定 pivot 的答案、30,674,269 个状态及约 2,690 MiB 峰值逐项相同。CPU11 上二者分别为 286.089 秒与 286.578 秒；CPU10 上分别为 373.613 秒与 371.960 秒。约 30% 的跨核心差异远大于算法差异，候选没有可重复的完整时间收益。

Orkut g=15,q5 随后采用两轮 1,200 秒交换核心前缀门。CPU11 上候选完成到累计 row 203、固定 pivot 完成到 row 195；CPU10 上二者都完成到 row 97。相同 row 的累计状态完全一致，候选只可能改变同根交集的物理时间，不能降低状态或空间。单侧约 4% 的前缀差没有跨核心复现，也远不足以解决目标查询的数量级状态问题。

### 决策

该方向判为中性并拒绝。为每个 mask 额外评估全部 pivot 会增加一层拆分工作，却没有稳定完整收益；主线继续使用最低 bit 作为最简单、最容易证明和 review 的规范对称破除。候选源码、隔离构建及原始结果目录均删除，只保留本文结论。

<a id="history-d2-support-anchor-negative-probe-20260810"></a>

## Ordinary D2 支撑选锚负面探针（2026-08-10）

> 原始记录：`D2_SUPPORT_ANCHOR_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

永久锚组不会出现在 ordinary mask 中。候选因此不再优化 A1，而是对每个原始
组对统计 `d_i(v)+d_j(v)` 加 farthest/endpoint-floor continuation 仍可能
改善 incumbent 的顶点数。某组作为锚时会整体移除所有与它 incident 的 pair；
所以选择 incident support 最大的组，等价于最小化留下的 D2 seed 支撑。

评分为确定性的 `O(g^2n)` 扫描，不使用图名、查询编号、组数阈值、运行时间、
整数权或经验系数。它只用于能力门，没有改变锚或运行状态搜索。

### 2. Orkut `g=15` 评分

| 查询 | 当前锚 incident | 最佳锚 incident | 当前留下的支撑 | 最佳留下的支撑 | 理论减少 |
|---|---:|---:|---:|---:|---:|
| q2 | 组 1：18,525,840 | 组 6：21,500,538 | 122,520,663 | 119,545,965 | 2.43% |
| q5 | 组 8：14,372,934 | 组 13：15,939,798 | 99,674,847 | 98,107,983 | 1.57% |

q5 的 D2 代理最多只减少 1.57%，而现有 A1 全锚精确评分已经证明当前组 8 是
q5 的严格最优 A1 锚。切换到组 13 会用很小的 D2 潜在收益交换已知的 A1
退化，远不足以修复 oracle 最优上界下仍超过 10000 秒的状态缺口。q2 的
2.43% 上限同样不足以支持全部查询支付一次 `O(g^2n)` 固定扫描。

### 3. 决策

候选在只评分门判负，不实现锚切换、不进入完整查询。诊断宏、隔离构建和空结果
目录全部删除，只保留本文。后续不再通过扩大锚评分代理解决 q5；A1 与 D2 两种
直接状态域代理都已经表明，单一永久锚选择不是主要瓶颈。

<a id="history-double-radix-queue-negative-probe-20260810"></a>

## Double radix queue 负面探针（2026-08-10）

> 原始记录：`DOUBLE_RADIX_QUEUE_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

候选只把 ordinary 图闭包的 `std::priority_queue` 替换为 65 桶 radix queue。key 使用非负有限 `double` 的 IEEE-754 单调序位；桶按真实 Dijkstra distance 调度，未来下界仍在 seed、松弛和 pop 三处执行完全相同的可采纳筛选。A1、forward、adjoint、上下界、状态定义和 incumbent 均不改变。该实现不依赖整数权、桶宽超参数、图名或组数阈值，并通过仓库 5/5 CTest。

### 2. Orkut `g=15,q5` 成对结果

候选和冻结对照分别绑定 CPU 10、11，同时运行正式 P2 第 5 条查询。两边图加载、查询、增强配置、strict reverse residual closure、势转置和 certified complement 读取完全相同。候选提前约 6 秒启动，但在 closure 后逐渐落后一张 ordinary row，因此主动停止；两边都不是完整查询结果。

| 指标 | radix 候选 | binary-heap 对照 |
|---|---:|---:|
| `prepare_end` | 176.254 s | 173.624 s |
| closure 购买前 row / scalars | 23 / 26,515,090 | 23 / 26,515,090 |
| closure 购买前累计 work | 6,245,044,756 | 6,245,118,154 |
| closure 总时间 | 165.731 s | 161.779 s |
| refilter 后 scalars | 10,508,369 | 10,508,369 |
| 停止时完成 ordinary row | 34 | 35 |

radix 只减少 73,398 次累计 primitive work，即购买前总量的约 0.0012%。目标 row 的主导工作是邻接扫描和每顶点 future 计算，不是过期堆项；radix 的桶重分布和不同调度顺序反而留下轻微物理退化。两边对应 row 的状态数、best 和主要 work 逐项一致。

### 3. 决策与清理

该方向淘汰。它不减少状态，也没有形成物理加速，不应作为论文机制。源码宏、radix queue 类、隔离构建和 `/tmp` 未完成输出全部删除，只保留本文；后续不再把 ordinary binary heap 当作 Orkut `g=15,q5` 的主要瓶颈。

<a id="history-dual-primal-support-union-negative-probe-20260810"></a>

## Dual primal 支撑并集负面探针（2026-08-10）

> 原始记录：`DUAL_PRIMAL_SUPPORT_UNION_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

延迟 residual closure 会恢复一棵新的真实 primal 树，而初始 directed-cut 已经恢复过另一棵树。候选不覆盖旧 bitmap，而是把两棵树的原图边取并集，再让现有 BuildPrimalFacilityUpper 在联合设施点和允许边上执行完全相同的 subset DP。候选空间严格包含单支撑版本；所有路径仍按原图边权计价，因此只可能安全收紧上界。

该操作只放在已经购买 closure 的 DirectedCut 配置中，不读取图名、组数区间、边权类型或运行秒数。为避免等待完整长查询，诊断构建在公共 A1 后立即购买一次，只比较购买结果。

### Orkut g15 q5 结果

输入为 experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt 第 5 条，Enhanced。

| 指标 | 单支撑 closure | 初始树与 closure 树并集 |
|---|---:|---:|
| 购买后上界 | 35 | 35 |
| 购买耗时 | 177.071 s | 177.558 s |
| closure 后首张 ordinary row 标量 | 394,232 | 394,232 |

两版的首张 row 逐项状态规模相同。联合支撑没有产生新的有效 facility 组合，也没有改变 lower certificate；继续运行不能凭该操作获得状态收益。

### 结论与清理

候选数学安全但在目标最困难询问上零上界、零状态收益，因此不进入正式实现。临时 eager 环境变量、支撑并集代码、对应测试断言和原始结果目录均已删除；仓库只保留本结论，防止以后重复试验。

<a id="history-full-potential-root-screen-negative-probe-20260810"></a>

## 完整 dual 根可行性早筛负面探针（2026-08-10）

> 原始记录：`FULL_POTENTIAL_ROOT_SCREEN_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选与证明动机

residual closure 后已经缓存每个顶点的全组势和。对覆盖组集合 `S` 的 exact ordinary 状态，directed-cut 可行性给出其已付值不小于 `S` 的组势和，因此状态值加剩余组势不小于全组势。诊断先用 certified downward error 修正统计满足下式的顶点：

```math
L_{\mathrm{all}}(v)\ge U.
```

这些顶点在数学上不可能成为严格改善当前真实上界 `U` 的 rooted continuation。性能候选不增加证书或状态剪枝，只在原 `CanImproveAllExcept` 中先检查已缓存的全组势；若足以拒绝，就不再累加 covered 组势。closure 前没有转置势，候选分支完全不执行，因此 P1 未购买查询不承担该热路径工作。

### 2. 静态命中率

独立统计 twin 通过 5/5 CTest，并在 residual primal 恢复后扫描一次 Orkut 的 3,072,441 个顶点：

| 查询 | 当时上界 | 可独立排除顶点 | 比例 |
|---|---:|---:|---:|
| `g=15,q5` | 35 | 941,284 | 30.64% |
| `g=15,q2` | 36 | 555,521 | 18.08% |

q2 紧接着由 facility 把上界收紧到 34，因此 18.08% 只是该时点的保守比例。静态比例足以支持进入动态门，但它不能代表真正到达 `CanImproveAllExcept` 的顶点分布。

### 3. q5 动态门

性能候选与冻结旧二进制固定在相邻独立核心，同时运行正式 `GPU4GST_Orkut/cross_g15.txt` 第 5 条。两边在 row 23 购买相同的严格逆序 residual closure；每张对应 row 的 `best`、累计标量数和 `layer_work` 完全相同。

- size-2 层末均为 91 row、48,987,763 个标量和 10,099,274,991 primitive work。
- 到 row 91 的墙钟没有形成候选优势；旧对照在后续相同墙钟下稳定领先约 1--2 张 row。
- 候选和对照分别在约 row 96 与 row 98 的同一观测点主动停止，不是完整结果或 timeout。

静态排除的顶点多数已经被更早的 `next >= distance` 或已有 lower-bound 路径过滤；对真正进入该函数的存活调用，新增的误差乘法与分支反而成为固定成本。因此 18%--31% 的全图比例没有转化为热路径收益。

### 4. 决策

该方向淘汰。统计方法、提前检查、两个隔离构建和 `/tmp` 原始输出全部删除，只保留本文。后续不得仅凭全图顶点命中率在每状态路径增加筛选；必须统计实际到达位置或先给出能减少邻接扫描/状态数的结构机制。

<a id="history-group-reinsert-upper-negative-probe-20260810"></a>

## 单组删枝重插上界负结果（2026-08-10）

> 原始记录：`GROUP_REINSERT_UPPER_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

候选保留共同三元路径生长取得最好权值时的真实边集，先对边并集做确定性 Kruskal，再递归删除不承担任何必需组最后一次命中的叶枝。随后依次把每个组暂时设为非必需组，剥离因此变得多余的叶枝，并从剩余核心中距离该组最近的顶点恢复一条真实 tight shortest path；重新 Kruskal 和叶剥离后只接受严格降费的候选。每轮在全部组中选择最好严格改进，直到不存在改进。

整个过程始终维护一棵原图真实连通树及每个必需组至少一个命中，因此每个候选都是可行上界。严格降费保证有限终止；规则没有轮数限制、图名、组数阈值、整数权假设或经验参数。Base 与 Enhanced 可以调用同一实现。隔离原型通过 5/5 CTest，包括零权 witness 和 144 个独立全子集 DP 对照。

### 2. Orkut `g=15` 预处理门

输入为正式 P2 `cross_g15.txt`，配置为 Enhanced。只观察完整预处理后的真实上界；若不能收紧，就不进入长时间状态搜索。

| 查询 | 当前三元上界 | 删枝重插后 | 预处理时间 |
|---|---:|---:|---:|
| q2 | 36 | 36 | 176.950 s |
| q5 | 35 | 35 | 174.852 s |

两条目标查询均没有任何上界改善。q5 相对同时期约 173–174 秒主线预处理只增加约 1 秒，但零收益已经足以判负；该局部邻域无法改变路径生长所选共享骨架，继续扩大到双组或人为规定交换深度会引入新的搜索参数且没有正信号。

### 3. 决策与清理

该方向不接入主代码。隔离实现、构建和原始输出删除，只保留本文结论。后续若研究上界，必须改变候选支撑或联合终端选择机制，不能重复单组删除再最近重插的同一邻域。

<a id="history-incremental-closure-buy-mixed-probe-20260810"></a>

## Residual closure 增量购买价混合探针（2026-08-10）

> 原始记录：`INCREMENTAL_CLOSURE_BUY_MIXED_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

初始 directed-cut 构造已经完成且其势会被 closure 重用。候选把初始构造的 `initial_build_work` 视为沉没成本，只用真正尚未执行的 replay 与 completion 工作估计整包 closure 的购买价。原式与候选分别为：

```math
B_{old}=\max\{B_{static},B_{initial}+B_{closure}\}.
```

```math
B_{candidate}=\max\{B_{static},B_{closure}\}.
```

两式都没有图名、组数分段、边权类型或经验系数；closure 本身仍只增加可采纳势、真实 primal/facility 上界并按更强 future 重滤已有 ordinary row，因此答案正确性不变。候选仅改变整包证书的购买时刻。

### 2. Orkut `g=15` 正向端点

第 5 条查询中，候选把 closure 从 ordinary row 23 提前到 row 7。购买前有 7,781,702 个 payload，重滤后剩 3,076,070，删除 4,705,632 个；size-2 累计 primitive work 从 10,099,274,991 降为 9,107,386,841，下降 9.82%。同样 1200 秒、同一 CPU 11 的端点由旧版约 195 行推进到 234 行，约增加 20% 的 row 进度。

第 2 条查询中，候选在 row 5 购买，重滤把 7,170,243 个 payload 降为 903,120，并由 closure facility 把 incumbent 从 36 收紧到 34。1200 秒端点推进到约 393 行。两条困难查询说明更早的完整 residual potential 确实能大幅减少后续工作；收益不只是上界生成。

### 3. 无退化门禁

同一 Orkut `g=15` 第 3 条查询用候选与冻结对照固定到相邻独立核心完成端到端运行：

| 版本 | 时间（秒） | 权值 | 峰值 MiB | 状态 |
|---|---:|---:|---:|---:|
| 冻结对照 | 286.318036 | 38 | 2689.773 | 30,674,269 |
| 增量购买价 | 372.377817 | 38 | 2690.523 | 30,674,269 |

候选时间增加 30.06%，答案和状态逐项相同。该查询的一张 early ordinary row 扫描工作很大，足以支付较低购买价，但最终保留的 payload 很少；完整 closure 没有减少最终状态，因而只留下整轮图线性证书成本。搜索 work 可以衡量“已经租了多久”，却不能单独证明整包证书会产生足够剪枝。

### 4. 决策

该候选淘汰，正式源码恢复原 additive buy；探针宏、隔离构建和原始输出均删除。q2/q5 的正向结果只证明 closure 值得进一步拆分，不足以接受会让易查询退化 30% 的整包调度。

后续若继续该方向，应把 residual completion 拆成逐组独立可采纳增量：每组按自身真实 replay/shortest-path 成本购买并立即重滤，只有搜索继续支付后才购买下一组。这个方向的目标是让低收益查询最多承担一个增量组，而不是再次发明图名、经验组数阈值或观测墙钟的开关；在完成正确性证明和成对门禁前不得写入正式方法文档。

<a id="history-incumbent-half-edge-growth-negative-probe-20260810"></a>

## `U/2` ordinary 边生长负面探针（2026-08-10）

> 原始记录：`INCUMBENT_HALF_EDGE_GROWTH_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选与正确性边界

候选采用 *Efficient and Progressive Group Steiner Tree Search* 的 optimal-tree decomposition：ordinary 状态结算后仍完整保留并参与全部同根合并，但已付值严格超过当前真实可行上界一半时不再沿原图边继续生长。一步跨过半界的状态仍会产生，因此没有把条件错误地改写为状态删除；候选也没有接入需要重新适配永久锚组与半格完成式的 `2U/3` 条件合并。

该规则不读取图名、组数区间、边权类型或经验参数，并通过 5/5 CTest，包括零权 witness 与 144 个独立全子集 DP 对照。

### Orkut `g=15,q5` 门禁

候选与主线预处理上界均为 35，A1 均为 25,188,706 个标量。residual closure 仍在 row 23 购买：购买前 26,515,090 个状态，重滤后 10,508,369 个状态，候选闭包总时间 164.725 秒，与主线约 163.7 秒同量级。

继续到累计 row 50，候选与主线均为 21,543,722 个状态；closure 后逐 row 的 primitive-work 也逐项相同。closure 前仅个别 row 少几十至约一百次邻接检查，相对单 row 上亿次工作不足百万分之一。原因是现有 `CanImprove` 已组合 directed-cut、farthest、tour 与 A1 future，超过半界且仍能通过这些证书继续传播的状态几乎不存在。

### 决策

候选数学安全且与 baseline 规则公平，但在目标劣势查询上被现有下界支配，不能形成可测收益，只会增加热循环分支。实现、隔离构建与半截输出删除，只保留本文，后续不重复测试同一 `U/2` 边生长规则。

<a id="history-oracle-optimal-upper-p2-negative-probe-20260810"></a>

## Orkut `g=15,q5` 精确 oracle 上界负面探针（2026-08-10）

> 原始记录：`ORACLE_OPTIMAL_UPPER_P2_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 问题

该探针只回答：如果预处理结束时已经免费知道查询精确值 32，现有搜索能否在 10000 秒内完成证明。隔离副本把 incumbent 强制设为 32，不改变任何下界、状态转移、row 布局或完成阶段；这是不可用于正式算法的 oracle，只给“继续改进可行上界”建立能力上限。

查询为 `GPU4GST_Orkut/cross_g15.txt` 第 5 条，正式 Enhanced 配置。启动时间为 2026-08-10 06:16:17，图加载 30.472 秒，查询前进程 RSS 基线为 6391.566 MiB。进程固定单核并由 `timeout -k 10s 10000s` 控制。

### 2. 结果

10000 秒硬超时，查询没有完成。超时时：

- incumbent 始终为精确值 32；
- size-4 的全部 1001 张 row 已经完成；
- 累计完成 2177 张 ordinary row，即进入 size-5 后完成 721/2002 张；
- 当前保留 569,913,251 个标量状态；
- 最后一次只读采样的进程 RSS 约为 13.8 GB，但查询未正常收尾，不能把它当正式峰值空间记录。

因此，即使给算法一个零成本、零延迟且不可能再改进的最优上界，当前 lower-bound/state 结构仍不能使该查询在 10000 秒内完成。真实上界生成器只可能比这个 oracle 更贵或更晚得到 32，所以“继续寻找更早的 32 上界”不能单独达成目标。

### 3. 决策

停止把 Orkut `g=15,q5` 的核心缺口归因于 incumbent=35。后续候选必须直接加强可采纳下界、减少 row 状态，或改变等价状态的物理处理；上界候选只有在同时带来独立结构收益时才值得测试。oracle 注入代码、隔离构建和未完成的原始输出全部删除，本文件只保留能力上限结论。

<a id="history-ordered-row-bitmap-negative-probe-20260810"></a>

## ordinary row 有序位图物化负面探针（2026-08-10）

> 原始记录：`ORDERED_ROW_BITMAP_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

ordinary 图闭包仍按原优先队列精确 settle；候选只把实际 settle 顶点写入两级原生 64-bit 位图，结束后按高层 word、低层 word、vertex bit 的顺序线性恢复严格递增且去重的 row。它替换原来的 `sort + unique`，不改变 seed、队列顺序、剪枝、状态值或 row 消费者。额外空间为 `O(n/64)` bit，扫描工作为 `O(s+n/4096)`，其中 `s` 是 row 中的不同 settle 顶点数；常数 64 来自机器字宽，不是实验超参数。

候选通过仓库 5/5 CTest。Orkut `g=15,q5` 的每张对应 row 均保持完全相同的 `best`、累计标量数和 `layer_work`。

### 2. P1 固定成本门

无位图和有位图二进制在同一 CPU 上相邻顺序运行 Youtube `g=7` 前 20 条作者查询。时间为 20 条逐查询求和；状态逐条核验一致。

| 配置 | 无位图时间（s） | 位图时间（s） | 变化 | 状态数 | 峰值 MiB（无/有） |
|---|---:|---:|---:|---:|---:|
| Enhanced | 141.295559 | 142.143515 | +0.60% | 4,896,171 | 217.539 / 217.520 |
| Base | 128.142036 | 129.095142 | +0.74% | 5,178,297 | 118.305 / 119.223 |

小 row 的排序成本很低，强制位图只留下额外置位与层级扫描工作。虽然时间差接近单次运行噪声，它没有满足 P1 完美接入所要求的明确非退化信号。

### 3. Orkut 高组门

输入为正式 P2 `GPU4GST_Orkut/cross_g15.txt` 第 5 条，Enhanced。候选与无位图对照固定在相邻独立核心；两者都启用相同的 residual 严格逆序闭包、势转置和 certified complement 读取。

- size-2 层末均为 91 row、48,987,763 个标量；候选约 15 分 23 秒到达，未优于此前约 15 分 09 秒的无位图记录。
- row 188 均为 79,665,791 个标量；候选约 19 分 50 秒到达，与无位图的 19--20 分钟区间相同。
- 候选到 row 203 与对照到相同区域的进度曲线没有分离；每张 row 的 `layer_work` 逐项相同。
- 候选在 row 203 后主动停止，对照在完成同位置核验后主动停止；二者都不是完整查询结果或 timeout 记录。

图闭包每张 row 仍需约数千万到一亿多次 queue/邻接工作，末尾排序不是目标查询的主导成本。因此即使改成按渐近工作量自动选择排序或位图，也只能消除 P1 固定税，无法创造 Orkut 收益。

### 4. 决策

该方向淘汰。位图宏、实现、隔离构建和 `/tmp` 原始输出全部删除，只保留本文。后续不再把 row 末尾排序当作 Orkut 状态爆炸的主要物理瓶颈；应直接加强可采纳下界、减少状态，或优化占主导的队列/邻接处理。

<a id="history-ordinary-pair-partition-upper-negative-probe-20260810"></a>

## Ordinary pair-partition 上界负面探针（2026-08-10）

> 原始记录：`ORDINARY_PAIR_PARTITION_UPPER_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选与正确性

候选在全部 singleton 与 size-2 ordinary row 完成后，选择一批已经由现有 primal 证书出现过的根。对每个根，把非锚组划分为 singleton 或 pair：singleton 使用该组到根的精确最短路，pair 使用已经求得的精确 `D({i,j},root)`；再加锚组到根的精确最短路。每一块都能展开为包含共同根的原图真实连通子树，故各块真实边并集连通并覆盖全部组，其去重权值不超过分块值之和。这是安全可行上界，不参与下界或精确状态域。

分块用无参数 subset DP 求最小值，没有读取图名、运行时间、边权类型或组数区间。测试了两档同源根集合：

1. 只用当前 witness 树顶点；
2. 使用 witness 树、规范 root-path union 以及 directed-cut primal 树中出现过的全部顶点。

第二档严格包含第一档，仍只复用算法已有证书，不做随机抽样。

### 2. Orkut `g=15,q5` 门禁

该查询精确值为 32；reverse residual closure 后 incumbent 为 35。两次隔离探针都运行到 size-2 层末立即判定，不继续等待无价值的 size-3/4 长跑。

| 根集合 | 根数 | 上界求值时间 | 求值后 incumbent | size-2 累计 row | size-2 累计标量状态 |
|---|---:|---:|---:|---:|---:|
| witness 树 | 47 | 0.012616 秒 | 35 | 91 | 48,987,763 |
| 全部已有 primal 证书顶点 | 93 | 0.046870 秒 | 35 | 91 | 48,987,763 |

拓宽根集合没有得到 32，也没有改变进入 size-3 前的状态。原因不是实现代价：即使在 Orkut 与 `g=15` 上，求值也不到 0.05 秒；结构性原因是 singleton/pair 分块只能在块内共享边，而真实优良 GST 需要三个及以上组跨块共享主干，分块求和会重复计价这些共享边。

### 3. 决策

候选淘汰。虽然物理代价很小且正确性清楚，但它在目标劣势询问上没有任何上界或状态收益；若保留，则 P1 的每条 `g>3` 查询都会无条件支付一次固定成本，与“小组数不劣于 PrunedDP++”的目标相冲突。正式源码不保留该函数、调用点或诊断事件，隔离构建和原始输出均删除；本文件只保留可复核的负面结论，防止后续重复尝试同一种 pair 分块上界。

<a id="history-original-graph-facility-upper-negative-probe-20260810"></a>

## 原图设施度量上界负面探针（2026-08-10）

> 原始记录：`ORIGINAL_GRAPH_FACILITY_UPPER_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

延迟 residual closure 恢复新的 primal 树后，已有设施 subset DP 原本只允许 residual 零弧支撑与 primal 树边。该候选保留完全相同的设施点、组到设施距离和 subset DP，只把设施间距离替换为原图最短路距离。每个候选仍可展开成原图真实路径的连通并集，因此是正确的可行上界。

该变化不读取图名、组数阈值、边权类型或运行时间，但需要从每个设施点在原图上执行一次截断 Dijkstra，代价明显高于 residual-support 度量。

### 2. Orkut $g=15$ q5

使用 additive buy 与逆序 residual completion：

- 初始预处理 173.468 秒，incumbent 为 35。
- closure 在 ordinary 23 行、26,515,090 个累计标量时购买，paid rent 为 6,245,118,154 primitive work。
- residual completion 与 primal 恢复耗时 151.516 秒，incumbent 仍为 35。
- 原图设施度量和 subset DP 额外耗时 38.696 秒，incumbent 仍为 35。
- 一次性 refilter 将当前 ordinary payload 从 26,515,090 降至 10,508,369，但这是新 dual 下界的作用，不是原图设施上界收益。
- 整次购买耗时 203.038 秒；设施上界没有带来任何状态剪枝。

### 3. 结论

该候选在目标劣势查询上产生确定的 38.7 秒额外成本，却没有把上界从 35 收紧，因此拒绝。最终实现恢复 residual-support 设施度量；原图域枚举、临时二进制和半截输出均不保留。

closure 后的 primal 设施集与初始 primal 设施集可能不同，因此又做了一次只到预处理结束的隔离探针：初始设施的 residual-support 主线预处理约为 173.5 秒，改为原图度量后为 210.932 秒，incumbent 仍为 35。第二棵设施树同样没有收益，并新增约 37.5 秒固定预处理。

两次独立设施集均判负，因此该方向彻底结束，无需再消耗 P1 门禁。后续优化应使用已经生成的 exact partial states 构造新的可行组合，而不是继续扩大同一设施度量域。

<a id="history-path-growth-witness-reuse-negative-probe-20260810"></a>

## 路径生长证书复用为 witness 的负结果（2026-08-10）

> 原始记录：`PATH_GROWTH_WITNESS_REUSE_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

共同三元路径生长上界原本只返回真实可行树的权值。本候选同时保留取得最好上界的真实边集，并在其顶点数严格少于当前配置 witness 时，把该边集重根为 `WitnessTree`，交给已有的统一 rent-or-buy 调度器。候选不新增树 DP、不修改购买公式，也不使用图名、组数阈值、边权类型或经验参数。

边集转树的隔离原型最初无条件插入旧 fallback root；当路径证书命中锚组的另一个终端时，该 root 会成为孤立点。Orkut `g=15,q2` 触发了这个实现边界。探针随后改为只在空边集时插入 fallback；非空证书从自身命中的锚组终端确定根。该错误从未进入主工作树。

### 2. Orkut 短门

输入为正式 P2 `cross_g15.txt` 的 q2 与 q5，配置为 Enhanced，运行固定在两个独立核心。主线与候选使用相同的三元路径上界；差异只有是否把其边证书替换为更小 witness。600 秒诊断门只比较预处理上界、A1、ordinary 前缀和条件式树 DP，不冒充完整运行。

| 查询 | 主线 witness 顶点 / buy | 候选 witness 顶点 / buy | 上界 | 状态与购买结果 |
|---|---:|---:|---:|---|
| `g=15,q2` | 33 / 236,756,949 | 26 / 186,535,778 | 36 / 36 | A1 均为 29,132,133 个标量；ordinary 前缀轨迹相同，多次购买均未收紧上界 |
| `g=15,q5` | 47 / 337,199,291 | 33 / 236,756,949 | 35 / 35 | A1 均为 25,188,706 个标量；观察到 40,501,654 个 ordinary 标量时轨迹仍相同，多次购买均未收紧上界 |

较小 witness 线性降低单次树 DP 的估计工作，却也按同一公式更早达到 buy。q5 因而从大约每三张新 row 购买变成大约每两张购买；在没有 incumbent 改善时，总购买工作没有形成稳定收益。两个强查询均没有减少主状态，远不足以支持 `g=15` 全部查询进入 10,000 秒的目标。

### 3. 决策与清理

该方向判负，不接入主代码。失败实现、构建目录和原始 `/tmp` 输出删除，只保留本文结论。三元路径上界继续只返回权值；后续优化应直接增强可采纳下界或产生更紧的真实可行上界，而不是仅更换 witness 的物理骨架。

<a id="history-residual-anchor-first-order-negative-probe-20260810"></a>

## Residual 永久锚优先顺序负面探针（2026-08-10）

> 原始记录：`RESIDUAL_ANCHOR_FIRST_ORDER_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

当前单证书按初始 far-to-near 顺序的严格逆序完成 residual 势。候选利用永久锚存在于每个 ordinary continuation 的结构，把规范锚移到 completion 首位，其余组仍保持严格逆序。它不读取图名、组数区间、时间、状态规模或边权类型，不增加证书数量、理论空间或每状态势读取。

任意固定顺序逐次只扣除尚未使用的 residual 容量，因此候选仍是可采纳证书，并通过仓库 5/5 CTest。

### Orkut g=15, q5 早停门

候选与主线具有相同的 A1 结果和购买价：A1 为 25,188,706 个标量，incumbent 为 35，residual closure buy 为 6,206,733,889 primitive work。主线严格逆序完成 closure、primal、facility 与 refilter 共约 163.7 秒。

候选改变组序后，residual Dijkstra 的实际传播显著增加。进程运行到 588 秒时仍未完成 closure；扣除约 30 秒图加载以及 closure 前的共同预处理和 A1 后，closure 已连续运行超过主线的两倍时间。此时进程 RSS 约 9.69 GiB，尚未得到可以与主线 10,508,369 个 refilter 后状态比较的输出。按预登记的构造成本门主动停止，不能把未完成运行写成查询 timeout 或性能结果。

### 决策

永久锚优先在理论调用种类不变的情况下仍显著放大了 residual 最短路的实际堆工作，已经违反性能不退化门。候选淘汰，源码恢复严格逆序；隔离构建与半截输出删除，只保留本结论。后续不得仅凭“锚出现在所有 continuation”再次移动 residual 组序，除非先给出不会增加证书构造工作的独立机制。

<a id="history-residual-closure-same-order-p2-probe-20260810"></a>

## Residual closure 同序完成探针（2026-08-10）

> 原始记录：`RESIDUAL_CLOSURE_SAME_ORDER_P2_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 探针目的

该中间态保存初始 directed-cut 的 changed-arc 位图，并在 ordinary 工作达到旧静态购买价后重建 residual。各组剩余势仍按初始 farthest-to-near 顺序完成，完成后恢复一棵新的 primal 树。它用于判断“只补全同一对偶证书”能否解决 Orkut $g=15$ 的状态爆炸，不是最终实现。

### 2. Orkut $g=15$ 结果

- q2 完整结束：6578.591772 秒，最优值 32，查询峰值 5669.066 MiB，429,143,659 个状态。历史版本为 25,635.596165 秒、15,342.992 MiB、1,264,668,489 个状态，因此 residual closure 对该查询有明确价值。
- q5 的旧购买价为 3,796,000,000 量级的 primitive work，closure 在 size-2 早期执行，耗时约 155.6 秒；primal/facility 上界仍为 35，没有收紧 incumbent。
- q5 的 size-2 最终 payload 为 53,586,095，size-3 最终 payload 为 204,569,731。继续到 2 小时 9 分后仍位于 size-4 中段，累计 1297 行、471,561,698 个标量，incumbent 仍为 35。
- size-4 尚有数百个 mask 行未完成，按已观察的逐行工作量，在 10,000 秒总时限内不可能完成，因此主动终止；该终止不是完成查询，不能进入正式结果矩阵。

### 3. 结论

同序 residual closure 能显著改善 q2，但不能单独使 q5 在 10,000 秒内完成。其主要缺口不是 closure 计算本身，而是 closure 后仍得不到 32 的早期可行上界，导致 size-4 继续保留大量状态。

该中间态还采用过早的静态购买价，在 DBpedia P1 困难查询上会产生约 24% 时间退化。最终候选必须改用由初始证书实际工作和第二份证书不可避免结构扫描共同给出的 additive buy；P1 未购买时应保持相同状态数和噪声级时间变化。

后续只保留本结论，不保留半截 q5 `weights.txt`，也不把主动终止伪装为 timeout 或完成记录。

<a id="history-residual-dual-order-ensemble-complement-retest-negative-probe-20260810"></a>

## Residual 双顺序证书在补集布局下的复核负面探针（2026-08-10）

> 原始记录：`RESIDUAL_DUAL_ORDER_ENSEMBLE_COMPLEMENT_RETEST_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 为什么允许复核旧负面方向

旧双顺序证书会在每个普通状态上分别按剩余组扫描两份按组布局的势；Orkut
`g=15` 的 size-2 状态因此读取约 26 个势值。当前单证书候选已经具备购买后
按顶点转置和经浮点区间认证的“全组和减已覆盖组和”布局。双证书在 size-2
只需读取两份证书各两个已覆盖组，共约四个势值。物理成本模型发生了实质变化，
因此本轮只复核一次；数学候选仍是两份完整可行证书的组和取最大值，绝不逐组
拼接证书。

### 2. 正确性门禁

初版复核实现被配置回归拦截：保留的 residual 对应第二份证书，而公开
`GroupAt` 对应第一份证书。候选通过交换主辅证书身份修复合同，不复制
`O(m)` residual。修复后候选和宏关闭构建均通过 5/5 CTest。该修复只属于
已淘汰候选，最终单证书源码不保留任何双证书字段或分支。

### 3. Orkut `g=15,q5` 阶段对照

候选与冻结单逆序证书对照分别固定在 CPU 10 和 11，查询上限 1800 秒。两个
CPU 位于不同 NUMA socket，候选/对照共同预处理分别为 238.385/179.891 秒，
因此不把查询总墙钟当作严格 A/B；购买位置、状态和购买后的阶段吞吐仍可审计。

| 指标 | 单逆序证书 | 原序+逆序双证书 |
|---|---:|---:|
| buy work | 6,206,733,889 | 8,679,570,609 |
| 购买位置 | row 23 | row 40 |
| 购买前状态 | 26,515,090 | 45,171,129 |
| refilter 后状态 | 10,508,369 | 17,201,080 |
| closure/primal 时间 | 154.164 秒 | 420.575 秒 |
| closure 总时间 | 161.455 秒 | 434.761 秒 |
| closure 期间观测 RSS | 约 8.63 GiB | 约 10.74 GiB |

所有购买里程碑和状态数都复现旧探针。新补集布局确实降低了双证书购买后的
单状态势读取量，但没有消除第二份证书的构造与热路径。相同后续观察窗口中，
双证书仅约从 row 40 推进到 row 65，单证书约从 row 219 推进到 row 268。
双证书既没有追回 273.306 秒的额外 closure，也没有表现出更高 row 吞吐。

### 4. 决策

复核仍判负并提前停止，不等待 1800 秒硬截止。双证书无法使目标 q5 接近
10000 秒完成，且增加 closure 时间、临时 RSS 和每状态常数。源码、构建和
半截输出全部删除，只保留本文。除非未来出现不需要物化第二份 `g×n` 势且
不增加每状态证书读取的新数学表示，否则不得再次枚举更多组顺序证书。

<a id="history-residual-dual-order-ensemble-negative-probe-20260810"></a>

## Residual 双顺序证书负面探针（2026-08-10）

> 原始记录：`RESIDUAL_DUAL_ORDER_ENSEMBLE_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选与正确性

单证书主线从初始截断 directed-cut 的同一 residual 检查点出发，按初始组序的严格逆序完成剩余势。候选从同一检查点独立构造两份完整证书：一份按原序完成，一份按逆序完成。对任意 `(mask,v)`，分别求两份证书在 mask 内的组势总和，再取两者较大值。

每份证书各自从完整原始容量开始，并在自己的组顺序中逐次扣除势梯度，因此分别满足全部有向弧容量约束。两份总和的最大值仍是可采纳下界。实现绝不逐组取最大后相加，因为那会把两份全局证书非法拼接。候选没有图名、组数阈值、整数权假设或经验参数；购买价把两次 residual 重建与势闭包都计入，第二份 `g×n` 势只在购买后分配。

候选通过仓库 5/5 CTest，包括 144 个确定性随机精确性实例与零权边回归。

### 2. Orkut `g=15,q5` 对照

查询精确值为 32，两种实现的 incumbent 均保持 35，所以差异只来自下界。两条 probe twin 固定在同一 CPU，使用相同查询、编译选项与 2400 秒上限。

| 指标 | 单逆序证书 | 原序+逆序双证书 |
|---|---:|---:|
| buy work | 6,206,733,889 | 8,679,570,609 |
| 购买位置 | row 23 | row 40 |
| closure/primal 时间 | 150.249 秒 | 304.248 秒 |
| closure 总时间 | 163.703 秒 | 329.042 秒 |
| 购买时 refilter 前状态 | 26,515,090 | 45,171,129 |
| 购买时 refilter 后状态 | 10,508,369 | 17,201,080 |
| size-2 层末状态 | 48,987,763 | 41,227,395 |
| row 143 状态 | 67,018,444 | 59,165,707 |
| row 188 状态 | 79,665,791 | 71,775,625 |
| 到达 row 188 的墙钟 | 27 分 53 秒 | 40 分 00 秒（timeout） |
| row 188 附近进程 RSS | 约 8.90 GB | 约 9.23 GB |

双证书在 size-2 层末比单证书少 15.84% 状态，在 row 188 少 9.90%，证明两个顺序并非重复证书。然而，它需要约两倍 closure 时间，且购买后的每次 `At(mask,v)` 都要读取和累加两份势；最终到达同一 row 慢 43.5%，并额外保留约 0.33 GB RSS。剪枝收益不足以抵消构造和热路径成本。

作为负向检查，曾在另一核启动双证书 `g=15,q2` 正式候选；q5 同位置判定已经证明净退化后立即停止，没有把未完成 q2 当作结果。

### 3. 决策

候选淘汰，正式源码恢复单逆序 residual closure。双证书实现、构建目录和所有未完成输出删除，只保留本文。后续不得仅靠增加更多组顺序形成证书 ensemble：该方向虽能减少状态，却会线性增加闭包构造、势空间和每状态下界读取，已经在目标劣势查询上显示负净收益。

<a id="history-residual-least-paid-order-negative-probe-20260810"></a>

## Residual least-paid-first 顺序负面探针（2026-08-10）

> 原始记录：`RESIDUAL_LEAST_PAID_ORDER_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

单逆序主线按初始 far-to-near 组序的严格逆序完成 residual 势。候选保持同一 residual 检查点、同一势闭包、同一 buy 和同一空间，只改变延迟闭包的确定性组序：按第一阶段各组在规范根上实际取得的势从小到大排序，优先让“欠支付”组使用剩余容量；并列按组号。该顺序不读取图名、组数区间、时间、状态规模或边权类型，可以解释为一次离散 water-filling。

任意固定组序逐次只扣除尚未使用的 residual 容量，因此正确性不依赖顺序。候选通过仓库 5/5 CTest，包括 144 个确定性随机精确性实例和零权边回归。

### 2. Orkut `g=15,q5` 早停门禁

两种顺序都在 row 23 购买，incumbent 都为 35，closure 时间和 buy work 基本相同：

| 指标 | 严格逆序主线 | least-paid-first |
|---|---:|---:|
| buy work | 6,206,733,889 | 6,206,733,889 |
| closure/primal 时间 | 150.249 秒 | 150.408 秒 |
| closure 总时间 | 163.703 秒 | 163.768 秒 |
| refilter 前状态 | 26,515,090 | 26,515,090 |
| refilter 后状态 | 10,508,369 | 15,184,576 |
| 删除状态 | 16,006,721 | 11,330,514 |

least-paid-first 在第一次可比较位置就比逆序多保留 4,676,207 个状态，即多 44.5%。两者成本相同，故不存在依靠后续摊销翻转这一劣势的合理信号，探针在 refilter 后立即停止。

根上总势只描述一个顶点和 full-query 的支付情况，不能代表 ordinary 中所有 `(mask,v)` 的证书需求。按该标量重新排序会把 residual 容量优先分给对全体状态不利的组；严格逆序对原始优先级的系统性补偿更稳健。

### 3. 决策

候选淘汰，源码恢复严格逆序。隔离构建和未完成输出删除，只保留本文；后续不得把规范根上的单一支付统计重新包装成 residual 组序启发式。

<a id="history-residual-single-demand-radius-negative-probe-20260810"></a>

## Residual single-demand radius 负面探针（2026-08-10）

> 原始记录：`RESIDUAL_SINGLE_DEMAND_RADIUS_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选与证明

设第一阶段 directed-cut 势为 $p_i$，扣除全部组势梯度后的非负有向剩余容量为 $r$。对每个组独立地在同一张冻结 residual 上计算从顶点到该组的精确有向距离。每个顶点只保留残余距离最大的一个组及其值；若 ordinary future 的 remaining mask 仍含该组，则候选下界为原 directed-cut 组势总和再加这一条 residual 距离，否则只用原组势总和。

对任意从根顶点出发的可行补全树，第一项只使用已经分配的有向边容量；树还必须包含一条到被选组的路径，该路径在未分配 residual 上的费用不小于上述精确距离。逐弧的“已分配容量 + residual”不超过原边权，因此相加仍是可采纳下界。每个顶点选择残余最远组是唯一的 top-1 single-demand 证书，不读取图名、组数区间、边权类型或运行表现。

原型只增加一个 `double` 和一个组号字节的顶点数组；购买后每次 future 只多一次 bit 检查。冻结 residual 上的 $g$ 次距离修复不继续扣减容量，因而比完整顺序闭包更轻。原型通过仓库 5/5 CTest；这些小测试没有触发长查询的延迟购买，所以性能门禁仍是必要证据。

### 2. Orkut `g=15,q5` 早停门禁

候选与严格逆序主线都在 row 23 购买，incumbent 均为 35：

| 指标 | 严格逆序 closure | single-demand radius |
|---|---:|---:|
| 证书构造时间 | 150.249 秒 | 88.995 秒 |
| closure 总时间 | 163.703 秒 | 101.154 秒 |
| refilter 前状态 | 26,515,090 | 26,515,090 |
| refilter 后状态 | 10,508,369 | 26,515,090 |
| 删除状态 | 16,006,721 | 0 |

候选虽节省约 61 秒构造，但没有拒绝任何状态，说明这条 single-demand residual 路径在目标查询上被现有 farthest、tour 或初始 dual 证书完全支配。完整逆序闭包能把 residual 容量分配给多个组并形成可相加组势，强度不可由一条剩余路径替代。

### 3. 决策

候选淘汰。探针在第一次 refilter 后立即停止；隔离源码、构建和输出删除，只保留本文。后续不把该方向扩展为 top-k 缓存，因为 k 会成为没有理论边界的工程参数，而 top-1 已在最有利的早期 remaining mask 上显示零状态收益。

<a id="history-reusable-ordinary-heap-negative-probe-20260810"></a>

## Ordinary 可复用二叉堆负面探针（2026-08-10）

> 原始记录：`REUSABLE_ORDINARY_HEAP_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

候选保持 `QueueNode`、比较器和二叉堆 push/pop 顺序不变，只在 `BuildOrdinaryRows` 生命周期内对底层 `vector` 执行 `clear` 并保留容量，避免每张 row 重新分配。候选无参数、无图或权重特化，并通过 5/5 CTest。

YouTube P1 `g=7` 前 20 条中，Enhanced heap-off/on 为 142.192836/142.524403 秒（+0.23%），状态均为 4,896,171；Base 为 129.469204/129.892807 秒（+0.33%），状态均为 5,178,297。Base 峰值从 114.809 增至 118.785 MiB。

Orkut `g=15,q5` 的全部状态与 primitive-work 逐行相同；size-2 层末均为 91 row、48,987,763 状态，heap-on 用时约 15 分 09 秒，与 heap-off 约 15 分钟没有可测差异。继续到 row 117 仍同为 58,511,812 状态且没有速度分离。

系统分配器已经有效复用相邻 row 释放的相似容量；显式持有只把最大堆容量延长到 ordinary 结束，没有提供净收益。候选淘汰，源码、隔离构建和半截输出删除，只保留本文。

<a id="history-rooted-component-cover-future-negative-probe-20260810"></a>

## Rooted component-cover future 负结果（2026-08-10）

> 原始记录：`ROOTED_COMPONENT_COVER_FUTURE_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选与证明

把原图零权连通分量收缩。预处理原本已经为全部组 mask 计算最少需要选择多少个终端分量的精确 set-cover DP，但主线只保留 full mask 的全局下界。本候选保留整张 `2^g` cover 表，并为每个顶点保存其零权分量可以免费命中的 16-bit 组 mask。

对根顶点 `v` 和剩余组 `R`，先删去 `v` 所在零权分量已经覆盖的组，再令 `c(v,R)` 为覆盖其余组至少需要选择的外部分量数。若全图最小正边权为 `w_min`，则任何从 `v` 出发的补全树在零权缩点图中至少需要 `c(v,R)` 条正边，所以

```math
L_{\mathrm{cc}}(v,R)=c(v,R)w_{\min}
```

是可采纳下界。零权边两端的免费 mask 相同；跨一条正边时，两端 cover 数之差至多为 1，而 `w_min` 不大于该边权，因此该下界也是 1-Lipschitz，可以安全加入共同 A1 cone。它不依赖整数权、图名、组数阈值或经验参数。

候选新增空间为每顶点 16 bit，加 `O(2^g)` 个 byte；正权图只遍历查询终端建立免费 mask，含零权边时复用已有并查集向全图传播。实现同时接入三个配置共同的 A1 continuation 和 ordinary future，并通过 5/5 CTest，包括零权 witness 与 144 个独立全子集 DP 对照。

### 2. Orkut `g=15` 短门

输入为正式 P2 `cross_g15.txt`，配置为 Enhanced，q2/q5 固定在两个独立核心。主线已经采用共同三元路径上界；候选只增加上述下界。

| 查询 | 指标 | 主线 | 候选 |
|---|---|---:|---:|
| q2 | 预处理上界 | 36 | 36 |
| q2 | A1 标量 | 29,132,133 | 29,132,133 |
| q2 | 首个 ordinary row 标量 | 1,512,674 | 1,512,674 |
| q5 | 预处理上界 | 35 | 35 |
| q5 | A1 标量 | 25,188,706 | 25,188,706 |
| q5 | 首个 ordinary row 标量 | 973,729 | 973,729 |
| q5 | 4 张 ordinary row 累计标量 | 4,235,986 | 4,235,986 |

关键状态逐项完全相同，说明目标 Orkut 查询上该下界被现有 farthest、endpoint-floor、tour 或 directed-cut 证书支配。即使其证明和固定成本都可接受，也没有解决十亿级状态爆炸的信号。

### 3. 决策与清理

该方向判负，不接入主代码。隔离实现、构建和原始输出删除，只保留本文结论。后续不能把同一下界换一个权重尺度重新试验；已有高组 component-cover 阈值探针和本次最小正边 rooted 版本已经共同覆盖这条思路。

<a id="history-tour-endpoint-screen-negative-probe-20260810"></a>

## Tour endpoint 预筛选负面探针（2026-08-10）

> 原始记录：`TOUR_ENDPOINT_SCREEN_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选与等价性

ordinary future 原本直接计算完整 rooted tour 下界。候选先计算常数时间的 `EndpointFloorAt`；若该松弛已经证明状态不能严格改善 incumbent，就不再计算完整 tour，否则继续执行原调用。`EndpointFloorAt` 始终不大于完整 tour，因此候选只改变求值顺序，不改变下界值、接纳状态、答案或工作口径。规则不读取图名、组数阈值、边权类型或经验参数，并通过仓库 5/5 CTest。

候选与对照都包含 strict reverse residual closure、势转置、certified complement potential 和 tour local gather，唯一差异是上述预筛选。

### 2. Orkut `g=15` 结果

在正式 P2 第 5 条查询上，两版固定到 CPU 10、11。residual closure 前完成的 23 张 ordinary row 逐张具有完全相同的标量状态、primitive work、witness buy 次数和 incumbent，两版推进锁步，没有形成可见领先。该次短门随后共同进入与候选无关的 residual closure，不作为完整查询结果。

为避免继续等待共同 closure，改用同一图、同一 `g=15` 的第 3 条查询完成端到端成对 gate：

| 版本 | 时间（秒） | 权值 | 峰值 MiB | 状态 |
|---|---:|---:|---:|---:|
| tour local gather 对照 | 280.663883 | 38 | 2690.324 | 30,674,269 |
| endpoint 预筛选 | 282.532442 | 38 | 2689.934 | 30,674,269 |

候选状态与答案逐项相同，但时间增加 0.67%。说明 endpoint 松弛在绝大多数状态上不能独立拒绝，额外距离读取与热分支超过了少量跳过完整 tour 的收益。

### 3. 决策与清理

候选淘汰。正式源码不保留预筛选宏或调用点；隔离构建与全部完整、半截探针输出删除，只保留本文结论。后续不得把同一个被完整 tour 严格支配的 endpoint floor 再次插到 ordinary 热路径前，除非先有不增加存活状态成本的融合实现。

<a id="history-triple-path-support-reuse-negative-probe-20260810"></a>

## 三元路径真实树支撑复用负面探针（2026-08-10）

> 原始记录：`TRIPLE_PATH_SUPPORT_REUSE_NEGATIVE_PROBE_20260810.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选

共同的三元一步前瞻路径生长上界已经构造并计价一棵真实可行树，但正式接口只返回权值。候选临时把最佳树的稀疏 edge ID 一并返回；若 Enhanced 后续购买 residual closure，则把初始 dual primal、三元真实树和 closure primal 的边并集交给现有 facility subset DP。

候选没有生成新路径，也没有读取图名、组数区间、边权类型或运行时间。所有允许边和设施连接仍按原图权值计价，因而所得值是安全上界。探针在公共 A1 后立即购买，只判断联合支撑是否改善目标 q5，不等待完整查询。

### Orkut g15 q5 结果

输入为 experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt 第 5 条，Enhanced。

| 指标 | 单 closure primal 支撑 | 再并入三元真实树 |
|---|---:|---:|
| 购买后上界 | 35 | 35 |
| 购买耗时 | 177.558 s | 179.909 s |
| closure 后首张 ordinary row 标量 | 394,232 | 394,232 |

三元树增加了 facility 工作，却没有增加有效组合；首 row 的 lower certificate 本来就相同，因此状态也逐项不变。

### 结论与清理

候选数学安全但零上界、零状态收益，不进入正式实现。临时返回结构、支撑注入函数、eager 环境变量和原始输出目录均已删除，只保留本文结论。

<a id="history-anchor-rooted-global-lower-negative-probe-20260811"></a>

## 锚终端 rooted 全局下界负向探针（2026-08-11）

> 原始记录：`ANCHOR_ROOTED_GLOBAL_LOWER_NEGATIVE_PROBE_20260811.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选与正确性

residual closure 完成后，对永久锚组的每个候选终端 t 计算现有 farthest、tour 与 directed-cut future 的最大值，再对全部 t 取最小。任意 GST 必须包含某个锚终端；对固定 t 的 future 不超过包含 t 的最优树，因此这些 rooted 下界的最小值不超过全局最优值。该量可安全并入全局下界，计算只扫描锚组终端，不使用图名、组数阈值、运行时间或整数权。

### Orkut g=15,q5 结果

closure 在第 7 张 ordinary row 购买时，当时 incumbent 为 35，锚终端 rooted 全局下界仅为 22。随后四元路径和证书并图可把可行上界依次收紧到 34 和 32，但下界 22 无法形成闭合，也不足以显著加强当前逐状态 future。

### 决策

该方向证明安全但在目标查询上强度不足，予以淘汰。纯诊断宏、隔离构建和原始输出删除，只保留本文，后续不再把同一个 rooted future 的锚终端最小值当作 q5 的直接闭合方案。

<a id="history-branch-word-enumeration-negative-probe-20260811"></a>

## Branch bitmap 置位枚举：负向探针

> 原始记录：`BRANCH_WORD_ENUMERATION_NEGATIVE_PROBE_20260811.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 动机

ordinary row 已保存规范 branch bitmap。旧的“枚举 branch 后二分”路径用 `branch_count × log(row size)` 估计工作，却逐项扫描整张 branch row 才找到置位项。候选直接枚举 64-bit word 的置位项；自适应版按 `bitmap words + branch_count` 与完整 row 长度的结构工作量选择两种枚举，不读取图名、组数、时间或边权类型，也不改变状态和答案。

### 正确性

候选五个 CTest 全部通过。所有 P1 样本与 Orkut 探针的答案、状态数和逐 row primitive work 均与对照一致；候选只改变同一 branch 候选集合的物理遍历。

### P1 固定门

YouTube 作者 `g=7` 前 20 条、Base、同一 CPU 11 严格串行：

| 版本 | 20 条求解时间和 | 相对对照 |
|---|---:|---:|
| fresh control | 126.561655 秒 | 1.0000× |
| 强制 bitmap 置位枚举 | 128.263211 秒 | 1.0134× |
| 结构工作量自适应 | 127.787061 秒 | 1.0097× |

自适应选择消除了一部分退化，但仍没有形成 P1 非退化证据，因此未继续运行无必要的 Enhanced P1 小门。

### Orkut 高组门

在 `GPU4GST_Orkut`、`g=15`、第 5 条询问上，certificate-support 正向版本和自适应候选均固定 CPU 11、时限 1200 秒。对照完成约 368 个 `ordinary_row` 诊断事件，候选完成 366 个；best 均为 32，状态与 primitive work 轨迹一致。候选没有提高高组吞吐。

### 结论

branch row 全扫并非目标 q5 的主导成本，bitmap 置位枚举无法解决状态爆炸，并带来约 1% 的 P1 风险。该方向淘汰；源码宏、隔离 build 和原始输出删除，只保留本文结论。

<a id="history-dual-reduced-partition-screen-negative-probe-20260811"></a>

## Directed-cut reduced partition screen：负向探针

> 原始记录：`DUAL_REDUCED_PARTITION_SCREEN_NEGATIVE_PROBE_20260811.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 动机与安全性

对 ordinary 分区 `A,B`，候选尝试利用同一份 directed-cut 可行势，为两张 row 保存最小约化值，并在二者与全组势的和已经不小于 incumbent 时，整体跳过稀疏行交集。该判定是可采纳下界，不依赖经验阈值，也不改变 exact 语义。

### 对照设置

- 数据：`GPU4GST_Orkut`，`g=15`，第 5 条询问。
- 配置：当前 certificate-support 正向版本的 Enhanced。
- 机器约束：候选与新鲜对照严格串行，固定 CPU 11，单次时限均为 1200 秒。
- 正确性：五个 CTest 回归全部通过，best 均收紧到 32。

### 结果

| 版本 | 1200 秒内完成的 ordinary row | 相对对照 | 额外查询工作区 |
|---|---:|---:|---:|
| 原实现对照 | 368 | 1.00× | 0 |
| 约化分区筛选 | 275 | 0.75× | 约 49 MiB |

部分 size-3 row 的工作量确实从数千万降到数百万，证明筛选会命中；但为了得到每张 row 的约化最小值，热路径需要维护已覆盖势和约化值。该逐状态成本以及额外工作集远大于跳过分区所节省的工作，最终少完成约 25.3% 的 row。

### 结论

该方向失败，不进入正式实现。源代码宏、候选 build 和原始探针输出均删除；只保留本结论，避免后续重复实现同一逐状态约化值方案。

<a id="history-fused-top-ordinary-static-negative-20260811"></a>

## 最高 ordinary 层直接融合到 adjoint 的静态否定（2026-08-11）

> 原始记录：`FUSED_TOP_ORDINARY_STATIC_NEGATIVE_20260811.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

候选试图让 Enhanced 不再物化最高 ordinary row `D(S,v)` 的图闭包，而只把同根合并种子 `M(S,u)` 交给 adjoint-H。动机是最高层随后主要用于构造 H 的边界终端，若 H 的图闭包能够吸收这次传播，就可删除高组数查询最昂贵的一层。

证明审计在编译前否定了该替换。旧边界在顶点 `v` 同时组合 `D(S,v)`、另一 ordinary 块和 H 前缀。若 `D(S,v)=M(S,u)+d(u,v)`，把终端移到 `u` 后，另一 ordinary 块与 H 前缀都可能分别增加至多 `d(u,v)`；原来只支付过一份路径，不能同时补偿两侧。因此不能证明融合边界支配旧边界，直接替换可能漏掉精确最优解。

该方向没有进入构建或性能实验。隔离宏和字段已从源码删除，只保留本记录。除非后续给出能同时保持两侧根位置的 min-plus 交换定理或等价递推，不得以“小规模测试通过”代替正确性证明。

<a id="history-independent-witness-ensemble-negative-probe-20260811"></a>

## 独立 witness ensemble 负向探针（2026-08-11）

> 原始记录：`INDEPENDENT_WITNESS_ENSEMBLE_NEGATIVE_PROBE_20260811.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选

在已购买 residual closure 且四元路径上界严格改善 incumbent 后，主 witness 会替换为新的路径树。本探针分别额外保留一棵自然产生的真实树：

1. 预处理阶段原有的 directed-cut primal witness；
2. residual closure 完成后的 primal witness。

每棵树都调用与主 witness 完全相同的树 DP。总 buy 是各树按统一工作公式得到的 buy 之和，不使用图名、组数阈值、运行时间阈值或候选数量超参数。

### 2. Orkut `g=15,q5` 结果

两种候选固定在两个独立核心并行运行 1200 秒。并行内存带宽竞争使预处理时间不一致，因此 wall time 和两边的进度不能互相作细粒度加速比较；上界轨迹与结构工作量仍可用于否决没有产生新解的候选。

| 方案 | 刷新后的 witness buy | 1200 秒内最好上界 | 最后完成 rows | retained scalars |
|---|---:|---:|---:|---:|
| 单路径树基准 | 243,931,402 | 33 | 281 | 73,262,451 |
| 路径树 + closure primal witness | 530,909,522 | 33 | 271 | 67,663,407 |
| 路径树 + 初始 primal witness | 581,130,693 | 33 | 140 | 40,460,660 |

closure-primal ensemble 在第 58 张 row 才把上界从 34 收紧到 33，未得到已知精确值 32。initial-primal ensemble 同样只得到 33。所有已完成阶段的答案轨迹与单树候选相容，没有正确性异常。

### 3. 判定

拒绝该方案。独立评估多棵树只能在每棵树内部组合 ordinary 子解，不能把一棵证书中的边与另一棵证书中的边混合。它没有产生更强上界，却把树 DP 的购买价增加到单树的约 2.18 倍和 2.38 倍，并推迟后续评估。若继续研究多证书，必须验证证书并图上的一次组合，而不是继续叠加彼此独立的树 DP。

探针宏、额外容器、隔离构建和原始输出均在结论记录后删除，不进入正式实现。

<a id="history-purchased-farthest-order-negative-probe-20260811"></a>

## closure 后完整最远组顺序负向探针（2026-08-11）

> 原始记录：`PURCHASED_FARTHEST_ORDER_NEGATIVE_PROBE_20260811.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

候选在 Enhanced 购买 residual closure 后，为每个顶点按完整组距离递减顺序保存全部组号。`FarthestRemaining` 顺次读取该表并返回第一个仍在 remaining mask 中的组，数值与原实现逐组扫描严格相同。候选无图名、组数阈值、边权假设或可调参数，Base 和未购买 closure 的查询不执行。

Orkut `g=15,q5` 使用相同 support-DP 版本，在同一 CPU 11 上先跑候选、再顺序跑无排序对照，各限时 1200 秒。候选停在第 357 张 ordinary row；对照至少完成第 360 张。第 357 张时两边上界均为 32，累计状态均为 68,111,919，逐 row primitive work 相同。完整顺序没有减少状态，只增加约 `g*n` 字节的排名表及其访问，净吞吐略低。

候选淘汰。源码宏、隔离构建与原始输出删除，只保留本文；后续不再用固定深度 top-k 缓存替代，因为那会引入缺乏理论边界的工程参数。

<a id="history-purchased-vertex-group-distance-negative-probe-20260811"></a>

## Purchased vertex-major group-distance layout：负向探针

> 原始记录：`PURCHASED_VERTEX_GROUP_DISTANCE_NEGATIVE_PROBE_20260811.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 动机

Enhanced 在购买完整 directed-cut closure 后已经持有全部组到全部顶点的精确距离。该探针额外复制一份按顶点连续的 `vertex × group` 布局，希望减少 `FarthestRemaining`、tour 下界和 adjoint 热路径中的跨数组访问。

### 对照设置

- 数据：`GPU4GST_Orkut`，`g=15`，第 5 条询问。
- 配置：当前 certificate-support 正向版本的 Enhanced。
- 机器约束：候选与对照严格串行，均固定 CPU 11，单次时限 1200 秒。
- 指标：相同时限内完成的 ordinary row 数；不将诊断版本绝对时间当作正式性能。

### 结果

| 版本 | 1200 秒内完成的 ordinary row | 观察到的额外内存 |
|---|---:|---:|
| 原布局对照 | 368 | 0 |
| 顶点连续复制 | 362 | 约 0.35 GiB |

候选少完成 6 行，约退化 1.6%，同时必须复制约 `n × g` 个 `double`。收益不足以补偿复制成本和更大的工作集。

### 结论

该方向失败，不进入正式实现。源代码宏、候选 build 和原始探针输出均删除；只保留本结论，避免后续重复尝试同一布局复制。

<a id="history-endpoint-pair-tree-bound-dominated-note-20260812"></a>

## 固定组端点树遍历下界：支配性排除记录

> 原始记录：`ENDPOINT_PAIR_TREE_BOUND_DOMINATED_NOTE_20260812.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

候选对每个剩余组集合 $R$ 和端点对 $a,b$ 计算

```math
\frac{P_R(a,b)+\delta(a,b)}{2},
```

其中 $P_R(a,b)$ 是组度量中覆盖 $R$ 的固定端点最短 Hamilton 路径， $\delta(a,b)$ 是两组之间的最短距离。该式可由树上从 $a$ 到 $b$ 的开放遍历证明为安全下界，且能从现有 tour DP 以每个 mask 一个常数读取得到。

本方向此前已经验证：在本仓库 Enhanced 的 future 中，它被 directed-cut potential 完全支配，不能额外删除状态。即使预计算不改变现有渐近复杂度，热路径仍会增加一次无收益读取。因此不进入实现、不再运行性能探针，也不得作为后续 Orkut 高组数优化候选重复提出。

<a id="history-omitted-two-ordinary-layers-negative-probe-20260812"></a>

## 省略两层 ordinary 与无条件三块终端：被否决的 Orkut 探针（2026-08-12）

> 原始记录：`OMITTED_TWO_ORDINARY_LAYERS_NEGATIVE_PROBE_20260812.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> 本文否决“连最高逻辑层 $D(q)$ 也省略”的反例仍然有效；但链接文档中曾称为最终正确的一层方案后来也被辅助半层固定反例否决。当前修复见 [辅助半层 Adjoint 正确性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-auxiliary-half-adjoint-correctness-audit-20260815)。

### 1. 研究问题

Orkut 的 `g=15` Enhanced 长查询曾在最高两层 ordinary 上产生大量状态。候选尝试把完整锚定格的最高逻辑层记为 $q=6$、半格记为 $h=7$，只完整物化到 $q-1=5$，再用两类局部机制补缺：

- 在已知少量 H 目标顶点后，目标驱动地结算 $D(q)$ 的空前缀消费者；
- 允许三个互不相交、大小至多 5 的 rooted ordinary 块在同一顶点形成转置 terminal，以补充单块/双块覆盖不到的外侧结构。

这个方向只有在同时满足精确性、无经验图名或组数开关、Base/Enhanced 同职责关系可解释，以及固定成本不会吞掉 P1 的前提下才可进入论文方法。

### 2. 关键反例：局部补偿不等价于完整 ordinary row

可信的完整精确版本在 Orkut `g=15` 第 2 条查询上返回 32。省略 $D(6)$ 后，即使同时保留目标驱动 $D(6)$ 结算和直接三块 terminal，仍返回 33：

| 版本 | 时间（s） | 权值 | 峰值增量（MiB） | 实际状态数 |
|---|---:|---:|---:|---:|
| 可信完整精确边界 | 3,805.630196 | 32 | 3,714.625 | 270,104,041 |
| 省略 $D(6)$，目标驱动补偿 | 3,582.335967 | 33 | 3,614.652 | 261,205,353 |
| 再加入直接三块 terminal | 5,288.398140 | 33 | 3,614.652 | 261,205,575 |
| 最终边界：完整保留 $D(6)$，只省略 $D(7)$ | 4,715.120165 | 32 | 3,616.320 | 261,581,592 |

原因不是三块枚举漏了某个简单组合，而是完整 $D(6)$ 还是 H successor、非空低层 A 边界和空前缀边界的共同逻辑依赖。ordinary row 在同根 split 之后还经过图最短路闭包并发布规范 branch；只在最终已知的少量目标顶点结算一个标量，不能恢复这张 row 在其他消费者处的状态与 branch 语义。三块 terminal 只负责表示更高的半格外侧树，也不能代替一张被多个递推阶段读取的普通 row。

因此，`ordinary_last_layer=q-1` 不是性能实现选择，而是错误的状态域缩减。该反例直接否决整个候选族。

### 3. 无条件三块的固定成本

直接三块没有修复上述反例，还在不改变主状态规模时产生显著固定成本。第 5 条查询上：

| 版本 | 时间（s） | 权值 | 峰值增量（MiB） | 实际状态数 |
|---|---:|---:|---:|---:|
| 目标驱动补偿，不做直接三块 | 7,289.085420 | 32 | 6,185.477 | 482,653,084 |
| 再加入直接三块 | 8,901.782839 | 32 | 6,185.473 | 482,653,728 |

直接三块多用 1,612.697419 秒，即增加 22.12%，状态数和空间几乎没有收益。第 3、4 条也分别为 371.234398 秒和 371.312578 秒。三块不能作为“只要 ordinary 低于半格就执行”的固定阶段。

pair-union 因子化已经由独立单元测试证明与直接三元枚举给出完全相同的三块最小值；它把稠密组维度从四路指派降为两个三路指派，但不会扩大候选集合，所以不可能修复第 2 条的 32→33 错误。第 3 条实测为 385.594061 秒，也没有胜过直接三块的 371.234398 秒。失败候选中不保留该实现。

### 4. 最终结构边界

最终实现完整物化到：

```math
r=\max\left\{q,\left\lceil\frac{k-(\ell+1)}{2}\right\rceil\right\}.
```

当前方案仍采纳这项负面结论：Enhanced 与 Base 对所有 $|S|\le q$ 调用同一个 ordinary 构造器，不能用目标驱动标量替代完整 $D(q)$。不同之处是，更高的半格 $D(h)$ 现在先由辅助 $H(h)$ 的同递推转置精确恢复，再递减 H；旧 separator 装箱与第三块 terminal 已从生产实现删除。下述容量分析只解释当时失败候选，不再定义当前算法。

```math
\tau(r)=\left\lfloor\frac{3r}{2}\right\rfloor+2.
```

旧候选曾在转置定义域允许外侧总量至少达到 $\tau(r)$ 时设置 `requires_three_block_terminal`。当前生产版本不再含该字段、`pair_best` 或三块热循环；它只在辅助半层缺少直接 D 时枚举 ordinary 双块 split。

固定成本移除前后，第 3 条权值和 31,051,638 个状态完全一致，时间从 390.156139 秒降到 298.400088 秒，下降 23.52%。第 4 条最终为 357.558646 秒、权值 28、15,061,435 个状态。长查询的最终五条门禁另行记录；本文件只负责否决错误边界和解释最终结构选择。

### 5. 清理决定

- 生产代码不保留 `ordinary_last_layer=q-1`、目标驱动最高 ordinary、延迟边界 evaluator 或对应配置字段。
- 当前源码完全删除三块 pair-union；本文保留其负面成本数字，防止以后把相同失败方向重新包装成优化。
- 被否决二进制和原始运行目录只位于 `/tmp`，不属于可复现实验 artifact；本文件保存必要数字后可以删除。
- 正式方法只描述“完整保留最高逻辑 ordinary 层、只替换半格 terminal 和高层 A”，不把失败候选包装成消融贡献。

<a id="history-ephemeral-complementary-dual-negative-probe-20260813"></a>

## 一次性互补 directed-cut 证书：负结果记录

> 原始记录：`EPHEMERAL_COMPLEMENTARY_DUAL_NEGATIVE_PROBE_20260813.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选设计

保留正式 Enhanced 的反序 residual closure 不变。第一次 closure 买入后，调度器从零重新累计 rent；若后续工作再次覆盖一份独立证书的静态构造成本，则按正序构造第二份完整 directed-cut 证书。第二份证书只与正式证书取最大值，对已经发布的 ordinary rows 做一次过滤，随后立即释放，因而不会永久增加每状态两次势函数读取，也不会长期保留第二份 $g n$ 数组。

安全性没有问题：两份证书都是对同一剩余组集合的可采纳下界，取最大值仍是可采纳下界；被它证明无法改善 incumbent 的状态在证书释放后也无需恢复。该候选通过了现有 5 项精确性测试。

### Orkut g=15 诊断结果

测试使用新增询问中的 q6 与 q10，两个进程分别固定在两个物理核上，诊断窗口为 1260 秒。

- q6：正式 closure 在 D2 第 9 行买入。互补证书直到 D2 第 87/91 行才买入，构造耗时 277.007 秒，把已发布标量数从约 3892 万降至 3553 万，只删除 3,389,557 个状态，约为 8.7%。买入过晚且构造成本显著，固定窗口进度弱于正式版本。
- q10：正式 closure 在 D2 第 2 行买入；互补证书在固定窗口中没有形成足以抵消构造成本的进度改善，查询仍未完成，也没有解决正式版本的 10000 秒超时风险。

### 结论

该方向不能同时满足“救回 q10”和“不拖慢已能在 10000 秒内完成的 q6”。其根因不是证书不安全，而是第二次全图 residual 构造成本很高，一次性过滤又只能删除已经发布的状态，不能持续加强随后产生的新状态。候选已从主代码完全删除；仅保留本文档，后续不得以相同形式重复实验。

<a id="history-lazy-maximum-residual-marginal-negative-probe-20260814"></a>

## 动态最大 residual 边际顺序负结果

> 原始记录：`LAZY_MAXIMUM_RESIDUAL_MARGINAL_NEGATIVE_PROBE_20260814.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 候选与理论动机

Residual closure 的固定 completion 顺序改为动态选择：每一步在当前 residual 上，优先处理根距离最大的未处理组。旧的精确根距离在 residual 继续下降后仍是上界，因此候选使用一个上界堆；只有重新计算的堆顶精确值不小于其余全部上界时才正式选择该组。每次被正式选择的增量仍是当前 residual 上的精确距离势，逐弧容量可行性不变，因而它是安全的 directed-cut 对偶证书，不依赖图名、组数阈值、时间阈值、整数权或精度压缩。

### Orkut g=15 q10 结果

诊断版绑定 CPU 2。由于起初与 CPU 3 的 q4 并发，绝对时间只用于识别数量级，不作为正式性能数字。首次 closure 在 ordinary D2 已发布两张 row 后执行：

| 指标 | 现行固定 completion | 动态最大根边际 |
| --- | ---: | ---: |
| residual SSSP 次数 | 15 | 30 |
| closure 后上界 | 59 | 59 |
| closure 前普通标量 | 4,600,116 | 4,600,116 |
| closure 后普通标量 | 1,169,801 | 3,543,340 |
| closure 本体时间 | 约 166 秒 | 364.355 秒（并发环境） |

动态候选没有改善 primal 上界，且保留状态约为现行顺序的 3.03 倍。其 30 次精确 SSSP 也超过现行 rent-buy 公式按每组一次 SSSP 计入的 15 次；若要在论文中诚实接入，购买价必须按可证明的最坏重算次数提高，触发还会更晚。

### 结论

根处总对偶增量不是 ordinary future 强度的合适代理。ABHSS 实际需要的是势在大量候选 `(mask,v)` 上的分布，而非只在所选根上的和值；最大化后者会把 residual 容量分配到对状态过滤更弱的极点。该方向在剪枝强度、闭包代价和 rent-buy 可解释性三方面均劣于现行反序 completion，故只保留本审计，源码与探针产物删除，不进入正式方法。

<a id="history-early-quad-and-half-pair-negative-probe-20260815"></a>

## [负结果] 提前 Quad 与辅助 H pair 分桶

> 原始记录：`EARLY_QUAD_AND_HALF_PAIR_NEGATIVE_PROBE_20260815.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

日期：2026-08-15

基线提交：`849c89640404eed42553cedaa1b51e37a69c7ce7`

结论：两个候选均否决；实现未保留，不得作为当前方法或性能证据引用。

### 1. 提前运行 Quad 路径生长

候选把原本只在 residual closure 购买后运行的四块 seeded path growth 提前到预处理，希望先把 Orkut `g=15` q10 的上界从 62 收紧到 59。它不读取图名或经验阈值，但改变了上界职责的购买时机，并使后续 witness evaluator 过早切换到较大的 certificate support。

| 阶段 | checkpoint | 提前 Quad |
|---|---:|---:|
| prepare 上界 / 秒 | 62 / 186.115 | 59 / 188.426 |
| A1 秒 / 状态 | 84.670 / 39,056,188 | 82.527 / 38,563,058 |
| closure 后上界 | 59 | 59 |
| D2 结束存活标量 | 73,563,652 | 73,563,652 |
| 后续单次 witness buy | 约 0.4 秒 | 约 1.4 秒 |

候选的早期上界只暂时减少 A1；checkpoint 在既定 residual closure 处得到同一个 59 后，D2 的最终状态域完全汇合。提前登记的 support 却让大量后续购买持续支付更高固定成本。1200 秒诊断只推进到第 322 张 row、200,958,993 个存活标量，没有形成可延续的状态优势。

五块 seed 也已在既有探针中验证不能把 q10 的预处理上界降到 59 以下。因此不再增加 seed 深度，不把 Quad 前移，也不保留按查询表现切换购买时机的代码。

本地证据目录为 `results/probes/orkut_g15_quad_phase_q10_cpu4_1200s`。

### 2. 辅助 H 的 pair 大小分桶

奇数组数的辅助半层需要枚举两个互不相交 ordinary 块。候选提前按块大小和结构兼容性建立 right-mask 分桶，只访问可能达到目标 cover 大小的 pair。候选覆盖的数学集合与 checkpoint 相同，并通过 5,000 个随机连通实例、500 个正权压力实例、160 个省略半层实例和固定 12 点反例。

Orkut `g=15` q3 的交换绑核正式结果为：

| 实现 | solver 秒 | 权值 | 状态 |
|---|---:|---:|---:|
| checkpoint | 291.618891 | 38 | 与候选相同 |
| pair 分桶 | 292.892928 | 38 | 与 checkpoint 相同 |

同轮诊断中，候选转置为 5.90179 秒，checkpoint 为 5.70865 秒，候选在目标热段慢约 3.4%。虽然候选的 adjoint 总计 18.9662 秒、checkpoint 为 19.0268 秒，但差值远小于整查询波动，不能覆盖转置本身的确定回归；额外分桶还增加了代码与证明分支。

因此保留当前“排序 pair 与互补 submask 估算二选一”的单一实现，不增加结构分桶。相关隔离构建和临时源码应清理；本地结果目录仅作为负证据保留：

- `results/probes/orkut_g15_halfpair_q3_cpu4`
- `results/probes/orkut_g15_halfpair_control_q3_cpu4`
- `results/probes/orkut_g15_halfpair_diag_twin_q3_cpu4`
- `results/probes/orkut_g15_current_diag_twin_q3_cpu5`

### 3. 后续约束

- 上界候选必须证明在既定消费点之前带来持续状态收益，不能只报告更早出现同一个上界。
- 逻辑枚举量下降仍须通过同核热段门；若新索引使实际转置变慢，就不属于可接受的“减成空”。
- 不按图名、查询编号、组大小、运行时间或观测状态量恢复任一候选。
- 失败实现只留本文和本地结果，不进入正式实验矩阵、方法文档或发布源码。

<a id="history-shared-ordinary-half-negative-probe-20260815"></a>

## [负结果] ordinary 半层直接复用为最高 H

> 原始记录：`SHARED_ORDINARY_HALF_NEGATIVE_PROBE_20260815.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

日期：2026-08-15

基线提交：`849c89640404eed42553cedaa1b51e37a69c7ce7`

结论：否决；实现已全部清理，不能作为当前方法、正确性证据或性能结果引用。

### 1. 试图删除的工作

checkpoint 的 Enhanced 先生成 ordinary 到最高逻辑层，再由辅助 `H(h)` 重建被省略半层的补集状态。候选试图把 ordinary 继续生成到平衡半层，将 `D(F\S,v)` 按补集直接转交给最高 `H(S,v)`，从而删除：

- 按顶点转置 ordinary 值；
- 奇数组数时的双块 terminal 枚举；
- 最高辅助 H 的第二次图闭包。

层边界只由 `g` 的平衡公式决定，没有读取图名、时间、状态密度或经验阈值。所有权转移先完成仍需两张 ordinary 半层的边界结算，再移动 payload，因此原型本身没有悬空引用或重复状态计数。

### 2. 初看有利但不足的证据

直接转交原型通过：

- 5,000 个确定性随机连通实例，`g=2..10`；
- 500 个正权互异单终端压力实例，`g=6..10`；
- 160 个专门覆盖省略半层的实例，`g=7..16`。

三种配置累计状态为 `103878/31982/28832`，而 checkpoint 为 `103878/31982/33869`。Orkut `g=15` q3 的同核结果为：

| 实现 | solver 秒 | 状态数 | 查询峰值 MiB |
|---|---:|---:|---:|
| checkpoint | 281.881173 | 31,191,511 | 2689.352 |
| ordinary 半层直接转交 | 271.603184 | 30,673,451 | 2689.727 |

时间下降 3.65%，状态下降 1.66%，但这些性能数字不能覆盖已知真值反例。

### 3. 决定性反例

SteinLib `wrp4-16` 的公开已知最优值为 1190：

| 版本 | 返回值 | 状态数 | 结论 |
|---|---:|---:|---|
| checkpoint Enhanced | 1190 | 19,123 | 正确 |
| 候选 Base | 1190 | 24,590 | ordinary/前向主路径仍正确 |
| 直接转交 Enhanced | 1191 | 18,437 | 错误 |
| ordinary seed 再做 H 闭包 | 1191 | 18,702 | 错误 |
| 平衡层停用 A1 后再闭包 | 1191 | 18,702 | 错误 |
| 平衡层停用全部 future 后再闭包 | 1191 | 30,521 | 错误 |

最后一项只按真实 incumbent 截断平衡层，仍未恢复 1190。因此错误不能归因于 A1、dual、farthest 或 tour 中某一个平衡层证书，也不能通过给最高 H 再加一次 Dijkstra 修复。

原始探针目录：

- `results/probes/shared_ordinary_half_steinlib`
- `results/probes/shared_half_wrp4_16_base`
- `results/probes/checkpoint_wrp4_16_enhanced_control`
- `results/probes/shared_half_reclosure_wrp4_16`
- `results/probes/shared_half_no_a1_boundary_wrp4_16`
- `results/probes/shared_half_no_future_wrp4_16`

这些目录仅是本地负结果证据，不进入正式实验矩阵。

### 4. 原因

`D(F\S,v)` 与 `H(S,v)` 的未截断数学值存在补集恒等关系，但当前 sparse row 不是“全顶点函数表”。较低 ordinary row 已按前向完成域安全截断，并只保留该域后续消费者所需的规范 branch。最高 ordinary 半层即使不再使用任何 future，它的 split seed 仍来自这些已经截断的低层 row。

checkpoint 的辅助 H 不是把一张完整 dense `D` 重算一次。它先把仍存活的低层 ordinary 值转到补集域，在 H 自己的 admissible prefix 下重新执行图闭包，再逐层递减。这个过程会恢复反向完成域需要、但不属于 ordinary 前向 payload 的顶点状态。要让普通半层直接复用保持精确，必须从更低层开始递归重建这种补集闭包；那会重新得到现有 H 递推，只是增加一套别名和生命周期。

因此，“ordinary 半层直接转交”没有严格支配辅助 H；它删除了有效操作。随机小图未覆盖该结构，SteinLib 已知真值 gate 才给出决定性反例。

### 5. 后续约束

- 不再尝试把截断后的 ordinary row 直接改名为 H。
- 不把 A1 描述为该反例的原因；停用 A1 和停用全部平衡层 future 都未修复。
- 可以继续优化转置的物理枚举、内存生命周期或 H 内部常数，但必须保留“补集域重新闭包”这一逻辑职责。
- 任一声称删除辅助层、跳过最高 H 闭包或复用 ordinary payload 的候选，必须先通过 `wrp4-16=1190`，再进入随机回归和 Orkut 性能门禁。

<a id="history-cached-dual-and-witness-relevance-negative-probe-20260816"></a>

## [负结果] cached dual 区间复用与 witness 输入相关调度

> 原始记录：`CACHED_DUAL_AND_WITNESS_RELEVANCE_NEGATIVE_PROBE_20260816.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

日期：2026-08-16

基线提交：`849c89640404eed42553cedaa1b51e37a69c7ce7` 及其当前已接受严格等价减空

结论：两个候选均否决；生产源码和公共接口均已恢复，不得作为当前方法或性能证据引用。

### 1. cached dual 区间的二次判定

ordinary 的分阶段 certificate cache 会保存同一顶点已经计算的 directed-cut 下界。候选在该缓存命中后，用 `value + cached_lower < incumbent` 先判定当前标签；只有该式仍可能改进时才进入现有后续证书阶段。该判定只复用可采纳下界，不改变下界数值、阶段次序、状态定义或 strict incumbent 比较，因此逻辑上安全。

候选与对照分别固定在 CPU 4 和 CPU 5，同时运行 Orkut g=15 q10 900 秒。两边的上界、存活标量、D2 完整层和 witness 购买轨迹一致：

| 实现 | 已发布 ordinary row | 进度 |
|---|---:|---|
| cached dual 候选 | 190 | D2 全部 91 张，D3 99 张 |
| 对照 | 194 | D2 全部 91 张，D3 103 张 |

候选没有减少任何存活状态或后续证书调用集合，反而在热路径增加一次比较与分支。当前 ordinary 已有 monotone certificate frontier；对缓存值追加同式判断不构成新的严格支配关系。

本地证据目录：

- `results/probes/cacheddual_round1_candidate_cpu4`
- `results/probes/cacheddual_round1_control_cpu5`

### 2. 只在新 row 触及 witness support 时重新购买树 DP

候选在每张 ordinary row 发布后，检查新增顶点是否与当前 witness/support 顶点相交；只有相交才增加 witness 输入修订号。support 被替换时强制增加修订号。rent、buy 公式和 `EvaluateWitnessTree` 本身均不改变。

候选通过 5 项 CTest；SteinLib 的 Base 与 Enhanced 各自 11 个已知最优实例全部一致，其中 `wrp4-16` 仍为 1190。候选与对照的已知最优值序列均为：

~~~text
361, 237, 497, 250, 422, 208, 179, 798, 290, 405, 1190
~~~

Orkut g=15 q10 候选与对照分别固定在 CPU 4 和 CPU 5，同时运行 900 秒。两边均发布 193 张 ordinary row，即 D2 全部 91 张和 D3 的 102 张；两边都发生 42 次 witness 购买。逐次购买的 row 序列完全相同，没有一张可被候选跳过。

这说明 q10 的每次既定购买之前都已有 row 触及当前 support。候选只能增加集合相交检查，不能减少树 DP 次数。它也没有命中主要瓶颈：一次树 DP 约 0.4 秒，而 ordinary D 的 certificate 与图闭包工作占绝大多数时间。

本地证据目录：

- `results/probes/witnessrelevance_round1_candidate_cpu4`
- `results/probes/witnessrelevance_round1_control_cpu5`
- `results/probes/witnessrelevance_steinlib_candidate_20260816`
- `results/probes/witnessrelevance_steinlib_control_20260816`

### 3. 后续约束

- 不在 cached dual 命中路径重复加入 `value + cached_lower` 判定。
- 不给 witness scheduler 增加 support 相交门；q10 的购买序列没有可跳项。
- 不把这两个候选写入正式 `METHOD.md`、`CODE_GUIDE.md` 或实验矩阵。
- 后续减空应瞄准 ordinary D 的大量 certificate/cache 工作或严格等价的查找成本；不得改浮点精度，不得增加图名、查询编号或经验性组数开关，不得删去辅助 H。
- 新候选须先核对输出与状态；严格等价实现再做同核或交换绑核性能门，有状态变化时另行给出完整精确性证明与已知真值 gate。

<a id="history-mandatory-a1-and-size1-scan-negative-probe-20260816"></a>

## 公共 A1 必然存在与 ordinary size-1 扫描负向探针

> 原始记录：`MANDATORY_A1_AND_SIZE1_SCAN_NEGATIVE_PROBE_20260816.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 严格不变量

PrepareProblem 对所有不超过三个组的查询执行共同精确闭包并直接返回。任何到达指数递推的查询因此满足 $g\ge4$。由平衡完成域定义，最高锚定层为 $\lfloor g/2\rfloor-1\ge1$，ordinary 截止层为 $\lfloor g/2\rfloor\ge2$。所以进入 ordinary 的查询必然已经构造公共 A1，且 ordinary size 1 不生成任何 $D(S,v)$ row。

这不是经验性的 $g$ 阈值或数据相关分派，而是基例与递推定义域共同推出的控制流事实。源码层可据此把 A1 future 从 nullable pointer 改为引用、删除 Base 与 staged future 中的两处空指针分支，并让 ordinary 外层循环直接从 size 2 开始。

### 2. 合并候选的正确性与机器码

合并候选同时执行上述三项改写。它通过 5/5 CTest，其中配置回归覆盖 5,000 个随机实例、500 个正权唯一终端实例和 160 个 omitted-half 高组实例。SteinLib 的 Base 与 Enhanced 各 11/11 命中已知最优值，权值、可行性与状态向量逐项等于 checkpoint；两配置总状态分别为 76,383 和 54,477。

候选可执行文件 SHA-256 为 cba651803382e0230df2a43ac491593ddfbdef0ce82c2694a33f998ab165098c。ELF 文件大小仍为 285,464 字节，text 由 235,468 减至 235,163 字节；BuildOrdinaryRows 主体由 0x368e 减至 0x365c，future lambda 由 0x862 减至 0x843。减空真实进入了机器码，不是编译器已自动消除。

### 3. 合并候选的 P1 门

Musae 使用 P1 原文 $g=7$ 全 300 条 Base 查询，候选与 checkpoint 交换绑定 CPU 4/5。全部权值和总状态 6,872,062 逐项相同。

| 轮次 | 候选 / 秒 | 对照 / 秒 | 变化 |
|---|---:|---:|---:|
| round 1 | 47.060697 | 46.499372 | +1.2072% |
| round 2 | 47.268215 | 46.358744 | +1.9618% |
| 两轮均值 | 47.164456 | 46.429058 | +1.5839% |

两轮方向一致，违反 P1 不退化底线。因此合并候选不能进入生产源码。

### 4. 单独删除 size-1 扫描

为排除 nullable-A1 改写的代码布局影响，又只把 ordinary 外层循环起点从 1 改为 2，并删除恒真的 size==1 拒绝项。该候选 text 仅减少 44 字节。Musae 两轮均值为 46.723103 秒，对照为 46.728161 秒，变化 -0.0108%，可视为持平；权值与状态逐项相同。

大图门使用 Orkut $g=15$ q3 Enhanced，继续交换 CPU 4/5：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 变化 |
|---|---:|---:|---:|
| round 1 | 293.196284 | 290.057148 | +1.0822% |
| round 2 | 292.742484 | 291.671154 | +0.3673% |
| 两轮均值 | 292.969384 | 290.864151 | +0.7238% |

四次运行的答案均为 38，状态均为 31,191,511；RSS 约 2,690 MiB。两轮都回归，说明源码层少一次很小的 mask 扫描改变了后续热代码布局，却没有足够的实际工作收益抵消它。

### 5. 结论与禁止重试边界

当前生产源码完整恢复 checkpoint：A1 接口继续保留短 nullable 连接分支，ordinary 继续从 size 1 进入并在 mask 谓词中立即跳过。论文不把这些连接操作写成算法步骤；它们只是当前 GCC 13.3 Release/LTO 二进制中更稳定的物理布局。

该负结果不否定“所有实际指数查询必有 A1”的数学事实，只说明按该事实重排当前热函数不能满足物理非退化门。以后只有 ordinary 内核、翻译单元或编译器发生实质变化，并重新通过完整 P1 与 Orkut 大组门禁时才值得重试；不得因为源码更短或渐近工作严格少一次扫描而直接合入。

<a id="history-queue-pop-and-empty-ready-negative-probe-20260816"></a>

## Queue pop 与 forward 空行发布负向探针

> 原始记录：`QUEUE_POP_AND_EMPTY_READY_NEGATIVE_PROBE_20260816.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 审计对象

本轮只考察由当前控制流严格蕴含、但未必能改善机器执行的两类操作：

1. ordinary $D$ 与 adjoint $H$ 队列出堆时的 `node.key < best` 复核；
2. 普通 forward $A$ 从未产生标签时，是否仍显式写入 `row.ready=true`。

它们都不增加下界、上界、状态或数据相关分派。实验候选通过 5/5 CTest，抽样权值与状态数均和对照一致；这里只判断这些逻辑等价改写能否进入论文二进制。

### 2. 出堆 cutoff 的逻辑边界

在一张 ordinary $D$ row 的 Dijkstra 闭包中，`best` 不会在队列循环内改变；在一张 adjoint $H$ row 中，完整边界上界也只在该 row 闭包结束后结算。因此二者队列建立后，已经满足 `key < best` 的有效节点在本 row 内仍满足该式。删除出堆时的第二次 cutoff 复核在逻辑上等价。

这个结论不能推广到 forward $A$ 或提前 A1。普通 forward $A$ 在每次有效出堆后尝试 root-star，可在同一队列循环内降低 `best`；提前 A1 还可能在累计 rent 达到共同 buy 公式时调用树 DP 并降低 `best`。这两处出堆 cutoff 是动态 incumbent 的有效消费者，不属于被支配操作。

### 3. forward 空行的真实生命周期

普通 forward $A$ 按 size 递增处理 mask。若某个 mask 的所有 seed 都为空，`touched.empty()` 说明它没有任何有限 payload；后续前向 mask 只会把它作为真子集读取，adjoint 边界也只调用 `RowValue`。空 payload 对每个顶点都返回无穷，所以严格层序已经足以证明“该 mask 已处理”，无需用 `ready` 再向这些直接读取者发布一次。

这不是全局 `Row` 合同。ordinary、提前 A1 owner 交接与 $H$ 的外部消费者会显式查询 `ready`，这些 row 即使为空也必须发布。为普通 forward 空行补写 ready 虽然数值等价，却增加热写入；在每层末尾扫描整个 mask 域批量补写，还额外增加 $O(2^k)$ 生命周期遍历。

### 4. 交换绑核结果

Musae 使用 Base、 $g=7$ 全 300 条；Orkut 使用 Enhanced、 $g=15$ q3。候选与 checkpoint 交叉交换 CPU 4/5 和启动顺序。表中比例按两轮均值计算；候选组合名称严格反映同轮包含的改动，不能把耦合结果错误归因给其中单项。

| 候选 | 数据 | 候选均值 / 秒 | 对照均值 / 秒 | 变化 |
|---|---|---:|---:|---:|
| 删除 D/H pop cutoff，并逐空行写 ready | Musae 全 300 | 46.839743 | 46.524746 | +0.68% |
| 仅删除 H pop cutoff，并逐空行写 ready | Musae 全 300 | 46.287562 | 46.421079 | -0.29% |
| 仅删除 H pop cutoff，并逐空行写 ready | Orkut q3 | 291.903568 | 289.955703 | +0.67% |
| 仅逐空行写 ready | Musae 全 300 | 46.781526 | 46.090182 | +1.50% |
| 每层末尾批量补齐空行 ready | Musae 全 300 | 46.701163 | 46.052365 | +1.41% |

Orkut q3 两边权值均为 38，状态数均为 31,191,511；Musae 各轮权值序列与总状态数 6,872,062 也一致。数值与状态一致只能证明等价，不能覆盖稳定的时间回归。

### 5. 结论与禁止重试边界

当前生产源码完整恢复以下实现：

- $D$、 $H$ 保留短的 pop cutoff 连接操作；论文不把它包装成算法增强；
- forward $A$ 在 `touched.empty()` 时直接进入下一个 mask，不写 ready，也不做层末全域扫描；
- forward $A$ 与提前 A1 中会响应动态 incumbent 的 pop cutoff 始终保留。

以后只有在翻译单元布局或编译器发生实质变化、并重新通过完整 P1 与 Orkut 大组门禁时，才应重试上述物理改写。不能因为它们在源码层“少一个比较”或“生命周期更整齐”就重新合入。

<a id="history-staged-cache-physical-reduction-negative-probe-20260816"></a>

## Staged certificate cache 物理减写与冻结分派负向探针

> 原始记录：`STAGED_CACHE_PHYSICAL_REDUCTION_NEGATIVE_PROBE_20260816.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 两个严格等价候选

本轮检查了两个不改变证书值、状态集合或配置语义的实现候选。

第一个候选延迟新 row epoch 的首次发布。原实现第一次看到顶点时先写 stage 0 和 `bound_cache=0`，随后 `CanImproveAllExcept` 在每条返回路径都覆盖 lower；exact 拒绝或放行还会立即把 stage 改为 1。候选删除必被覆盖的 cache 零写，只在 non-exact 拒绝确实需要继续停留 stage 0 时发布该状态。最终 `bound_state` 与 `bound_cache` 逐项相同。

第二个候选利用查询配置冻结不变量，把 `BuildOrdinaryRows` 写成一个以 flat/staged 为布尔模板参数的同源内核，并在公开入口只分派一次。它不复制源代码递推，也不按图、组数或运行统计选择；目标只是让逐候选的 `if (!staged_certificate_cache)` 在编译期减成空。

### 2. 正确性与机器码证据

两个候选都通过 5/5 CTest。Musae 全 300 条及 Orkut g15 q3 的逐询问权值、状态数完全一致。冻结分派候选确实生成两个模板实例：Base 的 `CanImprove` lambda 从 0x862 字节缩为 0x1f2，Enhanced 实例为 0x698；可执行文件由 285,464 字节增至 299,608 字节。也就是说热配置判断已经真实删除，结果不是编译器没有接受改写。

### 3. 交换绑核结果

Musae 使用 Base、g=7 全 300 条；两轮交换 CPU 4/5。Orkut 使用 Enhanced、g=15 q3，候选 CPU 4、对照 CPU 5。全部运行使用同一 Release/LTO 配置。

| 候选 | 数据与轮次 | 候选 / 秒 | 对照 / 秒 | 变化 |
|---|---|---:|---:|---:|
| 延迟 epoch/cache 发布 | Musae round 1 | 47.081493 | 46.316776 | +1.65% |
| 延迟 epoch/cache 发布 | Musae round 2 | 46.192639 | 46.348598 | -0.34% |
| 延迟 epoch/cache 发布 | Musae 两轮均值 | 46.637066 | 46.332687 | +0.66% |
| 延迟 epoch/cache 发布 | Orkut q3 | 291.621593 | 290.400302 | +0.42% |
| 冻结配置模板分派 | Musae round 1 | 46.734461 | 45.944558 | +1.72% |
| 冻结配置模板分派 | Musae round 2 | 46.502387 | 46.363025 | +0.30% |
| 冻结配置模板分派 | Musae 两轮均值 | 46.618424 | 46.153792 | +1.01% |

Musae 每轮总状态均为 6,872,062；Orkut q3 两边权值均为 38、状态均为 31,191,511、峰值均为 2690.289 MiB。两个候选都没有状态或空间收益，且时间不满足 P1 非退化底线。

### 4. 当时结论

生产源码恢复 checkpoint 的单一运行时 flat/staged 内核，以及新 epoch 立即写入 stage 0 和 cache 零值的短连接操作。论文不把这些连接操作描述成算法贡献；它们只是当前 GCC 13.3 Release/LTO 下更稳定的机器布局。

以后不能仅凭“少一次写入”或“把配置判断移到入口”重新合入。只有编译器、翻译单元边界或整个 ordinary 内核发生实质变化，并重新通过完整 P1 与 Orkut 大组门禁时，才值得复查。

### 5. 2026-08-17 ordinary 改写后的复查

此后 ordinary 内核加入 packed rejection frontier、线性建堆与共同 A1 分级视图，翻译单元和热 lambda 已发生上一节要求的实质变化，因此重新测试了入口模板分派。候选还暂时包含后来判负并删除的 chain envelope；该 envelope 只影响 DirectedCut 路径，所以 Base 两轮是模板分派的隔离证据，Enhanced 两轮是对“模板加额外无效分支”的保守联合门。

| 配置与轮次 | 候选 / 秒 | 运行时分支对照 / 秒 | 变化 |
|---|---:|---:|---:|
| Base round 1，候选 CPU4 | 45.975582 | 45.928158 | +0.10% |
| Base round 2，候选 CPU5 | 45.672162 | 46.078361 | -0.88% |
| Base 两轮合计 | 91.647744 | 92.006519 | -0.39% |
| Enhanced round 1，候选 CPU4 | 22.641265 | 22.317678 | +1.45% |
| Enhanced round 2，候选 CPU5 | 22.426750 | 22.690726 | -1.16% |
| Enhanced 两轮合计 | 45.068015 | 45.008404 | +0.13% |

每一对的 300 条权值、累计状态和峰值空间均一致；Base 累计状态为 6,872,062，Enhanced 为 4,283,084。交换核心后方向反转，合计差异均小于 0.4%，没有可辨认的物理退化。当前源码因此只恢复“入口分派一次、同源模板实例”这一项，并继续保留当时判负的 epoch/cache 延迟发布。它不作为算法贡献，也不支持性能增益主张；其目的仅是让 Base flat 与 DirectedCut staged 的机器热路径对应各自实际执行的职责。若 ordinary 内核再次发生实质变化，仍须重新做交换绑核门。

<a id="history-lazy-exact-dual-purchase-negative-probe-20260817"></a>

## Directed-cut 延迟精确购买负向探针（2026-08-17）

> 原始记录：`LAZY_EXACT_DUAL_PURCHASE_NEGATIVE_PROBE_20260817.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 候选与正确性边界

closure 转置后的 `CanImproveAllExcept` 先用全势和减去 excluded 势得到安全区间。旧状态机若以非精确区间下端拒绝当前标签，不结束 dual 阶段；较小标签再次穿过缓存下端时重新计算同一区间，必要时才回退到逐组精确和。

本候选不增加按顶点数组：第一次非精确下端拒绝只写入空闲的 stage 5；同 row 后续标签仍先复用缓存下端，首个真正穿过下端的更小标签直接调用同一个 `DualCutPotential::At`，把精确势写入原 `bound_cache` 并永久进入 stage 1。它不读取图名、组数阈值、时间、状态数或浮点近似；Base 不进入 staged directed-cut 状态机。

该变换保持精确性：缓存下端仍是可采纳下界，购买的 `At` 与旧区间歧义 fallback 使用相同原 `double` 求和；精确值只可能安全拒绝更多标签。它却不是严格的物理支配，因为第二次穿过 lower 时，旧 upper endpoint 偶尔可能直接放行，而候选会提前支付 exact。

production 与 probe 构建均通过 5/5 CTest。候选二进制 SHA-256 为 `8b3c526a0a7b7896cd770d6d53db3c9b1904e4343d9c150abf65cb9879522a24`，probe 二进制为 `08e7714dfec6aea9e302551e9a46a9f8308f24b8b2ed039336ade53cd4352711`；冻结对照分别为 `57d2afa0f7098cec9baff15fd8088695c006edf72adccdb1c5668f4ba2a4b505` 与 `37204d7f4c80d654a5ae58082ca907675ef001038467680d7458b416dd142095`。

### 2. Orkut g=15 q10 交换绑核结果

两轮均使用相同查询、Enhanced、Release/O2、普通稀疏 probe 和 900 秒求解预算；第二轮交换 CPU4/CPU5。

| 轮次 | 候选 | 对照 | row 进度比 | 候选 / 对照峰值 RSS |
|---|---:|---:|---:|---:|
| 候选 CPU4、对照 CPU5 | 209 | 212 | 0.985849 | 10061.398 / 10061.383 MiB |
| 候选 CPU5、对照 CPU4 | 210 | 209 | 1.004785 | 10044.129 / 10061.371 MiB |

交叉几何进度比为 0.995272，即候选净慢约 0.47%。两轮候选的完整 layer 2 均为 91 rows、73,563,652 scalars，与对照完全相同；候选 primitive work 为 11,572,129,606，对照为 11,572,111,577，多 18,029。事件从第 3 张 ordinary row 起出现这一稳定工作差异，但没有减少已完成层的状态数。

原始记录位于：

- `results/probes/lazy_exact_q10_round1_candidate_cpu4`
- `results/probes/lazy_exact_q10_round1_control_cpu5`
- `results/probes/lazy_exact_q10_round2_candidate_cpu5`
- `results/probes/lazy_exact_q10_round2_control_cpu4`

### 3. 结论

候选没有把 q10 的重复区间工作转化为净收益，提前 exact 的额外工作超过省下的区间重算；它也没有减少完整 layer 2 状态。源码、构建二进制和临时计数器均回退，不进入正式方法。后续不得把“发生第二次 lower crossing”本身当作购买 exact 的充分物理理由；若重试，必须先有更强的严格支配条件或新的状态削减证据。
