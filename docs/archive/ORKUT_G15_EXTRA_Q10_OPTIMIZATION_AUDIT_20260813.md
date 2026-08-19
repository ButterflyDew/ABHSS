# Orkut g=15 追加 q10 优化审计

> **状态：局部等价优化保留，旧 q10 整体门失效。** 本文记录的证书因子化、缓存与单调拒绝前沿仍是输入无关的安全工作消除；但最终长测二进制继承了缺失辅助 $H(h)$ 的精确性错误。文末 9,934.603 秒只作当前复跑风险估计，不能作为当前答案、状态或 10,000 秒门禁。修复见 [辅助半层 Adjoint 正确性审计](AUXILIARY_HALF_ADJOINT_CORRECTNESS_AUDIT_20260815.md)。

> **后续状态（2026-08-19）。** 修复后的冻结生产二进制已完成 Orkut g15 q1--q10，q10 为 9,544.561 秒、精确值 54、状态数 1,459,398,194；13 图 P1 逐图底线也全部通过。本文其余时间仍按原意保留为错误旧版或中间候选的历史数据，不能替换最终表。最终身份见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

## 1. 问题定位

P2 扩样后的 Orkut `cross_g15.txt` q6--q10 使用提交 `589894be3774ff6658e1120f9a9e899a53377ab1` 的 Enhanced、每查询 10,000 秒。q6、q7、q8、q9 均完成，q10 timeout。q10 的输入侧统计为 `mean_f=309.8`、`min_f=56`、`max_f=793`；它并不是追加五条中平均组最大的查询。

诊断版 q10 的共同预处理约 255 秒，初始可行上界为 62。共同 A1 约 113 秒、产生 39,056,188 个标量。Enhanced 在 D2 第 2 行买入 residual closure；购买后把已发布 ordinary 标量从 4,600,116 降到 1,169,801，并把上界收紧到 59。到 10,000 秒仍在处理大规模 ordinary 状态，最终结果、峰值和状态数不可用。对照 q6 虽也很难，但在 7,396.080 秒完成并产生 452,486,374 个状态。

PrunedDP++ 的短诊断不能解释为上界优势：q10 完成组距离预处理后，在 65K、262K、1.05M、4.19M 状态处的 incumbent 依次为 70、67、67、67，均弱于 Enhanced 的 59。q10 的核心矛盾是 ABHSS ordinary 状态域与现有 future 强度的组合，而不是 baseline 已经找到更好的可行树。

## 2. 第一阶段：拆分因子化与阶段证书缓存

对固定 ordinary row 和顶点，全部规范拆分共享同一 future。候选先精确聚合逐顶点最小 split，再只对最终 seed 求一次 future；size 2 只有一个规范拆分，仍走单遍路径。DirectedCut 配置还把 dual、farthest、公共 A1、tour 组织成阶段缓存：较大标签在中间阶段被拒绝后，较小标签可复用已算下界并继续尚未完成的阶段。Base 保留原有 flat realization，避免为不存在的 dual 早停支付阶段分支。

该机制不改变状态域。1200 秒同核诊断中，候选完成 79 张 ordinary row，冻结对照完成 58 张；对应 row 的 `best`、状态和 primitive work 逐项相同。候选累计消除了 3,327,089,563 次重复证书求值。q6 完整结果从 7,396.080169 秒降到 4,438.446981 秒，权值仍为 36，状态仍为 452,486,374，空间为 6,019.934 MiB。

但该阶段的 q10 正式 Release 门仍跑满 10,000 秒，结果文件只有 header。故它是有效的一般物理优化，却尚不足以单独解决新增 q10。

## 3. 第二阶段：完整 directed-cut fallback 复用

residual closure 后的转置查询先以缓存全势减去已覆盖组势，得到带标准浮点误差界的 certified interval。区间下端是可采纳值，但不足以代表完整证书；若 upper endpoint 已让当前标签严格通过，所有后续更小标签也会通过，因而可结束 dual 判定。只有区间不能判定而逐组求和时，或 closure 前本来就逐组求和时，返回值才是固定 `(mask,vertex)` 的完整 directed-cut 证书。新候选让 API 同时返回 `exact` 标志：当 exact 和拒绝当前标签时，该标志允许 ordinary staged cache 仍把 dual 阶段标为完成。后续更小标签复用逐位相同的 `double` 和，不压缩精度、不改求和顺序，也不缓存前一个标签的拒绝结论。

严格 1,200 秒总墙钟门包含约 39 秒图加载、254 秒预处理、112 秒 A1 和约 220 秒 residual closure。最终完成全部 91 张 size-2 row；第一阶段同口径完成 79 张，提升约 15.2%。全部对应 row 的 `best`、标量数和 primitive work 仍逐项相同。closure 前两张大 row 的缓存拒绝分别为 24,504,909 和 26,083,169 次，全部命中已经计算过的完整 dual 和；closure 后每 row 仍通常复用约 470 万至 980 万次 exact fallback。

正式 P1 门禁使用相同算法源码、无诊断宏。最终 Base 的 300 条 Youtube g7 答案和状态与冻结版逐条一致，时间从 1,890.144916 秒降到 1,873.186241 秒（-0.90%），状态均为 74,359,677；query-level 最大 RSS overhead 从 122.863 MiB 变为 128.035 MiB（+5.172 MiB，+4.2%）。条件分配 `bound_stage` 后，最终 Base 前 60 条为 382.221554 秒、122.008 MiB、16,793,059 状态；冻结版同前缀为 385.787714 秒、120.094 MiB、相同状态。源码已经少分配约一 byte/vertex 的 Base-only 无用阶段数组，故全量最大值差异来自 1 ms RSS 采样和分配器瞬时峰；审计仍如实保留该数字，不把它声称为空间改进。Enhanced 的 exact-cache 全量结果为 2,123.472070 秒、218.508 MiB、67,570,613 状态；加入 exact 标志前同一候选为 2,119.582756 秒、217.859 MiB、相同状态，答案和状态逐条零差异，时间差为 +0.18%，按同机波动视为等价门通过而不声称加速。

该阶段的 q10 正式门最终也跑满 10,000 秒，结果文件只有 header。随后加入 ordinary tour 上包络后，q10 在 CPU2 的外层 10,070 秒门内仍未产生结果行；其中图加载约 39.25 秒。因此截至这一阶段，q10 只是擦近边界，尚未解决。

## 4. 第三阶段：无深度的最远组 oracle 等价工作消除

旧实现已经为每个顶点保存唯一的全局最远组下标。构造该下标时，每次比较都重新通过 `GroupRow` 读取当前最远组；在完整势 realization 中，argmax miss 后的剩余组扫描也反复经过已经不再需要的布局分派；H 前缀则另写了一份 included-mask 扫描。当前候选不增加任何排名缓存：预处理在一次组扫描中用局部 `farthest_value` 维护唯一 argmax；完整势 miss 直接读取已知 dense 的底层值数组，bounded realization 仍走原 oracle；D、A、H 统一调用同一个 mask 最大距离函数。

这些操作不改变缓存深度、距离值、mask、比较顺序、状态域或浮点加法。唯一 argmax 数组仍是一 byte/vertex，空间与冻结版相同；规则不读取图名、组数、权值类型、状态量或运行时间。五项 CTest 全部通过。正式 Release 短门如下：

| 门禁 | 冻结版 | 无深度候选 | 结果 |
| --- | ---: | ---: | --- |
| Orkut g15 q3，CPU2 | 379.715 s | 376.944 s | 权值 38、状态 31,051,638 均相同 |
| Orkut g15 q4，CPU2 | 426.485 s | 412.695 s | 权值 28、状态 15,061,435 均相同 |
| Orkut g15 q8，CPU2 | 281.632 s | 279.067 s | 权值 18、状态 366,330 均相同 |
| Orkut g15 q9，CPU2 | 1,961.608 s | 2,145.202 s | 权值 35、状态 135,705,582 均相同；非同时运行，不能声称时间收益 |
| Youtube P1 Enhanced 前 60 条，CPU3 | 421.829 s | 419.024 s | 答案、状态逐条相同 |
| Youtube P1 Base 前 60 条，CPU3 | 382.991 s | 351.082 s | 答案、状态逐条相同 |

候选二进制 SHA-256 为 `ee1001526e8e82dae7cd51c1f7bfa4ea33652a9af886318cd70d0b3eb931ce2f`。该版 q10 严格门最终仍在外层 10,070 秒后退出，结果文件只有 run header，没有 `[Query 10]` 完成行。CPU2 与 CPU3 是同一型号 Xeon E5-2643 v4 的两个物理 socket 核，均为单线程绑核运行；结果必须记录绑核和并发邻居，不能把跨时段差异解释为算法收益。故无深度最远组 oracle 保留为一般等价工作消除，但没有单独解决 q10。

## 5. 本轮排除方向

所有候选均在隔离构建中测试；没有修改查询、浮点精度、整数权假设或按图/按组数超参数。

- 更深的五组种子路径上界：q10 上界仍为 59，搜索轨迹不变，只增加预处理；q6 也没有状态收益。
- residual 需求顺序替换：正序、稀有组优先、常见组优先、需求量分层、未支付量优先等均不能稳定强于正式的远到近初始增长加反序 completion；部分路线直接降低固定窗口吞吐。
- tour 数据布局、half-perimeter 松弛和延迟求值：没有减少 q10 的关键状态，额外分支或内存访问使固定窗口进度持平或下降。
- indexed heap 与显式复用普通二叉堆：indexed heap 在相同工作下更慢；复用堆已被既有 `REUSABLE_ORDINARY_HEAP_NEGATIVE_PROBE_20260810.md` 否决，本轮不重复保留实现。
- ordinary 重复 settle：在严格 1,200 秒 q10 门中，全部已完成 row 的重复 settle 均为 0。一致 A* 已自然保证首次有效弹出定型；增加 settled 检查不会减少邻接展开，诊断代码已删除。
- 固定组端点树遍历下界：已知被 directed-cut potential 完全支配，见 `ENDPOINT_PAIR_TREE_BOUND_DOMINATED_NOTE_20260812.md`。
- 一次性互补 directed-cut 证书：正确但购买过晚、成本过高，详见 `EPHEMERAL_COMPLEMENTARY_DUAL_NEGATIVE_PROBE_20260813.md`。
- 固定两级最远组缓存：分离的一字节第二名数组在 Orkut q4 上约快 6.4%，Youtube P1 Base/Enhanced 前缀也不退化；但它仍是固定深度 top-k。既有 `PURCHASED_FARTHEST_ORDER_NEGATIVE_PROBE_20260811.md` 已明确禁止用缺乏理论边界的固定 top-k 深度替代。q5/q6 分别运行约 15/29 分钟后主动停止，二者都没有结果行；源码随后删除第二名数组。该性能信号不能凌驾于预声明的论文约束。
- 全局 dual-first、仅 residual 重滤 dual-first 以及全局内联 tour 上包络：答案与状态不变，但 Youtube P1 前缀分别出现约 0.5%--2.6% 退化，均已回退；不再用 Orkut 单点波动覆盖 P1 门。

## 6. 当前决策边界

拆分因子化、阶段证书缓存、完整 dual fallback 复用与无深度的最远组 oracle 快路径均是输入无关的等价工作消除：它们不改变上界、下界、状态定义、浮点语义或渐近复杂度。前三者适合统一表述为 candidate-independent certificate factorization；后者是同一组距离 oracle 的布局专门化与唯一 argmax 线性构造。Base 继续使用同职责的 flat realization；Enhanced 只因新增 directed-cut 证书而采用 staged realization，没有产生 Base 独占逻辑操作。

当时的最终接入以三项证据为硬门：q10 的 `query_seconds` 严格小于 10,000；Orkut g15 q1--q10 的最终二进制结果全部完成；P1 Base/Enhanced 不退化。这三项后续已由文首冻结生产版通过。禁止项仍有效：不得删除 q10、延长该条专属 TL、按 `f` 或图名加开关，也不得把本节的阶段吞吐冒充完成结果。

后续把 staged cache 的 row epoch、stage 与 exact 标志合并为单一 32-bit 元数据字，属于同一证书 realization 的物理局部性优化；证明、空间账、两轮交换 CPU 的固定窗口结果及当前严格门见 `PACKED_CERTIFICATE_ROW_STATE_PROBE_20260814.md`。该严格门必须使用追加面板 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt`，不能误用图目录下含 300 条询问的 `query_g15.txt`。


## 7. 历史最终阶段：单调证书拒绝前沿

packed state、row-local 实际值前沿、all-stage 解析 cutoff 与 stage-0-on-miss 都没有同时满足严格 q10 和 P1 门。最终候选保留 row-local reuse admission，只在首次物化且证书仍处于 stage 0 时，把偶然的拒绝标签替换为由当前真实上界和已缓存可采纳下界解析出的拒绝 cutoff。实现再用原 `double` 加法与严格比较把 cutoff 向上修正到确实拒绝的首个可表示值。由非负浮点加法单调性，所有不小于该值的标签都必被原 `CanImprove` 拒绝；虚拟值不入堆、不计状态，较小 crossing 标签仍走完整证书链。该操作不使用图名、`g`、`f`、时间、内存、整数权或精度压缩。完整证明、失败路线和原始门禁见 `MONOTONE_CERTIFICATE_FRONTIER_PROBE_20260815.md`。

历史候选 Release 二进制 SHA-256 为 `a957bdcecafc486575cb7d78779dcbeda687eb0ea7283a0fc1104089dce83b86`。Youtube P1 Enhanced 全 300 条为 2,107.963 秒、218.477 MiB RSS overhead、67,570,613 状态；Base 全 300 条为 1,859.266 秒、122.453 MiB、74,359,677 状态。这些运行仍可隔离局部优化的成本方向，但 Enhanced 权值与聚合不能进入当前正式表。

旧 Orkut g15 q10 写出结果行：9,934.603 秒、返回值 54、19,776.727 MiB RSS overhead、1,654,690,262 状态；旧 q1--q10 也都产生结果。由于同一二进制不精确，当前只能继承“q10 极接近 TL”这一风险信号。当前辅助半层二进制必须重新完成十条，且旧状态数不设为相等门。
