# 严格支配操作与减空审计（2026-08-15）

本文记录当前 ABHSS 源码的一次全链路“减成空”审计。目标不是追求源码行数最少，而是在不改变精确语义、论文主线和物理性能的前提下，删除被其他操作严格覆盖的重复工作。正式方法口径仍以 `../METHOD.md` 和 `../CODE_GUIDE.md` 为准；本文保存接受与拒绝候选的证明、实测和最终硬门状态。

> **当前勘误。** 本文最初记录的候选 E 及之后的单调证书优化本身没有引入伴随层错误，但它们继承了“从最高逻辑层直接启动 H”的不精确实现。旧 P1 Enhanced 与 Orkut g15 q1--q10 结果因此全部降级为历史性能探针。当前源码已加入辅助 $H(h)$、必要的较低双块 terminal、successor 全值读取与互补 H 完成；事故演进、反例和证明见 [辅助半层 Adjoint 正确性审计](AUXILIARY_HALF_ADJOINT_CORRECTNESS_AUDIT_20260815.md)及 [Adjoint split 完备性审计](ADJOINT_SPLIT_COMPLETENESS_AUDIT_20260817.md)。当前 P1 与清理后 Orkut q1--q10 硬门仍须重新运行。

> **二次勘误（2026-08-17）。** 辅助半层的第一轮修复曾错误地删除全部较低直接 terminal；后续 Musae 全量审计证明该实现仍不完备：较低 H 的平衡 split 需要必要双块终端，successor 需要 ordinary 全值，非锚组二等分还需要互补 H 完成。本文第 5、6、8 节中与此冲突的减空结论只作失败方向历史记录，不再描述当前源码。现行证明与门禁见 [Adjoint split 完备性审计](ADJOINT_SPLIT_COMPLETENESS_AUDIT_20260817.md)。

> **长门更新（2026-08-17）。** 修复版 Orkut g15 q10 已以 30,000 秒诊断预算自然完成，精确值 54、solver 时间 11,178.235 秒，故没有通过正式 10,000 秒硬门。该轨迹还证伪并回退了后续证书链上包络；完整数据、阶段时间与当前构建门见同一 Adjoint split 审计。

> **当前更新（2026-08-18）。** 本地 checkpoint `e9eee92` 把 Enhanced 的共同前向前缀固定为 A1，并由 H 实现逻辑层 2 到 q。该 checkpoint 的诊断二进制在 Orkut g15 q10 上以精确值 54、9,431.057 秒通过单条 10,000 秒门；查询峰值 18,305.023 MiB，状态数 1,459,398,194。其后的工作树只增加了 adjoint 空域扫描减除，但二进制哈希已经改变，所以最终 q10 仍要无 10,000 秒截断地自然跑完。q1--q9 与完整 P1 也待复跑，详见 [最小共同 A1 与 q10 门禁](MINIMAL_FORWARD_A1_ADJOINT_GATE_20260818.md)。

## 1. 审计状态

- A--E 的局部减空仍保留；其历史回滚构建 SHA-256 `3d02cd2ec7ea8bd45db8acee5d2fc29e0232b39548e0539eaf7782bd7e46adad` 只证明这些局部改动的当时物理对照，不再标识当前源码。
- 当前修复版的两种构建均已通过 5/5 CTest；二进制 SHA-256、Musae 全量与 SteinLib 结果记录在 Adjoint split 审计中，不得沿用本文件的旧值。
- 接受项 A--E 与新增辅助半层减空均不读取图名、查询编号、运行时间、状态数、row 密度或经验阈值，不压缩 `double` 精度，也不假设边权为整数。
- 旧错误二进制的 Orkut `g=15` q1--q10 曾全部在 10,000 秒内结束，最紧 q10 为 9,934.603 秒；该数字只用于评估当前复跑风险，不是当前门禁结果。
- `e9eee92` 的 q10 已以 9,431.057 秒通过单条正式 TL；最终工作树的完整 P1 与 q1--q10 尚未执行，因此本文不宣称最终门禁通过。
- `e9eee92` 已通过 A1 购买分支直接回归、Musae g7 全 300 条逐条一致，以及 Base/Enhanced 各 11/11 SteinLib 已知真值；最终空扫描候选另通过两种构建各 5/5 CTest 和本文件记录的 Musae 门。完整 P1 与 Orkut q1--q10 全门仍是独立最终硬门。

## 2. “被支配”的判定标准

只有同时满足以下条件，才能把操作减成空：

1. **数值支配**：被删操作的结果在所有合法输入上都不可能改变当前最小值、最大下界、可行上界、队列顺序、规范代表或输出 witness。
2. **时序支配**：支配结果在同一消费点之前已经可用。一个更强但更晚才计算的证书，不能支配更早用于拒绝候选的便宜证书。
3. **副作用为空**：被删操作不承担状态首次登记、owner 移交、branch 标记、changed-arc 登记、真实边恢复、诊断计数或缓存物化职责。
4. **浮点轨迹不变**：不能用重新结合求和、降低精度、epsilon 放宽或整数化来制造“等价”。
5. **物理不退化**：对热代码，源级严格减空仍须经过同机对照。LTO、内联和代码布局可能使逻辑更少的版本实际更慢；这种候选不能进入论文实现。

允许保留少量平凡连接操作，例如边界检查、调用接口和可选诊断参数，只要它们使证明边界清楚或实测有利。它们不应被包装成新的算法步骤。

## 3. 全链路职责清单

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

## 4. 接受的严格减空

### 4.1 A：删除已命中 cache 的第二次 accessor

`CanImprove` 成功返回时已经把当前 epoch 的完整 future 写入 `bound_cache[vertex]`。随后 heap key 再调用 `Bound` 或 `Prefix` 只会命中同一 cache，不可能计算新值。当前实现直接读取 cache；同时把 `staged_certificate_cache` 提到循环外，并让 exact-dual 位只在诊断构建中写入。

这不改变证书阶段、值、heap key、状态数或拒绝集合。YouTube 作者查询前 30 条的同机结果为：

| 配置 | 对照 / 秒 | 候选 A / 秒 | 变化 |
|---|---:|---:|---:|
| Base | 188.152039 | 187.907820 | -0.13% |
| Enhanced | 206.065450 | 204.751011 | -0.64% |

### 4.2 B：用 tour 上包络跳过必然无贡献的精确求值

调用点先得到已经需要的 farthest 与 dual 最大值。若该前缀已经不小于 `tour.UpperEnvelope`，则精确 `tour.At` 不可能提高最终最大值，因而被严格支配；只有前缀低于上包络时才求精确 tour。

交叉绑核结果中 CPU5 从 206.165259 秒降到 204.657361 秒；CPU4 为 206.277161 与 206.314776 秒，属于噪声范围。候选不改变权值和状态数，且删除条件直接来自上包络证明，因此接受。

### 4.3 C1：删除 `GroupRow touched` 中不可能产生的去重

多源 Dijkstra 仅在顶点标签由无穷首次变为有限时把顶点加入 `touched`，之后只允许严格改善；同一组内天然唯一。完整与 bounded-dense 布局不依赖顶点递增，直接使用 dense payload；ranked-bitmap 布局只排序一次以生成递增压紧 payload，不再调用结果必为空的 `unique`。

| 配置 | 对照 B / 秒 | C1 / 秒 | 变化 |
|---|---:|---:|---:|
| Base | 189.306721 | 183.410831 | -3.11% |
| Enhanced | 205.710297 | 205.451459 | -0.13% |

### 4.4 D：把 membership 检查与精确 singleton 读取合成一次

旧路径先调用 `IsExact(v)`，再通过另一入口重复定位相同 dense/bitmap 位置。现在所有随机 singleton 消费者统一调用 `ExactValueOrInf(v)`：精确位置返回真实距离，cutoff 占位返回正无穷。`operator[]` 仅保留给需要 cutoff 下界占位的消费者，`ForEachExact` 保留给枚举消费者。

路径恢复还把当前 tight distance 缓存在顶点循环外，避免每条邻边重复 rank 定位。接口合并没有把 cutoff 当 DP seed，也不改变任何距离值。

| 配置 | 对照 C1 / 秒 | D / 秒 | 变化 |
|---|---:|---:|---:|
| Base | 182.929343 | 180.326254 | -1.42% |
| Enhanced | 207.535515 | 205.834483 | -0.82% |

### 4.5 E：每条无向边只回写唯一可能为正的势梯度

一条无向边两端势相等时两个方向梯度均为零；不等时只有高势端到低势端的方向为正。旧实现对两向都执行 `max` 回写，零方向严格无效。初始 cone 扣减与购买后 residual 补全现在共用 `SubtractPositiveGradient`，只标记并扣除唯一正方向；恢复初始 residual 时也跳过零梯度赋值。

这保持相同减法、residual、changed-arc 集和 primal 支撑，不利用容差。

| 探针 | 对照 D / 秒 | E / 秒 | 变化 |
|---|---:|---:|---:|
| YouTube Enhanced 前 30 条 | 205.910918 | 203.704031 | -1.07% |
| Orkut g15 q3 | 288.672101 | 285.567882 | -1.08% |

### 4.6 F：完整 D 只支配同目标 pair

正确的 H 不能从最高逻辑层 $q=h-1$ 直接启动；它必须先在辅助层 $H(h)$ 完成被省略 ordinary 半层的精确转置。更早版本曾进一步声称“较低 H 的全部直接 terminal 都被 successor 支配”，Musae q162/q295 已给出反例：若 ordinary 规范 split 的两侧都不超过 q，而任一侧加入当前目标后都会越过 H 的物理上界，则必要双块 terminal 是唯一入口；successor 还必须读取新增 ordinary 块的全部精确值，不能只读 branch。故“删除全部较低 terminal”不是接受项，相关源码已经恢复。

当前只保留一个逐值严格支配：对固定 H 目标 S，若完整 ordinary $D(Q)$ 已发布，其中 $Q$ 是 S 的补集，则 $D(Q,v)$ 已是所有同根 split seed 经相同图闭包后的精确最小值；同目标任意 pair 都是两棵可行 rooted 子树的和，不可能小于 $D(Q,v)$。因此该目标只装载单块 $D(Q)$，不再枚举 pair。若完整 $D(Q)$ 不存在，所有证明所需的双块 terminal 仍完整保留。这个删减只读 row 的已证明发布域，不读取图名、查询统计或运行表现。

### 4.7 G1--G7：当前辅助半层版本中的严格等价减空

这一组改动不增加下界或上界，只删除由当前生命周期和有序容器合同严格蕴含的重复工作。

1. **证书升级后的 ordinary 稳定重滤原地压紧。** 旧实现为每张 row 重新分配 branch bitmap、重算全部 branch 数和最小值。新实现保持顶点顺序，用 read/write 游标原地搬移仍存活项；branch 位随项搬移，branch 数只减去被删项。只有被删值与旧最小值精确相等时才重扫剩余 payload，否则旧最小值仍由未删除项实现。若一项未删，row 完全不写。删除谓词、剩余 payload 和 padding 位逐项相同。
2. **D/A/H 初始标签统一线性建堆。** `touched` 中每个顶点只出现一次，且 key、distance、vertex 三元组已经确定。`BuildInitialQueue` 把完全相同的节点交给标准线性 heapify；`QueueNode` 以 vertex 作末级比较，形成全序，所以与逐项 `push` 的 pop 轨迹一致，只删除建堆的重复对数调整。
3. **只在层序已证明处删除 ready 检查。** ordinary split 的两侧都是真子集，已经由较低层发布；平衡补集只在更低层或同层更小编号时消费；forward A 的锚定侧是真子集或隐式空侧，ordinary 侧在整个 A 阶段前已经完成；H successor 严格位于已完成的更高层。普通 forward A 若从未产生标签，会省去 ready 写入，但严格 size 层序已证明该 mask 被处理，后继只把空 payload 读作无穷。ordinary、提前 A1 owner 交接、H 和其他跨阶段边界仍显式保留 `ready`，没有把普通 row 的“已发布空”与“未生成”混写。
4. **转置不变量移出 64 顶点块。** ordinary 按完整层发布，因此一个规范 representative 的 availability 等价于整层 availability；可转置 mask 在进入顶点块前按原数值升序筛一次。pair 与 submask 两种等价枚举的工作量选择中，一旦累计 pair work 已严格超过 submask work，后续非负增量不可能改变选择，立即停止计数。`Update` 的目标 popcount 由三个调用点的 cover 条件逻辑蕴含，但删除它在偶数 g=14 的四轮 Release 门中合计回退 0.494%，因此作为局部域合同保留，见第 5 节。
5. **H 边界只生成 successor 子掩码。** 旧循环扫描整个子集格，再用 `mask & ~successor` 拒绝绝大多数 mask。新循环按相同数值升序生成 successor 的全部子掩码，并保留相同的低层域条件；所有被省略 mask 都必定在旧谓词处失败。H seed 中的 successor 由当前 mask 加非空 outside block 得到，严格位于已完成高层，因而删除第二次生命周期读取。
6. **二分工作量用位宽直接计算。** 非空长度的旧循环结果严格等于其二进制位宽，空表仍定义为 1。编译器位扫描只替换计数循环，不改变交集算法选择式。
7. **缺少最小单块 cover 时跳过整段直接扫描。** 单块 terminal 的 cover 区间从 $k-h$ 开始，ordinary 发布域按块大小向下闭合。若这一最小层的 representative 不可用，则更大的完整 D 也全部不可用；旧逐值循环的 cover 条件必定逐项失败。当前实现只跳过这段严格空扫描，双块 terminal、successor、互补 H 和 `Update` 域合同全部保留。判断只读取层域 availability，不直接按奇偶、图名、查询编号或计时分派。

普通 probe 现在只保留稀疏阶段事件；逐候选证书计数需要显式 `GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS`。这项拆分只消除探针构建的观测开销，不属于论文算法步骤。共同 A1 的 lazy/顺序物化则是相同精确视图的物理调度，不是严格删状态；其证明、直接分支覆盖和小门见 [A1 top-two 自适应物化门禁](ADAPTIVE_A1_TOP_TWO_MATERIALIZATION_GATE_20260816.md)。

### 4.8 A1 tail 的精确物理候选（不计入严格支配）

1. **精确 mask-rent 因子化。** top-two 购买后，同一 remaining mask 的逐 bit 租金只重复相加由 row 长度决定的整数。子集递推表逐项等于原和式，不改变累计 rent 或排名购买点；tail 查询足够多时，它把重复加法换成表读取。但若购买后几乎没有 tail 查询，预构造 $2^k$ 项可能净增工作，所以它不是所有输入上的严格支配，只能按物理缓存接受并经过 P1 门禁。
2. **一次性物化的冷机器码边界。** `MaterializeAllTopTwo` 与 `MaterializeRankedTail` 每条查询各至多执行一次；非内联边界只阻止 IPO 把冷购买代码并入逐状态 `Future`。它不是新开关，也不删除数学操作；两轮 Musae 小门分别为候选/对照 0.990564 和 0.979960。

完整 byte 排名、租金表和冷边界都是同一精确 A1 视图的物理 representation，不是严格删状态或新的算法证书；是否最终保留必须由完整 P1 与 Orkut 硬门共同决定。无条件和购买后 staged second-rank ceiling 均已实测回退。证明、复杂度、P1/q10 窗口和负结果见 [A1 完整排名、精确租金因子化与 ceiling 负向门禁](A1_COMPLETE_RANKING_AND_STAGED_CEILING_GATE_20260817.md)。

最终空扫描候选的 production SHA-256 为 `96a4add04c2a731f76c1c61bd7762760ee2e8eb11f0bfab7c2e3abfcf25554c3`，diagnostic SHA-256 为 `5c81392125b8b003ec681e679599f9caf202befb0f6f1caf53e2f65b885992c9`；两种构建均通过 5/5 CTest，包括 5000 个随机、500 个正权唯一终端和 160 个辅助半格实例。Musae g7 全 300 条四轮合计为候选 89.201 秒、对照 89.072 秒，变化 +0.145%；奇数 g15 十条两轮为候选 334.142 秒、对照 334.836 秒，变化 -0.207%；偶数 g14 十条两轮为 +0.046%。三组答案、状态与空间逐项一致，故只将它判为端到端中性、transpose 局部减空，不包装成算法贡献。`e9eee92` 的 q10 以 9,431.057 秒返回精确值 54；最终 SHA 的完整 P1 与 q1--q10 仍须运行。

### 4.9 A1 发布屏障后的恒真检查

1. **singleton ready。** A1 重启循环只有在全部 singleton row（含空 payload）写入 `ready` 后才初始化只读 future；发布屏障之后的重复检查不再承担合法性、fallback、购买或 owner 职责。删除后 Musae 两轮几何比为 1.000479，Orkut q10 900 秒两轮均把对照事件序列严格延长 2 个事件，内存不增。
2. **ranked buy 正性。** A1 域推出 $g\ge4$、 $k\ge3$、 $n\ge1$，故 `ranked_buy_work` 严格为正。热路径只比较 rent 是否达到 buy。Musae 两轮几何比为 0.998610；Orkut q10 两轮分别延长 5 和 8 个事件，内存不增。

两项的代码、证明、SHA 与完整交换轮见 [A1 发布屏障与恒真检查减空门禁](A1_PUBLICATION_BARRIER_REDUCTION_GATE_20260817.md)。它们删除共同 A1 realization 的重复检查，不是新证书或可调开关。

## 5. 已证明逻辑可删、但因物理回归而拒绝

### 5.1 H--J：热边界检查和可选计数接口

H 同时删除 bitmap padding guard、`GroupRow` 边界、`RecoverPrimal` 可选工作量指针、A1 pop cutoff 与路径恢复根检查。I 恢复 A1 cutoff 和根检查。J1 再恢复 `RecoverPrimal(..., long long* work=nullptr)` 后，Enhanced 恢复；J2 则尝试只恢复 bitmap 检查，但再次明显回归。

| 版本 | Base / 秒 | Enhanced / 秒 | 结论 |
|---|---:|---:|---|
| E 同轮对照 | 181.086371 | 203.580140 | 接受基线 |
| H | 178.755346 | 205.342039 | Enhanced 回归，拒绝 |
| I | 179.731421 | 204.552449 | Enhanced 回归，拒绝 |
| J1 | 179.017812 | 202.590997 | 说明可选计数接口影响 LTO 布局 |
| J2 | 181.273388 | 207.432285 | 明显回归，拒绝 |

可选 `work` 只用于隔离探针计数，论文调用传空，不是算法步骤。保留它和短边界检查是物理实现选择，不能在论文中包装成额外优化。

### 5.2 L--M：按 primal bitmap 枚举 witness 边

`BuildDualWitness` 当前扫描原图边数组并检查 primal bitmap。L 改为逐 64-bit word 枚举置位，理论工作由全边扫描变为 word 扫描加 witness 边数，而且保持 edge-id 顺序。5 个 CTest 全部通过，但 LTO 改变了相邻 ordinary 热函数的机器码形状，Enhanced 的 30 条中有 28 条变慢。M 用 `noinline` 隔离该冷边界仍未恢复。

| 版本 | Base / 秒 | Enhanced / 秒 | 结论 |
|---|---:|---:|---|
| E 同轮对照 | 181.086371 | 203.580140 | 接受基线 |
| L：bitmap 置位枚举 | 179.051621 | 205.879566 | Enhanced +1.13%，拒绝 |
| M：L + noinline | 181.441349 | 206.206884 | Enhanced +1.29%，拒绝 |

因此“渐近工作更少”不足以进入当前论文二进制。若未来要重试，必须隔离翻译单元并重新跑完整 P1，而不能直接恢复 L。

### 5.3 N--Q：D/H pop cutoff 与 forward 空行 ready 补写

ordinary D 和 adjoint H 在单张 row 的队列闭包内不更新 `best`，所以出堆时的第二次 `key < best` 在逻辑上由入队证书蕴含；forward A 与提前 A1 会在队列内收紧 incumbent，不能使用这一结论。普通 forward A 的零标签行也可由严格层序与空 payload 直接表示，无需显式补写 ready。候选全部保持权值与状态，但删除 D/H pop cutoff 的组合在 Musae 回归 0.68%，删除 H cutoff 并补 ready 在 Orkut q3 回归 0.67%；单独逐行和批量补 ready 在 Musae 分别回归 1.50% 与 1.41%。当前源码已经全部恢复，详细交换绑核证据见 [Queue pop 与 forward 空行发布负向探针](QUEUE_POP_AND_EMPTY_READY_NEGATIVE_PROBE_20260816.md)。

### 5.4 R--S：staged cache 延迟发布与冻结配置分派

新 epoch 的临时 cache 零写会被首次 dual 结果覆盖，查询内 frozen config 判断也可由入口模板分派一次；两者在源码语义上均可减空。当时前者在 Musae 两轮均值回归 0.66%、Orkut q3 回归 0.42%，模板分派也回归 1.01%，故都恢复。ordinary 内核后来发生实质改写后按原文的重试条件复查：模板分派的 Base 两轮合计快 0.39%，Enhanced 与一个后来删除的额外分支联合时合计慢 0.13%，交换核方向反转，判定为无可辨认退化；当前仅恢复同源模板的入口分派，epoch/cache 延迟发布仍拒绝。详细证据见 [Staged certificate cache 物理减写与冻结分派负向探针](STAGED_CACHE_PHYSICAL_REDUCTION_NEGATIVE_PROBE_20260816.md)。

### 5.5 T--U：公共 A1 必然存在与 ordinary size-1 扫描

共同精确基例已经闭包全部 $g\le3$ 查询，因此进入指数递推的查询必含 A1，ordinary 也不需要 size-1 row。删除 nullable A1 分支并跳过 size-1 扫描的合并候选虽减少 305 字节 text，Musae 全 300 条两轮均值仍回归 1.58%；单独跳过扫描在 Musae 持平，但 Orkut g15 q3 两轮均值回归 0.72%。全部正确性、权值和状态证据一致，源码已恢复。详细证明与交换绑核结果见 [公共 A1 必然存在与 ordinary size-1 扫描负向探针](MANDATORY_A1_AND_SIZE1_SCAN_NEGATIVE_PROBE_20260816.md)。

### 5.6 A1 top-two 哨兵与 Future 入口域检查

两个候选都能由当前调用链证明恒真/恒假，但删后发生稳定物理回归：top-two `>=0` / `!=255` 守卫的 Musae 两轮几何回归 0.9028%；`Future` 入口 `first.empty() || !remaining` 的两轮几何回归 1.6241%。权值与状态逐项一致，说明回归来自机器码布局而非算法差异。两项均已恢复；没有使用 NOP、强制对齐或按编译器特调来掩盖回归。

### 5.7 Adjoint `Update` 目标域复查

direct 单块、排序 pair 和互补 submask 三个调用点都先把 cover 限制到合法区间，因此 `Update` 内再次读取目标 popcount 并拒绝越界在逻辑上恒不触发。删除该检查的候选通过两种构建各 5/5 CTest，答案、状态和空间也逐项不变，但无诊断 Release 在 Musae g14 十条查询的四轮交换绑核中合计由 410.412 秒增至 412.439 秒，回退 0.494%。恢复短检查后，同一偶数域两轮只差 +0.046%。当前把它作为 lambda 的局部输入合同保留；这属于已测得更优的平凡连接操作，论文不把它写成剪枝或独立优化。

## 6. 其他拒绝项与原因

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

## 7. 不能误判为支配的有效操作

- **便宜 farthest 与较晚的 A1/tour/dual**：后者即使数值更强，也不能删除更早且便宜的拒绝。
- **staged certificate 的各阶段**：每一级都可能在支付下一阶段成本前拒绝；它们是时序链，不是重复求同一值。
- **证书升级后的 refilter**：新 incumbent 或更强 future 会使已经生成的 row 出现过期存活项，重滤不是重复闭包。
- **triple 与 quad path growth**：triple 在购买前提供共同早期上界；quad 只在购买后追加一层，候选集合和时机不同。
- **SPT、root-star、path-growth、facility/primal 与 witness 上界**：任何一个在某些实例更强，不代表在全部实例、全部时间点支配其余。
- **Bootstrap SPT 上界**：除了初始 incumbent，还决定 bounded 多源 Dijkstra 的安全 cutoff；后续 root-star 更优也不能追溯删除其初始化职责。
- **root-path union**：同时提供可行上界、根候选和 witness 输入，不是 Base 独占的额外方法步骤。
- **A1 pop cutoff、恢复根检查和 bitmap padding guard**：前置不变量通常蕴含它们，但实测删除损害机器码或丧失局部断言边界，按平凡连接操作保留。
- **`BuildDualWitness` 全边扫描**：L/M 已证明源级减空但物理退化，当前保留。

## 8. 论文可写性核验

当前接受项可统一描述为“在相同精确递推和证书链内，避免重复 realization”：

1. 已物化 cache 直接消费；
2. 已有上包络证明无贡献时不求精确 tour；
3. 利用 Dijkstra 首次触及唯一性，不做空去重；
4. singleton membership 与读取合并为一个有类型含义的接口；
5. 势梯度只回写真正非零的方向；
6. 辅助 H 半层建立精确基例；只有已有完整 $D(Q)$ 时才不再枚举被其逐值支配的同目标 pair，缺少完整 D 的必要较低 terminal 全部保留；
7. 最小单块 cover 层不可用时，由 ordinary 发布域的向下闭合证明整个单块扫描为空，但双块 terminal 和 successor 不变。

它们没有引入第三种方法、图特化、经验分派或近似数值语义。A1 完整排名、租金表和非内联边界只属于经过实测选择的物理布局，不进入上述严格支配清单，也不应写成独立算法贡献。Base 与 Enhanced 的论文关系仍是：共同执行精确主干；Enhanced 在相同 A1 和 ordinary 基础上增加 DirectedCut，并用结构同职责的 H realization 替换高层 A realization。接受项不会让 Base 获得 Enhanced 不包含的算法职责。

## 9. 最终硬门

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

`e9eee92` 已满足第 2 项中的 q10 子门，但最终空扫描 SHA 尚未复跑；最终发布前仍必须满足：

1. 最终源码的完整 P1 按图聚合后，13 图中每图的最快 ABHSS 配置不劣于 PrunedDP++；
2. 最终源码的 Orkut `g=15` q1--q10 每条都在 10,000 秒内精确完成；
3. q10 权值仍为 54，状态数按当前辅助半层状态域重新如实记录，且不存在超时后复用旧结果；旧状态数 1,654,690,262 不再是相等门；
4. CTest、Markdown 渲染检查、环境清单和源码—文档矛盾检查全部通过。

在这四项取得当前文件和当前二进制的直接证据前，总目标保持未完成。
