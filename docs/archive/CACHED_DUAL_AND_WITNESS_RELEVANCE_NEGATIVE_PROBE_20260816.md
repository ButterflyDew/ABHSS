# [负结果] cached dual 区间复用与 witness 输入相关调度

日期：2026-08-16

基线提交：`849c89640404eed42553cedaa1b51e37a69c7ce7` 及其当前已接受严格等价减空

结论：两个候选均否决；生产源码和公共接口均已恢复，不得作为当前方法或性能证据引用。

## 1. cached dual 区间的二次判定

ordinary 的分阶段 certificate cache 会保存同一顶点已经计算的 directed-cut 下界。候选在该缓存命中后，用 `value + cached_lower < incumbent` 先判定当前标签；只有该式仍可能改进时才进入现有后续证书阶段。该判定只复用可采纳下界，不改变下界数值、阶段次序、状态定义或 strict incumbent 比较，因此逻辑上安全。

候选与对照分别固定在 CPU 4 和 CPU 5，同时运行 Orkut g=15 q10 900 秒。两边的上界、存活标量、D2 完整层和 witness 购买轨迹一致：

| 实现 | 已发布 ordinary row | 进度 |
|---|---:|---|
| cached dual 候选 | 190 | D2 全部 91 张，D3 99 张 |
| 对照 | 194 | D2 全部 91 张，D3 103 张 |

候选没有减少任何存活状态或后续证书调用集合，反而在热路径增加一次比较与分支。当前 ordinary 已有 monotone certificate frontier；对缓存值追加同式判断不构成新的严格支配关系。

本地证据目录：

- `results/probes/cacheddual_round1_candidate_cpu4`
- `results/probes/cacheddual_round1_control_cpu5`

## 2. 只在新 row 触及 witness support 时重新购买树 DP

候选在每张 ordinary row 发布后，检查新增顶点是否与当前 witness/support 顶点相交；只有相交才增加 witness 输入修订号。support 被替换时强制增加修订号。rent、buy 公式和 `EvaluateWitnessTree` 本身均不改变。

候选通过 5 项 CTest；SteinLib 的 Base 与 Enhanced 各自 11 个已知最优实例全部一致，其中 `wrp4-16` 仍为 1190。候选与对照的已知最优值序列均为：

~~~text
361, 237, 497, 250, 422, 208, 179, 798, 290, 405, 1190
~~~

Orkut g=15 q10 候选与对照分别固定在 CPU 4 和 CPU 5，同时运行 900 秒。两边均发布 193 张 ordinary row，即 D2 全部 91 张和 D3 的 102 张；两边都发生 42 次 witness 购买。逐次购买的 row 序列完全相同，没有一张可被候选跳过。

这说明 q10 的每次既定购买之前都已有 row 触及当前 support。候选只能增加集合相交检查，不能减少树 DP 次数。它也没有命中主要瓶颈：一次树 DP 约 0.4 秒，而 ordinary D 的 certificate 与图闭包工作占绝大多数时间。

本地证据目录：

- `results/probes/witnessrelevance_round1_candidate_cpu4`
- `results/probes/witnessrelevance_round1_control_cpu5`
- `results/probes/witnessrelevance_steinlib_candidate_20260816`
- `results/probes/witnessrelevance_steinlib_control_20260816`

## 3. 后续约束

- 不在 cached dual 命中路径重复加入 `value + cached_lower` 判定。
- 不给 witness scheduler 增加 support 相交门；q10 的购买序列没有可跳项。
- 不把这两个候选写入正式 `METHOD.md`、`CODE_GUIDE.md` 或实验矩阵。
- 后续减空应瞄准 ordinary D 的大量 certificate/cache 工作或严格等价的查找成本；不得改浮点精度，不得增加图名、查询编号或经验性组数开关，不得删去辅助 H。
- 新候选须先核对输出与状态；严格等价实现再做同核或交换绑核性能门，有状态变化时另行给出完整精确性证明与已知真值 gate。
