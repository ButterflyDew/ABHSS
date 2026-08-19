# A1 top-two 自适应物化门禁（2026-08-16）

> **后续状态（2026-08-19）。** 本文是中间 checkpoint 的局部证据；当时尚未完成的生产版 Orkut q1--q10 与 13 图 P1 后续均已通过。本文的 900 秒进度仍不能冒充最终时间，最终结果见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

## 1. 状态与边界

本记录审计共同 A1 future 的物理查找方式。候选在该时点可以进入本地 checkpoint，但不构成最终性能结论：当时完整 P1 尚未在该生产二进制上复跑，Orkut g15 q10 也尚未在 10,000 秒内完成。不得把本文的 900 秒进度领先改写成“q10 已过线”；后续最终结论只使用文首生产版记录。

候选只改变 top-two 精确视图的构造顺序：

- 少量新顶点仍逐 singleton row 二分，并把最大/次大 bit 与原 `double` payload 下标写入已有缓存。
- 已支付的首次查询工作达到由 row 形状计算的购买点后，按顶点和 bit 递增顺序扫描全部 row，在同一缓存中填满完全相同的 bit、locator 与 fallback 标志。
- Base、DirectedCutOnly 与 Enhanced 调用同一个 `InitializeLookupPlan`、`Future` 和 `MaterializeAllTopTwo`；代码不读取增强位、图名、查询编号、计时或经验组数阈值。
- 没有压缩浮点精度，没有新增近似值，没有删状态，也没有增加永久 `double[n]` 表。

正式定义、购买式和内存边界见 `../METHOD.md` 第 9.4、9.5 与 14.1 节；代码入口见 `../CODE_GUIDE.md`。

## 2. 为什么数值与搜索轨迹不变

对每个固定顶点，两条路径都按相同的 singleton bit 升序读取同一值。row 命中时复制原 `row.value` 与 32-bit 下标；缺项时调用同一个 `FallbackValue`。最大和次大只用严格 `>` 更新，所以并列选择也一致。

购买只改变“逐顶点二分”与“按 row 游标顺扫”两种枚举次序。它不改变 A1 row、future 最大值、ordinary 接纳谓词、堆节点、状态计数或 incumbent。新增生产回归 `CheckAnchoredSingletonMaterializationEquivalence` 构造 64 点、4 个 dense singleton row；其结构购买点小于顶点数，因而必然进入顺序物化。测试对全部 64 个顶点和 15 个非空 remaining mask 与独立逐 bit 最大值逐项比较，并断言购买路径确实发生。

## 3. 被拒绝的中间实现

以下实现均未保留：

- 无条件 eager 物化或额外保存两张 dense `double` 表：小查询必付全图工作，且放大 RSS。
- 在每个 singleton bit 内维护细粒度 rent：它把共同 top-two 比较和二分差分混在一起，Musae g7 Base 出现约 1.7% 回归。
- 在同一次首次查询中重复判断或重复记 rent：属于纯冗余热分支。
- 以运行秒数、图名、查询编号或固定 g 阈值决定购买：不能形成论文可解释的统一规则。

当前版本只在一个顶点第一次构造 top-two 后递减一次剩余购买计数；fallback 成本不计入既付 rent，避免因可选成本的乐观估计而过早购买。

## 4. 正确性门

| 门禁 | 结果 |
|---|---|
| 仓库 CTest | 5/5 通过 |
| 独立随机真值 | 5000 个 g=2..10 随机实例，Base、DirectedCutOnly、Enhanced 全部匹配独立 subset DP |
| 正权唯一终端压力 | 500 个 g=6..10 Enhanced 实例全部匹配 |
| 辅助半格压力 | 160 个 g=7..16 Enhanced 实例全部匹配 |
| A1 购买分支直接覆盖 | 64 点乘 15 个 remaining mask 全部逐项一致，且 `lookup_materialized=true` |
| 零权边与状态口径 | `abhss_zero_weight_regression`、`mask_vertex_state_regression` 通过 |
| SteinLib 已知真值 | Base 与 Enhanced 各 11/11；值序列均为 361、237、497、250、422、208、179、798、290、405、1190 |
| GitHub Markdown | `validate_markdown.py` 通过全部 87 个 Markdown 文件 |

生产 SteinLib 证据目录：

- `results/probes/a1material_production_v2_steinlib_base_20260816`
- `results/probes/a1material_production_v2_steinlib_enhanced_20260816`

两项 SteinLib gate 为并行正确性核验，其运行时间不能作为论文性能数。

## 5. 非退化探针

### 5.1 Musae g7 Base 全 300 条

| 启动顺序 | 候选总时间 / s | 对照总时间 / s | 候选 / 对照 | 两边状态数 | 候选 / 对照峰值 MiB |
|---|---:|---:|---:|---:|---:|
| 候选先 | 46.684363 | 46.712625 | 0.9994 | 6,872,062 | 4.551 / 4.531 |
| 对照先 | 46.479372 | 46.694294 | 0.9954 | 6,872,062 | 4.535 / 4.500 |

300 条权值逐项一致；该图未达到顺序物化购买点，因此结果主要证明最终 countdown 热路径没有重现早期细粒度记账回归。

### 5.2 Orkut g15 q3 完成对

| 版本 | 时间 / s | 最优值 | 状态数 | 峰值 MiB |
|---|---:|---:|---:|---:|
| 候选，CPU 4 | 292.941140 | 38 | 31,191,511 | 2690.480 |
| 对照，CPU 5 | 293.053563 | 38 | 31,191,511 | 2689.539 |

该查询未购买顺序物化；时间差为 -0.04%，状态与权值完全相同。

### 5.3 Orkut g15 q10 的 900 秒同跑

最终 countdown 候选固定 CPU 5，对照固定 CPU 4。候选在 0.676516 秒内顺序物化 14 张 A1 row、39,056,188 个标量，并在超时前完成 208 张 ordinary row；对照完成 206 张。两边已完成对应 row 的 `best`、状态标量和 row work 一致。候选终点 watchdog RSS 为 10061.40 MiB，对照为 10005.63 MiB，但候选多推进两张大 row，不能把两个不同进度的终点 RSS 解释为固定缓存开销。

这只说明候选方向没有在 q10 前 900 秒退化并带来约两张 row 的进度收益。它没有证明总耗时小于 10000 秒；当前有效完整版本的 q10 记录仍是 10000 秒超时。

证据目录：

- `results/probes/a1material_round2_candidate_cpu5`
- `results/probes/a1material_round2_control_cpu4`
- `results/probes/a1material_p1_musae_g7_countdown_cpu4`
- `results/probes/a1material_p1_musae_g7_countdown_control_first_cpu4`
- `results/probes/a1material_q3_candidate_cpu4`
- `results/probes/a1material_q3_control_cpu5`

## 6. 结论

候选满足“共同操作、精确值不变、无经验超参数、论文可解释”的 checkpoint 条件，并消除了大图 dense A1 上反复随机二分的一部分物理工作。它不是解决 Orkut 状态爆炸的算法性剪枝，单独收益不足以宣告最终目标完成。下一阶段仍需在生产二进制上完成 q10 的谨慎长门，并继续审查 ordinary/adjoint 中可被严格支配而减成空的操作；最终接受条件仍是完整 P1 不劣且 Orkut g15 q1--q10 每条小于 10000 秒。
