# 代码导读

阅读顺序是 `main.cpp → solver.cpp → preprocess.cpp → core.cpp → forward.cpp / adjoint.cpp`。先理解一条查询如何经过这些模块，再查看稀疏行和证书缓存。数学定义与证明见[方法说明](METHOD.md)，输入与命令见 [README](../README.md)。

## 1. 入口与数据

`src/main.cpp` 一次读入图和查询文件，对指定区间逐条调用 `SolveOneQuery`。模式在整次运行中固定；计时包含单条查询的全部算法工作，不包含加载。输出四列：查询号、秒数、最优权值、累计状态数。

`src/graph.h` 定义 `Graph`、`UndirectedEdge`、`AdjEdge` 和 `Query`。图保留原边编号，邻接表的两个方向指向同一条原边；恢复上界时因此可以按边编号去重。`minimum_edge_weight` 和 `component_of` 在加载时计算，供全部查询共享。

`src/io.cpp` 的 `Reader` 使用 8 MiB `fread` 缓冲。整数直接解析，边权用 `std::from_chars` 转成 `double`，不会先转成单精度。`LoadGraph` 先统计度数并预留邻接空间，再填入原顺序的邻接项。`IsQueryFeasible` 判断是否存在一个连通分量同时触及全部组；它是不可行性求解步骤，不是输入合法性检查。

## 2. 一条查询的控制流

`src/abhss/abhss.h` 只公开 `SolveResult` 和 `SolveOneQuery(graph, query, enhanced)`。`enhanced=false` 为 Base，`true` 为 Enhanced。

```text
SolveOneQuery
  空查询、不可行查询、单组查询
  PrepareProblem
    分量覆盖下界与距离—根初始化
    根路径并集、锚组、tour
    构造当前模式的 witness
    共同三元路径生长上界
  WitnessUpperScheduler（rent=0）
  BuildReusableAnchoredSingletonLayer（q>0）
  BuildOrdinaryRows
  释放 A1 查询缓存，保留 A1 行
  BuildForwardAnchoredRows
  SolveHighAdjoint（存在非空 H 后缀）
```

令 `h=g/2`、`q=max(0,h-1)`。Base 的前向 A 到 `q`；Enhanced 的前向 A 到 `min(1,q)`。后者小于 `q` 时才需要 H，此时 D 到 `q`、H 从 `h` 递减；否则两边都使用 D 到 `h` 的完整前向完成方式。判断来自状态范围，不是额外的组数调优参数。

## 3. 公共结构：internal.h

| 结构/函数 | 阅读时关注的内容 |
|---|---|
| `Problem` | 本条查询的组编号映射、上界、距离、证书和 ordinary 行；`best` 只下降 |
| `Row` | 顶点有序的 `vertex/value`，D 专用 `branch_bits`，以及发布标记 `ready` |
| `GroupRow` | 完整或有界组距离；`operator[]` 可返回 cutoff 下界，`ExactValueOrInf` 才能用作真实状态种子 |
| `QueueNode` | 按 `key`、真实 `distance`、顶点编号依次排序；过期项在弹出时跳过 |
| `BuildInitialQueue` | 从聚合后的每顶点标签线性 heapify，避免每个拆分单独入堆 |
| `AccountMaskVertexStates` | 每行批量累加首次变为有限值的顶点数；A1 移交后不重复计数 |
| `CertificateSupportDpCache` | Enhanced 购买证书更新后使用的固定支撑图度量与增量子集 DP |

`original_mask` 使用输入组编号，供 tour、组距离和 dual 查询；D/A/H 的 mask 则只编码非锚组。两者不能混用。空 ordinary 集合在合并中取零；单组 D 直接读 `GroupRow`，不复制一张状态行。

## 4. 预处理与真实上界

`preprocess.cpp` 按以下职责组织：

| 函数 | 职责 |
|---|---|
| `ComputeComponentCover` | 零权分量覆盖下界；纯正权图只处理查询涉及的顶点 |
| `BuildDistanceRootInitialization` | 统一返回组距离、候选根与真实上界；Base 有界，Enhanced 完整 |
| `BuildRootPathUnion` | 在候选根处恢复到各组的路径，对真实边去重 |
| `TourLowerBound::Build / At` | 组维度固定端点路径 DP，以及指定顶点的 tour 下界 |
| `BuildRootPathWitness / BuildDualWitness` | 两种来源的真实树，交给同一个上界调度器 |
| `EvaluateWitnessTree` | 在树上组合单组距离、已发布 D 和父子边，返回可行上界 |
| `BuildPrimalFacilityUpper` | 在 primal 设施点及允许的真实路径上做小型 DP |
| `RefreshPurchasedPathGrowthCertificate` | 四元路径严格改善时，登记它与 primal 的边并图 |
| `CertificateSupportDpCache::Evaluate` | 对支撑图上的受影响 mask 重新合并和闭包 |

`path_growth_upper.cpp` 共用一个生长内核：先枚举有序组对和第三组，再按最近未覆盖组连接；购买增强证书后多固定一个早期组。`Recover` 只沿 tight edges 找真实路径，并用访问标记避开零权环；边费用始终取原图值。

两种模式在预处理中只构造各自 witness，不立即运行树 DP。`core.cpp` 的 `WitnessUpperScheduler` 从零累计 A1/D 的堆弹出和边扫描工作；达到树大小决定的购买点、且输入 D 有新修订时才求值。新证书收紧上界后会重滤已发布 D。

## 5. core.cpp：提前 A1 与普通 D

`BuildReusableAnchoredSingletonLayer` 为每个非锚组构造同一 A1 cone。其 `Continuation` 同时服务 cone 判定和缺项下界，不读取 dual。若条件式树 DP 在中途收紧上界，当前 pass 丢弃，以新 cutoff 重启，最终发布完整的一份 A1。

`AnchoredSingletonFuture` 在 D 中查询“剩余组的最大 A1 值”。第一次触及顶点时缓存前两名及其原值下标；累计查询成本达到顺序扫描成本后物化全图前两名；尾部二分的累计成本再达到排序成本时购买完整 byte 排名。排名不存距离值。D 完成后 `ReleaseLookupCache` 释放这些缓存，A1 行本身移交前向模块。

`BuildOrdinaryRowsImpl` 的一张 D 行可按五块阅读：

1. `Gather` 聚合同根拆分，形成每个顶点的 `split` 值；`ForEachPivotBranch` 消除可进一步同根拆分的重复分支。
2. `CanImprove` 求候选的剩余下界；Base 使用完整缓存，Enhanced 按 dual、farthest、A1、tour 逐阶段求值。
3. `BuildInitialQueue` 后做非负边图闭包；更小距离仍可重新入堆，不使用“访问过即永久关闭”。
4. 对存活顶点排序并发布 `Row`；严格小于同根拆分值的位置才标记为 branch。
5. 更新上下界调度器，并尝试已有行的两块、三块完成。

Enhanced 的拒绝前沿复用 `distance` 工作区存放证书已经拒绝的阈值。它不是实际标签：不入堆、不写 row、不计状态；更小候选通过证书后才登记真实首次发现，行结束时复位剩余虚拟位置。

`core.h` 的有序交集函数按比较次数选择双指针或较小侧二分，枚举的候选集合相同。值交集与 branch 交集是两种不同的数学用途，不能互换。

## 6. forward.cpp 与 adjoint.cpp

`BuildForwardAnchoredRows` 接收公共 A1，然后按子集大小增加 A。`ForEachAnchoredSum` 合并锚定前缀与 ordinary branch；`CompleteAnchoredRow` 在同根补上至多两块 D。最后一层若没有 H 消费者，只完成答案而不持久保存。

`adjoint.cpp` 按补集反向计算 H：

- `BuildTransposedTerminals` 每次处理 64 个连续顶点，把 mask-major D 行转成顶点局部的单块/双块终端。64 只是位图 word 对齐的块宽。
- `ForEachBackwardValueSum` 用 H successor 加 ordinary 的全部值生成递减 H。此处不能要求 ordinary 侧也是 branch。
- `ForEachBackwardBranchSum` 只用于 H 与低层 A 的前向边界完成，此处保留 branch 约束。
- `SolveHighAdjoint` 对每个 H mask 做图闭包和边界结算；必要时另做两个互补辅助 H 半层加锚距离的完成。

## 7. dual_cut.h：增强证书

`DualCutPotential::Build` 逐组构造截断可行势，只重松弛已改变 residual 的弧。势更新只需处理 cone 涉及的边；每条无向边最多一个方向具有正梯度。

`At` 求剩余组势之和。`CanImproveAllExcept` 在 residual 全势闭包后可先用全势减已覆盖势的浮点包围区间判定；区间不能判定时回到逐组求和，不改变 `double` 精度。分阶段缓存区分“足够安全的区间下端”和“已经完整求过的势”。

`CompleteResidualClosure` 在 `ResidualClosureScheduler` 达到结构购买条件时仅执行一次：恢复 residual，补全未支付容量上的势，再恢复 primal。之后释放 residual 临时数组；持久保存的势继续服务 D/H。

## 8. Base 与 Enhanced 的对应关系

| 职责 | Base | Enhanced |
|---|---|---|
| 距离—根初始化 | 上界截断的组距离 | 为对偶势构造完整距离；相同输出结构 |
| 初始 witness | 根路径树 | dual-primal 树；相同树 DP 与购买公式 |
| 提前 A1 | 共同 cone、fallback、缓存、移交 | 相同内核 |
| 普通 D | 共同合并、闭包、branch 发布 | 相同递推；增加对偶证书及分阶段缓存 |
| 高层完成 | 前向 A，加 ordinary 半层 | A1 加 H；辅助 H 半层承担省略 D 半层的职责 |
| 增强上界 | — | primal/facility，以及条件式 residual/support 更新 |

`bool enhanced` 只选择这两套最终执行方式。源码没有中间模式、查询自选模式或按运行结果选择曲线的接口。
