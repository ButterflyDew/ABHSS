# 全量实验人类 Review 细节文档

这份文档不是论文里的精简实验矩阵，而是给作者、合作者和 artifact reviewer 逐项核验用的事实清单。它说明每张图从哪里来、怎样转换、点边和组是什么、每个实验具体跑什么、为什么这样选、能够支持什么 claim，以及出现反常结果时先检查什么。机器可读真值仍以 `experiments/official_sources.json`、各 `dataset_manifest.json`、查询 panel manifest 和 `experiments/paper_matrix.json` 为准。

## 1. 一眼看懂当前设计

正式性能对照始终是 ABHSS-Light、ABHSS-Heavy 和 PrunedDP++-Safe，全部单计算线程、同一 Release 构建、逐查询 10,000 秒。Light 与 Heavy 是两条曲线，不允许逐实例取较快值。主实验只有三类：

- A：在 DBLP 与 IMDb 上严格控制 `<g,f>`，回答参数因果问题；
- B：在六个 SNAP 图、MovieLens 和 Toronto 上跨 `g`，回答来源组相关性和跨图可扩展性；
- C：在 DBpedia 与 LinkedMDB 上运行自然语言来源查询，回答外部有效性。

副实验是 tiny smoke、SteinLib 已知最优值 gate 和四格固定消融。它们不与主性能曲线混合聚合。

## 2. 最终工作量

| 家族 | 数据集 | cell | 查询 | 每条正式方法 | 总任务 |
|---|---:|---:|---:|---:|---:|
| A | 2 | 36 | 360 | 3 | 1,080 |
| B | 8 | 104 | 1,040 | 3 | 3,120 |
| C | 2 | 24 | 664 | 3 | 1,992 |
| 主实验合计 | 12 | 164 | 2,064 | 3 | 6,192 |
| smoke | 1 | 1 | 1 | 4 | 4 |
| correctness | 11 WRP | 11 | 11 | 5 全覆盖 + Basic+ 7 条 | 62 |
| ablation | 4 | 4 | 55 | 5 | 275 |

“每格 10 条”适用于 A 和 B，不是对 C 自然查询强行凑数。A 没有复刻 PrunedDP++ 每格 50 条，因为高 `g` 精确运行的成本远高于旧实验；10 条固定种子实例在两个大图上形成配对重复，且 B/C 另提供更广分布证据。

## 3. 图与查询事实总表

`components` 和 `LCC` 来自正式查询可行性扫描。`groups` 是候选来源组总数，不等于单条查询的 `g`。

| 图 | `n` | `m` | components | LCC | groups / memberships | 单组范围 | 权重 | 正式查询 |
|---|---:|---:|---:|---:|---:|---:|---|---:|
| DBLP-AMiner-V18 | 12,177,749 | 96,628,159 | 125,643 | 11,803,412 | 57,727 / 24,701,679 | 50..4,800 | 1 | A 180 |
| IMDb-daily-20260722 | 18,588,661 | 100,556,350 | 107,160 | 18,083,600 | 61,020 / 19,661,596 | 50..4,800 | 1 | A 180 |
| SNAP-Wikipedia-2018 | 19,109 | 400,497 | 3 | 11,631 | 13,183 / 1,060,946 | 1..3,349 | Jaccard 0..100 | B 130 |
| SNAP-Twitch-2018 | 34,118 | 429,113 | 6 | 9,498 | 3,163 / 687,900 | 1..31,579 | Jaccard 30..100 | B 130 |
| SNAP-GitHub-2019 | 37,700 | 289,003 | 1 | 37,700 | 4,005 / 690,358 | 1..28,188 | Jaccard 19..100 | B 130 |
| SNAP-YouTube | 1,134,890 | 2,987,624 | 1 | 1,134,890 | 5,000 / 72,959 | 2..2,217 | Jaccard 4..100 | B 130 |
| SNAP-Orkut | 3,072,441 | 117,185,083 | 1 | 3,072,441 | 5,000 / 1,078,576 | 3..4,785 | Jaccard 1..100 | B 130 |
| SNAP-LiveJournal | 3,997,962 | 34,681,189 | 1 | 3,997,962 | 5,000 / 139,012 | 3..1,441 | Jaccard 0..100 | B 130 |
| MovieLens-32M | 87,585 | 44,143,611 | 52,526 | 35,060 | 20 / 154,170 | 195..34,175 | 1 | B 130 |
| Toronto-current | 44,184 | 63,400 | 5 | 44,176 | 59 / 1,865 | 2..320 | 1,288..10,144,376 mm | B 130 |
| DBpedia-2022.12-en | 7,958,883 | 22,787,803 | 81,185 | 7,760,416 | 1,176 / 2,824,808 | 1..211,002 | `ln(1+freq)` | C 464 |
| LinkedMDB-2012 | 1,326,784 | 2,132,796 | 64,770 | 1,166,496 | 292,816 / 1,672,764 | 1..197,410 | `ln(1+freq)` | C 200 |

Wikipedia 有 4 条零权边，LiveJournal 有 2,037 条；零权合法且由当前算法回归测试覆盖。Toronto 权重单位是毫米，因此整数范围大不代表道路异常。MovieLens 的 52,526 个分量主要来自未进入 5-star co-rating 核心的电影；查询生成器必须以共同分量 gate 约束，而不能把不可行查询留给 solver。

## 4. A 数据为什么这样构造

### 4.1 DBLP-AMiner-V18

来源是 AMiner 官方 DBLP Citation Network V18，而不是任何论文 GitHub 中名为 DBLP 的接口文件。6,729,828 个 paper 顶点与 5,447,921 个 author 顶点形成异构图；22,987,638 条 authorship 边和 73,640,521 条快照内 citation 边均为单位权。作者优先用源 ID 合并，没有 ID 时才使用规范化 name+organization；fallback 数量 1,990,160，无法构造身份的作者行 32，快照外引用 432。

为什么选：它最接近 PrunedDP++ 案例中的论文—作者—引用关键词图语义，同时是当前可审计的官方完整来源。为什么不叫“复现 2016 DBLP”：历史快照字节和完整转换不可得，点边规模也不同；本文只能复刻图构造家族。

Review 重点：作者 fallback 是否可能合并同名人；citation 是否重复；单位权是否与本方法模型一致；candidate label 是否在顶点内去重；当前 manifest 是否记录 57,727 组和 24,701,679 memberships。

### 4.2 IMDb-daily-20260722

来源是冻结于 2026-07-22 的 IMDb non-commercial daily `title.basics`、`title.principals` 和 `name.basics`。图包含 11,446,677 个 title 和 7,141,984 个 person；每条 principals 源行是一条单位边。11,446,677 个 title 均有元数据，7,138,785 个 person 有 name 元数据，3,199 个 person 只保留图结构而不产生标签。

为什么选：PrunedDP++ 使用 IMDb 作为真实标签频率实验；当前官方 daily export 能独立恢复同类 title-person 图。它与旧快照不同，所以名称、来源日期和图哈希必须完整报告。

Review 重点：principals 的多重行是否原样保留；title/person ID 次序是否确定；缺失 name metadata 是否错误丢点；标签是否来自规定字段；`source_normalization.json` 的 title+person 是否严格等于 `n`。重建器曾在释放 ID 表后误写 0，当前代码已把计数保存在释放前，环境 validator 会拦截回归。

## 5. A 实验具体做什么

A 的 `g` sweep 在两个图上逐点运行 `g=4..16,f_target=400`。这 13 个点用于找出小组数区域是否不退化、从何处开始出现完成率优势，以及 `g=9..16` 的数量级变化。

A 的 `f` sweep 固定 `g=10`，运行 `f_target={100,200,400,800,1600,3200}`。选择 `g=10` 是因为它位于传统小 `g` 证据和本文扩展区间的交界，既不把 `f` 实验放在过于简单的区域，也不把 baseline 全部推成 timeout。向 100 和 3,200 各扩一档能检查组很小或很大时的趋势是否反转。

每个唯一 cell 10 条，`g=10,f=400` 不重复。查询从真实标签组抽样，实际平均频率不得偏离目标超过 2%。最终 `g=10` 的实际值如下：

| 图 | target `f` | realized mean | realized query range | 可选标签数 |
|---|---:|---:|---:|---:|
| DBLP | 100 | 99.87 | 98.4..101.4 | 29,655 |
| DBLP | 200 | 199.10 | 196.6..202.0 | 19,545 |
| DBLP | 400 | 399.96 | 395.2..405.4 | 12,982 |
| DBLP | 800 | 797.08 | 784.0..815.8 | 8,658 |
| DBLP | 1,600 | 1,588.96 | 1,568.7..1,617.7 | 5,699 |
| DBLP | 3,200 | 3,190.36 | 3,144.7..3,252.1 | 3,956 |
| IMDb | 100 | 99.25 | 98.1..101.8 | 35,740 |
| IMDb | 200 | 199.49 | 196.4..203.9 | 20,922 |
| IMDb | 400 | 399.29 | 393.5..406.9 | 12,458 |
| IMDb | 800 | 797.21 | 785.6..813.2 | 7,448 |
| IMDb | 1,600 | 1,602.74 | 1,581.5..1,631.5 | 4,413 |
| IMDb | 3,200 | 3,195.69 | 3,142.8..3,258.7 | 2,460 |

图表要分别显示 target 和 realized 值。不能把 DBLP/IMDb 合并成一个“平均图”，也不能把 A 描述为对 PrunedDP++ 50-query 参数表的严谨数值复现；严谨复刻的是图语义，参数和样本量是本论文设计。

预期有利点：`g` 是 ABHSS 要解决的核心指数维，完整 4..16 能呈现转折而不是只挑高 `g`；大 `f` 可能提高候选冗余并暴露 baseline 状态初始化成本。防守点：同时保留小 `g`、小 `f` 和两个不同大图，不能隐藏 ABHSS 的固定成本。

## 6. B 图逐项角色

Wikipedia、Twitch、GitHub 是小而密集的 feature graphs，适合观察组高度重叠时的状态收敛。Wikipedia 是三个语言主题子图的并集，Twitch 是六个国家子图的并集，因此共同分量 gate 必不可少。

YouTube、Orkut、LiveJournal 是百万点社区图。YouTube 边较稀、组较小；Orkut 边数最大且组较大；LiveJournal 点最多并含大量零权边。三者成组展示可把点数、边数、组大小和零权影响区分开。

MovieLens 的 20 个来源组是真实 genre，图由共同 5-star 评分构成，极密但 LCC 只占约 40%。它回答“少量、很大、强重叠语义组”场景，不再用均匀顶点合成组冒充 MovieLens 查询。

Toronto 使用完整当前 Centreline V2：64,400 源行中 64,307 行有效，907 行与已有端点对重复，最终 63,400 条边；895 个官方 POI 全部映射到最近道路节点，形成 59 组。它回答有真实距离权重、组较小、道路图近似稀疏平面的情形。POI 到节点距离中位数 36.636 m、p95 159.984 m、最大 1,082.604 m，最大值必须在局限性中披露。

## 7. B 实验具体做什么

八张图每个 `g=4..16` 先生成 300 条 related-group BFS 候选，再按 `log1p(f_observed)` 的十个秩层各稳定选一条。因此每图 13 cells、130 条，合计 1,040 条。选择只看来源组和大小，不看任何方法的时间或状态数。

正式样本的 `f_observed` 范围反映来源差异：Wikipedia 41..490.5，Twitch 178.2..8,080，GitHub 95.57..14,862.29，YouTube 2.43..486，Orkut 88..1,519.18，LiveJournal 3..396.5，MovieLens 3,607..18,905.25，Toronto 11..191.5。这些值不能被当成固定 `f`；B 只对 `g` 作严格横向解释。

主要输出是每图 Light/Heavy/Safe 的完成率与 runtime-vs-`g` 曲线，再给 dataset-equal-weight 宏平均。不能只给 query-weighted 总体，因为 8 图每格数量虽然相同，图的解释角色不同；也不能把 Orkut 的 1.17 亿边当作更高统计权重。

若某图与总体趋势相反，优先检查组重叠、组间距离、LCC、Jaccard 零权边、平均度和每次预处理成本，而不是删除该图。MovieLens 与 Toronto 是特意加入的非 SNAP 结构对照，不能在结果不利时降为 appendix。

## 8. C 自然查询来源与数量

DBpedia 使用官方 DBpedia-Entity v2 stopped 文本，不再自行二次去停用词。467 条源行中一条 `g=0`；`INEX_XER-109` 和 `QALD2_tr-17` 两条映射后没有共同连通分量，作为带原因的审计排除。464 条可执行查询全部进入主实验：

| `g` | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| DBpedia 查询 | 5 | 76 | 80 | 59 | 60 | 71 | 54 | 27 | 14 | 13 | 2 | 3 |

LinkedMDB 的 116,137 条源问题来自 Meta ParlAI 官方 WikiMovies 镜像。映射后固定 200 条，配额如下；`g=9..12` 的 35 个唯一映射全部保留：

| `g` | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| LinkedMDB 查询 | 20 | 20 | 20 | 20 | 20 | 20 | 20 | 25 | 26 | 6 | 2 | 1 |

为什么不用旧作者查询：正式规则要求查询来自源集合，旧转换文件只能做审计。为什么可以用 Meta 镜像：其 57,070,041-byte archive SHA-256 与记录的失效原始 MovieQA archive 完全一致。

## 9. C 实验具体做什么

C 不是 `f` 控制实验。它保留自然查询映射后的组大小、重复词和 `g` 分布，用于检查 A/B 结论能否转移到用户语言。DBpedia 不抽样；LinkedMDB 因源问题超过 11 万而遵循 cited practical paper 的 200-query预算，但在运行前分层，并保留全部稀有高 `g`。

自然低 `g` 与高 `g` 都在主实验，而不是把 `g=1..3` 或 `g=11..12` 藏到 appendix。低 `g` 是“不退化”证据，高 `g` 是存在性和案例证据。由于 DBpedia/LinkedMDB 的 `g>=11` 数量很少，单独高 `g` 不作显著性很强的总体结论；应同时报告原始分母和个例轨迹。

主要输出是按数据集、`g` 分层的完成率、配对 runtime、timeout/OOM 和目标一致性。对所有 664 条给完整结果；双方完成子集的 speedup 只能作为补充。

## 10. 副实验具体做什么

`S0_smoke` 在 11 点 13 边 tiny 图上跑 Light、Heavy、Safe、DPBF，只检查二进制、接口、Ready marker、计时和记录格式，任何速度都不进论文。

`S1_steinlib_exactness_gate` 使用 11 个公开 WRP3/WRP4 实例，覆盖 `g=11..16`。Light、Heavy、Safe、DPBF 和 SCIP-Jack 全覆盖，Basic+ 在支持的 `g<=14` 上覆盖 7 条。每个结果必须匹配从发布 optimum 减去 dummy-terminal connector cost 后的 GST optimum。

`S2_ablation` 固定 DBLP `g=10,f=400`、IMDb `g=10,f=400`、Wikipedia `g=12`、LinkedMDB `g=8`。四格分别覆盖 A/B/C，而不是根据探针挑最有利数据。Light 对比 no-early 与 no-recurring-witness；Heavy 对比 forward-only anchoring。成对报告 runtime、完成率和状态计数，不把不同基础版本相减后相加。

## 11. 公平计时与恢复语义

一个 native 进程先加载一张图，输出 `[Ready]` 后连续处理同一 cell 查询文件中的查询。因此图加载不在 solver time 内，并且通常每个 `(cell,method)` 只加载一次；若某条查询 timeout，runner 杀死进程、记录该条删失状态，再从下一条重启。graph-load watchdog 为 1,800 秒，只区分损坏/无法加载与求解 timeout。

所有方法同一查询 10,000 秒。记录 solver seconds、watchdog wall seconds、query memory peak、峰值 RSS、目标值和失败状态。主全量只运行一次；代表性 calibration 可重复，但选择性重跑论文值时必须把完整 cell 的三种方法一起重跑并保留旧记录。

正式运行前必须冻结 CPU、RAM、OS、编译器、build type、commit、环境变量和线程数。不能在同一物理机并发多个内存带宽重任务后仍把结果当单机公平计时。

## 12. 结果应该怎样画

A 主图：两个数据集分面，`f=400` 时 runtime/完成率随 `g=4..16`；`g=10` 时随 `f=100..3200`。横轴 `f` 用对数 2 倍刻度，图注写 realized tolerance。

B 主图：八图 small multiples 展示 `g=4..16`；另给 dataset-equal-weight performance profile。图例同时展示每格 `f_observed` 中位数或范围，防止审稿人把结构差异误解为算法随机性。

C 主图：DBpedia/LinkedMDB 分面，按自然 `g` 给完成率和配对分布；在 `g=11,12` 点旁标原始 `n`。附表给完整状态分母。

全局表：每个 family 和每个数据集列 solved/timeout/OOM/error、双方完成 geomean speedup、capped runtime 和峰值 RSS。禁止只列成功样本平均时间。

## 13. 高 `g` motivation 的安全表述

可以说：精确 GST 的子集状态随 `2^g` 增长；真实 DBpedia/LinkedMDB 已经出现 `g=9..12`；旧实验常止于更小 `g`，因此 `g=9..16` 是精确可扩展性的缺口；A/B 在真实官方图上系统扩展到 16，C 验证自然查询确实进入该区间。

不能说：自然用户通常有 `g=16`；历史工作已经证明 9–16 都是日常负载；当前 DBLP/IMDb 是旧论文同一张图；五条自然高 `g` 足以单独给强统计结论。

## 14. 如果 Safe 很好而 ABHSS 差

方法层必须检查状态扩展数、队列 push/pop、lower-bound 计算与命中、upper-bound 出现时刻、witness/anchor 命中、Light/Heavy 特定预处理时间、哈希冲突、内存与 cache/page fault。重点区分“固定预处理拖慢小 `g`”“下界不适合高重叠组”“Heavy 额外状态没有换来剪枝”等原因。

数据层必须检查平均度、LCC、组大小与重叠、组间最短距离、hub 集中、边权离散与零值、自然 token 的高频程度，以及 `g` 与 `f_observed` 的相关性。A 能隔离参数，B 能定位图结构，C 能定位自然映射偏差。

任何不利数据都保留。可以修实现缺陷或调整论文 claim；若算法本身改变，所有受影响固定单元统一重跑。不能据此换数据、换 query seed、减少某方法 timeout 或只重跑 ABHSS。

## 15. 人类 reviewer 最后逐项勾选

- [ ] 12 张正式图均来自所有者官方站点，raw hash 可查。
- [ ] DBLP/IMDb 只写“图构造家族复刻”，不写历史字节或参数复现。
- [ ] A 是 18 cells/图、10 queries/cell，默认 `g=10` 的六点 `f` sweep。
- [ ] A 的每条 realized `f` 在 target ±2%，且 360/360 共同分量可行。
- [ ] B 是八图 × 13 个 `g` × 10 条，MovieLens/Toronto 不再缺失。
- [ ] B 的 `f` 只叫 observed，不与 A 混做受控回归。
- [ ] C 的 DBpedia 464 条和 LinkedMDB 200 条都包含低/高 `g`。
- [ ] 两条 DBpedia 不可行源查询及原因保留在审计，而非 solver timeout。
- [ ] Light、Heavy、Safe 同线程、同 deadline、同计时边界；没有 best-of-two。
- [ ] correctness mismatch 会阻断性能结论。
- [ ] 所有图表给 timeout/OOM/error 分母，并同时考虑数据集等权聚合。
- [ ] 旧作者图、合成 MovieLens、GPU timing 和近似质量表未进入主聚合。
