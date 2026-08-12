# Double radix queue 负面探针（2026-08-10）

## 1. 候选

候选只把 ordinary 图闭包的 `std::priority_queue` 替换为 65 桶 radix queue。key 使用非负有限 `double` 的 IEEE-754 单调序位；桶按真实 Dijkstra distance 调度，未来下界仍在 seed、松弛和 pop 三处执行完全相同的可采纳筛选。A1、forward、adjoint、上下界、状态定义和 incumbent 均不改变。该实现不依赖整数权、桶宽超参数、图名或组数阈值，并通过仓库 5/5 CTest。

## 2. Orkut `g=15,q5` 成对结果

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

## 3. 决策与清理

该方向淘汰。它不减少状态，也没有形成物理加速，不应作为论文机制。源码宏、radix queue 类、隔离构建和 `/tmp` 未完成输出全部删除，只保留本文；后续不再把 ordinary binary heap 当作 Orkut `g=15,q5` 的主要瓶颈。
