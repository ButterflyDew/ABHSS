# ABHSS 代码入口与阅读指南

本文档回答“从哪里编译、怎样运行、一条查询经过哪些函数、每个源文件负责什么”。算法动机、递推、证明和复杂度见 [`METHOD.md`](METHOD.md)；论文实验的数据与报告口径见 [`EXPERIMENT_PLAN.md`](EXPERIMENT_PLAN.md)。

## 1. 代码与实验的真值层级

为避免命令、文档和长时间结果之间漂移，当前仓库按下列顺序解释冲突：

1. C++ 公开接口与测试决定“程序实际做什么”。
2. `experiments/paper_matrix.json` 决定“正式实验跑什么”。
3. `experiment_data/**/manifest.json` 和 `experiments/query_feasibility_audit.json` 决定“具体输入是哪个文件”。
4. `experiments/environment_lock.json` 决定 timeout、计时边界、运行配置和报告契约。
5. 本文档和 `EXPERIMENT_PLAN.md` 是对上述机器真值的人类可读解释。`docs/archive` 只保留历史决策，不能覆盖当前口径。

## 2. Linux 上的最短入口

远程服务器需要 GNU Make、CMake 3.16 或更新版本、支持 C++17 的 GCC/Clang、Python 3.10 或更新版本以及 pthread。仓库顶层 `Makefile` 是 Linux 推荐入口：

```bash
make release JOBS=16
make validate
```

`make release` 配置 Release、编译当前可用的仓库内 target，然后运行 CTest。只构建论文性能运行所需的两个二进制可用：

```bash
make paper-binaries JOBS=16
make validate-paper-binaries
```

一次 smoke 运行的六个公共位置参数依次是 `graph_selector result_root query_selector data_root query_begin query_limit`：

```bash
./build/abhss example results/manual query.txt data 1 1 --enhancements=none
./build/abhss example results/manual query.txt data 1 1 --enhancements=all
./build/pruneddp example results/manual query.txt data 1 1 --state-storage=hash --mst-upper=on --lb2-pathmax=off
```

`graph_selector` 可以是图目录的显式路径、`data_root` 下的目录名或字典序 1-based 编号；正式实验不使用会随目录变化的数字编号。`query_selector` 可以是显式查询文件路径，因此 P2/S2 panel 不必复制到大图目录中。`query_limit=-1` 表示从 `query_begin` 起运行到文件结尾。

Windows 可直接用 CMake 的 Visual Studio 或 MinGW generator；多配置 generator 的二进制位于 `build/Release`，Linux/MinGW 单配置 generator 通常位于 `build`。实验 runner 会按矩阵中的主路径与 fallback 顺序查找，不需要修改矩阵。

## 3. 输入、输出与计时契约

### 3.1 图接口

每个图目录包含 `graph.txt` 或兼容名 `Graph.txt`：

```text
n m
u_1 v_1 w_1
...
u_m v_m w_m
```

顶点为 `1..n`，图是无向图，边权必须是有限非负数。重边、自环和零权边均保留。读取器要求声明后恰好有 $m$ 条边，尾部多余 token 会立即报错。加载后保留原边顺序、`edge_id`、两端邻接插入顺序，并一次性构建连通分量缓存。

### 3.2 查询接口

查询文件首 token 是查询数 $q$；每条查询先写组数 $g$，然后对每组写 `size vertex_1 ... vertex_size`。组内顶点数必须为正，组间允许重叠。查询读取器验证记录完整性；结合图的顶点范围与共同连通分量检查在 solver timer 内执行。

### 3.3 输出契约

每条结果是 `seconds weight query_peak_rss_overhead_mib mask_vertex_states`。无解时 `weight=-1`，有解时为最优权值。当前 ABHSS 公开返回结构包含 `best_weight`、`feasible` 和实际发现的主状态项数；其实际键包含状态族，因此 D/A/H 中数值相同的 `(mask,v)` 分别计数。它在内部构造可行 witness 来证明上界，但没有序列化最终最优树边集。

`graph_load_seconds` 和 `query_load_seconds` 写在结果 header 与 `[Ready]` marker 中，不进入算法时间。`[Ready]` 之后，每条查询的 timer 包含可行性检查、查询预处理和搜索。1 ms RSS 采样线程只读内存统计，不参与搜索。

## 4. 一条 ABHSS 查询的调用主线

```text
main
  -> ParseAbhssOptions
  -> LoadGraphFromFolder / LoadQueriesFromFolder
  -> SolveOneQuery
       -> ResolveQueryPrelude
       -> PrepareProblem
       -> DescribeConfiguration          [fixed add-or-replace profile]
       -> MakeAnchoredCompletionSchedule [derive logical A/H layer boundary]
       -> logical A1 responsibility      [only when the logical grid contains A1]
            Base: build once before D, reuse in forward A
            DirectedCutOnly: build as the first full-forward A layer
            Enhanced: build in low A, or replace the high layer by H
       -> BuildOrdinaryWithProbe
            Base: A1-cone future
            DirectedCut/Enhanced: dual-potential future
            -> BuildOrdinaryRows         [shared D grid]
       -> RunForwardAnchoredStage        [Base / DirectedCutOnly: full A]
          or
          RunForwardAnchoredStage        [Enhanced: low A]
            -> SolveHighAdjoint           [H replaces high A]
       -> SolveResult{best_weight, feasible, mask_vertex_states}
```

`src/main.cpp` 是二进制公共批处理入口。CMake 通过宏将同一入口编译为 `abhss`、`pruneddp`、`dpbf` 及可选的第三方 adapter，使图加载、查询分片、计时和输出格式一致。每个 `weights.txt` 查询行依次写 time、weight、query peak RSS 和 `mask_vertex_states`；正式 ABHSS/PrunedDP++ 返回非负计数，无统一口径的 adapter 写 `-1`。

`SolveOneQuery` 先通过 `IsValid`（内部读取 `DescribeConfiguration`）拒绝非法开关，再处理空查询、$g>16$、无共同分量、单组等入口情形。非平凡查询建立 `Problem` 并完成预处理后，才保存本次执行所需的 `ConfigurationProfile` 和状态层计划。`Problem` 为避免改变热对象布局，仍只读保存原来的冻结 bit mask；`UsesBoundedGroupDistances`、`UsesDirectedCut` 和 `UsesAdjointCompletion` 与 profile 映射由同一配置回归共同约束。代码与论文共用这张“新增或替换”契约，不能再把位掩码单调误写成逐指令包含。

`PrepareProblem` 依次构建零权分量下界、组距离、真实上界、锚组映射、tour 下界和 witness。Base 的规范 SPT 为 bounded 组距离提供安全 cutoff；完整距离 realization 不需要这一步。两种配置随后都计算 root-star 与 root-path-union 上界。Base 把该边并集整理为 root-path witness，并在主状态前执行一次 witness DP；开启 `DirectedCut` 时，以 primal upper、dual-primal witness 和后续同一 rent-or-buy 接口替换这项 pre-D/周期 witness 职责，facility 上界另作安全新增。两者都生成同一 `Problem` 和同一 ordinary $D$ 状态。

`MakeAnchoredCompletionSchedule` 从平衡证明得到完整锚定格的正层域 $\mathcal L_A=\{1,\ldots,q\}$，其中 $q=\max\{0,\lfloor g/2\rfloor-1\}$。代码只判断某个逻辑层是否属于该域，不含 `g >= 常数` 一类经验分段。域为空时，完成式直接使用隐式 $A(\varnothing)$；域非空时，A1 是第一个成员，Base 一律在 ordinary 前生成它。该 row 形成 `AnchoredSingletonFuture`，在 ordinary 后按所有权移交给公共前向内核，既不重复闭包也不重复计数。

`BuildOrdinaryRows` 按 mask 大小生成普通 $D$，将同根 split seed 做图闭包，并标准化 branch。开启 `DirectedCut` 后，ordinary future 这一相同职责由对偶势替换。DirectedCutOnly 仍在完整前向格中生成 A1；Enhanced 若低层前向区间包含 A1，就由同一 `BuildForwardAnchoredRows` 内核生成，否则该逻辑层属于 adjoint 高层，由 $H$ 的补集状态实现。`complete_implicit_anchor` 只表示完整正层域为空，不能用“低层前缀为空”或某个组数比较代替。这里区分的是状态层 realization，不是按查询参数选择快路径。

Base 和 DirectedCutOnly 经 `RunForwardAnchoredStage` 调用 `BuildForwardAnchoredRows`，生成完整的低/高层锚定 $A$。Enhanced 仍调用同一内核生成由平衡完成域确定的低层 $A$，再由 `SolveHighAdjoint` 以补集转置终端和递减 $H$ 代替未物化的高层 $A$。切分是递推域的固定 meet-in-the-middle 边界，不读取数据集名或运行表现。

从论文和代码审计角度，配置关系必须按下表理解：

| 逻辑职责 | Base realization | Enhanced realization | 关系 |
|---|---|---|---|
| 组距离 oracle | 规范 SPT 给出 cutoff，再构造 bounded `GroupRow` | 不需要 cutoff，直接构造完整距离势 `GroupRow` | 同一“值 + `IsExact`”读取合同的表示替换；非精确 cutoff 只作下界 |
| ordinary future | 提前调度的 A1 cone | directed-cut potential | 同一可采纳下界职责的替换 |
| pre-D 与周期 witness | root-path tree，并在 D 前先做一次树 DP | primal upper + dual-primal tree，随后进入同一 rent-or-buy 调度 | 同一真实可行上界/witness 职责的替换；共同的 root-star/root-path-union 仍由两边执行，facility 是额外安全上界 |
| 锚定 singleton $A_1$ | 逻辑格包含 A1 时在 ordinary 前生成并移交 | 若属低层则由公共前向 A 生成；若属高层则由 $H$ 替换 | 同一完成层语义；每张实际 row 至多生成一次 |
| 高层锚定完成 | 完整前向高层 $A$ | 低层 $A$ 加高层 $H$ | 等价完成式的方向替换 |
| 无 Base 对应物的工作 | 无 | directed-cut 可行证书、额外 facility 收紧 | 安全新增 |

`DirectedCutOnly` 采用表中的 DirectedCut 距离、future 和 witness realization，但仍保留完整前向高层 $A$，只作为隔离 `AdjointCompletion` 的正确性/消融配置。表中不允许出现“Base 独有且 Enhanced 没有同职责替代物”的逻辑阶段。

## 5. ABHSS 源文件导读

| 文件 | 主要职责 | 阅读时需要抓住的不变式 |
|---|---|---|
| `src/abhss/abhss.h` | 公开 `SolveOptions`、增强位、`ConfigurationProfile`、`AnchoredCompletionSchedule` 和 `SolveResult` | 开关链只表达安全新增/同职责替换；层计划只表达平衡递推域及 A/H realization |
| `src/abhss/solver.cpp` | 单一 solver 入口与配置调度 | 只在这里选择完整前向或 adjoint 完成；不存在按查询 oracle |
| `src/abhss/pipeline.{h,cpp}` | 平凡/无解前置、预处理和 ordinary 的公共 probe 边界 | 诊断包装不改变算法语义 |
| `src/abhss/internal.h` | `Problem`、`Row`、`GroupRow`、witness、状态计数和热路枚举器的共同定义 | $D$、$A$、$H$ 共用一个有序稀疏 `Row`；每张 row 按首次进入工作区的顶点批量计数；`ready` 与空 payload 不能混淆 |
| `src/abhss/preprocess.cpp` | 零权 cover、组距离、多种真实上界、tour、witness、统一 future | cutoff 不得当作精确状态；`best` 只由真实可行子图收紧 |
| `src/abhss/core.{h,cpp}` | A1 的 ordinary 前调度视图、ordinary $D$、row 交集、规范 branch、witness rent-or-buy | 提前 A1 仍是标准 `Row`，每张至多生成一次；cone 与 dual 是同职责 future 替换 |
| `src/abhss/forward.{h,cpp}` | 公共前向锚定 $A$ 递推与完整解结算 | 隐式 $A(0)$、提前 A1 的所有权交接和正常生成 row 都走同一完成函数 |
| `src/abhss/dual_cut.h` | `DirectedCut` 的对偶势、residual、primal 边恢复 | 势只作下界，上界必须由原图真实边计价 |
| `src/abhss/adjoint.{h,cpp}` | ordinary 按顶点转置、高层 $H$ 递减、低层 $A$ 边界结算 | $H(S)$ 覆盖 $S$ 外侧，与 $A(L)+D(S\setminus L)$ 恰好覆盖全组 |
| `src/abhss/diagnostics.h` | 编译期可关闭的稀疏 phase 诊断 | 正式构建不因诊断改变状态或配置 |

建议阅读顺序是 `abhss.h -> solver.cpp -> internal.h -> preprocess.cpp -> core.cpp -> forward.cpp -> adjoint.cpp -> dual_cut.h`。若先读 adjoint 热循环而未理解 `Problem::original_mask` 和 $H$ 的补集语义，很容易把“外侧已付”误读成普通 rooted DP。

## 6. 公共工程与 baseline 文件

| 路径 | 作用 | 论文角色 |
|---|---|---|
| `src/common/fast_numeric_reader.h` | 求解器与离线审计共用的 8 MiB ASCII 数字扫描器，含旧 libstdc++ 浮点回退 | 统一大图读取热路径，避免工具与 solver 语义/速度分叉 |
| `src/common/graph_io.{h,cpp}` | 精确度数预留、边/邻接构建、连通分量缓存 | 所有本地方法的共同 I/O，不是 ABHSS speedup |
| `src/common/query_io.{h,cpp}` | 查询路径解析、批量读取与格式拒绝 | 所有方法共用 |
| `src/common/query_feasibility.{h,cpp}` | 使用加载期分量索引判定查询是否有共同分量 | 算法 timer 内的共同入口检查 |
| `src/common/memory_usage.{h,cpp}` | Windows/Linux RSS 读取与逐查询采样 | 工程监控，不是第二个计算线程 |
| `src/common/output_manager.{h,cpp}` | 安全创建结果目录并追加 header/记录 | artifact 输出 |
| `src/pruneddp` | PrunedDP++ 论文路径的本仓库重建，支持 Safe、strict-pathmax 与实际 StateStore 项数 | Safe 是当前主性能 baseline；Hash 直接用容器 size，Dense 只数 present，不是原作者 2016 代码的 bit-for-bit 镜像 |
| `src/dpbf` | 稠密全子集 Dreyfus–Wagner/DPBF | 小图正确性 baseline，80M cell 安全上限 |
| `src/baselines/basic_plus.*` | PVLDB 2021 作者 header 的输入 adapter | 可选 correctness-only，$g\le14$ |
| `src/baselines/gpu4gst_pruneddp.*` | GPU4GST artifact 内 CPU PrunedDP++ header 的输入 adapter | 可选 artifact 核验，只接受非负整数边权，不在冻结性能矩阵 |

`basic_plus` 和 `gpu4gst_pruneddp_artifact` 只在所需 `third_party` header 已恢复时由 CMake 创建。SCIP-Jack 是独立外部二进制，由 Python runner 适配，不链接到本项目。

## 7. 测试与正确性门禁

| CTest | 覆盖范围 | 防止的回归 |
|---|---|---|
| `fast_graph_io_structure` | 零/小数/科学计数边权、原边和邻接顺序、自环双邻接项、连通分量、错误 token | 快速读取改变图语义或静默接受损坏输入 |
| `query_io_validation` | 合法多查询，以及负查询/组计数、空组、截断 payload 和声明查询后的多余 token | 批处理文件错位或静默截断 |
| `abhss_zero_weight_witness` | 历史零权父指针环反例 | witness 重根不终止或误计上界 |
| `abhss_configuration_exactness` | 144 个确定性随机连通小图，$2\le g\le10$，三个合法配置对照独立全子集 DP；同时断言 `ConfigurationProfile` 的新增位/四类 realization、未知增强位/adjoint-only 拒绝，以及 $0\le g\le16$ 每个必需层恰由 A 或 H 覆盖一次 | 配置重构丢解、重新引入经验组数分派、“新增/替换”契约漂移、非法配置漏入、零权错误或 epsilon 误闭合 |
| `mask_vertex_state_accounting` | 七点路径上 ABHSS Base/Enhanced 重复计数，以及 PrunedDP++ Hash/Dense 计数一致性和平凡查询零计数 | 状态数不稳定、A1 所有权交接后重复计数、误把 Dense 容量或辅助预处理当实际状态 |

本地 CTest 是每次改码必跑的快速门禁，不替代 `S1_steinlib_exactness_gate`。后者在 $11\le g\le16$ 的已知最优实例上同时比对 ABHSS、PrunedDP++-Safe、DPBF 以及已恢复的外部 correctness 方法。

## 8. 实验工具链

| 路径 | 职责 |
|---|---|
| `tools/data/build_published_workloads.py` | 从 MonoGST+/GPU4GST 作者输入生成 P1 接口与身份 manifest |
| `tools/data/build_gpu_query_panels.py` | 对已生成的 300 条 P2 候选按输入组大小分层固定 5 条 |
| `tools/data/generate_controlled_queries.py` | 生成 DBLP/IMDb 的 $\langle g,f\rangle$ panel 并写实现后组大小 |
| `tools/data/build_query_feasibility_audit.py` | 重用图分量扫描并将每个矩阵 case 的可行性与当前矩阵哈希绑定 |
| `tools/experiments/validate_environment.py` | 在运行前检查矩阵总数、方法配置、路径、哈希和可行性审计 |
| `tools/experiments/run_experiments.py` | 稳定分片、断点续跑、逐查询 timeout、图加载 watchdog、一任务一 JSON 记录，并解析行末状态数 |
| `tools/experiments/summarize_results.py` | 数据集/cell 汇总、PAR-2、共同完成时间/状态倍率、timeout 方向、目标值和可行性不一致 |
| `tools/experiments/plot_results.py` | 从冻结 supervisor JSON records 绘制 P2/S2 曲线，不重新挑选查询 |

一次正式运行不直接循环调用二进制，而是由 `run_experiments.py` 展开机器矩阵。runner 用 case/method/query 的稳定 key 分片，为每个任务写独立 JSON；同一 `run-dir` 下已完成 key 不会重跑。Linux 上保留 `PATH` 的大小写并为外部 solver 同步添加 `LD_LIBRARY_PATH`；Windows 上会合并大小写重复的 Path 环境项。

## 9. 修改算法时的最小安全流程

1. 先写明要保持的数学不变式：真实上界、可采纳下界、精确 row 还是 cutoff 证书。
2. 优先在公共 `Problem`/`Row`/future/交集函数中实现，不得恢复另一套 Base/Enhanced 数据结构。
3. 若是增强操作，从 `SolveOptions::Base()` 通过 `With` 增加，在 `DescribeConfiguration` 中明确登记为“安全新增”或“同职责替换”，并在 `IsValid` 中执行依赖检查；不得出现 Base 独有但 Enhanced 无同职责 realization 的阶段，也不得依据图名、$g$、当前速度或内存自动开关。
4. 为最小反例增加 CTest，然后运行 `make release`。修改剪枝、闭合、零权边或 adjoint 时，三种合法配置都必须对照独立 DP。
5. 运行 SteinLib 已知最优 gate；任何目标值/可行性不一致都先当正确性错误，不能用“浮点容差”直接解释。
6. 只在正确性门禁通过后跑旧/新性能 panel；保留每个 panel 的权重序列和超时方向，不仅比较总时间。

## 10. 当前明确边界

- ABHSS 只支持无向、有限非负边权与 $g\le16$。
- 当前公开二进制输出精确权值和 feasibility，不输出最优树边集。
- 代码对解析后 `double` 边权做组合精确搜索；上下界闭合使用原始顺序比较，但跨实现结果报告仍使用 $10^{-6}$ 核验容差。
- PrunedDP++-Safe 是对公开论文路径的纠错重建，不能在 artifact 中写成“2016 原作者原码”。GPU4GST 2025 artifact 的 CPU 版本是独立核验对象。
- MonoGST+ 论文与作者 workload 目前是本地来源，不应假设已获得公开再分发许可。这是 artifact 发布问题，不改变本地哈希冻结的实验身份。
