# ABHSS 完整实验方案与人工审阅细则

## 1. 论文问题与冻结边界

目标论文是 SIGMOD/VLDB 的单线程精确 GST 算法工作。核心比较对象是 PrunedDP++-Safe。ABHSS 只有一个算法入口；正文分别报告不启用增强的 Base 和依次启用 directed-cut、adjoint-completion 的全增强配置，不允许按查询取两个配置中较快值。

当前实验只回答三个问题：

1. 在最近两项直接相关工作的完整 workload 上，ABHSS 的总体效率和完成率是否优于 PrunedDP++？
2. 当组数从常见范围继续增长到 `g=15` 时，优势是否稳定扩大？
3. 在固定图上同时改变 `g` 与平均组大小目标 `f`，结论是否仍成立？

最小消融已在第 6 节单独预登记，但不计入 P1/P2/S2 主矩阵；近似解质量、GPU/异构比较和额外应用实验尚未冻结。正确性 gate 是运行前条件，不作为性能贡献。

## 2. 统一执行规则

- 计时项：同一 `abhss` 二进制的 Base 配置、全增强配置，以及 PrunedDP++-Safe。
- 配置冻结：`abhss_base` 和 `abhss_enhanced` 必须指向同一可执行文件，参数分别固定为 `--enhancements=none` 与 `--enhancements=all`；运行中不得切换。两者差异只能是安全新增证书或同一逻辑职责的 realization 替换，不能存在 Base 独有而 Enhanced 无对应物的状态族；完整映射见 [`METHOD.md`](METHOD.md) 第 3.1 节。
- 计算资源：每个 solver process 只允许一个计算线程；监控线程只采样 RSS。
- 构建：同一编译器、Release、相同 IPO/优化规则。
- 计时：图在一个 query block 中加载一次；逐查询 timer 在 `[Ready]` 之后开始，包含该方法的查询预处理与求解，不包含共同图加载。
- 运行监控：native solver 在同一个 query block 内常驻并连续处理多条查询，因此 `ps etime`、tmux pane 存活时间和 native 进程启动时间都只是整个 block 的累计墙钟，不能解释为当前单条查询耗时。单条时间与 timeout 状态只能读取 supervisor 的任务记录、已完成的 `[Query i]` 行或 watchdog 当前 deadline；没有这些直接证据时只报告“当前查询尚未完成”。
- 预处理公平性：若采用 2-core、叶剥离、度二压缩等改变共同输入图的 kernel，必须对 ABHSS 与 PrunedDP++ 使用同一实现并计入同一口径；不得只替本文方法缩图。endpoint-floor 属于 ABHSS 的算法内部下界，Base/Enhanced 都执行且其构造时间计入各自 query timer，不要求 baseline 实现本文下界。
- I/O 审计：`graph_load_seconds` 与 `query_load_seconds` 单独写入 header；它们用于发现 artifact 工程瓶颈，不并入算法 speedup。图加载包含 8 MiB 数字扫描、精确邻接容量预留和一次 $O(n+m)$ 连通分量建索引；逐查询可行性检查只按组成员求分量交，不重复扫描整图。
- timeout：每条查询 10,000 秒；图加载 watchdog 1,800 秒。
- 顺序：同一 case 的首个方法由稳定哈希轮换，避免固定方法总是占用冷机或热机位置。
- 正确性：所有完成的可行查询必须目标值一致；已审计无解的查询必须一致返回 infeasible。
- 失败记录：timeout、graph-load-timeout、OOM、error 分开保存，不能删掉失败行后计算平均值。
- 数据身份：名称、来源和最终 `graph.txt` SHA-256 共同构成图身份；两个 DBLP 永不合并。

机器矩阵是 [`experiments/paper_matrix.json`](../experiments/paper_matrix.json)，输入哈希与查询分布是 [`experiment_data/p1_published_workloads/manifest.json`](../experiment_data/p1_published_workloads/manifest.json)。单入口重构、“共同操作 + 安全新增或同职责替换”配置契约，以及严格公共 A1 在 SteinLib、Musae、YouTube、Orkut 上的正确性与 A1-active 非退化结果固化在 [`experiments/abhss_configuration_refactor_gate.json`](../experiments/abhss_configuration_refactor_gate.json)。Reddit 两条查询在 A1 前即闭合，作为不归因于 A1 的负对照保留全部波动数字，不能把它冒充 A1 性能证据。

### 2.1 来源证据链与可声称强度

| 实验对象 | 当前使用的直接来源 | 本仓库如何转换/冻结 | 可以声称什么 | 尚未解决什么 |
|---|---|---|---|---|
| MonoGST+ P1 五图与全查询 | *A Practical Sublinear Approximation for Group Steiner Tree* 作者提供的处理后接口；论文已被 VLDB 2026 录用但截至冻结日未公开 | 不重新下载图、不重生成查询；`build_published_workloads.py` 只转为统一文件名并记录 SHA-256 | “使用 MonoGST+ 作者实验接口的字节级冻结” | 论文/数据的公开获取路径和再分发许可仍需作者确认；不能只用公开前作 [GroupSteinerTree artifact](https://github.com/YahuiSun/GroupSteinerTree) 冒充这五个最终输入 |
| GPU4GST P1 八图与全查询 | [GPU4GST 作者 artifact](https://github.com/toziki/GPU4GST-sigmod) 指向的 OneDrive `.in`/`.g`/CSV 产品 | `prepare_gpu4gst` 保留图边语义，将每个 `g={3,5,7}` CSV 的前 300 行转为统一 query 接口；原文件元数据和哈希在 `data_origin` | “使用作者 artifact 实际发布的成品图与全部登记查询” | GitHub 仓库/数据包未见清晰的仓库级再分发许可；公开 artifact 前应获取许可或只发布下载步骤与哈希 |
| P2 related-group 扩展 | [Approximating Probabilistic Group Steiner Trees in Graphs](https://www.vldb.org/pvldb/vol16/p343-sun.pdf) 的组共现图、均匀根组、最小可用 BFS 深度和近邻均匀抽样协议 | 在 GPU4GST 作者 `.g` 候选组上用新 seed 生成 300 条/格，再只按实现后平均组大小分为五层，以两轮稳定哈希选择每层 2 条；第二轮不改变第一轮的 5 条 | “按原论文的 related-group 方法类生成的新扩展 panel” | 原论文/GPU4GST 没有公开能恢复其已发表查询的完整 seed 链；P2 不能写成“原作者查询”或“复现其具体随机样本” |
| S2 IMDb | [IMDb 官方 non-commercial datasets](https://developer.imdb.com/non-commercial-datasets/) 在 2026-07-22 取得的每日快照 | 从 `title.basics`、`title.principals`、`name.basics` 构建 title–person 无向单位权二部图，密集重编号并冻结三个 raw hash | “在明确日期的官方 IMDb 快照上做敏感性实验” | 该页面是可变的每日导出且适用 non-commercial 条款；它不是 PrunedDP++ 2016 的历史 IMDb 快照，不得声称复现旧论文绝对数值 |
| SteinLib 正确性 gate | [SteinLib 官方 test sets](https://steinlib.zib.de/testset.php) 的 WRP3/WRP4 | `convert_steinlib.py` 保留实例与已知最优值映射 | “在公开已知最优实例上做多实现精确性核验” | 只是 correctness panel，不应将其小规模时间当作大图性能结论 |

上表的机器可读版本是 [`experiments/data_sources.json`](../experiments/data_sources.json)。正式论文的 reproducibility 附录应对每个 P1 图同时报“论文表数值”、“作者成品实测数值”和“最终接口 SHA-256”，不应用同名推断图相同。

### 2.2 Baseline 身份与提交前必须决定的事项

当前主性能 baseline `pruneddp_safe` 是本仓库对 PrunedDP++ 公开算法路径的纠错重建：它保留 admissible lower bound，关闭会与 permanent-closed 相互作用并产生反例的 pathmax，且在出现更小 `g-cost` 时允许 reopen。它已经与 ABHSS、DPBF 和 SCIP-Jack 在 11 个 WRP 已知最优实例上一致，但必须准确称为“corrected reconstruction”，不能称为“2016 原作者原码”。

GPU4GST 2025 artifact 现在包含一份 CPU PrunedDP++ header，仓库中的 `gpu4gst_pruneddp_artifact` adapter 已在 wrp4-11 和 Musae `g=5` 小 panel 上核对权值。它仍有两个直接限制：上游只支持非负整数边权且 $g\le14$，无法无条件覆盖 P1/P2/S2 全矩阵。提交前应与导师做一次显式决策：是否将它增加为“适用子集上的 artifact 校准列”。若不加，论文必须说明为什么主表选择可覆盖 $g=15$ 和小数边权的 Safe reconstruction，并在 artifact 附录保留上述一致性证据。

当前还有一个与论文 claim 直接相关的边界：ABHSS 二进制返回精确权值与 feasibility，但不序列化最优树的边集。若最终论文定义的问题输出要求“返回树”，则在 artifact freeze 前必须增加决策回溯/第二遍等式恢复；否则正文应明确将实验输出写成 exact objective value。

## 3. 主实验 1：完整 published workload

### 3.1 它在回答什么

这是论文的总体性能主表，目的不是研究某一个参数，而是尽可能直接地复刻 MonoGST+ 与 GPU4GST 已经使用的图、图转换、边权、组语义和具体查询。这样性能差异更难被归因于本文重新下载图或重新生成查询。

所有 8,318 条作者查询都运行。GPU4GST 的 `g=3,5,7` 文件在运行和审计时仍是三个 query blocks，但论文主表不按 `g` 拆列；每个图、每个 ABHSS 固定配置及 baseline 最终各有一行。

### 3.2 十三张图

| 正式身份 | 来源 | `n` | `m` | 查询数 | 查询构成 | 已知无解 |
|---|---|---:|---:|---:|---|---:|
| Toronto-MonoGSTPlus | MonoGST+/ImprovAPP artifact | 46,073 | 68,353 | 160 | `g=5..8`，每个 `g` 40 条受控查询 | 0 |
| MovieLens-MonoGSTPlus | MonoGST+/ImprovAPP artifact | 62,423 | 35,323,774 | 160 | `g=5..8`，每个 `g` 40 条受控查询 | 0 |
| DBLP-MonoGSTPlus | MonoGST+/ImprovAPP artifact | 2,497,782 | 12,786,329 | 160 | `g=5..8`，每个 `g` 40 条受控查询 | 0 |
| LinkedMDB-MonoGSTPlus | MonoGST+/KeyKG+ workload | 1,326,784 | 2,132,796 | 200 | WikiMovies 自然查询，`g=1..10` | 46 |
| DBpedia-MonoGSTPlus | MonoGST+/KeyKG+ workload | 5,887,296 | 18,338,729 | 438 | DBpedia-Entity v2 自然查询，`g=1..10` | 9 |
| Musae-GPU4GST | GPU4GST author artifact | 19,109 | 400,497 | 900 | `g={3,5,7}` 各 300 条 | 0 |
| Twitch-GPU4GST | GPU4GST author artifact | 34,118 | 429,113 | 900 | `g={3,5,7}` 各 300 条 | 0 |
| Github-GPU4GST | GPU4GST author artifact | 37,700 | 289,003 | 900 | `g={3,5,7}` 各 300 条 | 0 |
| Youtube-GPU4GST | GPU4GST author artifact | 1,134,890 | 2,987,624 | 900 | `g={3,5,7}` 各 300 条 | 0 |
| DBLP-GPU4GST | GPU4GST author artifact | 2,497,782 | 12,786,329 | 900 | `g={3,5,7}` 各 300 条 | 0 |
| Orkut-GPU4GST | GPU4GST author artifact | 3,072,441 | 117,185,083 | 900 | `g={3,5,7}` 各 300 条 | 0 |
| LiveJournal-GPU4GST | GPU4GST author artifact | 3,997,962 | 34,681,189 | 900 | `g={3,5,7}` 各 300 条 | 0 |
| Reddit-GPU4GST | GPU4GST author artifact | 4,262,834 | 12,502,767 | 900 | `g={3,5,7}` 各 300 条 | 0 |

两个 DBLP 虽然实测 `n,m` 相同，但最终图哈希不同：MonoGST+ 为 `f3f60f5b…e5e0e9`，GPU4GST 为 `841ddb99…629a82e`。这通常来自边权、边顺序或转换差异，因此必须作为两个图身份，并分别使用各自查询。

GPU4GST 论文表与作者成品还存在两个已记录差异：作者 DBLP 成品为 2,497,782 点而论文表写 2,423,455；Orkut 成品为 3,072,441 点而论文表写 3,072,440。主实验使用作者实际成品，表格同时保留论文值与实测值，不能悄悄“修正”为另一张图。

### 3.3 55 条无可行解的自然查询

共同分量审计在作者原图上发现：LinkedMDB 查询 200 条中有 46 条、DBpedia 438 条中有 9 条，不存在一个同时与所有查询组相交的连通分量。这是原始 workload 的属性，不是本文生成器造成的。

由于本实验承诺使用全询问，处理规则是：

- 保留全部 55 条，manifest 固定其 1-based query index；
- 三种精确方法都必须结束并返回 infeasible，返回可行权重属于正确性错误；
- infeasible detection 的时间计入图总时间；
- 不把这 55 条替换成容易查询，也不从分母删除；
- P2 和副实验的新增查询仍要求 100% 可行。

### 3.4 主表怎么报

每个图、每种方法报告：`n`、`m`、查询总数、完成数、其中 infeasible 数、timeout、error、峰值内存摘要和总时间。

- 如果全部查询完成，主时间列使用 `observed_total_seconds_if_all_solved`。
- 如果存在 timeout/error，主排序先看完成数；同时报告已完成查询总时间和 `capped_total_seconds`。后者给每个未完成查询计一次 10,000 秒，而不是把部分时间伪装成全图总时间。
- `g` 只留在审计文件和补充分析中，不进入 P1 主表拆分。

预期 claim：在不改变已有论文 workload 的情况下，ABHSS 在小组数不明显退化，并在更难图/查询上获得显著总体完成率或数量级优势。若某图不支持该结论，应保留并分析，不得因为 P2 另有高 `g` 实验而移除。

## 4. 主实验 2：`g=5..15` 扩展

### 4.1 六图选择

P2 只使用同一 GPU4GST 作者图族，使图转换、候选组和 related-group 生成语义一致。图在查看任何 ABHSS/PrunedDP++ 时间之前按顶点规模分层固定：

| 规模层 | 图 | `n` | `m` | 选择理由 |
|---|---|---:|---:|---|
| small | Musae-GPU4GST | 19,109 | 400,497 | 最小图之一，检验固定开销和小图不退化 |
| small | Twitch-GPU4GST | 34,118 | 429,113 | 六个来源子图并集，连通结构与 Musae 不同 |
| medium | Youtube-GPU4GST | 1,134,890 | 2,987,624 | 百万点、相对稀疏 |
| medium | DBLP-GPU4GST | 2,497,782 | 12,786,329 | 论文主流规模和大量候选组 |
| large | Orkut-GPU4GST | 3,072,441 | 117,185,083 | 极稠密大图，压力主要来自边数 |
| large | Reddit-GPU4GST | 4,262,834 | 12,502,767 | 点数更大但较稀疏，与 Orkut 互补 |

不选 GitHub、LiveJournal 不是因为结果：GitHub 与两张 small 图规模重复；LiveJournal 与 large 层重复且密度位于 Orkut/Reddit 之间。P1 已经完整运行它们，P2 只承担参数曲线职责。

### 4.2 查询生成与数量

对每张图和每个 `g=5..15`：

1. 使用 GPU4GST `.g` 中的作者候选组构造组共现图；共享原图顶点的两个组相邻。
2. 均匀选择根组，BFS 到能够提供足够相关组的最小深度，再随机抽取组。
3. 拒绝没有共同原图连通分量的查询。
4. 用生成 seed `2025` 固定产生 300 条候选；只用输入侧 `log1p(realized mean group size)` 排序。
5. 分成五个等秩层，每层按 panel seed `20260723` 的稳定 SHA-256 key 排序；第一轮每层选第一名构成原 q1--q5，第二轮每层选第二名并追加为 q6--q10。第二轮不改变第一轮的查询身份和顺序。

因此每图 11 cells、110 条，六图共 66 cells、660 条。十条/格用于画趋势和完成率；原五条形成 tranche 1，追加五条形成 tranche 2，既可合并报告，也能单独核验追加样本没有推翻原趋势。逐条 tranche、选择索引、组大小、group ID、seed 和哈希均保存在 `experiment_data/p2_cross_g`。

### 4.3 报告

每图画一条随 `g` 变化的曲线，至少同时展示：

- 完成数/10，并同时保留 tranche 1 与 tranche 2 各自的完成数/5；
- PAR-2；
- 双方都完成时的成对加速比；
- `baseline timeout / ABHSS solved` 与反方向数量；
- 实际平均组大小范围，防止把 `f` 的随机变化误解为纯 `g` 效应。

P2 的核心 claim 是从 `g=5` 到 15 的趋势和转折位置，而不是六图平均后的单一倍率。小图必须保留，即使 ABHSS 固定成本导致轻微劣势。

### 4.4 已完成的 Orkut `g=15` Enhanced 硬门

Orkut `g=15` 是当前 P2 中已知最重的 cell。冻结生产二进制已在相同的 10,000 秒逐查询 TL 下完成 q1--q10；最慢的 q10 为 9,544.561 秒，返回精确权值 54 和 1,459,398,194 个主状态。该结果证明当前 Enhanced 配置能够覆盖预登记 panel，不允许据此删除 q10、放宽 TL 或按 `mean_f` 添加经验开关。逐条时间、空间与状态见第 8.4 节；旧错误二进制、优化轨迹和诊断分解已移入[历史门禁卷](archive/ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-human-experiment-plan-legacy-gates-20260820)。

## 5. 副实验： $\langle g,f\rangle$ 受控敏感性

### 5.1 图和网格

| 图 | `n` | `m` | 用途 |
|---|---:|---:|---|
| DBLP-MonoGSTPlus | 2,497,782 | 12,786,329 | 在 published graph 上观察 `g,f` 交互 |
| IMDb-latest-20260722 | 18,588,661 | 100,556,350 | 补入 PrunedDP++ 脉络常见的大型 IMDb 图；没有冒充 2016 快照 |

网格为 `g={6,10,14}` 与 `f={200,400,800,1600,3200}` 的笛卡尔积，每格 5 条。因此每图 15 cells、75 条，总计 30 cells、150 条。

### 5.2 MonoGST+ 查询协议

对每个组独立采样：

```math
|S_i|=\mathrm{clamp}\left(\mathrm{round}(N(f,(0.15f)^2)),0.5f,1.5f\right).
```

随后从 `1..n` 中无放回均匀选择 `|S_i|` 个顶点。不同组之间允许重叠。本文不再把多个组大小强行平衡为“每条查询恰好平均等于 `f`”，因为那会改变 MonoGST+ 原生成方法；manifest 同时记录目标 `f` 和真实 `mean_f/min_f/max_f`。

该实验不是复刻 PrunedDP++ 的历史数值：DBLP 使用 MonoGST+ 作者图，IMDb 使用 2026-07-22 官方冻结。它只研究本文算法对 `g` 与目标平均组大小的敏感性。

### 5.3 报告

使用 2×3 或分面图：每个数据集分别给 `g=6,10,14`，横轴 `f` 使用 2 倍对数刻度。每点报告完成数/5、PAR-2、成对加速比和实际 `f` 范围。不能只展示 `g=14` 或 `f=3200` 的有利区域。

### 5.4 当前远端前置探针与正式 S2 的边界

在正式 150 条 S2 之前，允许运行一个固定且不可扩选的资源探针：每个 `(dataset,g,f)` cell 只取预生成文件中的第 3 条查询，共 30 条/方法；每查询 TL 为 1,000 秒。先完整运行 Enhanced，再以预先声明的规则冻结 Base 与 PrunedDP++ 的共同探针 TL：若 Enhanced 有 timeout，则取 1,000 秒；否则取 `min(1000, ceil(2 * Enhanced 最大完成时间))`。该规则只用于避免探针先消耗数天，不是算法参数，也不得按结果逐 cell 调整。

探针必须单列 `target_f`、实际 `mean_f`、完成/timeout 方向、时间、峰值内存和实际 `(mask,v)` 状态数。它只用于判断正式 S2 的可运行性、识别长尾并检查预期趋势，不能替代每格 5 条、每查询 10,000 秒的正式 S2，不能与 P1/P2 正式主表混算。

## 6. 最小消融实验

### 6.1 固定面板与选择原则

消融只回答方法章节中的两个核心因果问题，不逐个关闭缓存、布局或调度微优化。面板在读取消融结果前固定为 P2 的 Musae-GPU4GST、Youtube-GPU4GST、Orkut-GPU4GST，取 $g\in\{6,10,14\}$，每个 cell 只取原 tranche 1 的 q1--q5。三图分别代表 small、medium sparse 和 large dense；三个 $g$ 覆盖低、中、高组数；五条查询恰好来自五个输入组大小等秩层。共 9 cells、45 条查询，不根据方法运行时间换图、换查询或增加有利样本。

消融沿用正式服务器、编译器、单线程计时边界和每查询 10,000 秒 TL。生产 Base/Enhanced 若与消融处于相同 commit、二进制和机器，可直接复用 P2 正式记录；否则必须重跑，不能跨环境拼接。所有变体先通过仓库 CTest、固定零权反例和独立小图精确对照，完成查询的目标值必须与生产配置一致。

### 6.2 消融 A：配置链贡献

比较同一 `abhss` 二进制的三种预声明配置：

1. Base：`--enhancements=none`；
2. DirectedCutOnly：`--enhancements=directed-cut`；
3. Enhanced：`--enhancements=all`。

`Base -> DirectedCutOnly` 隔离安全新增的 directed-cut 证书及其同职责 realization；`DirectedCutOnly -> Enhanced` 隔离用 Adjoint $H$ 替换 A1 之后高层前向 $A$ 的效果。该面板新增 45 个 DirectedCutOnly 任务；Base/Enhanced 使用主实验同一批查询。不能把三种配置的逐查询最小值合成为新方法。

### 6.3 消融 B：公共 A1 continuation 的 endpoint-floor

为了检验公共 A1 锥体而不破坏精确递推，构造一个隔离的 build-time 变体：`AnchoredSingletonContinuation` 仍保留 `FarthestRemaining`，但删除与 `TourLowerBound::EndpointFloorAt` 的最大值。该变体不是运行时开关，不读取图名、 $g$、状态量或时间；除这一行安全下界外，源码、编译选项和配置均与生产版本一致。它同时运行 Base 与 Enhanced，因而不会把共同操作伪装成某一配置独占的贡献。

完整对照为 `production` 与 `without-endpoint-floor` 乘以 `{Base, Enhanced}`，共新增 90 个变体任务。机器可读选择、唯一允许的源码差异和构建身份记录在 [`experiments/ablation_plan.json`](../experiments/ablation_plan.json)。若 isolated build 的 diff 不再恰好是一处语义修改，消融不得启动。

### 6.4 报告与停止规则

两个消融分别报告完成数/5、PAR-2、双方完成查询的成对时间、峰值 RSS、`mask_vertex_states` 和 timeout 方向；正文优先用一张两面板图，逐查询明细放补充材料。消融只用于解释机制，不替代 P1/P2/S2，也不据此改变正式配置。任何权值/可行性不一致立即判为正确性失败并停止该变体；性能变差仍须原样报告。

## 7. 工作量与运行次序

| Suite | 角色 | cells/query blocks | 查询 | 运行任务 |
|---|---|---:|---:|---:|
| `P1_monogstplus_published` | 主 1 | 5 | 1,118 | 3,354 |
| `P1_gpu4gst_published` | 主 1 | 24 | 7,200 | 21,600 |
| `P2_cross_g` | 主 2 | 66 | 660 | 1,980 |
| `S2_controlled_gf` | 副 | 30 | 150 | 450 |
| 合计 | 性能 | 125 | 9,128 | 27,384 |

本轮冻结二进制的 P1 已完整结束，因此不再重跑。新 campaign 的顺序是：身份/正确性 gate 与历史记录复用审计 → 剩余 P2 和全部 S2 的 Enhanced（按只读历史估计从快到慢）→ P2 的 Base/PrunedDP++ 递增 `g` frontier → S2 的 Base/PrunedDP++ → 最小消融。所有长任务使用 CPU 4/5 两个已校准物理核、独立 worker directory 和逐任务记录断点续跑。

## 8. 统计、图表与 claim guardrails

- P1 的主要量是数据集级总工作量，不对 13 图按查询数再次加权成一个“总体平均倍率”。
- P2 的十条/格与 S2 的五条/格均先报告原始分母和 timeout 方向；共同完成的 geomean speedup 不能代表 timeout 查询。P2 还保留 tranche 字段，必要时可分别报告原 q1--q5 与追加 q6--q10，检查扩样是否改变结论。
- 同时保留 query-weighted 与 dataset-equal-weight 的补充汇总，但正文结论以逐图/逐层趋势为主。
- 任何目标值或 feasibility 不一致都会冻结相应性能结论。
- 不比较不同完成子集的平均时间。
- 不把 ABHSS Base/全增强配置的逐查询最小值当作第三种曲线。
- 若 PrunedDP++ 在某图明显更好，保留该图并从 `n,m`、密度、候选组大小、连通分量、实际 `f`、状态数和上界命中率分析原因。
- 每条完成查询同时保存 `mask_vertex_states`：ABHSS 为首次进入 $D$、 $A$、 $H$ 行的状态项总数，状态族属于实际键，因此不同状态族中数值相同的 `(mask,v)` 分别计数；PrunedDP++ 为主 StateStore 的实际 `(mask,v)` 项数。两边都排除组距离/route/tour/dual、重复队列项和 full-mask 完成候选。该指标用于解释各自状态域的削减，不是跨算法完全同成本的基本操作，也不替代时间或内存。
- cell 级报告完成查询的中位/p90 状态数，并在双方均完成且状态数为正的配对上报告 `PrunedDP++ / ABHSS` 状态倍率。timeout 没有最终状态数，必须单列完成分母，不能以完成子集冒充完整 workload，也不能把 Dense 容量当 PrunedDP++ 实际状态数。

### 8.1 正确性 gate 与运行准入

每次修改 solver、图/查询 I/O、状态统计或编译选项后，先运行五个仓库内 CTest：

1. 求解器与可行性审计共用的快速数字读取器保留原边、`edge_id`、双向邻接顺序、自环双邻接项、零/小数/科学计数边权和连通分量缓存，并拒绝尾部 token/非法权重。
2. 查询读取器保留合法多查询与空查询，并拒绝负查询/组计数、空组、截断 payload 和声明记录之后的多余 token。
3. Base、DirectedCutOnly 和 Enhanced 都通过以四个逻辑组保留的历史零权 witness 父指针环反例；50,000 顶点逆序零权并查集链还必须在不依赖递归栈深度的情况下闭合为 0。
4. 三个合法配置在 5,000 个确定性随机连通小图、 $2\le g\le10$ 上逐例匹配独立全子集 DP；另以 500 个 $6\le g\le10$ 的正权、互异单终端实例压测 Enhanced 高层，并在 $g=7,8,\ldots,16$ 上各运行 16 个正权互异单终端实例，共 160 个 omitted-half transpose 高组压力实例。显式 $g=2,3$ 非零最优实例要求全部配置在零主状态处共同闭包；directed-cut 固定图还要逐弧复算全部组势梯度与最终 residual。测试另外锁定一个 12 点、7 组的辅助半层反例：独立全子集 DP 真值为 5.75，旧式从逻辑层直接启动 H 会返回 6.25，当前 Enhanced 必须恢复 5.75；并逐一核对 $0\le g\le16$ 的 A/H 职责、ordinary 截止、辅助 $H(h)$ 物理层和每个逻辑层的唯一 realization，以及空/单组、重叠零代价、非连通无解、 $g>16$、未知增强位、非法 adjoint-only 配置和小于 $10^{-9}$ 的严格正 gap。
5. ABHSS Base/Enhanced 的状态数重复运行稳定；PrunedDP++ Hash 与 Dense 后端报告相同实际状态数；平凡查询报告 0。

然后运行 `S1_steinlib_exactness_gate`。当前冻结证据包含 11 个 $11\le g\le16$ 的 WRP 已知最优实例：ABHSS Base/Enhanced、PrunedDP++-Safe、DPBF 和 SCIP-Jack 已全部匹配；Basic+ 只在其 $g\le14$ 能力范围内参加。当前证据摘要在 [`experiments/correctness_audit.json`](../experiments/correctness_audit.json)。未恢复第三方二进制时可先跑仓库内 gate，但不能因此声称完成了六方 SteinLib 核验。

下列任一条会阻止正式性能 claim：完成查询的权值不一致；feasible/infeasible 不一致；生成的 P2/S2 出现无共同分量查询；当前 `paper_matrix.json` 哈希与 feasibility audit 不匹配；或运行二进制的配置/header 与矩阵登记不一致。

### 8.2 Linux 编译与服务器锁定

正式服务器推荐用顶层 GNU `Makefile`：`make release JOBS=<physical-core-count>` 完成 Release 配置、当前可用 target 编译和 CTest；`make validate-paper-binaries` 只要求两个正式性能二进制 `abhss`/`pruneddp`，`make validate-all-binaries` 才要求已恢复的 Basic+/SCIP-Jack。CMake 禁用 compiler extensions，在工具链支持时对正式本地 target 统一开启 Release IPO/LTO，并在 GCC 9/10 没有可链接浮点 `from_chars` 时自动回退 `strtod`。

`.github/workflows/linux-ci.yml` 在 Ubuntu 24.04 上执行同一 `make release`。它是每次 push/PR 的平台门禁，不替代正式服务器的环境记录。首次服务器运行必须在结果 metadata 中记录 CPU 型号、物理核、RAM、Linux 发行版、kernel、compiler/CMake 版本、Release/IPO 状态，并用 `--dry-run` 核对矩阵展开数后才开始长跑。

### 8.3 冻结版后的分级性能回归门

当前冻结二进制已经完成一次完整 P1 与 Orkut `g=15` q1--q10；自该冻结点起，不再为每个局部删除默认重跑全量。后续按以下层级执行：

1. 文档、注释或重建后生产二进制逐字节相同：只运行静态、正确性与 Markdown 门，不运行性能实验；
2. 单点、输入无关等价且局限于一个物理热阶段的小删除：运行固定 P1 哨兵，并只运行 Orkut `g=15` q10；q1--q9 不重复运行；
3. 跨多个热阶段，或改变状态定义域、递推依赖、row 发布生命周期、证书可采纳性或真实上界见证链：才重新考虑完整 P1。这里按语义影响范围判断，不按删除行数、图名、组数或历史计时设置经验开关。

P1 哨兵已在候选不可见的冻结结果上一次选定：每图取其最终最快 ABHSS 配置中 `solver_seconds` 最大的一条，共 13 条；再加入已知历史反向项 Orkut `g=7` q175 Enhanced；最后加入 Musae `g=5` q1--q100 Enhanced 固定成本块。标准参考使用同一冻结二进制在 CPU 5 顺序运行，共 114 条，权值、feasible/infeasible、`(mask,v)` 状态及任务身份均与正式 P1 逐项一致。具体 task key、时间、空间和状态写入 [`correctness_audit.json`](../experiments/correctness_audit.json) 的 `final_dominated_operation_gate.post_freeze_sentinel`。候选只有在这些离散字段完全一致且时间明确不高于冻结参考时才接受；接近或交换 CPU 后方向不稳定时拒绝该删除，不用宽容阈值把不确定性改写成通过。

双进程只使用两个已校准的独立物理核。相同面板的单路/双路校准中，ABHSS 为 290.232177/289.247931 秒，双路/单路为 0.996609；PrunedDP++ 为 395.350486/391.142916 秒，比值为 0.989357。四路 PrunedDP++ 增至 426.358209 秒，比单路慢 7.84%，因此正式长门和后续哨兵都固定最多双路。当前 CPU 4/5 与校准 CPU 0/1 具有相同的不同物理核、不同 socket 拓扑。小删除复测可让一个核运行候选 q10，另一核运行 P1 哨兵；每个 solver process 仍严格单线程。

服务器本地原始记录分别位于 `results/paper_runs/parallel1_calibration_abhss_672bd253cdde_20260802`、`parallel2_calibration_abhss_672bd253cdde_20260802`、`parallel1_calibration_pruneddp_672bd253cdde_20260802`、`parallel2_calibration_pruneddp_672bd253cdde_20260802` 和 `parallel4_calibration_pruneddp_672bd253cdde_20260802`；记录数依次为 80、80、82、82、82。它们属于 Git 忽略的大结果，提交前应把上述汇总与机器拓扑写入机器可读审计，不能依赖未上传目录作为唯一证据。

这套哨兵只用于后续小改的“无可识别退化”决策，不替代本轮冻结二进制的 P1 正式结果。若候选二进制不同，论文和 artifact 必须保留两者身份，不能把哨兵外推成候选已完整复跑。

### 8.4 冻结版最终 P1、Orkut 硬门与哨兵基准

冻结求解器源码提交为 `12d6adb`，正式 ABHSS 二进制 SHA-256 为 `793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced`，PrunedDP++ 二进制 SHA-256 为 `4c1d3599f03da6073d368a6a83fcbd31ea0a625f9ba90892b22b0b239eb42bf2`，矩阵 SHA-256 为 `aed5db1ed83d134f5882f4c9d4544bf26549e9ee5a5392d74c3066b96c273162`。机器为 `test-PowerEdge-R630`，Linux `6.14.0-24-generic`，每个求解器严格单线程，正式 TL 均为 10,000 秒。Base 与 Enhanced 来自同一可执行文件；P1 当前结果固定在 CPU 4/5。PrunedDP++ 没有因 ABHSS 改码而变化，因此复用同机、同矩阵、同 TL、同 baseline 哈希的 `p1_full` 记录，避免无意义重跑；独立审计按 task key 对齐了三方法全部 8,318 个查询身份。

P1 共 16,636 条当前 ABHSS 记录和 8,318 条 PrunedDP++ 记录，任务键全部唯一，55 个原始 infeasible 查询在三方法间逐项一致，没有目标值、feasibility 或 expected-status 不一致。表中时间是该图全部查询的 `solver_seconds` 总和；`min/P` 是 `min(Base, Enhanced) / PrunedDP++`，只用于验收“每图至少一个 ABHSS 配置不劣”，不把逐查询挑快者虚构成第三种算法。

| 图 | 每方法查询数 | Base / 秒 | Enhanced / 秒 | PrunedDP++ / 秒 | 最快 ABHSS | `min/P` |
|---|---:|---:|---:|---:|---|---:|
| DBLP-GPU4GST | 900 | 8,035.299 | 9,694.387 | 13,683.019 | Base | 0.5872 |
| DBLP-MonoGSTPlus | 160 | 3,766.660 | 3,160.483 | 29,049.935 | Enhanced | 0.1088 |
| DBpedia-MonoGSTPlus | 438 | 8,386.674 | 7,593.174 | 20,288.408 | Enhanced | 0.3743 |
| Github-GPU4GST | 900 | 90.900 | 76.562 | 338.436 | Enhanced | 0.2262 |
| LinkedMDB-MonoGSTPlus | 200 | 611.421 | 491.172 | 6,612.577 | Enhanced | 0.0743 |
| LiveJournal-GPU4GST | 900 | 14,080.989 | 24,257.869 | 27,535.664 | Base | 0.5114 |
| MovieLens-MonoGSTPlus | 160 | 1,090.124 | 869.355 | 3,041.379 | Enhanced | 0.2858 |
| Musae-GPU4GST | 900 | 61.146 | 38.497 | 186.901 | Enhanced | 0.2060 |
| Orkut-GPU4GST | 900 | 30,744.397 | 45,405.709 | 120,913.770 | Base | 0.2543 |
| Reddit-GPU4GST | 900 | 6,246.078 | 6,150.121 | 7,294.956 | Enhanced | 0.8431 |
| Toronto-MonoGSTPlus | 160 | 10.374 | 16.916 | 16.241 | Base | 0.6387 |
| Twitch-GPU4GST | 900 | 77.349 | 81.723 | 448.326 | Base | 0.1725 |
| Youtube-GPU4GST | 900 | 2,804.511 | 3,603.514 | 3,004.472 | Base | 0.9334 |

13/13 图通过底线。最紧的是 YouTube，其次是 Reddit 和 Toronto；因此后续不能删除这些图，也不能只报告全局平均。空间为图内单查询峰值，状态为图内全部查询的实际 `(mask,v)` 累计：

| 图 | Base 峰值 MiB | Enhanced 峰值 MiB | PrunedDP++ 峰值 MiB | Base states | Enhanced states | PrunedDP++ states |
|---|---:|---:|---:|---:|---:|---:|
| DBLP-GPU4GST | 333.973 | 614.207 | 1,746.500 | 299,668,791 | 230,338,803 | 342,635,750 |
| DBLP-MonoGSTPlus | 722.949 | 678.297 | 7,112.348 | 667,306,185 | 293,343,923 | 1,106,115,160 |
| DBpedia-MonoGSTPlus | 1,322.848 | 2,333.750 | 13,451.219 | 365,248,307 | 285,581,028 | 400,984,226 |
| Github-GPU4GST | 9.562 | 20.660 | 161.324 | 21,383,350 | 16,002,423 | 39,874,418 |
| LinkedMDB-MonoGSTPlus | 646.016 | 332.176 | 10,126.340 | 197,480,559 | 89,326,477 | 258,789,081 |
| LiveJournal-GPU4GST | 442.316 | 1,145.863 | 1,127.133 | 197,337,482 | 172,752,139 | 160,626,901 |
| MovieLens-MonoGSTPlus | 46.129 | 562.117 | 173.492 | 1,134,768 | 153,418 | 2,247,152 |
| Musae-GPU4GST | 4.500 | 11.480 | 73.805 | 7,161,062 | 4,529,006 | 18,424,783 |
| Orkut-GPU4GST | 507.406 | 2,661.902 | 3,001.832 | 1,001,249,923 | 712,638,167 | 1,140,199,706 |
| Reddit-GPU4GST | 396.719 | 882.809 | 634.125 | 1,882,349 | 1,975,393 | 2,656,110 |
| Toronto-MonoGSTPlus | 6.410 | 10.734 | 26.438 | 915,047 | 563,556 | 1,866,233 |
| Twitch-GPU4GST | 10.125 | 15.980 | 147.320 | 20,964,233 | 10,710,978 | 40,520,002 |
| Youtube-GPU4GST | 122.812 | 221.379 | 408.125 | 80,248,354 | 73,109,126 | 12,450,269 |

Orkut `g=15` 正式十条使用同一生产二进制、同一 10,000 秒逐查询 TL。q10 最紧，但仍有 455.439 秒余量；全部权值与精确参考一致。

| 查询 | CPU | 权值 | 秒 | 查询峰值 MiB | `(mask,v)` states |
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

CPU 5 标准化 P1 哨兵由 14 条跨图/历史风险项与 100 条 Musae 固定成本项组成，共 114/114 成功。前者总 `solver_seconds` 为 816.198581，后者为 4.199400；固定块峰值 10.312 MiB、累计 76,770 states。它只定义后续小删除的参考，不进入论文 P1 主表。正式聚合、独立字段核验、逐条 Orkut 与哨兵机器可读值统一保存在 [`correctness_audit.json`](../experiments/correctness_audit.json)；服务器本地原始目录为 `results/paper_runs/final_793d4e_p1_w0_cpu4_20260818`、`final_793d4e_p1_w1_cpu5_20260818`、`p1_full`、`final_793d4e_orkut_g15_w0_cpu4`、`final_793d4e_orkut_g15_w1_cpu5` 和 `post_freeze_p1_sentinel_793d_cpu5_20260819`。

### 8.5 本轮双核全量 campaign 与提前停跑语义

机器计划冻结在 [`experiments/final_campaign_plan.json`](../experiments/final_campaign_plan.json)，调度入口为 `tools/experiments/run_parallel_campaign.py`。生产二进制仍是第 8.4 节的两个哈希；当前矩阵删除 P2 `g=16` 后的 SHA-256 为 `9f8f6fadcd1569382bf7eb3e0e43ea2dc3a7021e6e00484c99372b462e80dae7`。P1 的 24,954 个三方法任务和 Orkut `g=15` Enhanced q1--q10 不重跑，但调度器启动前必须重新展开当前矩阵，并验证任务键全集、查询路径、方法二进制和历史 audit；不能仅因目录名相似就复用。

Enhanced 优先运行剩余 P2 与全部 S2。队列只使用旧版只读探针估计排序：P2 把旧 q1--q5 cell 时间按十条缩放，S2 把预登记 q3 时间按五条缩放；缺失项才使用图规模与子集数的确定性分数。排序只决定先后，不改变 solver、timeout、结果筛选或论文统计。任何 Enhanced 查询真实达到 10,000 秒时，supervisor 先写入 timeout 记录，再以退出码 4 通知父调度器；父调度器停止发新任务并终止另一核的当前进程组，整个 campaign 标为 `stopped`，不得继续用部分 Enhanced 结果拼表。

Base 与 PrunedDP++ 使用完全相同的 P2 保守 frontier。每个固定 `(graph,g,method)` 先运行预登记 tranche 1 的 q1--q5，它们恰好覆盖五个实现后组大小等秩层。只有五条都真实达到 10,000 秒、且对应 Enhanced 五条全部完成时，才不启动当前格 q6--q10 和该图更大 `g`；每个未运行任务写入单独的 `not_run_likely_timeout` 清单，并保存五个真实 timeout 的 task key。该清单不是正式 timeout：不得进入完成数、PAR-2、时间总和或“已完成矩阵”统计，需要完整纸面点时必须后续重跑。只要五条中有一条完成，就继续 q6--q10 和下一个 `g`。该规则同时作用于 Base/PrunedDP++，没有图名、方法特例或可调阈值。

S2 不使用上述外推，因为目标 `f` 与运行时间不具备预先可声称的单调关系；Base 与 PrunedDP++ 的 150 条各自完整运行。消融最后运行：DirectedCutOnly 使用生产二进制；without-endpoint-floor 从当前提交的隔离 `git archive` 构建，只允许把 `max(farthest, endpoint-floor)` 一处改成 `farthest`，随后完整 CTest。任一完成项与 Enhanced 的权值/feasibility 不一致，或 isolated build 身份不满足唯一差异，立即停止。

正式命令先执行 `prepare` 审计，再在 tmux 中执行 `run`；`status` 只读汇总状态。CPU 固定为 4/5，两个 worker 各自保存 metadata、日志和任务 JSON；父进程持有排他锁，重复命令只恢复缺失 task key，不会同时启动第二个 campaign。具体命令见 [`RUN.md`](../RUN.md)。

## 9. 人工审阅清单

### 数据身份

- [ ] P1 恰好 13 个图身份；`DBLP-MonoGSTPlus` 与 `DBLP-GPU4GST` 不合并。
- [ ] P1 manifest 仍为 29 个 query blocks、8,318 条全询问。
- [ ] GPU4GST 每个 `g=3,5,7` 恰好使用作者前 300 条 CSV。
- [ ] P1 图/查询哈希与 manifest 一致；论文表与作者成品差异仍有说明。
- [ ] IMDb raw 哈希、日期和 non-commercial 条款有记录。
- [ ] MonoGST+ 作者数据的公开获取/再分发方案已获确认，未用公开前作偷换最终 workload。
- [ ] GPU4GST 代码与 OneDrive 数据的再分发方案已确认；若未获许可，公开包只含官方下载指针、哈希和转换器。

### 查询

- [ ] P2 是六图、`g=5..15`、10 条/格，共 660 条；q1--q5 与原 panel 一致，q6--q10 是五个原 size strata 各追加一条。
- [ ] P2 选择只使用输入组大小与固定哈希，不读取任何 solver 结果。
- [ ] S2 是两图、3 个 `g`、5 个 `f`、5 条/格，共 150 条。
- [ ] S2 未把实际平均组大小强制调成目标 `f`。
- [ ] 新增查询全部可行；P1 的 55 条已知 infeasible 原查询仍完整保留。

### 执行与报告

- [ ] 三种正式计时项使用同一编译环境、timer 边界和 10,000 秒 timeout。
- [ ] 两个 ABHSS 配置调用同一 `abhss` 可执行文件，开关在查询前冻结，没有 oracle。
- [ ] Linux `make release` 和 Ubuntu CI 通过；正式服务器的硬件/系统/编译器/IPO 状态已写入运行记录。
- [ ] 本地全部 CTest、5,000 个随机精确实例、500 个正权互异单终端实例、160 个 $g=7..16$ omitted-half transpose 压力实例、12 点辅助半层固定反例、状态计数契约和 SteinLib 已知最优 gate 通过。
- [ ] 正式 ABHSS/PrunedDP++ 的每任务 JSON 都含非负 `mask_vertex_states`；汇总没有为 timeout 猜测状态数。
- [ ] 论文将 `pruneddp_safe` 准确标注为 corrected reconstruction；已决定是否添加 GPU4GST CPU artifact 的适用子集校准列。
- [ ] 正文不声称当前二进制已输出最优树；若问题定义要求树边集，artifact freeze 前已实现并测试回溯。
- [ ] P1 主表每图一行，不按 `g` 拆分，也不把部分 solved time 写成总时间。
- [ ] P2/S2 每点显示完整分母和 timeout 方向。
- [ ] `quality_mismatches.csv` 与 `feasibility_mismatches.csv` 均为空。
- [ ] 消融、近似质量和历史探针没有混入当前矩阵或正文汇总。
