# 方法结构、正确性事故与修复历史

本卷保存 ABHSS 配置关系、D/A/H 结构演进和正确性修复的完整历史。它用于追溯设计决策，不是当前方法规范；当前实现与证明只以 [`../METHOD.md`](../METHOD.md) 和 [`../CODE_GUIDE.md`](../CODE_GUIDE.md) 为准。旧记录中的勘误、失效结论和历史二进制边界均原样保留。

## 卷内目录

- [[归档] ABHSS 单入口与增强开关重构审计](#history-abhss-configuration-refactor) — 原文件 `ABHSS_CONFIGURATION_REFACTOR.md`
- [H 阶段调度与提前边界求值探针（2026-08-07）](#history-h-scheduling-probe-20260807) — 原文件 `H_SCHEDULING_PROBE_20260807.md`
- [结构性省略半格的 Orkut `g=15` 门禁（2026-08-12）](#history-structural-omitted-half-orkut-g15-gate-20260812) — 原文件 `STRUCTURAL_OMITTED_HALF_ORKUT_G15_GATE_20260812.md`
- [辅助半层 Adjoint 正确性事故与修复审计（2026-08-15）](#history-auxiliary-half-adjoint-correctness-audit-20260815) — 原文件 `AUXILIARY_HALF_ADJOINT_CORRECTNESS_AUDIT_20260815.md`
- [Adjoint split 完备性事故与修复审计（2026-08-17）](#history-adjoint-split-completeness-audit-20260817) — 原文件 `ADJOINT_SPLIT_COMPLETENESS_AUDIT_20260817.md`
- [最小共同 A1 前缀与 Orkut q10 门禁](#history-minimal-forward-a1-adjoint-gate-20260818) — 原文件 `MINIMAL_FORWARD_A1_ADJOINT_GATE_20260818.md`

<a id="history-abhss-configuration-refactor"></a>

## [归档] ABHSS 单入口与增强开关重构审计

> 原始记录：`ABHSS_CONFIGURATION_REFACTOR.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 1. 重构目标

本次重构直接回应论文叙述风险：旧实现把 Light/Heavy 暴露为两个公开函数、两个源文件入口、两个二进制和两个“method”记录，容易让审稿人理解为两套算法后再按数据选择。新实现要求：

1. 公开 API 只有一个 `SolveOneQuery`。
2. 正式构建只有一个 `abhss` 可执行文件。
3. Base 不开增强；在 Base 上依次开启 `DirectedCut` 和 `AdjointCompletion` 得到全增强配置。
4. 从全增强配置反向关掉相同开关，必须得到中间配置和 Base。
5. 配置在查询开始前冻结，运行中禁止自动切换。
6. 重构前后的精确答案逐项相同，代表性小图、参数增长和大图面板不得出现可归因于代码结构的性能退化。

### 2. 旧结构问题与新边界

| 维度 | 旧结构 | 新结构 | 审稿风险变化 |
|---|---|---|---|
| 公开 API | `SolveLightOneQuery`、`SolveHeavyOneQuery`、`SolveHeavyForwardOneQuery` | `SolveOneQuery(..., SolveOptions)` | 不再像三个 solver |
| 正式二进制 | `abhss_light`、`abhss_heavy` | `abhss` | 两条曲线可核验为同一 artifact |
| 内部模式 | `SolverVariant::{Light,Heavy}` 二选一 | 增强位集合及显式依赖 | 可从 Base 单调加开关 |
| 顶层编排 | `light.cpp`、`heavy.cpp` 各有入口 | `solver.cpp` 唯一编排 | 不会悄悄漂移阶段顺序 |
| 高层增强 | 与 Heavy 入口绑定 | `adjoint.cpp` 仅提供一个可选操作 | 文件表达的是操作而非方法 |
| 实验矩阵 | 两个不同 executable | 同一路径、不同冻结参数 | 环境 gate 可机器检查 |
| 消融 | 第三个专用函数 | `--enhancements=directed-cut` | 消融是关开关，不复制递推 |

### 3. 配置链的代码证据

`SolveOptions` 提供三个工厂：

```cpp
SolveOptions::Base();
SolveOptions::DirectedCutOnly();
SolveOptions::Enhanced();
```

并提供不可变式修改：

```cpp
auto enhanced = SolveOptions::Enhanced();
auto directed_only = enhanced.With(Enhancement::AdjointCompletion, false);
auto base = directed_only.With(Enhancement::DirectedCut, false);
```

`abhss_configuration_regression` 会断言这两个结果分别等于中间工厂和 Base 工厂。`IsValid` 还拒绝未知位以及“开 adjoint、关 directed-cut”的非法组合。

### 4. 唯一调度器

`solver.cpp` 中只有一次前置检查、一次 `Problem` 构造、一次公共 prepare 和一次 ordinary 构造。之后只有完成策略的开关：

- 未开 adjoint：调用完整前向 `A`。
- 开 adjoint：调用同一个前向内核的低层计划，再追加高层 `H`。

开启 directed-cut 时，`PrepareProblem` 选择完整距离、dual/primal 证书；关闭时选择 bounded 距离、根路径 witness 和 early-A1。所有能力通过 `Problem::UsesDirectedCut()`、`UsesAdjointCompletion()` 和 `UsesBoundedGroupDistances()` 读取，代码中不再存在 `SolverVariant`。

### 5. 命令行与结果元数据

稳定预设：

```powershell
abhss ... --enhancements=none
abhss ... --enhancements=directed-cut
abhss ... --enhancements=all
```

消融也可显式覆盖：

```powershell
abhss ... --enhancements=all --adjoint-completion=off
abhss ... --enhancements=all --adjoint-completion=off --directed-cut=off
```

每个结果批次 header 写入：`algorithm_family=ABHSS`、`configuration`、`enhancement_mask`、`directed_cut` 和 `adjoint_completion`。算法计时仍只包围单条查询的 `SolveOneQuery` 调用；参数解析、图加载、查询加载、结果目录和 header 均在计时外。

### 6. 实验矩阵约束

`paper_matrix.json` 使用 schema 4：

- `abhss_base`：`build/Release/abhss.exe --enhancements=none`。
- `abhss_enhanced`：同一个 `build/Release/abhss.exe --enhancements=all`。

环境验证器要求两项 `algorithm_family` 都为 ABHSS、可执行路径完全相同、参数和增强列表精确匹配。任意人把两项改回不同二进制，正式运行前都会失败。

### 7. 正确性门

重构后必须通过：

1. 零权父指针历史反例：Base、DirectedCutOnly、Enhanced 都返回 1。
2. 120 个确定性随机连通小图：三种配置逐例匹配独立全子集 DP。
3. SteinLib WRP 11 例：Base 与全增强配置逐例匹配已知最优值，并与重构前冻结二进制逐项一致。
4. 公开配置依赖：非法 adjoint-only 在求解前报错。
5. 命令行配置依赖：同一非法组合返回非零退出码，不能静默修正。
6. 快速图读取结构夹具：原边顺序、`edge_id`、双向邻接顺序、零/小数/科学计数边权全部匹配真值。

### 8. 非退化验证设计

非退化门比较同一工作区、同一机器、同一 MinGW 15.2.0 Release/O2、同一查询顺序下的“重构前冻结二进制”和“重构后单一二进制”。程序内部 query time 排除图/查询加载；端到端加载只检查 artifact 可运行，不混入算法速度比。

预声明面板：

| 层次 | 图与参数 | 查询 | 用途 |
|---|---|---:|---|
| 已知真值 | 11 个 SteinLib WRP，`g=11..16` | 11 | 同时检查高 `g` 精确性和状态层性能 |
| 小图跨 `g` | Musae，`g=6,10,14` | 每档冻结 P2 panel 的 5 条 | 检查开关重构是否随组数放大开销 |
| 大图 1 | Orkut，`g=6` | 冻结 P2 panel 第 1 条 | 检查超大图预处理/内存路径 |
| 大图 2 | Reddit，`g=10` | 冻结 P2 panel前 2 条 | 检查较高 `g` 的大图状态路径 |

查询在查看重构后结果前已经固定，不能按速度重新挑选。Orkut 只取一条是因为单次图加载与基础预处理已经很重；它是工程 gate，不冒充统计实验。论文正式性能结论仍必须来自完整实验矩阵。

重构前已冻结的首轮参考：

| 面板 | Base 总 query time | 全增强总 query time | 权重 |
|---|---:|---:|---|
| WRP 11 例 | 15.663659 s | 4.597346 s | 两配置逐项相同，依次为 361、237、497、250、422、208、179、798、290、405、1190 |
| Musae `g=8` 前 5 条（补充历史面板，三遍范围） | 2.212065–2.284086 s | 1.139958–1.183412 s | 两配置逐项为 296、392、297、231、393 |

接受标准不是要求每次受系统噪声影响的单条都更快，而是：目标值全部一致；无新增超时/不可行；每个预声明面板总 query time 不出现超过 3% 且可重复的退化；若单次超出 3%，必须交替复跑至少三次并报告中位数。

最终同机、同编译器、同查询顺序的结果如下。`ratio` 小于 1 表示重构后更快；所有目标值逐条相同，所有面板均一次通过，因此没有触发“交替复跑三次”的规则。

| 面板 | 查询数 | 配置 | 重构前总 query time | 重构后总 query time | ratio | 3% gate |
|---|---:|---|---:|---:|---:|---|
| SteinLib WRP `g=11..16` | 11 | Base | 15.663659 s | 15.346104 s | 0.9797 | PASS |
| SteinLib WRP `g=11..16` | 11 | Enhanced | 4.597346 s | 3.761492 s | 0.8182 | PASS |
| Musae `g=6` | 5 | Base | 0.340344 s | 0.333497 s | 0.9799 | PASS |
| Musae `g=6` | 5 | Enhanced | 0.624449 s | 0.619780 s | 0.9925 | PASS |
| Musae `g=10` | 5 | Base | 14.650781 s | 14.810915 s | 1.0109 | PASS |
| Musae `g=10` | 5 | Enhanced | 2.555014 s | 2.445644 s | 0.9572 | PASS |
| Musae `g=14` | 5 | Base | 1080.617007 s | 1087.887453 s | 1.0067 | PASS |
| Musae `g=14` | 5 | Enhanced | 289.519924 s | 292.948647 s | 1.0118 | PASS |
| Orkut `g=6` | 1 | Base | 24.446206 s | 23.535858 s | 0.9628 | PASS |
| Orkut `g=6` | 1 | Enhanced | 90.243451 s | 87.818050 s | 0.9731 | PASS |
| Reddit `g=10` | 2 | Base | 3.168216 s | 3.100777 s | 0.9787 | PASS |
| Reddit `g=10` | 2 | Enhanced | 45.452257 s | 40.184602 s | 0.8841 | PASS |

最慢的 Musae `g=14` 面板仍只有 +1.18%（Enhanced），小于预声明门限的一半；两张大图的两种配置均更快。完整数字、权重序列、二进制身份和查询来源固化在 [`experiments/abhss_configuration_refactor_gate.json`](../../experiments/abhss_configuration_refactor_gate.json)，而不是依赖本段手工摘要。

#### 8.1 大图快速读入是独立工程门

Orkut 的作者图为 3,072,441 点、117,185,083 条无向边，文本文件约 2.12 GB。旧 `operator>>` 路径逐字段执行格式化流检查，并在逐边插入邻接表时让各顶点反复扩容；其加载墙钟远大于单条查询的求解时间。新 `LoadGraphFromFolder` 因而做两项不改变图语义的优化：

1. 以 8 MiB 大块缓冲直接扫描 ASCII 整数；浮点 token 用无分配的 `from_chars` 转换，保持标准十进制舍入语义。
2. 第一阶段保留原边并统计每个顶点的精确度数，第二阶段一次性 `reserve` 邻接容量，再按原边顺序构造双向邻接表。

边序、`edge_id`、每个顶点的邻接插入顺序、边权和无向双边表示均保持不变。程序在结果 header 与 `[Ready]` marker 中分别写入 `graph_load_seconds` 和 `query_load_seconds`；这两个字段只审计工程等待时间，绝不并入逐查询 solver timer，也不作为论文算法 speedup。验证要求是：小图逐边结构与旧读入等价；Orkut 可完整加载；新加载墙钟显著下降；Base/增强配置的答案和 query time 非退化门仍独立通过。

实测结果：Orkut 的旧格式化读取从进程启动到 `[Ready]` 约 565–566 秒，新读取的内部计时为 25.996–26.880 秒，即约 21 倍；加载后 RSS 从约 8561 MiB 降到约 6463 MiB。最后一次注释/测试整理并重新构建后，又用最终二进制复跑 Orkut：Base/Enhanced 加载为 28.079/30.868 秒，query time 为 24.446510/90.984837 秒，相对旧版为 1.0000/1.0082，权重仍均为 11。Reddit 旧端到端墙钟扣除 query time 后的加载约 63 秒，新读取为 3.892–3.902 秒，即约 16 倍。两图随后都以相同权重通过独立的 query-time 非退化门，所以该收益没有混淆为算法 speedup。

### 9. 中文函数注释审计

注释范围是本次方法实现与入口：`src/abhss/*.h`、`src/abhss/*.cpp`、`src/main.cpp` 和两个 ABHSS 回归测试。每个命名函数/成员/模板至少在声明或局部定义处说明：

- 它计算什么；
- 输入输出或所有权；
- 关键 exactness 不变量；
- 与增强开关的关系；
- 热路径中重要的常数选择或生命周期。

局部 lambda 不机械逐行注释；其用途由所属函数段落和必要的行内不变量说明。baseline 与无关通用 I/O 不属于本次算法重构，不为了凑注释数量做无关改写；本次明确修改的快速图读取器则逐函数说明缓冲、解析、错误与所有权语义。

### 10. 论文写法

推荐：

> ABHSS exposes a base configuration and two composable accelerators. Enabling the directed-cut accelerator and then the adjoint-completion accelerator yields the fully enhanced configuration; disabling them in reverse order recovers the base configuration. Both configurations invoke the same executable and share the ordinary and anchored DP kernels.

禁止：

- 把 Base 和全增强配置称为两个独立提出的方法；
- 写成运行时自适应选择；
- 报逐查询二者最小值；
- 声称全增强配置逐条 CPU 指令包含 Base；
- 用本重构 gate 的少量计时代替正式实验矩阵。

<a id="history-h-scheduling-probe-20260807"></a>

## H 阶段调度与提前边界求值探针（2026-08-07）

> 原始记录：`H_SCHEDULING_PROBE_20260807.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

### 目的

本轮只考察不改变精确性的 H 阶段实现调整，目标是降低 Orkut 在大组数下的状态数量。候选均不依赖图名、不引入组数阈值，也不使用整数权等数据特化假设。

### 候选

1. **层内按 `terminal_value` 调度**：同一 H 层内优先处理较小的终端值，掩码相同时仍保持确定性顺序。
2. **种子行提前边界求值**：H 种子组装完成后、图闭包之前，复用现有精确边界完成逻辑尝试提前收紧 incumbent；闭包后仍执行原有边界求值。

两项候选均通过确定性随机精确性 gate：在 `g=2..10` 的 144 个实例上与精确子集 DP 一致。

### 已完成结果

Orkut，`g=16`，第 4 条查询：

| 版本 | 时间（s） | 峰值空间（MiB） | `(mask,v)` 状态数 | 最优值 |
|---|---:|---:|---:|---:|
| 当前正式 Enhanced | 1194.775808 | 2722.359 | 26,517,767 | 20 |
| 层内按 `terminal_value` 调度 | 1181.357557 | 2722.555 | 26,517,767 | 20 |
| 种子行提前边界求值 | 1165.662636 | 2722.555 | 26,517,767 | 20 |

两项候选都没有减少状态数；小幅时间差不足以构成稳定的算法改进证据。

### 中止的扩大验证

两个候选随后分别在独立核上运行 Orkut `g=15` 第 1 条查询。按用户要求，本轮在约 6 分钟处终止并清理；两者当时均只完成图加载，未输出完整查询记录，因此不用于任何性能结论。正式版本的参考结果为 10241.959915 s、3758.242 MiB、253,636,180 个状态、最优值 21。

### 结论与处置

这两个方向目前不能证明能够缓解大组数状态爆炸，不合入正式实现。临时候选源码、构建目录、输出目录和日志均已删除，只保留本归档文档，避免失败方向污染主代码与正式实验结果。

<a id="history-structural-omitted-half-orkut-g15-gate-20260812"></a>

## 结构性省略半格的 Orkut `g=15` 门禁（2026-08-12）

> 原始记录：`STRUCTURAL_OMITTED_HALF_ORKUT_G15_GATE_20260812.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> **状态：被辅助半层修复取代。** 本文正确否决了继续省略 $D(q)$ 的两层方案，也正确指出完整 $D(q)$ 不能由少量目标标量替代；但当时接受的一层方案仍从逻辑层 $q$ 直接启动 H，没有执行被省略 $D(h)$ 的图闭包。其五条 Orkut 时间仅作历史性能证据，不能作为当前精确门。当前方案以辅助 $H(h)\equiv D([k]\setminus S)$ 为基例，见 [辅助半层 Adjoint 正确性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-auxiliary-half-adjoint-correctness-audit-20260815)。

### 1. 目标与结论边界

本门禁曾用于核验 Enhanced 的高层 adjoint realization 能否省略 ordinary 半格的最后一层，并让 Orkut `g=15` 正式 panel 的五条查询都在逐查询 10,000 秒内完成。当时五条全部产生结果，最慢 q5 为 8,668.464606 秒；后来发现该二进制不精确，所以不得再把这些结果称为精确门禁。

这些运行有意让 CPU10/CPU11 各承载一个单线程任务，以提供偏保守的 TL 压力证据。不同查询的并发邻居不同，因此时间不能用来宣称相对旧版加速；权值、状态数和峰值仍可用于精确性与资源规模审计。

### 2. 层边界来自递推消费者

令锚组之外的组数为 `k=g-1`，完整平衡半格为 `h=floor(g/2)`，最高逻辑锚定层为 `q=h-1`，Enhanced 的公共前向前缀末层为 `ell`。H 的 successor、非空低层 A 边界和空前缀边界共同要求 ordinary 至少物化到：

```math
r=
\max\left\{
q,
\left\lceil\frac{k-(\ell+1)}{2}\right\rceil
\right\}.
```

对 Orkut `g=15`，有 `k=14`、`h=7`、`q=6`、`ell=3`，因此外侧最大组数为 10，`r=6`。当时的一层方案完整保留所有 `|S| <= 6` 的 ordinary row，只省略 `D(7)`；它后来因没有等价恢复 `D(7)` 闭包而被取代。省略 `D(6)` 的两层候选反例仍然有效：即使加入目标驱动结算和直接三块 terminal，答案仍从精确值 32 变成 33。原因是 `D(6)` 不是单个 terminal 标量，而是多个递推消费者共享、经过图闭包并带规范 branch 的完整 row。

separator 组件最多含 `r` 个组。两个容量为 `r` 的块首次可能无法装下的最小组件总量为：

```math
\tau(r)=
\left\lfloor\frac{3r}{2}\right\rfloor+2.
```

这里 `tau(6)=11`，而外侧最多只有 10 组，所以当时方案认为单块/双块 terminal 已完备。当时生产代码不会为本 panel 分配 `pair_best`、构造组合数表、扫描最大 entry 大小或在 pair 热循环调用空三块操作。当前源码已经删除整套装箱结构，改由辅助半层的精确 DP 转置承担证明责任。

### 3. 当时的正确性门及其覆盖缺口

当时实现通过了以下证据，但这些测试没有包含“必须先恢复被省略 $D(h)$ 闭包”的固定反例：

- 5,000 个 `2 <= g <= 10` 确定性随机实例逐项对照独立全子集 DP；
- 500 个 `6 <= g <= 10` 正权互异单终端压力实例；
- `g=7,8,...,16` 每个组数 16 个 omitted-half transpose 实例，并断言每个组数都实际进入主状态搜索；
- 独立穷举整数分拆，核验所有总量小于 `tau(r)` 的组件都可装入两个容量 `r` 的块，首次反例恰出现在 `tau(r)`，且当前定义域都可由三块覆盖；
- 三块直接枚举与 pair-union 最小值因子化逐 mask 等价；
- 完整 CTest 5/5 通过，包括零权 witness、配置合同和实际状态计数。

这些长查询使用的是同一旧结构族，但“同结构”不能替代精确性证明。当前修复加入辅助 $H(h)$ 后，状态域和时间都可能改变，必须重新运行而不能复用本节文件哈希或状态数。

### 4. 五条查询结果

| 查询 | 时间（s） | 精确权值 | 查询峰值增量（MiB） | 实际状态数 |
|---|---:|---:|---:|---:|
| q1 | 4,761.437028 | 21 | 3,266.098 | 105,902,192 |
| q2 | 4,715.120165 | 32 | 3,616.320 | 261,581,592 |
| q3 | 298.400088 | 38 | 2,690.520 | 31,051,638 |
| q4 | 357.558646 | 28 | 2,688.832 | 15,061,435 |
| q5 | 8,668.464606 | 32 | 7,130.660 | 560,869,444 |
| 合计 | 18,800.980533 | -- | -- | 974,466,301 |

旧二进制的五条结果行均低于 10,000 秒；q5 留有 1,331.535394 秒余量。峰值是 solver timer 内相对图加载后基线 RSS 的增量，不包含约 6.39 GiB 的 Orkut 图常驻内存。这些时间可用于当前风险估计，权值与状态不得当成修复版证据。

q2 的完整 ordinary 精确对照为 3,805.630196 秒、3,714.625 MiB、270,104,041 个状态；最终结构为 261,581,592 个状态和 3,616.320 MiB，分别减少 3.15% 和 98.305 MiB。两次时间环境不同，最终 q2 与另一个大查询并发，所以不比较时间。该对照只证明省略 `D(7)` 确实删除了状态与空间，而不是把工作转移到未计数结构。

#### 4.1 当时哈希二进制的代表复核

注释审计后重新构建的最终二进制 SHA-256 为 `8bbce11a3b07964db3b31bc871456a4ce42867e538b63e7bc1695935d5ddc32d`。CPU10/CPU11 并行补跑 q3/q4，得到：

| 查询 | 当时源码时间（s） | 当时返回值 | 查询峰值增量（MiB） | 实际状态数 |
|---|---:|---:|---:|---:|
| q3 | 388.825334 | 38 | 2,690.332 | 31,051,638 |
| q4 | 359.044152 | 28 | 2,689.957 | 15,061,435 |

两条查询的返回值和状态数与上表原运行逐项相同，只证明当时二进制进入同一旧状态域。后来发现状态域本身缺少辅助半层，这种轨迹一致不能推出精确性或当前 10,000 秒门；当前版本必须重新运行。

### 5. 被拒绝方向与论文表述

- 省略 `D(6)` 的两层方案不精确，生产源码中不保留目标驱动最高 ordinary、延迟边界 evaluator 或对应配置字段。
- 无条件直接枚举三块使 q5 从同一候选族的 7,289.085420 秒增加到 8,901.782839 秒，且仍无法修复 q2；生产实现改为容量证明触发和 pair-union 因子化。
- 不使用图名、`g=15`、状态量、时间或整数权特化开关。
- 论文应表述为“Enhanced 用 separator terminal 与反向 H 替换完整前向完成的高层 realization，并由消费者集合推出 ordinary 截止；当前定义域只省略半格最后一层”，不能表述为“针对 Orkut 关闭一层 DP”。

失败候选的完整反例见 [省略两层 ordinary 与无条件三块终端](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-omitted-two-ordinary-layers-negative-probe-20260812)。

<a id="history-auxiliary-half-adjoint-correctness-audit-20260815"></a>

## 辅助半层 Adjoint 正确性事故与修复审计（2026-08-15）

> 原始记录：`AUXILIARY_HALF_ADJOINT_CORRECTNESS_AUDIT_20260815.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> 本文记录一次已经定位并修复的精确性事故。提交 `589894be3774ff6658e1120f9a9e899a53377ab1` 之后、采用旧 H 起点得到的 P1 与 Orkut `g=15` 长测只可作为历史性能探针，不能作为当前精确结果或最终门禁。本文主体中的 11,178.235 秒是第一版完备性修复的历史 q10 结果；当前冻结生产版状态以第三条勘误为准。

> **二次勘误（2026-08-17）。** 本文加入辅助 $H(h)$ 的第一轮修复仍错误地删除了全部较低直接 terminal，并让 successor 只读规范 branch；Musae q162/q295 证明该实现仍会高报。现行代码补齐必要较低双块终端、全值 successor 与互补 H 完成。本文第 6 节的相反陈述已经失效，详见 [Adjoint split 完备性审计](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-adjoint-split-completeness-audit-20260817)。

> **最终版状态（2026-08-19）。** 在 split 完备性修复之后，冻结求解器源码提交 `12d6adb` 采用最小共同 A1 与 certificate-support 增量求值。其生产二进制完成 Orkut g15 q1--q10，q10 为 9,544.561 秒、精确权值 54、状态数 1,459,398,194；13 图 P1 也以 13/13 通过逐图底线。9,250.911 秒仍只表示同源码诊断构建的演进门。最终身份和表格见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

### 1. 事故表现与定位

旧实现完整生成 ordinary 到最高逻辑层

```math
q=\left\lfloor\frac{g}{2}\right\rfloor-1,
```

却让反向 H 直接从 `q` 启动，并试图用一至三块同根 ordinary terminal 补偿未物化的半层

```math
h=\left\lfloor\frac{g}{2}\right\rfloor.
```

这只枚举了若干同根 split 标量，没有执行被省略 `D(h)` 的完整图最短路闭包。它因此不是 `D(h)` 的等价 realization，也不能作为后续 H 递推的精确基例。

全量 P1 的逐查询权值审计首先发现 LiveJournal 与 Orkut 上的高报。独立 Base、DirectedCutOnly 与 Enhanced 对照把问题隔离到 AdjointCompletion：例如 Orkut 作者查询 g7 q7 的 Base 和 DirectedCutOnly 都返回 29，旧 Enhanced 返回 30。进一步比较提交边界确认旧前向版本仍正确，而引入从逻辑层直接启动 H 的版本开始出现差异。

已知生产反例共九条：

| 图与查询 | 旧 Enhanced | 精确值 / 当前 Enhanced |
| --- | ---: | ---: |
| LiveJournal g7 q44 | 23 | 22 |
| LiveJournal g7 q46 | 61 | 60 |
| Orkut g7 q7 | 30 | 29 |
| Orkut g7 q16 | 32 | 31 |
| Orkut g7 q18 | 25 | 23 |
| Orkut g7 q22 | 30 | 29 |
| Orkut g7 q23 | 37 | 36 |
| Orkut g7 q27 | 31 | 30 |
| Orkut g7 q28 | 32 | 31 |

修复版对九条查询的直接结果保存在 `results/probes/final_auxiliary_known_mismatches`。该目录只作服务器本地审计证据，不作为仓库内分发数据。

已经启动但只完成一部分的错误 P1 campaign 已从 `results/paper_runs` 移到 `results/probes/invalid_exactness_p1_2fc72775176b_20260815`，避免正式汇总器把其中的 Enhanced 记录混入当前结果。

### 2. 最小固定反例

回归测试加入一个 12 点、7 组的确定性图。独立全子集 DP 的精确值为 5.75；旧 H 起点返回 6.25。该反例含零权边与重叠组，但错误不来自浮点容差或零权路径恢复，而是缺失 `D(h)` 的图闭包。当前三个合法配置都必须返回 5.75。

这个 fixture 比随机对拍更适合长期门禁：它固定触发被省略半层，能够在相关代码发生任何重构时直接锁住同一证明责任。

### 3. 当前修复

令锚组之外有 `k=g-1` 个组。Enhanced 仍与 Base 调用同一个 ordinary 构造器，完整生成所有 `|S|<=q` 的 `D(S)`。有非空 H 后缀时，物理 H 区间扩展为

```math
H(h),H(h-1),\ldots,H(\ell+1),
```

其中 `H(h)` 是辅助层，不是新的前向 A 职责。

- 若 `g=2h`，则 `k=2h-1`，辅助目标 `S` 的补集大小为 `h-1=q`。代码直接转置已经 ready 的 $D([k]\setminus S)$。
- 若 `g=2h+1`，则 `k=2h`，补集大小为 `h`。代码枚举 ordinary 递推中覆盖该补集的全部双块 split seed，再执行与 ordinary 相同的图闭包。

由此在当前可改善锥体内得到

```math
H(S,v)=D([k]\setminus S,v),
\qquad |S|=h.
```

随后从大到小构造 H。若 successor 已满足上述等式，则

```math
H(S\cup B,v)+D(B,v)
```

正好对应 ordinary 在补集侧把规范 branch `B` 加回去；执行同一图闭包后得到 $D([k]\setminus S)$。按层归纳，该等式对所有 $\ell<|S|\le h$ 成立。边界的

```math
A(L,v)+D(S\setminus L,v)+H(S,v)
```

因此覆盖锚组、低层组、边界块与全部外侧组，且每个有限项都可展开为原图真实子树。

### 4. 本轮严格支配减空

修复后只在辅助 `H(h)` 播种直接 terminal。

1. 较低 `H(S)` 的直接 terminal 已由精确辅助层、successor 转移和图闭包生成同一 $D([k]\setminus S)$；重复播种不会产生更优精确值，因而删除。
2. 偶数 `g` 时，直接 `D(Q,v)` 已 ready。ordinary 的精确递推保证它不大于任意同根 `D(B1,v)+D(B2,v)`，所以完全跳过 pair 枚举。
3. 奇数 `g` 缺少直接 `D(h)` 时才枚举双块。排序 pair 与互补 submask 只是同一候选集合的两种枚举顺序；代码用可预估的循环项数选择常数较小者，不读取图名、查询编号、时间、状态量或经验参数。

还测试过只枚举某一规范 branch 的更窄实现。它通过精确性门且在 Orkut g15 q3 仅少十余个状态，但两轮交换绑核后总时间约持平或略慢，并显著增加证明与 review 负担，故没有接入。这里遵循“逻辑可删还不够，热路径必须物理不退化”的既定准则。

### 5. 已通过与仍待完成的证据

当前修复已经具备：

- 12 点固定反例返回 5.75；
- 5,000 个确定性随机连通实例、500 个正权互异单终端实例及 160 个 g7--g16 omitted-half 实例逐例匹配独立全子集 DP；
- 上表九条生产反例恢复精确权值；
- 五项 CTest 全部通过。

下列旧证据全部降级为历史性能证据：旧二进制 SHA-256 `a957bdcecafc486575cb7d78779dcbeda687eb0ea7283a0fc1104089dce83b86`、旧 Enhanced 状态总数 28,064、旧 Orkut g15 q1--q10 状态与时间、以及旧 P1 Enhanced 聚合。它们验证不了当前修复版。

当前版本仍须直接完成两项硬门：

1. P1 的 13 图全量按图聚合，最快 ABHSS 配置不劣于 PrunedDP++，并逐查询核对精确权值；
2. Orkut g15 q1--q10 每条当前结果都严格小于 10,000 秒。旧 q10 的 9,934.603 秒只有历史风险估计意义，不能替代复跑。

在两项门禁完成前，任何文档都不得把旧 q10 或旧 P1 写成“当前最终版本已通过”。

<a id="history-adjoint-split-completeness-audit-20260817"></a>

## Adjoint split 完备性事故与修复审计（2026-08-17）

> 原始记录：`ADJOINT_SPLIT_COMPLETENESS_AUDIT_20260817.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> **最终状态（2026-08-19）。** 本文的完备性修复仍是现行正确性基础；后续最小共同 A1 与 certificate-support 增量求值没有改变该递推契约。冻结求解器源码提交 `12d6adb` 的生产二进制已完成 Orkut g15 q1--q10 十条正式门，其中 q10 为 9,544.561 秒、精确值 54；13 图 P1 也已完成，逐图均满足最快 ABHSS 配置不劣于 PrunedDP++。完整身份和表格见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

### 1. 审计结论

此前“只给辅助 H(h) 播种、较低 H 只从 successor 加规范 branch”的实现并不完备。它遗漏了三种不能互相替代的结构职责：

1. 较低 H 目标中，两侧 ordinary 块都已物化、但任一侧并入目标都会越过 H 最高层的平衡 split；
2. successor 所代表的补集侧已经承担规范 branch 时，新增 ordinary 侧可能是 accumulator，不能再次强制读取 branch；
3. 非锚组恰能二等分时，完整前向格的“锚组加两张半格 D”没有 ordinary 半格可供普通 A/H 边界读取。

当前修复不增加图名、查询编号、运行时间、状态数、内存或经验组数阈值。它按集合大小和递推生命周期补齐：

- 全部物化 H 目标所需的完整 D 单块终端；
- 两侧均位于 ordinary 边界内的双块直接终端；
- H successor 加新增 ordinary 块的全部精确值；
- 两张互补辅助 H row 加锚组距离的平衡完成式。

这些操作不是近似补丁。每个新候选都是组集互斥的真实 rooted 子树在同一顶点的并；它们只补回完整普通 DP 已有的推导，不会产生虚假的较低值。

### 2. 两个独立反例

在 Musae 的 300 条 g=7 查询中，旧 Enhanced 与 Base 出现多处权值不一致。两个定位用例为：

| 查询 | Base / 独立 DP 真值 | 旧 Enhanced | 缺口 |
|---|---:|---:|---|
| q162 | 491 | 492 | 两张互补辅助 H 未结算锚组加两个半格 |
| q295 | 493 | 514 | 同类平衡完成缺口 |

关闭 adjoint 的 prefix 与 terminal 剪枝后，q162 仍返回 492，且状态数明显增大。这排除了“下界过强或数值比较错误”。临时把 ordinary 继续物化到完整半格后得到 491，进一步把原因限定到 H 对被省略半格的表示，而非 Dijkstra 闭包、witness 或 directed-cut。

只把 successor 从 branch 改成全值也不能单独修复 q162/q295，因为它解决的是另一类 split；只有补上互补 H 完成式后，两条反例才分别恢复 491 与 493。这说明“全值 successor”和“互补半格完成”都不能被对方严格支配。

### 3. 三类 split 覆盖

固定待构造的 H(S)，其语义是 S 关于全体非锚组之补集的普通 D 值。令该补集为 Q，并考虑普通 D 的一条规范二分，其中 X 与 Y 不交且并为 Q。

- 若完整 D(Q) 已物化，直接转置该 row。任意同根 D(X)+D(Y) 都是一棵覆盖 Q 的可行 rooted 子树，因此逐值不小于精确 D(Q)，并被单块及其图闭包支配；若某位置没有保留 D(Q)，ordinary 的可采纳 future 已证明该位置及从它出发的传播不能改善 incumbent。
- 否则若 X 与 Y 的大小都不超过 ordinary 边界，则双块直接终端保留该 split。此时把任一侧并入 S 都可能超过 H 最高层，successor 不一定存在。
- 否则至少一侧超过 ordinary 边界。由平衡层边界可推出较小侧并入 S 后仍不超过 H 最高层，因此已完成的 successor 表示较大侧，新增 ordinary row 表示较小侧。

第三类中，规范 branch 可能在较大侧，即已经封装进 successor 的补集语义。故 successor 合并必须读取较小侧 ordinary row 的全部精确值；只读 branch 会漏解。最终 A/H 边界仍只读 branch，因为那里执行的是前向 A 首次跨界的规范 ordinary 一侧，职责不同。

当非锚组数满足 k=2h 时，两侧大小都为 h，ordinary 只到 h-1。两张 D(h) 都由辅助 H realization 承担，因此需在互补 H row 均发布后结算锚组距离与两张 H。该条件完全由集合大小推出，不是对 g 数值作经验分段。

### 4. 当前实现与门禁

实现落点：

- `BuildTransposedTerminals` 为全部物化 H 层生成必要的单/双块直接终端；
- `ForEachBackwardValueSum` 只服务 H successor，枚举 ordinary 全值；
- `ForEachBackwardBranchSum` 保留给最终 A/H 边界；
- `ForEachRowValueIntersection` 在两张有序稀疏 row 间确定性选择线性或较小侧二分；
- `SolveHighAdjoint` 在互补辅助 row 发布后执行一次平衡完成，并输出逐 H 层耗时。

结构回归枚举全部受支持组数及规范 D split，断言每个 split 至少有完整 D 终端、双块终端或合法 successor 路径；非锚组二等分时另断言互补辅助域闭合。

修复后的 Musae g=7 全 300 条查询与 Base 权值逐条一致：

| 配置 | 总时间（秒） | 累计状态 |
|---|---:|---:|
| Base | 46.527316 | 6,872,062 |
| Enhanced（全值 successor + 互补 H） | 22.218182 | 4,282,071 |

与“branch-only successor + 互补 H”的隔离构建相比，全 300 条的权值和状态数也逐条一致；总时间从 21.969781 秒变为 22.218182 秒，单轮差异约 1.13%，属于需要在更大门禁中继续观察的固定遍历代价，不能据此把全值读取删回错误版本。

回退无效链上包络后的 production 与诊断构建 SHA-256 分别为：

- `0cfcc06053ca349154c0023013351e0d607f90bd68bc5ed799d84b8a28d8832d`
- `abb463d3322f16b766d2d69bbff0e421636a74361d1f20d1a31a6d08ef303384`

两种构建均重新通过 5/5 CTest；配置精确性测试包含 5,000 个随机图、500 个正权单终端实例、160 个高组 omitted-half 实例、固定反例与全部 split 结构枚举。当前诊断构建的 Musae $g=7$ 全 300 条再次得到 Base/Enhanced 逐条权值一致，总时间 45.464851/22.210271 秒、累计状态 6,872,062/4,282,071。SteinLib 的 11 个 $g=11..16$ 实例也全部逐项一致，包含 `wrp4-16 = 1190`；Base/Enhanced 状态总数为 76,383/54,518。两项计时只作正确性门，不进入论文性能表。

### 5. Orkut q10 完整长门

旧诊断构建的 q10 曾在 11221.686261 秒完成，权值 56、累计状态 1,764,062,894、查询峰值 21,260.715 MiB、watchdog 峰值 27,652.289 MiB。由于该构建缺少上述 split 与互补完成职责，权值 56 不得再作为精确答案或性能主结果。

当前修正诊断构建使用 30,000 秒预算独占 CPU 5 自然跑完 q10：

本地原始证据目录为 `results/probes/balanced_h_complete_q10_full_diag_cpu5`；`records.jsonl` 保存可机读阶段事件和 watchdog 指标，`logs/2d18259080db3d0a.log` 保存逐 row 诊断，原生结果行为 `native_output/2d18259080db3d0a/GPU4GST_Orkut/ABHSS-Enhanced/cross_g15/weights.txt`。这些结果文件受 Git 忽略，只在服务器本地保留；本文表格是可提交的证据摘要。

| 指标 | 结果 |
|---|---:|
| 精确权值 | 54 |
| solver 时间 | 11,178.235 秒 |
| 超过正式 10,000 秒 TL | 1,178.235 秒 |
| 查询峰值 | 21,337.590 MiB |
| watchdog 总 RSS 峰值 | 27,729.191 MiB |
| 累计首次发现状态 | 1,761,794,764 |

阶段轨迹如下：

| 阶段 | 秒数 | 阶段末上界 | row / scalar 摘要 |
|---|---:|---:|---:|
| 预处理 | 189.940 | 62 | — |
| 共同 A1 | 85.956 | 62 | 14 / 39,056,188 |
| ordinary | 6,524.740 | 58 | 6,461 / 1,091,601,442 |
| 低层 A | 2,040.340 | 57 | 469 / 333,266,044 |
| adjoint 总计 | 2,337.210 | 54 | 9,438 / 130,660,288 |
| 其中转置 | 135.165 | 57 | — |
| 其中 H7 | 1,278.810 | 57 | 3,432 / 98,394,030 |
| 其中 H6 | 626.668 | 54 | 6,435 / 130,451,580 |
| 其中 H5 | 151.333 | 54 | 8,437 / 130,651,997 |
| 其中 H4 | 145.169 | 54 | 9,438 / 130,660,288 |

H7 没有产生互补半格上界事件；真正把 57 收紧到最终 54 的候选出现在 H6。上界收紧后，H5/H4 合计只净增 208,708 个保留 scalar，说明主要浪费已经发生在 ordinary、低层 A、H7 和 H6。要达到 10,000 秒，需相对当前总时间真实减少约 10.54%，不能解释成计时噪声。

该构建还含一个“directed-cut 已盖住 A1/tour 上包络时跳过后续证书链”的候选。它与可比轨迹的 ordinary row、scalar、layer work 完全相同，却把 ordinary 从 6,455.520 秒拖到 6,524.740 秒，增加 69.220 秒（1.07%）。该候选已从源码和正式文档回退；以上完整结果因此是当时 split 完备性修复的正确性证据和清理后性能的保守上界，不是最终发布二进制的正式计时。当时尚未完成的 q1--q10 与 P1 后续已由文首所列冻结生产二进制通过。

### 6. 禁止回退的旧方向

- 不能再声称“较低 H 直接终端都被 successor 严格支配”。
- 不能让 H successor 只读新增 ordinary 的 branch。
- 不能删除非锚组二等分时的互补 H 完成式。
- 不能用关闭 prefix、浮点容差、降精度、固定 top-k、固定三块装箱或按图/按查询开关掩盖结构缺口。
- 后续减空只能删除已由同目标精确 D 逐值支配的 pair，或另行给出覆盖所有规范 split 的结构证明与独立 DP 门禁。

<a id="history-minimal-forward-a1-adjoint-gate-20260818"></a>

## 最小共同 A1 前缀与 Orkut q10 门禁

> 原始记录：`MINIMAL_FORWARD_A1_ADJOINT_GATE_20260818.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

> **最终状态（2026-08-19）。** 本文冻结最小共同 A1 checkpoint `e9eee92` 的历史证据。后续冻结求解器源码提交 `12d6adb` 又加入 adjoint 空域减空与 certificate-support 增量求值；其生产二进制已经完成 Orkut g15 q1--q10 与 13 图 P1，q10 为 9,544.561 秒、精确值 54，P1 为 13/13 图通过逐图底线。本文的 9,431.057 秒 checkpoint 和 9,250.911 秒诊断值仍不能冒充最终正式时间；最终身份见 [全实验方案](../EXPERIMENT_PLAN.md#710-冻结版最终-p1orkut-硬门与哨兵基准)。

### 1. 本轮改动

完整锚定格的最高逻辑层为：

```math
q=\max\left\{0,\left\lfloor g/2\right\rfloor-1\right\}.
```

Base 与 DirectedCutOnly 仍以前向 A 实现全部逻辑层。Enhanced 的前向边界由旧的固定中点改为：

```math
\ell=\min\{1,q\}.
```

因此正层域非空时，三个配置仍逐项运行同一个共同 A1 构造、future 视图、所有权移交、重滤和完整解结算；Enhanced 不再生成 A2 及更高前向层，逻辑层 2 到 q 全由 H 实现。若 q 不大于 1，H 后缀为空，Enhanced 直接复用完整前向入口。

这里的 1 不是经验超参数，而是论文要求的最小非空共同前向前缀：A1 必须由 Base 与 Enhanced 执行相同操作。调度函数不读取图名、询问编号、g 的经验区间、组大小、row 密度、incumbent、时间或内存。

### 2. 正确性与论文口径

现有 Adjoint 完备性证明对任意合法边界 ell 成立。辅助 H(h) 精确实现省略的 ordinary D(h)；对较低 H 目标，每个 ordinary 规范 split 由以下三类之一覆盖：已有完整 D(Q) 的单块 terminal、两侧都在 ordinary 域内的双块 terminal，或更大 H successor 加回较小 ordinary 块的全部精确值。逐层图闭包后有：

```math
H(S,v)=D([k]\setminus S,v),
\qquad
\ell<|S|\le h.
```

令 ell 等于 1 只把 A/H 的等价 realization 边界向下移动，不改变 terminal、successor、互补半格完成式或可采纳 prefix 的证明。A1 仍由同一前向内核消费，H 从逻辑层 2 开始覆盖剩余职责，不存在遗漏或重复逻辑层。

这项选择不能写成“前向 A 被 H 严格支配”。A 与 H 的物理稀疏性没有输入无关的全序，某些输入可能更适合多生成几层 A。论文应写成两个已证明等价 realization 之间的冻结结构边界：Enhanced 只保留必须共同的 A1，其余层统一交给自身的 H realization。实验负责说明该冻结边界在目标工作负载上的时间与空间效果，不能声称对所有输入逐项更快。

### 3. 正确性与小规模门

当前候选在干净的 Release 目录 `build-check-minimal-a1` 完成构建，生产二进制 SHA-256 为 `996183f87efd30d3e6c98d8e520c239c42acbb73787d33f1e721c85a74cdd985`。五项 CTest 全部通过，包括图/询问输入、零权边回归、配置覆盖与确定性/随机小图全子集 DP 对照、辅助半层、高 g SteinLib 已知最优值和状态统计口径；配置测试逐个检查 g=0 到 16 的 A/H 层覆盖。Musae P1 g7 全 300 条此前也已经证明当前 H 完备性修复与 Base 逐条同值。

### 4. 交换绑核性能门

Musae P2 g14 的 10 条查询在 CPU 4 与 CPU 5 上交换候选和旧边界，所有权值逐条相同：

| 轮次 | 候选秒数 | 对照秒数 | 候选状态 | 对照状态 | 时间变化 |
|---|---:|---:|---:|---:|---:|
| 候选 CPU4、对照 CPU5 | 111.675 | 125.944 | 38,284,403 | 43,912,777 | -11.33% |
| 候选 CPU5、对照 CPU4 | 111.081 | 128.727 | 38,284,403 | 43,912,777 | -13.71% |

状态减少 12.82%；候选峰值约 252.2 MiB，对照约 250.0 MiB，增加约 2.2 MiB。Musae P1 g7 的 300 条查询在两轮交换绑核中状态均为 4,282,071；合计时间为候选 44.609 秒、对照 44.647 秒，属于中性波动，没有观察到小组路径退化。

### 5. Orkut g15 q10 完整长测

输入为 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt` 的逻辑 q10，询问统计为 g=15、mean_f=309.8、min_f=56、max_f=793。进程固定在 CPU 5；图加载不计入 solver timer。诊断二进制 SHA-256 为 `067620fbac9b16a661b74d2c1071a1ea15648f01034a893f5de7e06b09ab11d2`。

原始记录位于：

- `results/paper_runs/minimal_forward_q10_full_diag_cpu5/records.jsonl`
- `results/paper_runs/minimal_forward_q10_full_diag_cpu5/logs/bfffd2ab66121357.log`
- `results/paper_runs/minimal_forward_q10_full_diag_cpu5/resource_progress.log`

最终结果如下：

| 指标 | `e9eee92` checkpoint |
|---|---:|
| 精确权值 | 54 |
| solver 时间 | 9,431.057 秒 |
| 距 10,000 秒余量 | 568.943 秒 |
| 查询峰值 | 18,305.023 MiB |
| watchdog 总 RSS 峰值 | 24,696.602 MiB |
| 首次发现状态 | 1,459,398,194 |

阶段轨迹为：

| 阶段 | 秒数 | 结束 best | row / scalar 说明 |
|---|---:|---:|---|
| prepare | 188.733 | 62 | 距离、证书和初始上界 |
| 共同 A1 | 85.170 | 62 | 14 / 39,056,188 |
| ordinary | 6,710.250 | 58 | 6,461 / 1,091,601,442 |
| A1 移交与前向结算 | 8.766 | 58 | 14 / 17,060,805 |
| ordinary 转置 | 160.837 | 58 | H terminal 工作区 |
| H7 | 1,194.620 | 57 | 累计 3,432 / 109,252,671 |
| H6 | 578.485 | 56 | 累计 6,435 / 142,146,692 |
| H5 | 216.292 | 56 | 累计 8,437 / 144,447,727 |
| H4 | 160.669 | 56 | 累计 9,438 / 144,617,407 |
| H3 | 92.691 | 56 | 累计 9,802 / 144,623,692 |
| H2 | 34.438 | 54 | 累计 9,893 / 144,623,751 |

最重要的代价是最终上界比旧边界更晚收紧：旧路径在 A2 把 58 收紧为 57，并在 H6 得到 54；当前路径没有 A2/A3，在 H7 得到 57、H6 得到 56，直到 H2 才得到 54。收益是删除了约 2,031.6 秒的低层前向工作；新增 H 后缀约增加 100.9 秒，净结果仍通过 TL。

### 6. 与历史完备版的边界

| 指标 | 历史完备版 | `e9eee92` checkpoint | 变化 |
|---|---:|---:|---:|
| 精确权值 | 54 | 54 | 相同 |
| solver 时间 | 11,178.235 秒 | 9,431.057 秒 | -15.63% |
| 查询峰值 | 21,337.590 MiB | 18,305.023 MiB | -14.21% |
| 首次发现状态 | 1,761,794,764 | 1,459,398,194 | -17.16% |

历史二进制还包含随后因普通阶段退化而回退的证书链候选，故此表是端到端风险对照，不是隔离层边界的严格微基准。ordinary 的规范事件前缀在排除当前新增的 A1 ranked-tail 物化事件后逐项一致，但其 wall time 仍有约 2.84% 差异，不能把该差异解释为算法状态变化。层边界本身的受控证据应引用第 4 节交换绑核结果。

### 7. 后续最终版状态

当前变化会影响 P1 中 g=10 的查询：LinkedMDB 15 条、DBpedia 2 条，共 17 条。GPU4GST P1 的组数只有 3、5、7，其层计划不变。17 条已经完成两轮交换绑核定向门，答案逐条一致：LinkedMDB 状态减少 1.98%，两轮总时间比分别为 1.0003 和 0.9688；DBpedia 状态减少 0.12%，两轮总时间比分别为 0.9719 和 0.9513。当时尚未完成的 13 图 P1 与 Orkut 十条全门后续均已由文首冻结生产版通过。

冻结求解器源码提交 `12d6adb` 的 q10 使用 86,400 秒外层诊断预算保留自然结束轨迹，但在正式门线之前以 `solver_seconds=9250.911428` 完成，精确权值 54，状态数 1,459,398,194。外层预算不是论文 TL；论文仍统一按 10,000 秒判定。该运行保留了 `records.jsonl`、完整 probe 日志和每分钟资源采样，详细分解见 [certificate-support 增量求值门](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-incremental-certificate-support-dp-gate-20260818)。

任何后续失败都不得通过图名、查询编号、mean_f、运行时间或内存阈值动态恢复旧边界。若需要回退，只能对整个 Enhanced 配置恢复统一层计划，并重新跑同一组正确性与性能门。
