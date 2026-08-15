# 严格支配操作与减空审计（2026-08-15）

本文记录当前 ABHSS 源码的一次全链路“减成空”审计。目标不是追求源码行数最少，而是在不改变精确语义、论文主线和物理性能的前提下，删除被其他操作严格覆盖的重复工作。正式方法口径仍以 `../METHOD.md` 和 `../CODE_GUIDE.md` 为准；本文保存接受与拒绝候选的证明、实测和最终硬门状态。

> **当前勘误。** 本文最初记录的候选 E 及之后的单调证书优化本身没有引入伴随层错误，但它们继承了“从最高逻辑层直接启动 H”的不精确实现。旧 P1 Enhanced 与 Orkut g15 q1--q10 结果因此全部降级为历史性能探针。当前源码已加入辅助 $H(h)$ 并删除被其严格支配的低层直接 terminal；事故、反例和证明见 [辅助半层 Adjoint 正确性审计](AUXILIARY_HALF_ADJOINT_CORRECTNESS_AUDIT_20260815.md)。当前 P1 与 Orkut 硬门必须重新运行。

## 1. 审计状态

- A--E 的局部减空仍保留；其历史回滚构建 SHA-256 `3d02cd2ec7ea8bd45db8acee5d2fc29e0232b39548e0539eaf7782bd7e46adad` 只证明这些局部改动的当时物理对照，不再标识当前源码。
- 当前修复版的最终 CTest、二进制 SHA-256 与仓库一致性结果在本轮本地提交前重新生成；不得沿用旧值。
- 接受项 A--E 与新增辅助半层减空均不读取图名、查询编号、运行时间、状态数、row 密度或经验阈值，不压缩 `double` 精度，也不假设边权为整数。
- 旧错误二进制的 Orkut `g=15` q1--q10 曾全部在 10,000 秒内结束，最紧 q10 为 9,934.603 秒；该数字只用于评估当前复跑风险，不是当前门禁结果。
- 完整 P1 与最终 Orkut 硬门尚未执行，因此本文不提前宣称最终门禁通过。

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
| 公共 A1 | seed、farthest、endpoint-floor cone、正 fallback、图闭包、owner 移交 | Base 与 Enhanced 完全相同，不能由 dual 或 H 替换 |
| ordinary D | 规范 split 聚合、统一 future、图闭包、branch 标准化 | 都参与精确状态生成；只删除重复 accessor 和不可能有作用的容器操作 |
| future 链 | dual、farthest、A1、tour 与 staged cache | 后段可能更强，但前段能更早拒绝，故按时序保留 |
| forward A / adjoint H | 公共低层 A、辅助半层 terminal、递减 H | 辅助 $H(h)$ 精确转置被省略的 $D(h)$；逻辑 H 后缀替换高层 A，不能省略半层闭包 |
| DirectedCut | potential、cone、changed arc、residual、exact fallback、primal 恢复 | 只删除零梯度方向回写；其余操作维持对偶可行性与可行上界 |
| witness rent-or-buy | 两边各自 witness、统一零起点 rent、同一 buy 公式、同一树 DP | witness 不同，但调度合同和树 DP 共同；不能恢复无条件树 DP |
| 状态容器 | epoch、bitmap padding guard、ready、branch bitmap、settled 排序 | 部分检查逻辑上由上游不变量蕴含，但删后物理回归，按实测保留 |

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

### 4.6 F：辅助半层只保留必要 terminal

正确的 H 不能从最高逻辑层 $q=h-1$ 直接启动；它必须先在辅助层 $H(h)$ 完成被省略 ordinary 半层的精确转置。偶数 $g$ 时，辅助目标的补集已有完整 $D(q)$，该单张 D 逐值支配所有同目标 pair；奇数 $g$ 时才用双块 split seed 重建 $D(h)$ 并执行图闭包。此后每张较低 H row 都由 successor 加规范 ordinary branch 及同一图闭包归纳得到，因此其旧式直接 terminal 是重复 realization，可减成空。

该操作修复的是精确职责，不是针对 Orkut 的性能开关。它只读取由递推域决定的层号和 ordinary ready 状态，不读取实际 $g$ 常数分段、图名、查询特征或运行统计。12 点固定反例从旧错误值 6.25 恢复为 5.75，九条已知生产差异全部恢复；完整证据见辅助半层事故审计。进一步把奇数情形 pair 限制为某一规范 branch 的候选虽仍精确，但交换绑核实测略慢且显著增加 review 复杂度，因此拒绝。

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
6. 辅助 H 半层建立精确基例后，不再重复播种较低 terminal；已有完整 D 时不再枚举被其支配的 pair。

它们没有引入第三种方法、图特化、经验分派或近似数值语义。Base 与 Enhanced 的论文关系仍是：共同执行精确主干；Enhanced 在相同 A1 和 ordinary 基础上增加 DirectedCut，并用结构同职责的 H realization 替换高层 A realization。接受项不会让 Base 获得 Enhanced 不包含的算法职责。

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

最终发布前必须满足：

1. 最终源码的完整 P1 按图聚合后，13 图中每图的最快 ABHSS 配置不劣于 PrunedDP++；
2. 最终源码的 Orkut `g=15` q1--q10 每条都在 10,000 秒内精确完成；
3. q10 权值仍为 54，状态数按当前辅助半层状态域重新如实记录，且不存在超时后复用旧结果；旧状态数 1,654,690,262 不再是相等门；
4. CTest、Markdown 渲染检查、环境清单和源码—文档矛盾检查全部通过。

在这四项取得当前文件和当前二进制的直接证据前，总目标保持未完成。
