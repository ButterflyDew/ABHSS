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
- I/O 审计：`graph_load_seconds` 与 `query_load_seconds` 单独写入 header；它们用于发现 artifact 工程瓶颈，不并入算法 speedup。图加载包含 8 MiB 数字扫描、精确邻接容量预留和一次 $O(n+m)$ 连通分量建索引；逐查询可行性检查只按组成员求分量交，不重复扫描整图。
- timeout：每条查询 10,000 秒；图加载 watchdog 1,800 秒。
- 顺序：同一 case 的首个方法由稳定哈希轮换，避免固定方法总是占用冷机或热机位置。
- 正确性：所有完成的可行查询必须目标值一致；已审计无解的查询必须一致返回 infeasible。
- 失败记录：timeout、graph-load-timeout、OOM、error 分开保存，不能删掉失败行后计算平均值。
- 数据身份：名称、来源和最终 `graph.txt` SHA-256 共同构成图身份；两个 DBLP 永不合并。

机器矩阵是 [`experiments/paper_matrix.json`](../experiments/paper_matrix.json)，输入哈希与查询分布是 [`experiment_data/p1_published_workloads/manifest.json`](../experiment_data/p1_published_workloads/manifest.json)。单入口重构、“新增或同职责替换”配置契约，以及 SteinLib、Musae、Reddit、Orkut 的非退化结果固化在 [`experiments/abhss_configuration_refactor_gate.json`](../experiments/abhss_configuration_refactor_gate.json)。

### 2.1 来源证据链与可声称强度

| 实验对象 | 当前使用的直接来源 | 本仓库如何转换/冻结 | 可以声称什么 | 尚未解决什么 |
|---|---|---|---|---|
| MonoGST+ P1 五图与全查询 | *A Practical Sublinear Approximation for Group Steiner Tree* 作者提供的处理后接口；论文已被 VLDB 2026 录用但截至冻结日未公开 | 不重新下载图、不重生成查询；`build_published_workloads.py` 只转为统一文件名并记录 SHA-256 | “使用 MonoGST+ 作者实验接口的字节级冻结” | 论文/数据的公开获取路径和再分发许可仍需作者确认；不能只用公开前作 [GroupSteinerTree artifact](https://github.com/YahuiSun/GroupSteinerTree) 冒充这五个最终输入 |
| GPU4GST P1 八图与全查询 | [GPU4GST 作者 artifact](https://github.com/toziki/GPU4GST-sigmod) 指向的 OneDrive `.in`/`.g`/CSV 产品 | `prepare_gpu4gst` 保留图边语义，将每个 `g={3,5,7}` CSV 的前 300 行转为统一 query 接口；原文件元数据和哈希在 `data_origin` | “使用作者 artifact 实际发布的成品图与全部登记查询” | GitHub 仓库/数据包未见清晰的仓库级再分发许可；公开 artifact 前应获取许可或只发布下载步骤与哈希 |
| P2 related-group 扩展 | [Approximating Probabilistic Group Steiner Trees in Graphs](https://www.vldb.org/pvldb/vol16/p343-sun.pdf) 的组共现图、均匀根组、最小可用 BFS 深度和近邻均匀抽样协议 | 在 GPU4GST 作者 `.g` 候选组上用新 seed 生成 300 条/格，再只按实现后平均组大小五分层选 5 条 | “按原论文的 related-group 方法类生成的新扩展 panel” | 原论文/GPU4GST 没有公开能恢复其已发表查询的完整 seed 链；P2 不能写成“原作者查询”或“复现其具体随机样本” |
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
5. 分成五个等秩层，每层按 panel seed `20260723` 的稳定 SHA-256 key 选一条。

因此每图 12 cells、60 条，六图共 72 cells、360 条。五条/格用于画趋势和完成率，不宣称能给出很窄的单格置信区间。逐条选择索引、组大小、group ID、seed 和哈希均保存在 `experiment_data/p2_cross_g`。

### 4.3 报告

每图画一条随 `g` 变化的曲线，至少同时展示：

- 完成数/5；
- PAR-2；
- 双方都完成时的成对加速比；
- `baseline timeout / ABHSS solved` 与反方向数量；
- 实际平均组大小范围，防止把 `f` 的随机变化误解为纯 `g` 效应。

P2 的核心 claim 是从 `g=5` 到 16 的趋势和转折位置，而不是六图平均后的单一倍率。小图必须保留，即使 ABHSS 固定成本导致轻微劣势。

## 5. 副实验：$\langle g,f\rangle$ 受控敏感性

### 5.1 图和网格

| 图 | `n` | `m` | 用途 |
|---|---:|---:|---|
| DBLP-MonoGSTPlus | 2,497,782 | 12,786,329 | 在 published graph 上观察 `g,f` 交互 |
| IMDb-latest-20260722 | 18,588,661 | 100,556,350 | 补入 PrunedDP++ 脉络常见的大型 IMDb 图；没有冒充 2016 快照 |

网格为 `g={6,10,14}` 与 `f={200,400,800,1600,3200}` 的笛卡尔积，每格 5 条。因此每图 15 cells、75 条，总计 30 cells、150 条。

### 5.2 MonoGST+ 查询协议

对每个组独立采样：

$$
|S_i|=\operatorname{clamp}\left(\operatorname{round}(N(f,(0.15f)^2)),0.5f,1.5f\right).
$$

随后从 `1..n` 中无放回均匀选择 `|S_i|` 个顶点。不同组之间允许重叠。本文不再把多个组大小强行平衡为“每条查询恰好平均等于 `f`”，因为那会改变 MonoGST+ 原生成方法；manifest 同时记录目标 `f` 和真实 `mean_f/min_f/max_f`。

该实验不是复刻 PrunedDP++ 的历史数值：DBLP 使用 MonoGST+ 作者图，IMDb 使用 2026-07-22 官方冻结。它只研究本文算法对 `g` 与目标平均组大小的敏感性。

### 5.3 报告

使用 2×3 或分面图：每个数据集分别给 `g=6,10,14`，横轴 `f` 使用 2 倍对数刻度。每点报告完成数/5、PAR-2、成对加速比和实际 `f` 范围。不能只展示 `g=14` 或 `f=3200` 的有利区域。

## 6. 工作量与运行次序

| Suite | 角色 | cells/query blocks | 查询 | 运行任务 |
|---|---|---:|---:|---:|
| `P1_monogstplus_published` | 主 1 | 5 | 1,118 | 3,354 |
| `P1_gpu4gst_published` | 主 1 | 24 | 7,200 | 21,600 |
| `P2_cross_g` | 主 2 | 72 | 360 | 1,080 |
| `S2_controlled_gf` | 副 | 30 | 150 | 450 |
| 合计 | 性能 | 131 | 8,828 | 26,484 |

建议顺序：正确性 gate → P1 小图和 Mono 自然图 → P2 → S2 → P1 大图长任务。P1 是全询问承诺，不能因为后半段成本高而只发表先完成的图。所有长任务使用固定 case sharding 和同一 run directory 断点续跑。

## 7. 统计、图表与 claim guardrails

- P1 的主要量是数据集级总工作量，不对 13 图按查询数再次加权成一个“总体平均倍率”。
- P2/S2 的五条/格先报告原始分母和 timeout 方向；共同完成的 geomean speedup 不能代表 timeout 查询。
- 同时保留 query-weighted 与 dataset-equal-weight 的补充汇总，但正文结论以逐图/逐层趋势为主。
- 任何目标值或 feasibility 不一致都会冻结相应性能结论。
- 不比较不同完成子集的平均时间。
- 不把 ABHSS Base/全增强配置的逐查询最小值当作第三种曲线。
- 若 PrunedDP++ 在某图明显更好，保留该图并从 `n,m`、密度、候选组大小、连通分量、实际 `f`、状态数和上界命中率分析原因。
- 每条完成查询同时保存 `mask_vertex_states`：ABHSS 为首次进入 $D$、$A$、$H$ 行的状态项总数，状态族属于实际键，因此不同状态族中数值相同的 `(mask,v)` 分别计数；PrunedDP++ 为主 StateStore 的实际 `(mask,v)` 项数。两边都排除组距离/route/tour/dual、重复队列项和 full-mask 完成候选。该指标用于解释各自状态域的削减，不是跨算法完全同成本的基本操作，也不替代时间或内存。
- cell 级报告完成查询的中位/p90 状态数，并在双方均完成且状态数为正的配对上报告 `PrunedDP++ / ABHSS` 状态倍率。timeout 没有最终状态数，必须单列完成分母，不能以完成子集冒充完整 workload，也不能把 Dense 容量当 PrunedDP++ 实际状态数。

### 7.1 正确性 gate 与运行准入

每次修改 solver、图/查询 I/O、状态统计或编译选项后，先运行五个仓库内 CTest：

1. 求解器与可行性审计共用的快速数字读取器保留原边、`edge_id`、双向邻接顺序、自环双邻接项、零/小数/科学计数边权和连通分量缓存，并拒绝尾部 token/非法权重。
2. 查询读取器保留合法多查询与空查询，并拒绝负查询/组计数、空组、截断 payload 和声明记录之后的多余 token。
3. Base、DirectedCutOnly 和 Enhanced 都通过历史零权 witness 父指针环反例。
4. 三个合法配置在 144 个确定性随机连通小图、$2\le g\le10$ 上逐例匹配独立全子集 DP；同一测试还覆盖空/单组、重叠零代价、非连通无解、$g>16$、未知增强位、非法 adjoint-only 配置和小于 $10^{-9}$ 的严格正 gap。
5. ABHSS Base/Enhanced 的状态数重复运行稳定；PrunedDP++ Hash 与 Dense 后端报告相同实际状态数；平凡查询报告 0。

然后运行 `S1_steinlib_exactness_gate`。当前冻结证据包含 11 个 $11\le g\le16$ 的 WRP 已知最优实例：ABHSS Base/Enhanced、PrunedDP++-Safe、DPBF 和 SCIP-Jack 已全部匹配；Basic+ 只在其 $g\le14$ 能力范围内参加。当前证据摘要在 [`experiments/correctness_audit.json`](../experiments/correctness_audit.json)。未恢复第三方二进制时可先跑仓库内 gate，但不能因此声称完成了六方 SteinLib 核验。

下列任一条会阻止正式性能 claim：完成查询的权值不一致；feasible/infeasible 不一致；生成的 P2/S2 出现无共同分量查询；当前 `paper_matrix.json` 哈希与 feasibility audit 不匹配；或运行二进制的配置/header 与矩阵登记不一致。

### 7.2 Linux 编译与服务器锁定

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

- [ ] P2 是六图、`g=5..16`、5 条/格，共 360 条。
- [ ] P2 选择只使用输入组大小与固定哈希，不读取任何 solver 结果。
- [ ] S2 是两图、3 个 `g`、5 个 `f`、5 条/格，共 150 条。
- [ ] S2 未把实际平均组大小强制调成目标 `f`。
- [ ] 新增查询全部可行；P1 的 55 条已知 infeasible 原查询仍完整保留。

### 执行与报告

- [ ] 三种正式计时项使用同一编译环境、timer 边界和 10,000 秒 timeout。
- [ ] 两个 ABHSS 配置调用同一 `abhss` 可执行文件，开关在查询前冻结，没有 oracle。
- [ ] Linux `make release` 和 Ubuntu CI 通过；正式服务器的硬件/系统/编译器/IPO 状态已写入运行记录。
- [ ] 本地五个 CTest、144 随机精确实例、状态计数契约和 SteinLib 已知最优 gate 通过。
- [ ] 正式 ABHSS/PrunedDP++ 的每任务 JSON 都含非负 `mask_vertex_states`；汇总没有为 timeout 猜测状态数。
- [ ] 论文将 `pruneddp_safe` 准确标注为 corrected reconstruction；已决定是否添加 GPU4GST CPU artifact 的适用子集校准列。
- [ ] 正文不声称当前二进制已输出最优树；若问题定义要求树边集，artifact freeze 前已实现并测试回溯。
- [ ] P1 主表每图一行，不按 `g` 拆分，也不把部分 solved time 写成总时间。
- [ ] P2/S2 每点显示完整分母和 timeout 方向。
- [ ] `quality_mismatches.csv` 与 `feasibility_mismatches.csv` 均为空。
- [ ] 消融、近似质量和历史探针没有混入当前矩阵或正文汇总。
