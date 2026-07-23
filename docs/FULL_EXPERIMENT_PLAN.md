# SIGMOD / VLDB 全量实验运行方案

本文实验只回答一个核心问题：在单计算线程、精确、无向边权 Group Steiner Tree 场景中，ABHSS-Light 和 ABHSS-Heavy 相对最强可复现主 baseline PrunedDP++-Safe，能否在小 `g` 不明显退化，并在较大 `g` 获得显著的完成率和数量级优势。正式性能矩阵固定为 A/B/C 三类主实验；正确性、消融和工程检查均为副实验，不与主结果混为一张总表。

## 1. 方法与 baseline

正式性能方法只有 `abhss_light`、`abhss_heavy` 和 `pruneddp_safe`。Light 与 Heavy 始终分别作图、分别聚合，不允许逐查询取较快者组成 “ABHSS-best”。PrunedDP++-Safe 使用 Hash 状态、MST upper bound 和 `lb2-pathmax=off`；关闭的是已发现不安全的 pathmax/永久 closed 组合，不是关闭 admissible lower bound。

`DPBF`、上游 `Basic+` 和 SCIP-Jack 只用于公开已知最优值 gate。`pruneddp_strict` 只保留为错误语义审计，不具有精确性声明。GPU、异构和索引方法不做性能横比，因为硬件、预处理和在线阶段不能形成可审计的同条件比较。

不把近似算法只按解质量塞入主实验。精确方法完成时近似比恒为 1，单列近似质量不能回答 exact scalability；若未来加入近似方法，必须另做包含预处理、内存、运行时间、完成率和质量的完整 Pareto 实验，不能只选一个有利维度。

## 2. 主实验 A：DBLP 与 IMDb 上的 `<g,f>` 受控实验

A 只复刻 PrunedDP++ 使用的图构造家族，不复刻其历史参数表、50 条询问数或不可获得的 2016 字节。两个图都由当前官方源独立构建，并使用明确的新快照名。

- `DBLP-AMiner-V18`：来自官方 AMiner DBLP Citation Network V18；论文与作者分别为点，论文—作者关系和快照内引用为单位边。标签来自论文标题、源关键词、venue、文献类型和作者名。
- `IMDb-daily-20260722`：来自冻结的 IMDb daily non-commercial export；title 与 person 为二部图顶点，`title.principals` 每一行产生一条单位边。标签来自 title/person 的官方文本字段。

两个图均保留频率 `50..4800` 的真实标签，足以覆盖扩展后的 `f` 范围。查询不合成顶点集合，而是从真实标签组中固定种子抽样；每条查询的实际平均组大小必须落在目标 `f` 的 ±2% 内，否则生成器记录 2,000 次候选中的最接近值并由环境 gate 拒绝偏离正式容差的单元。

A 的参数设计如下：

- `g` sweep：`g=4..16` 的每一个整数点，固定 `f_target=400`。
- `f` sweep：固定 `g=10`，使用 `f_target={100,200,400,800,1600,3200}`。
- 重叠的 `(g=10,f=400)` 只运行一次，因此每个图有 18 个唯一单元。
- 每个单元固定 10 条查询，而不是照搬 PrunedDP++ 的 50 条；两个图合计 36 个单元、360 条查询。

选择 `g=10` 作为 `f` 默认值，是因为它处在传统精确 GST 常见实验上界与本文高 `g` 扩展区间的交界处，既能让 baseline 有足够完成样本，又能观察组大小变化是否改变转折。`f` 使用 2 倍几何级数，避免只在窄区间得出结论。图中同时报告 `f_target`、逐查询 `f_realized` 以及单组 min/max；不能把名义目标当作实际值。

## 3. 主实验 B：SNAP 类来源的跨 `g` 实验

B 使用六个官方 SNAP 图，以及查询生成方式同样基于来源 feature/group 的 MovieLens 与 Toronto：

- SNAP-Wikipedia-2018、SNAP-Twitch-2018、SNAP-GitHub-2019；
- SNAP-YouTube、SNAP-Orkut、SNAP-LiveJournal；
- MovieLens-32M，来源组为官方 genre；
- Toronto-current，来源组为官方 POI 的 `Interests` 和 `Neighbourhood` 类别。

每张图在每个 `g=4..16` 上先用固定种子生成 300 条 related-group BFS 候选查询。正式选择只依赖 `log1p(f_observed)` 的秩：分为十个等秩层，每层用稳定哈希选一条，得到每格 10 条。选择过程不读取任何求解器结果。B 共 `8 × 13 = 104` 个单元、1,040 条查询。

B 的横轴是严格的 `g`，但 `f` 是来源组自然产生的 `f_observed`，不是受控参数。论文必须分图展示并把实际 `f` 作为描述变量；不能把八张图的组大小差异误写成同一个 `f` 实验。MovieLens 不再承担 A 的合成控制角色，Toronto 也不使用论文仓库中的旧同名图。

## 4. 主实验 C：DBpedia 与 LinkedMDB 自然查询

C 只使用来源自然语言查询，且自然低 `g` 与自然高 `g` 全部进入主实验。

DBpedia 使用官方 DBpedia-Entity v2 `queries-v2_stopped.txt`。467 条源查询中，一条映射后 `g=0`，两条无法通过共同连通分量 gate；其余 464 条 `g=1..12` 查询全部保留，不做性能抽样。逐 `g` 数量为 `{1:5, 2:76, 3:80, 4:59, 5:60, 6:71, 7:54, 8:27, 9:14, 10:13, 11:2, 12:3}`。

LinkedMDB 使用官方 LinkedMDB RDF 图和 Meta ParlAI 官方 WikiMovies 镜像。镜像归档与失效原始 MovieQA URL 所记录归档具有相同 SHA-256。按论文协议去标点和冻结 stopword 后映射到 LinkedMDB 标签组，先做共同连通分量 gate，再固定为 200 条分层查询。配额为 `{1:20, 2:20, 3:20, 4:20, 5:20, 6:20, 7:20, 8:25, 9:26, 10:6, 11:2, 12:1}`；所有 35 条唯一 `g=9..12` 映射全部保留，低 `g` 用稳定哈希补足预算。

C 共 24 个 `g` 单元、664 条查询。自然高 `g` 样本少时只作存在性和案例证据，不单独声称具有高统计功效；整体完成率、配对速度和删失分布仍对所有自然查询完整报告。

## 5. 冻结矩阵规模

| 实验 | 角色 | 单元 | 查询 | 方法 | 单实例任务 |
|---|---|---:|---:|---|---:|
| `A_controlled_dblp` | 主 A | 18 | 180 | Light、Heavy、Safe | 540 |
| `A_controlled_imdb` | 主 A | 18 | 180 | Light、Heavy、Safe | 540 |
| `B_related_cross_g` | 主 B | 104 | 1,040 | Light、Heavy、Safe | 3,120 |
| `C_dbpedia_natural` | 主 C | 12 | 464 | Light、Heavy、Safe | 1,392 |
| `C_linkedmdb_natural` | 主 C | 12 | 200 | Light、Heavy、Safe | 600 |
| 主实验合计 |  | 164 | 2,064 |  | 6,192 |
| `S0_smoke` | 副：工程检查 | 1 | 1 | Light、Heavy、Safe、DPBF | 4 |
| `S1_steinlib_exactness_gate` | 副：正确性 | 11 | 11 | 五个全覆盖方法，Basic+ 覆盖 `g<=14` | 62 |
| `S2_ablation` | 副：机制 | 4 | 55 | 两主版本及三个消融 | 275 |

S2 在任何正式结果产生前固定四格：DBLP `g=10,f=400`、IMDb `g=10,f=400`、Wikipedia `g=12`、LinkedMDB `g=8`。比较 Light vs no-early、Light vs no-recurring-witness，以及 Heavy vs forward-only anchoring。只解释成对机制差异，不把不同基础算法的差值相加成虚构的“总贡献”。

## 6. 公平运行协议

所有正式方法使用同一台机器、同一 Release 工具链、同一图和查询字节、单计算线程。允许操作系统和监控线程存在，但必须记录 CPU 型号、物理内存、系统版本、编译器、提交哈希和实际线程设置。禁止同时运行会竞争内存带宽的正式任务。

每个方法每条查询的求解 deadline 统一为 10,000 秒。图解析和共享的只读预处理按相同计时边界处理；runner 另设 1,800 秒 graph-load 防护，只用于识别损坏或不可承受的输入加载，不得把 load timeout 伪装成 solver timeout。所有 timeout 作为右删失值保留，不能只对双方完成的查询作速度均值。

主矩阵每条查询运行一次。由于单条可能达到 10,000 秒，不做全矩阵三次重复；在 smoke、正确性 gate、消融以及 A 的代表点上做独立 calibration，检查运行时间变异。任何选择性重跑必须同时重跑同一单元的全部正式方法并保留原记录。

内存报告峰值 RSS、完成率和 OOM/系统终止状态。若某方法超过机器可用内存，记录为资源失败，不擅自给另一方法更大内存或交换空间。

## 7. 正确性与输入 gate

正式计时前必须依次通过：

1. 源文件 byte count 与 SHA-256；
2. 图头、边数、点编号、非负权、候选组编号和查询格式；
3. 每条查询存在一个同时命中全部组的连通分量；
4. tiny smoke；
5. 11 个 SteinLib WRP 已知最优值与独立 SCIP-Jack；
6. Light、Heavy 和 Safe 在共同完成的正式查询上目标值一致。

容差为 `abs(w_i-w_ref) <= 1e-6 * max(1,abs(w_ref))`。更小但不一致的答案是 correctness failure，不是性能优势。任何 mismatch 会冻结受影响图和二进制的性能结论，修复后统一重跑。

## 8. 指标、图表与统计

主指标包括逐查询 solver time、wall time、峰值 RSS、完成率、timeout、目标值和状态扩展/剪枝诊断。核心图表为：

- A：固定 `f=400` 的 runtime/完成率随 `g` 曲线，以及固定 `g=10` 的 runtime/完成率随 `f` 曲线；DBLP 与 IMDb 分面。
- B：八图分面的跨 `g` 曲线，并给 dataset-equal-weight 宏平均；不让 Orkut 或查询更多的图自动获得更大权重。
- C：按数据集和自然 `g` 分层的完成率、配对 runtime 与 timeout 分布；稀有高 `g` 同时显示原始样本数。
- 全局：performance profile 或带 timeout 的 capped-runtime 图；双方完成子集上的 speedup 只作为补充。

速度比使用同一查询配对。聚合同时给 query-weighted 和 dataset-equal-weight 版本并明确标签。时间采用对数坐标，完成率给分母，所有表都列出完成/timeout/OOM/error 数量。禁止以 “只看双方完成” 替代完整完成率结论。

## 9. `g=9..16` 的论文动机怎样成立

高 `g` 的必要性不依赖“以往论文已经完整跑过”。论证链是：状态空间随 `2^g` 增长是精确 GST 的核心瓶颈；真实 DBpedia/LinkedMDB 查询已经出现 `g=9..12`；组合检索、实体消歧和多约束探索会自然要求更多组；旧工作通常止于较小 `g` 恰好说明该区间缺少可扩展的精确方法。A/B 在真实官方图上把严格 `g` 扩展到 16，C 诚实展示自然分布到 12，两者共同支撑 motivation，但不能把 `g=13..16` 称为常见自然用户负载。

## 10. 执行与投稿检查

正式顺序为：冻结环境 → 输入/连通性 gate → S0/S1 → 15 小时探针 → 根据探针只修算法或实验工程问题、不改固定样本 → A → B → C → S2 → 汇总和论文作图。探针若发现 Safe 明显更好，必须从方法层分析状态数、下界、上界、队列、witness/anchor 命中和预处理代价，从数据层分析图密度、连通分量、组重叠、组间距离、`f` 分布和边权分布；不能删除不利数据。

投稿前逐项确认：完整数据名和快照写入表格；DBLP/IMDb 只声称图构造家族复刻；A 的 `g/f` 和 10 条样本明确是本文设计；B 的 `f` 只叫 observed；C 的低/高 `g` 都在主实验；Light/Heavy 无 oracle switch；10,000 秒和单线程一致；所有失败状态和分母可审计；旧作者图、GPU 性能和近似质量未混入主聚合。
