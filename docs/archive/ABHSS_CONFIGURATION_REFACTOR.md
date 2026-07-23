# [归档] ABHSS 单入口与增强开关重构审计

## 1. 重构目标

本次重构直接回应论文叙述风险：旧实现把 Light/Heavy 暴露为两个公开函数、两个源文件入口、两个二进制和两个“method”记录，容易让审稿人理解为两套算法后再按数据选择。新实现要求：

1. 公开 API 只有一个 `SolveOneQuery`。
2. 正式构建只有一个 `abhss` 可执行文件。
3. Base 不开增强；在 Base 上依次开启 `DirectedCut` 和 `AdjointCompletion` 得到全增强配置。
4. 从全增强配置反向关掉相同开关，必须得到中间配置和 Base。
5. 配置在查询开始前冻结，运行中禁止自动切换。
6. 重构前后的精确答案逐项相同，代表性小图、参数增长和大图面板不得出现可归因于代码结构的性能退化。

## 2. 旧结构问题与新边界

| 维度 | 旧结构 | 新结构 | 审稿风险变化 |
|---|---|---|---|
| 公开 API | `SolveLightOneQuery`、`SolveHeavyOneQuery`、`SolveHeavyForwardOneQuery` | `SolveOneQuery(..., SolveOptions)` | 不再像三个 solver |
| 正式二进制 | `abhss_light`、`abhss_heavy` | `abhss` | 两条曲线可核验为同一 artifact |
| 内部模式 | `SolverVariant::{Light,Heavy}` 二选一 | 增强位集合及显式依赖 | 可从 Base 单调加开关 |
| 顶层编排 | `light.cpp`、`heavy.cpp` 各有入口 | `solver.cpp` 唯一编排 | 不会悄悄漂移阶段顺序 |
| 高层增强 | 与 Heavy 入口绑定 | `adjoint.cpp` 仅提供一个可选操作 | 文件表达的是操作而非方法 |
| 实验矩阵 | 两个不同 executable | 同一路径、不同冻结参数 | 环境 gate 可机器检查 |
| 消融 | 第三个专用函数 | `--enhancements=directed-cut` | 消融是关开关，不复制递推 |

## 3. 配置链的代码证据

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

## 4. 唯一调度器

`solver.cpp` 中只有一次前置检查、一次 `Problem` 构造、一次公共 prepare 和一次 ordinary 构造。之后只有完成策略的开关：

- 未开 adjoint：调用完整前向 `A`。
- 开 adjoint：调用同一个前向内核的低层计划，再追加高层 `H`。

开启 directed-cut 时，`PrepareProblem` 选择完整距离、dual/primal 证书；关闭时选择 bounded 距离、根路径 witness 和 early-A1。所有能力通过 `Problem::UsesDirectedCut()`、`UsesAdjointCompletion()` 和 `UsesBoundedGroupDistances()` 读取，代码中不再存在 `SolverVariant`。

## 5. 命令行与结果元数据

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

## 6. 实验矩阵约束

`paper_matrix.json` 使用 schema 4：

- `abhss_base`：`build/Release/abhss.exe --enhancements=none`。
- `abhss_enhanced`：同一个 `build/Release/abhss.exe --enhancements=all`。

环境验证器要求两项 `algorithm_family` 都为 ABHSS、可执行路径完全相同、参数和增强列表精确匹配。任意人把两项改回不同二进制，正式运行前都会失败。

## 7. 正确性门

重构后必须通过：

1. 零权父指针历史反例：Base、DirectedCutOnly、Enhanced 都返回 1。
2. 120 个确定性随机连通小图：三种配置逐例匹配独立全子集 DP。
3. SteinLib WRP 11 例：Base 与全增强配置逐例匹配已知最优值，并与重构前冻结二进制逐项一致。
4. 公开配置依赖：非法 adjoint-only 在求解前报错。
5. 命令行配置依赖：同一非法组合返回非零退出码，不能静默修正。
6. 快速图读取结构夹具：原边顺序、`edge_id`、双向邻接顺序、零/小数/科学计数边权全部匹配真值。

## 8. 非退化验证设计

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

### 8.1 大图快速读入是独立工程门

Orkut 的作者图为 3,072,441 点、117,185,083 条无向边，文本文件约 2.12 GB。旧 `operator>>` 路径逐字段执行格式化流检查，并在逐边插入邻接表时让各顶点反复扩容；其加载墙钟远大于单条查询的求解时间。新 `LoadGraphFromFolder` 因而做两项不改变图语义的优化：

1. 以 8 MiB 大块缓冲直接扫描 ASCII 整数；浮点 token 用无分配的 `from_chars` 转换，保持标准十进制舍入语义。
2. 第一阶段保留原边并统计每个顶点的精确度数，第二阶段一次性 `reserve` 邻接容量，再按原边顺序构造双向邻接表。

边序、`edge_id`、每个顶点的邻接插入顺序、边权和无向双边表示均保持不变。程序在结果 header 与 `[Ready]` marker 中分别写入 `graph_load_seconds` 和 `query_load_seconds`；这两个字段只审计工程等待时间，绝不并入逐查询 solver timer，也不作为论文算法 speedup。验证要求是：小图逐边结构与旧读入等价；Orkut 可完整加载；新加载墙钟显著下降；Base/增强配置的答案和 query time 非退化门仍独立通过。

实测结果：Orkut 的旧格式化读取从进程启动到 `[Ready]` 约 565–566 秒，新读取的内部计时为 25.996–26.880 秒，即约 21 倍；加载后 RSS 从约 8561 MiB 降到约 6463 MiB。最后一次注释/测试整理并重新构建后，又用最终二进制复跑 Orkut：Base/Enhanced 加载为 28.079/30.868 秒，query time 为 24.446510/90.984837 秒，相对旧版为 1.0000/1.0082，权重仍均为 11。Reddit 旧端到端墙钟扣除 query time 后的加载约 63 秒，新读取为 3.892–3.902 秒，即约 16 倍。两图随后都以相同权重通过独立的 query-time 非退化门，所以该收益没有混淆为算法 speedup。

## 9. 中文函数注释审计

注释范围是本次方法实现与入口：`src/abhss/*.h`、`src/abhss/*.cpp`、`src/main.cpp` 和两个 ABHSS 回归测试。每个命名函数/成员/模板至少在声明或局部定义处说明：

- 它计算什么；
- 输入输出或所有权；
- 关键 exactness 不变量；
- 与增强开关的关系；
- 热路径中重要的常数选择或生命周期。

局部 lambda 不机械逐行注释；其用途由所属函数段落和必要的行内不变量说明。baseline 与无关通用 I/O 不属于本次算法重构，不为了凑注释数量做无关改写；本次明确修改的快速图读取器则逐函数说明缓冲、解析、错误与所有权语义。

## 10. 论文写法

推荐：

> ABHSS exposes a base configuration and two composable accelerators. Enabling the directed-cut accelerator and then the adjoint-completion accelerator yields the fully enhanced configuration; disabling them in reverse order recovers the base configuration. Both configurations invoke the same executable and share the ordinary and anchored DP kernels.

禁止：

- 把 Base 和全增强配置称为两个独立提出的方法；
- 写成运行时自适应选择；
- 报逐查询二者最小值；
- 声称全增强配置逐条 CPU 指令包含 Base；
- 用本重构 gate 的少量计时代替正式实验矩阵。
