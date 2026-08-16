# [负结果] ordinary 半层直接复用为最高 H

日期：2026-08-15

基线提交：`849c89640404eed42553cedaa1b51e37a69c7ce7`

结论：否决；实现已全部清理，不能作为当前方法、正确性证据或性能结果引用。

## 1. 试图删除的工作

checkpoint 的 Enhanced 先生成 ordinary 到最高逻辑层，再由辅助 `H(h)` 重建被省略半层的补集状态。候选试图把 ordinary 继续生成到平衡半层，将 `D(F\S,v)` 按补集直接转交给最高 `H(S,v)`，从而删除：

- 按顶点转置 ordinary 值；
- 奇数组数时的双块 terminal 枚举；
- 最高辅助 H 的第二次图闭包。

层边界只由 `g` 的平衡公式决定，没有读取图名、时间、状态密度或经验阈值。所有权转移先完成仍需两张 ordinary 半层的边界结算，再移动 payload，因此原型本身没有悬空引用或重复状态计数。

## 2. 初看有利但不足的证据

直接转交原型通过：

- 5,000 个确定性随机连通实例，`g=2..10`；
- 500 个正权互异单终端压力实例，`g=6..10`；
- 160 个专门覆盖省略半层的实例，`g=7..16`。

三种配置累计状态为 `103878/31982/28832`，而 checkpoint 为 `103878/31982/33869`。Orkut `g=15` q3 的同核结果为：

| 实现 | solver 秒 | 状态数 | 查询峰值 MiB |
|---|---:|---:|---:|
| checkpoint | 281.881173 | 31,191,511 | 2689.352 |
| ordinary 半层直接转交 | 271.603184 | 30,673,451 | 2689.727 |

时间下降 3.65%，状态下降 1.66%，但这些性能数字不能覆盖已知真值反例。

## 3. 决定性反例

SteinLib `wrp4-16` 的公开已知最优值为 1190：

| 版本 | 返回值 | 状态数 | 结论 |
|---|---:|---:|---|
| checkpoint Enhanced | 1190 | 19,123 | 正确 |
| 候选 Base | 1190 | 24,590 | ordinary/前向主路径仍正确 |
| 直接转交 Enhanced | 1191 | 18,437 | 错误 |
| ordinary seed 再做 H 闭包 | 1191 | 18,702 | 错误 |
| 平衡层停用 A1 后再闭包 | 1191 | 18,702 | 错误 |
| 平衡层停用全部 future 后再闭包 | 1191 | 30,521 | 错误 |

最后一项只按真实 incumbent 截断平衡层，仍未恢复 1190。因此错误不能归因于 A1、dual、farthest 或 tour 中某一个平衡层证书，也不能通过给最高 H 再加一次 Dijkstra 修复。

原始探针目录：

- `results/probes/shared_ordinary_half_steinlib`
- `results/probes/shared_half_wrp4_16_base`
- `results/probes/checkpoint_wrp4_16_enhanced_control`
- `results/probes/shared_half_reclosure_wrp4_16`
- `results/probes/shared_half_no_a1_boundary_wrp4_16`
- `results/probes/shared_half_no_future_wrp4_16`

这些目录仅是本地负结果证据，不进入正式实验矩阵。

## 4. 原因

`D(F\S,v)` 与 `H(S,v)` 的未截断数学值存在补集恒等关系，但当前 sparse row 不是“全顶点函数表”。较低 ordinary row 已按前向完成域安全截断，并只保留该域后续消费者所需的规范 branch。最高 ordinary 半层即使不再使用任何 future，它的 split seed 仍来自这些已经截断的低层 row。

checkpoint 的辅助 H 不是把一张完整 dense `D` 重算一次。它先把仍存活的低层 ordinary 值转到补集域，在 H 自己的 admissible prefix 下重新执行图闭包，再逐层递减。这个过程会恢复反向完成域需要、但不属于 ordinary 前向 payload 的顶点状态。要让普通半层直接复用保持精确，必须从更低层开始递归重建这种补集闭包；那会重新得到现有 H 递推，只是增加一套别名和生命周期。

因此，“ordinary 半层直接转交”没有严格支配辅助 H；它删除了有效操作。随机小图未覆盖该结构，SteinLib 已知真值 gate 才给出决定性反例。

## 5. 后续约束

- 不再尝试把截断后的 ordinary row 直接改名为 H。
- 不把 A1 描述为该反例的原因；停用 A1 和停用全部平衡层 future 都未修复。
- 可以继续优化转置的物理枚举、内存生命周期或 H 内部常数，但必须保留“补集域重新闭包”这一逻辑职责。
- 任一声称删除辅助层、跳过最高 H 闭包或复用 ordinary payload 的候选，必须先通过 `wrp4-16=1190`，再进入随机回归和 Orkut 性能门禁。
