# Queue pop 与 forward 空行发布负向探针

## 1. 审计对象

本轮只考察由当前控制流严格蕴含、但未必能改善机器执行的两类操作：

1. ordinary $D$ 与 adjoint $H$ 队列出堆时的 `node.key < best` 复核；
2. 普通 forward $A$ 从未产生标签时，是否仍显式写入 `row.ready=true`。

它们都不增加下界、上界、状态或数据相关分派。实验候选通过 5/5 CTest，抽样权值与状态数均和对照一致；这里只判断这些逻辑等价改写能否进入论文二进制。

## 2. 出堆 cutoff 的逻辑边界

在一张 ordinary $D$ row 的 Dijkstra 闭包中，`best` 不会在队列循环内改变；在一张 adjoint $H$ row 中，完整边界上界也只在该 row 闭包结束后结算。因此二者队列建立后，已经满足 `key < best` 的有效节点在本 row 内仍满足该式。删除出堆时的第二次 cutoff 复核在逻辑上等价。

这个结论不能推广到 forward $A$ 或提前 A1。普通 forward $A$ 在每次有效出堆后尝试 root-star，可在同一队列循环内降低 `best`；提前 A1 还可能在累计 rent 达到共同 buy 公式时调用树 DP 并降低 `best`。这两处出堆 cutoff 是动态 incumbent 的有效消费者，不属于被支配操作。

## 3. forward 空行的真实生命周期

普通 forward $A$ 按 size 递增处理 mask。若某个 mask 的所有 seed 都为空，`touched.empty()` 说明它没有任何有限 payload；后续前向 mask 只会把它作为真子集读取，adjoint 边界也只调用 `RowValue`。空 payload 对每个顶点都返回无穷，所以严格层序已经足以证明“该 mask 已处理”，无需用 `ready` 再向这些直接读取者发布一次。

这不是全局 `Row` 合同。ordinary、提前 A1 owner 交接与 $H$ 的外部消费者会显式查询 `ready`，这些 row 即使为空也必须发布。为普通 forward 空行补写 ready 虽然数值等价，却增加热写入；在每层末尾扫描整个 mask 域批量补写，还额外增加 $O(2^k)$ 生命周期遍历。

## 4. 交换绑核结果

Musae 使用 Base、 $g=7$ 全 300 条；Orkut 使用 Enhanced、 $g=15$ q3。候选与 checkpoint 交叉交换 CPU 4/5 和启动顺序。表中比例按两轮均值计算；候选组合名称严格反映同轮包含的改动，不能把耦合结果错误归因给其中单项。

| 候选 | 数据 | 候选均值 / 秒 | 对照均值 / 秒 | 变化 |
|---|---|---:|---:|---:|
| 删除 D/H pop cutoff，并逐空行写 ready | Musae 全 300 | 46.839743 | 46.524746 | +0.68% |
| 仅删除 H pop cutoff，并逐空行写 ready | Musae 全 300 | 46.287562 | 46.421079 | -0.29% |
| 仅删除 H pop cutoff，并逐空行写 ready | Orkut q3 | 291.903568 | 289.955703 | +0.67% |
| 仅逐空行写 ready | Musae 全 300 | 46.781526 | 46.090182 | +1.50% |
| 每层末尾批量补齐空行 ready | Musae 全 300 | 46.701163 | 46.052365 | +1.41% |

Orkut q3 两边权值均为 38，状态数均为 31,191,511；Musae 各轮权值序列与总状态数 6,872,062 也一致。数值与状态一致只能证明等价，不能覆盖稳定的时间回归。

## 5. 结论与禁止重试边界

当前生产源码完整恢复以下实现：

- $D$、 $H$ 保留短的 pop cutoff 连接操作；论文不把它包装成算法增强；
- forward $A$ 在 `touched.empty()` 时直接进入下一个 mask，不写 ready，也不做层末全域扫描；
- forward $A$ 与提前 A1 中会响应动态 incumbent 的 pop cutoff 始终保留。

以后只有在翻译单元布局或编译器发生实质变化、并重新通过完整 P1 与 Orkut 大组门禁时，才应重试上述物理改写。不能因为它们在源码层“少一个比较”或“生命周期更整齐”就重新合入。
