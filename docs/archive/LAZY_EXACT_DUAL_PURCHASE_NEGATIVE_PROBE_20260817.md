# Directed-cut 延迟精确购买负向探针（2026-08-17）

## 1. 候选与正确性边界

closure 转置后的 `CanImproveAllExcept` 先用全势和减去 excluded 势得到安全区间。旧状态机若以非精确区间下端拒绝当前标签，不结束 dual 阶段；较小标签再次穿过缓存下端时重新计算同一区间，必要时才回退到逐组精确和。

本候选不增加按顶点数组：第一次非精确下端拒绝只写入空闲的 stage 5；同 row 后续标签仍先复用缓存下端，首个真正穿过下端的更小标签直接调用同一个 `DualCutPotential::At`，把精确势写入原 `bound_cache` 并永久进入 stage 1。它不读取图名、组数阈值、时间、状态数或浮点近似；Base 不进入 staged directed-cut 状态机。

该变换保持精确性：缓存下端仍是可采纳下界，购买的 `At` 与旧区间歧义 fallback 使用相同原 `double` 求和；精确值只可能安全拒绝更多标签。它却不是严格的物理支配，因为第二次穿过 lower 时，旧 upper endpoint 偶尔可能直接放行，而候选会提前支付 exact。

production 与 probe 构建均通过 5/5 CTest。候选二进制 SHA-256 为 `8b3c526a0a7b7896cd770d6d53db3c9b1904e4343d9c150abf65cb9879522a24`，probe 二进制为 `08e7714dfec6aea9e302551e9a46a9f8308f24b8b2ed039336ade53cd4352711`；冻结对照分别为 `57d2afa0f7098cec9baff15fd8088695c006edf72adccdb1c5668f4ba2a4b505` 与 `37204d7f4c80d654a5ae58082ca907675ef001038467680d7458b416dd142095`。

## 2. Orkut g=15 q10 交换绑核结果

两轮均使用相同查询、Enhanced、Release/O2、普通稀疏 probe 和 900 秒求解预算；第二轮交换 CPU4/CPU5。

| 轮次 | 候选 | 对照 | row 进度比 | 候选 / 对照峰值 RSS |
|---|---:|---:|---:|---:|
| 候选 CPU4、对照 CPU5 | 209 | 212 | 0.985849 | 10061.398 / 10061.383 MiB |
| 候选 CPU5、对照 CPU4 | 210 | 209 | 1.004785 | 10044.129 / 10061.371 MiB |

交叉几何进度比为 0.995272，即候选净慢约 0.47%。两轮候选的完整 layer 2 均为 91 rows、73,563,652 scalars，与对照完全相同；候选 primitive work 为 11,572,129,606，对照为 11,572,111,577，多 18,029。事件从第 3 张 ordinary row 起出现这一稳定工作差异，但没有减少已完成层的状态数。

原始记录位于：

- `results/probes/lazy_exact_q10_round1_candidate_cpu4`
- `results/probes/lazy_exact_q10_round1_control_cpu5`
- `results/probes/lazy_exact_q10_round2_candidate_cpu5`
- `results/probes/lazy_exact_q10_round2_control_cpu4`

## 3. 结论

候选没有把 q10 的重复区间工作转化为净收益，提前 exact 的额外工作超过省下的区间重算；它也没有减少完整 layer 2 状态。源码、构建二进制和临时计数器均回退，不进入正式方法。后续不得把“发生第二次 lower crossing”本身当作购买 exact 的充分物理理由；若重试，必须先有更强的严格支配条件或新的状态削减证据。
