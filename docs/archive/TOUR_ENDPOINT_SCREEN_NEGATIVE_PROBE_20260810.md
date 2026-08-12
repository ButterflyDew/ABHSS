# Tour endpoint 预筛选负面探针（2026-08-10）

## 1. 候选与等价性

ordinary future 原本直接计算完整 rooted tour 下界。候选先计算常数时间的 `EndpointFloorAt`；若该松弛已经证明状态不能严格改善 incumbent，就不再计算完整 tour，否则继续执行原调用。`EndpointFloorAt` 始终不大于完整 tour，因此候选只改变求值顺序，不改变下界值、接纳状态、答案或工作口径。规则不读取图名、组数阈值、边权类型或经验参数，并通过仓库 5/5 CTest。

候选与对照都包含 strict reverse residual closure、势转置、certified complement potential 和 tour local gather，唯一差异是上述预筛选。

## 2. Orkut `g=15` 结果

在正式 P2 第 5 条查询上，两版固定到 CPU 10、11。residual closure 前完成的 23 张 ordinary row 逐张具有完全相同的标量状态、primitive work、witness buy 次数和 incumbent，两版推进锁步，没有形成可见领先。该次短门随后共同进入与候选无关的 residual closure，不作为完整查询结果。

为避免继续等待共同 closure，改用同一图、同一 `g=15` 的第 3 条查询完成端到端成对 gate：

| 版本 | 时间（秒） | 权值 | 峰值 MiB | 状态 |
|---|---:|---:|---:|---:|
| tour local gather 对照 | 280.663883 | 38 | 2690.324 | 30,674,269 |
| endpoint 预筛选 | 282.532442 | 38 | 2689.934 | 30,674,269 |

候选状态与答案逐项相同，但时间增加 0.67%。说明 endpoint 松弛在绝大多数状态上不能独立拒绝，额外距离读取与热分支超过了少量跳过完整 tour 的收益。

## 3. 决策与清理

候选淘汰。正式源码不保留预筛选宏或调用点；隔离构建与全部完整、半截探针输出删除，只保留本文结论。后续不得把同一个被完整 tour 严格支配的 endpoint floor 再次插到 ordinary 热路径前，除非先有不增加存活状态成本的融合实现。
