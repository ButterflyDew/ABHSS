# ABHSS 完整实验方案与 human-review 细则

## 1. 论文问题与冻结边界

目标论文是 SIGMOD/VLDB 的单线程精确 GST 算法工作。核心比较对象是 PrunedDP++-Safe。ABHSS 只有一个算法入口；正文分别报告不启用增强的 Base 和依次启用 directed-cut、adjoint-completion 的全增强配置，不允许按查询取两个配置中较快值。

当前实验只回答三个问题：

1. 在最近两项直接相关工作的完整 workload 上，ABHSS 的总体效率和完成率是否优于 PrunedDP++？
2. 当组数从常见范围继续增长到 `g=16` 时，优势是否稳定扩大？
3. 在固定图上同时改变 `g` 与平均组大小目标 `f`，结论是否仍成立？

消融、近似解质量、GPU/异构比较和额外应用实验尚未冻结，不进入当前正式矩阵。正确性 gate 是运行前条件，不作为性能贡献。

## 2. 统一执行规则

- 计时项：同一 `abhss` 二进制的 Base 配置、全增强配置，以及 PrunedDP++-Safe。
- 配置冻结：`abhss_base` 和 `abhss_enhanced` 必须指向同一可执行文件，参数分别固定为 `--enhancements=none` 与 `--enhancements=all`；运行中不得切换。两者差异只能是安全新增证书或同一逻辑职责的 realization 替换，不能存在 Base 独有而 Enhanced 无对应物的状态族；完整映射见 [`METHOD.md`](METHOD.md) 第 3.1 节。
- 计算资源：每个 solver process 只允许一个计算线程；监控线程只采样 RSS。
- 构建：同一编译器、Release、相同 IPO/优化规则。
- 计时：图在一个 query block 中加载一次；逐查询 timer 在 `[Ready]` 之后开始，包含该方法的查询预处理与求解，不包含共同图加载。
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

GPU4GST 2025 artifact 现在包含一份 CPU PrunedDP++ header，仓库中的 `gpu4gst_pruneddp_artifact` adapter 已在 wrp4-11 和 Musae `g=5` 小 panel 上核对权值。它仍有两个直接限制：上游只支持非负整数边权且 $g\le14$，无法无条件覆盖 P1/P2/S2 全矩阵。提交前应与导师做一次显式决策：是否将它增加为“适用子集上的 artifact 校准列”。若不加，论文必须说明为什么主表选择可覆盖 $g=16$ 和小数边权的 Safe reconstruction，并在 artifact 附录保留上述一致性证据。

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

## 4. 主实验 2：`g=5..16` 扩展

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

对每张图和每个 `g=5..16`：

1. 使用 GPU4GST `.g` 中的作者候选组构造组共现图；共享原图顶点的两个组相邻。
2. 均匀选择根组，BFS 到能够提供足够相关组的最小深度，再随机抽取组。
3. 拒绝没有共同原图连通分量的查询。
4. 用生成 seed `2025` 固定产生 300 条候选；只用输入侧 `log1p(realized mean group size)` 排序。
5. 分成五个等秩层，每层按 panel seed `20260723` 的稳定 SHA-256 key 排序；第一轮每层选第一名构成原 q1--q5，第二轮每层选第二名并追加为 q6--q10。第二轮不改变第一轮的查询身份和顺序。

因此每图 12 cells、120 条，六图共 72 cells、720 条。十条/格用于画趋势和完成率；原五条形成 tranche 1，追加五条形成 tranche 2，既可合并报告，也能单独核验追加样本没有推翻原趋势。逐条 tranche、选择索引、组大小、group ID、seed 和哈希均保存在 `experiment_data/p2_cross_g`。

### 4.3 报告

每图画一条随 `g` 变化的曲线，至少同时展示：

- 完成数/10，并同时保留 tranche 1 与 tranche 2 各自的完成数/5；
- PAR-2；
- 双方都完成时的成对加速比；
- `baseline timeout / ABHSS solved` 与反方向数量；
- 实际平均组大小范围，防止把 `f` 的随机变化误解为纯 `g` 效应。

P2 的核心 claim 是从 `g=5` 到 16 的趋势和转折位置，而不是六图平均后的单一倍率。小图必须保留，即使 ABHSS 固定成本导致轻微劣势。

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

修复版诊断构建在独占 CPU 5、30,000 秒外层预算下自然完成 Orkut q10：精确权值为 54，solver 时间 11,178.235 秒，超过正式 10,000 秒 TL 1,178.235 秒；查询峰值为 21,337.590 MiB，watchdog 总 RSS 峰值为 27,729.191 MiB，累计首次发现状态为 1,761,794,764。阶段时间为预处理 189.940 秒、共同 A1 85.956 秒、ordinary 6,524.740 秒、低层 A 2,040.340 秒、adjoint 2,337.210 秒。H7 用 1,278.810 秒完成但上界仍为 57；H6 用 626.668 秒把上界收紧到最终值 54，之后 H5/H4 分别只用 151.333/145.169 秒。该长门还包含一个随后因零状态收益和普通阶段退化而回退的后续证书链候选，因此只作为修复版正确性与保守运行风险证据；q1--q10 仍须用清理后的最终提交全部重跑。旧状态数不设为相等门，当前版按自己的精确状态域如实报告。

`mean_f` 仍不是难度的单调解释变量：旧 q10 属于最低 size stratum，却远慢于组更大的 q8；因此不能按平均组大小删除 q10 或设置经验超参数。正式复跑仍对每条查询使用固定 10,000 秒，并以冻结机器、当前 commit、当前二进制哈希和新结果行为准；30,000 秒只用于看清 q10 超时后的完整阶段轨迹，不改变正式 TL。当前修复版尚未通过 q10 硬门，正式矩阵应如实计入 timeout/完成率/PAR-2；后续只能寻找输入无关且精确的严格支配优化，不能延长该条专属 TL。事故演进与当前修复证据见 [辅助半层 Adjoint 审计](archive/AUXILIARY_HALF_ADJOINT_CORRECTNESS_AUDIT_20260815.md)和 [Adjoint split 完备性审计](archive/ADJOINT_SPLIT_COMPLETENESS_AUDIT_20260817.md)。

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

## 6. 工作量与运行次序

| Suite | 角色 | cells/query blocks | 查询 | 运行任务 |
|---|---|---:|---:|---:|
| `P1_monogstplus_published` | 主 1 | 5 | 1,118 | 3,354 |
| `P1_gpu4gst_published` | 主 1 | 24 | 7,200 | 21,600 |
| `P2_cross_g` | 主 2 | 72 | 720 | 2,160 |
| `S2_controlled_gf` | 副 | 30 | 150 | 450 |
| 合计 | 性能 | 131 | 9,188 | 27,564 |

建议顺序：正确性 gate → P1 小图和 Mono 自然图 → P2 → S2 → P1 大图长任务。P1 是全询问承诺，不能因为后半段成本高而只发表先完成的图。所有长任务使用固定 case sharding 和同一 run directory 断点续跑。

## 7. 统计、图表与 claim guardrails

- P1 的主要量是数据集级总工作量，不对 13 图按查询数再次加权成一个“总体平均倍率”。
- P2 的十条/格与 S2 的五条/格均先报告原始分母和 timeout 方向；共同完成的 geomean speedup 不能代表 timeout 查询。P2 还保留 tranche 字段，必要时可分别报告原 q1--q5 与追加 q6--q10，检查扩样是否改变结论。
- 同时保留 query-weighted 与 dataset-equal-weight 的补充汇总，但正文结论以逐图/逐层趋势为主。
- 任何目标值或 feasibility 不一致都会冻结相应性能结论。
- 不比较不同完成子集的平均时间。
- 不把 ABHSS Base/全增强配置的逐查询最小值当作第三种曲线。
- 若 PrunedDP++ 在某图明显更好，保留该图并从 `n,m`、密度、候选组大小、连通分量、实际 `f`、状态数和上界命中率分析原因。
- 每条完成查询同时保存 `mask_vertex_states`：ABHSS 为首次进入 $D$、 $A$、 $H$ 行的状态项总数，状态族属于实际键，因此不同状态族中数值相同的 `(mask,v)` 分别计数；PrunedDP++ 为主 StateStore 的实际 `(mask,v)` 项数。两边都排除组距离/route/tour/dual、重复队列项和 full-mask 完成候选。该指标用于解释各自状态域的削减，不是跨算法完全同成本的基本操作，也不替代时间或内存。
- cell 级报告完成查询的中位/p90 状态数，并在双方均完成且状态数为正的配对上报告 `PrunedDP++ / ABHSS` 状态倍率。timeout 没有最终状态数，必须单列完成分母，不能以完成子集冒充完整 workload，也不能把 Dense 容量当 PrunedDP++ 实际状态数。

### 7.1 正确性 gate 与运行准入

每次修改 solver、图/查询 I/O、状态统计或编译选项后，先运行五个仓库内 CTest：

1. 求解器与可行性审计共用的快速数字读取器保留原边、`edge_id`、双向邻接顺序、自环双邻接项、零/小数/科学计数边权和连通分量缓存，并拒绝尾部 token/非法权重。
2. 查询读取器保留合法多查询与空查询，并拒绝负查询/组计数、空组、截断 payload 和声明记录之后的多余 token。
3. Base、DirectedCutOnly 和 Enhanced 都通过以四个逻辑组保留的历史零权 witness 父指针环反例；50,000 顶点逆序零权并查集链还必须在不依赖递归栈深度的情况下闭合为 0。
4. 三个合法配置在 5,000 个确定性随机连通小图、 $2\le g\le10$ 上逐例匹配独立全子集 DP；另以 500 个 $6\le g\le10$ 的正权、互异单终端实例压测 Enhanced 高层，并在 $g=7,8,\ldots,16$ 上各运行 16 个正权互异单终端实例，共 160 个 omitted-half transpose 高组压力实例。显式 $g=2,3$ 非零最优实例要求全部配置在零主状态处共同闭包；directed-cut 固定图还要逐弧复算全部组势梯度与最终 residual。测试另外锁定一个 12 点、7 组的辅助半层反例：独立全子集 DP 真值为 5.75，旧式从逻辑层直接启动 H 会返回 6.25，当前 Enhanced 必须恢复 5.75；并逐一核对 $0\le g\le16$ 的 A/H 职责、ordinary 截止、辅助 $H(h)$ 物理层和每个逻辑层的唯一 realization，以及空/单组、重叠零代价、非连通无解、 $g>16$、未知增强位、非法 adjoint-only 配置和小于 $10^{-9}$ 的严格正 gap。
5. ABHSS Base/Enhanced 的状态数重复运行稳定；PrunedDP++ Hash 与 Dense 后端报告相同实际状态数；平凡查询报告 0。

然后运行 `S1_steinlib_exactness_gate`。当前冻结证据包含 11 个 $11\le g\le16$ 的 WRP 已知最优实例：ABHSS Base/Enhanced、PrunedDP++-Safe、DPBF 和 SCIP-Jack 已全部匹配；Basic+ 只在其 $g\le14$ 能力范围内参加。当前证据摘要在 [`experiments/correctness_audit.json`](../experiments/correctness_audit.json)。未恢复第三方二进制时可先跑仓库内 gate，但不能因此声称完成了六方 SteinLib 核验。

下列任一条会阻止正式性能 claim：完成查询的权值不一致；feasible/infeasible 不一致；生成的 P2/S2 出现无共同分量查询；当前 `paper_matrix.json` 哈希与 feasibility audit 不匹配；或运行二进制的配置/header 与矩阵登记不一致。

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

逐次观测、RSS、输入哈希、被否决变体和二进制身份见 [`experiments/abhss_configuration_refactor_gate.json`](../experiments/abhss_configuration_refactor_gate.json) 的 `common_witness_rent_or_buy_gate_20260725`。该门只证明结构修改不退化，不替代 P1/P2/S2 正式实验。

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

完整逐次时间、RSS、状态、输入身份和二进制身份冻结在 [`experiments/abhss_configuration_refactor_gate.json`](../experiments/abhss_configuration_refactor_gate.json) 的 `distance_root_initialization_gate_20260725`。本地 `.tmp_canonical_spt_refactor_20260725` 仅为 Git 忽略的原始运行目录。

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

最终候选通过 Release 全构建、5/5 CTest、显式 $g=2,3$ 共同闭包回归、逐弧 residual 复算和 15/15 GitHub Markdown 校验。完整机器信息、二进制/源文件 SHA-256、RSS 和被否决中间态记录在 [`p1_local_optimization_probe_20260729.json`](../experiments/p1_local_optimization_probe_20260729.json)。下一步必须在正式 Linux 服务器上重新构建，并全量运行 P1 的 13 图、8,318 条查询；只有服务器结果才能回答“13 图中是否仍存在 PrunedDP++ 同时快于 Base 与 Enhanced”的论文底线。

### 7.6 Linux 编译与服务器锁定

正式服务器推荐用顶层 GNU `Makefile`：`make release JOBS=<physical-core-count>` 完成 Release 配置、当前可用 target 编译和 CTest；`make validate-paper-binaries` 只要求两个正式性能二进制 `abhss`/`pruneddp`，`make validate-all-binaries` 才要求已恢复的 Basic+/SCIP-Jack。CMake 禁用 compiler extensions，在工具链支持时对正式本地 target 统一开启 Release IPO/LTO，并在 GCC 9/10 没有可链接浮点 `from_chars` 时自动回退 `strtod`。

`.github/workflows/linux-ci.yml` 在 Ubuntu 24.04 上执行同一 `make release`。它是每次 push/PR 的平台门禁，不替代正式服务器的环境记录。首次服务器运行必须在结果 metadata 中记录 CPU 型号、物理核、RAM、Linux 发行版、kernel、compiler/CMake 版本、Release/IPO 状态，并用 `--dry-run` 核对矩阵展开数后才开始长跑。

## 8. Human-review checklist

### 数据身份

- [ ] P1 恰好 13 个图身份；`DBLP-MonoGSTPlus` 与 `DBLP-GPU4GST` 不合并。
- [ ] P1 manifest 仍为 29 个 query blocks、8,318 条全询问。
- [ ] GPU4GST 每个 `g=3,5,7` 恰好使用作者前 300 条 CSV。
- [ ] P1 图/查询哈希与 manifest 一致；论文表与作者成品差异仍有说明。
- [ ] IMDb raw 哈希、日期和 non-commercial 条款有记录。
- [ ] MonoGST+ 作者数据的公开获取/再分发方案已获确认，未用公开前作偷换最终 workload。
- [ ] GPU4GST 代码与 OneDrive 数据的再分发方案已确认；若未获许可，公开包只含官方下载指针、哈希和转换器。

### 查询

- [ ] P2 是六图、`g=5..16`、10 条/格，共 720 条；q1--q5 与原 panel 一致，q6--q10 是五个原 size strata 各追加一条。
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
