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

每条结果是 `seconds weight query_peak_rss_overhead_mib`。无解时 `weight=-1`，有解时为最优权值。当前 ABHSS 公开返回结构只含 `best_weight` 和 `feasible`；它在内部构造可行 witness 来证明上界，但没有序列化最终最优树边集。

`graph_load_seconds` 和 `query_load_seconds` 写在结果 header 与 `[Ready]` marker 中，不进入算法时间。`[Ready]` 之后，每条查询的 timer 包含可行性检查、查询预处理和搜索。1 ms RSS 采样线程只读内存统计，不参与搜索。

## 4. 一条 ABHSS 查询的调用主线

```text
main
  -> ParseAbhssOptions
  -> LoadGraphFromFolder / LoadQueriesFromFolder
  -> SolveOneQuery
       -> ResolveQueryPrelude
       -> PrepareProblem
       -> BuildEarlyAnchorRows          [Base only, g large enough]
       -> BuildOrdinaryWithProbe
            -> BuildOrdinaryRows        [shared D grid]
       -> RunForwardAnchoredStage       [Base / DirectedCutOnly]
            -> BuildForwardAnchoredRows [full A grid]
          or
          RunForwardAnchoredStage       [Enhanced low A grid]
            -> BuildForwardAnchoredRows
          -> SolveHighAdjoint            [Enhanced high H grid]
       -> SolveResult{best_weight, feasible, mask_vertex_states}
```

`src/main.cpp` 是二进制公共批处理入口。CMake 通过宏将同一入口编译为 `abhss`、`pruneddp`、`dpbf` 及可选的第三方 adapter，使图加载、查询分片、计时和输出格式一致。每个 `weights.txt` 查询行依次写 time、weight、query peak RSS 和 `mask_vertex_states`；正式 ABHSS/PrunedDP++ 返回非负计数，无统一口径的 adapter 写 `-1`。

`SolveOneQuery` 先拒绝非法开关，再处理空查询、$g>16$、无共同分量、单组等入口情形。进入 `Problem` 后，配置为只读值，所有后续差异只能通过 `UsesBoundedGroupDistances`、`UsesDirectedCut` 和 `UsesAdjointCompletion` 三个能力查询产生。

`PrepareProblem` 依次构建零权分量下界、组距离、真实上界、锚组映射、tour 下界和 witness。Base 使用 bounded 组距离与根路径 witness；开启 `DirectedCut` 时使用完整组距离、对偶势、primal/facility 上界和 dual witness。两者都生成同一 `Problem` 和同一 ordinary $D$ 状态。

`BuildOrdinaryRows` 按 mask 大小生成普通 $D$，将同根 split seed 做图闭包，并标准化 branch。Base 可从 `EarlyAnchor` 取锚感知下界；开启 `DirectedCut` 的配置从同一 `FutureBound` 接口增加对偶势。

Base 和 DirectedCutOnly 经 `RunForwardAnchoredStage` 调用 `BuildForwardAnchoredRows`，生成完整的低/高层锚定 $A$。Enhanced 仍调用同一内核生成预先固定的低层 $A$，再由 `SolveHighAdjoint` 以补集转置终端和递减 $H$ 代替未物化的高层 $A$。切分层只由 $g$ 决定，不读取数据集名或运行表现。

## 5. ABHSS 源文件导读

| 文件 | 主要职责 | 阅读时需要抓住的不变式 |
|---|---|---|
| `src/abhss/abhss.h` | 公开 `SolveOptions`、增强位、配置依赖和 `SolveResult` | `none -> directed-cut -> directed-cut+adjoint` 是唯一合法配置链 |
| `src/abhss/solver.cpp` | 单一 solver 入口与配置调度 | 只在这里选择完整前向或 adjoint 完成；不存在按查询 oracle |
| `src/abhss/pipeline.{h,cpp}` | 平凡/无解前置、预处理和 ordinary 的公共 probe 边界 | 诊断包装不改变算法语义 |
| `src/abhss/internal.h` | `Problem`、`Row`、`GroupRow`、witness、状态计数和热路枚举器的共同定义 | $D$、$A$、$H$ 共用一个有序稀疏 `Row`；每张 row 按首次进入工作区的顶点批量计数；`ready` 与空 payload 不能混淆 |
| `src/abhss/preprocess.cpp` | 零权 cover、组距离、多种真实上界、tour、witness、统一 future | cutoff 不得当作精确状态；`best` 只由真实可行子图收紧 |
| `src/abhss/core.{h,cpp}` | early-A1、ordinary $D$、row 交集、规范 branch、witness rent-or-buy | 只剪掉不可能严格改善 `best` 的状态；交集策略只改常数 |
| `src/abhss/forward.{h,cpp}` | 公共前向锚定 $A$ 递推与完整解结算 | 隐式 $A(0)$、early-A1 交接和新生成 row 都走同一完成函数 |
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
| `abhss_configuration_exactness` | 144 个确定性随机连通小图，$2\le g\le10$，三个合法配置对照独立全子集 DP；另含平凡、不可行、重叠组、$g>16$、非法开关和 sub-nanogap 闭合 | 配置重构丢解、零权边错误、用 epsilon 把正 gap 当作闭合 |
| `mask_vertex_state_accounting` | 七点路径上 ABHSS Base/Enhanced 重复计数，以及 PrunedDP++ Hash/Dense 计数一致性和平凡查询零计数 | 状态数不稳定、重复计算 early row、误把 Dense 容量或辅助预处理当实际状态 |

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
3. 若是增强操作，从 `SolveOptions::Base()` 通过 `With` 增加，在 `IsValid` 中声明依赖；不得依据图名、$g$、当前速度或内存自动开关。
4. 为最小反例增加 CTest，然后运行 `make release`。修改剪枝、闭合、零权边或 adjoint 时，三种合法配置都必须对照独立 DP。
5. 运行 SteinLib 已知最优 gate；任何目标值/可行性不一致都先当正确性错误，不能用“浮点容差”直接解释。
6. 只在正确性门禁通过后跑旧/新性能 panel；保留每个 panel 的权重序列和超时方向，不仅比较总时间。

## 10. 当前明确边界

- ABHSS 只支持无向、有限非负边权与 $g\le16$。
- 当前公开二进制输出精确权值和 feasibility，不输出最优树边集。
- 代码对解析后 `double` 边权做组合精确搜索；上下界闭合使用原始顺序比较，但跨实现结果报告仍使用 $10^{-6}$ 核验容差。
- PrunedDP++-Safe 是对公开论文路径的纠错重建，不能在 artifact 中写成“2016 原作者原码”。GPU4GST 2025 artifact 的 CPU 版本是独立核验对象。
- MonoGST+ 论文与作者 workload 目前是本地来源，不应假设已获得公开再分发许可。这是 artifact 发布问题，不改变本地哈希冻结的实验身份。
