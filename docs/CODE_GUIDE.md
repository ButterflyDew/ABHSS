# ABHSS 代码入口与审阅指南

本文档面向第一次阅读本分支代码的人，说明怎样编译、怎样运行、一条查询经过哪些函数，以及 Base 与 Enhanced 究竟在哪些代码位置不同。数学定义、正确性证明和复杂度见 [`METHOD.md`](METHOD.md)。

## 1. 分支边界

本分支刻意只保留最终论文方法，不包含 baseline、中间消融态、实验调度器、探针、运行时配置账本或输入合法性测试框架。公开 API 只有：

```cpp
SolveResult SolveOneQuery(const Graph& graph, const Query& query, bool enhanced = false);
```

`enhanced=false` 是 Base，`enhanced=true` 是 Enhanced。这个布尔值在一条查询开始前确定，搜索过程中不会根据图名、组数、状态密度、时间或内存切换模式。

建议按以下顺序阅读：

```text
src/main.cpp
  -> src/abhss/abhss.h
  -> src/abhss/solver.cpp
  -> src/abhss/internal.h
  -> src/abhss/preprocess.cpp
  -> src/abhss/core.{h,cpp}
  -> src/abhss/forward.{h,cpp}
  -> src/abhss/adjoint.{h,cpp}
  -> src/abhss/dual_cut.h
```

## 2. 编译、运行与接口

Linux 推荐入口：

```bash
make release JOBS=16
```

直接使用 CMake：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 16
```

程序调用格式：

```bash
./build/abhss <graph_folder> <query_file> <base|enhanced> [first_query] [query_count]
```

`graph_folder` 下固定读取小写 `graph.txt`，`query_file` 是显式查询文件。图和全部查询只加载一次，单条查询计时从 `SolveOneQuery` 调用前开始。输出每行依次为：

```text
query_index seconds best_weight mask_vertex_states
```

程序按竞赛代码约定信任正式输入满足 README 的格式和取值范围。保留的入口判断只有参数个数、最终模式名和查询区间；算法内部的 `assert(g <= 16)` 表达方法适用域，不是另一套配置合法性系统。

## 3. 一条查询的完整调用线

```text
main
  LoadGraph
    BuildConnectedComponentIndex
  LoadQueries
  for selected query
    SolveOneQuery(graph, query, enhanced)
      SolveTrivialQuery
      Problem(graph, query, enhanced)
      PrepareProblem
        ComputeComponentCover
        BuildDistanceRootInitialization
          bounded representation     [Base]
          complete representation    [Enhanced]
          RootStarUpper               [共同]
        BuildRootPathUnion            [共同]
        choose anchor + build masks   [共同]
        TourLowerBound::Build         [共同]
        BuildRootPathWitness          [Base witness]
        DualCutPotential + facility   [Enhanced certificates]
        BuildDualWitness              [Enhanced witness]
      WitnessUpperScheduler           [共同，rent=0]
      BuildCommonA1
        BuildReusableAnchoredSingletonLayer
      BuildOrdinaryRows
      ReleaseLookupCache
      FinishBase
        BuildForwardAnchoredRows through all required layers
      or FinishEnhanced
        BuildForwardAnchoredRows through the low prefix
        SolveHighAdjoint for the remaining high suffix
      return SolveResult
```

其中 `BuildCommonA1` 只判断由递推推导出的最高逻辑层是否为 0。它不读取 `enhanced`，没有经验性的组数阈值。只要 A1 层存在，Base 与 Enhanced 都调用同一个构造函数、同一个 witness 调度器和同一种所有权移交。

## 4. 核心数据对象

### 4.1 `Graph` 与 `Query`

`Graph::edges` 按输入顺序保存无向原边，`edge_id` 永远等于原边下标；`Graph::adj` 保存双向邻接项。读图时先统计每个顶点的精确度数，再一次预留邻接容量，最后构造 `component_of`。这避免大图逐边扩容，也让每条查询的可行性判断不再扫描整图。

`Query::groups[i]` 是第 `i` 个组的候选顶点。组可以重叠；一个顶点可同时覆盖多个组。

### 4.2 `Problem`

`Problem` 是一条查询的全部上下文，只保存输入引用、一个 `enhanced` 位和算法状态。重要字段分为：

- 图与组编号：`g`、`anchor_group`、`bit_to_group`、`original_mask`；
- 状态域：`nonanchor_count`、`subset_count`、`full_mask`、`half`；
- 上下界：`best`、`component_cover`、`tour`、`dual`；
- 上界 witness：`root_path_union`、`witness_tree`；
- DP：`ordinary`、`ordinary_minimum`；
- 报告量：`mask_vertex_states`。

构造函数不做预处理。所有字段的建立顺序集中在 `PrepareProblem`，审阅时不需要追踪配置对象或延迟合法性映射。

### 4.3 `Row`

ordinary D、前向 A 和反向 H 共用唯一物理格式：

```text
vertex[]       严格递增的顶点
value[]        与 vertex 对齐的精确状态值
branch_bits[]  ordinary D 专用的规范 branch 位图
branch_count   branch 数量
ready          已计算标志；与 payload 是否为空无关
```

`ready=false` 表示依赖尚未生成；`ready=true` 且数组为空表示该逻辑 row 已完整处理，但严格上界锥体中没有候选。三种状态不各自维护 dense/hash 容器。

### 4.4 `GroupRow`

`GroupRow` 保存一个查询组到全图的多源最短距离。Base 使用安全 cutoff 下的有界表示，Enhanced 使用完整 dense 表。有界表示可以是 dense cutoff 数组，也可以是“递增顶点和值 + membership/rank 位图”。所有布局只暴露三种共同操作：读取值、`IsExact`、枚举精确值。

cutoff 外的位置不是精确距离，不能作为 singleton seed。所有需要真实子树值的交集都通过 `IsExact` 或 `ForEachExact` 进入；作为下界读取时，cutoff 值仍然安全。

### 4.5 `WitnessTree` 与 `AnchoredSingletonFuture`

`WitnessTree` 只存局部顶点、父亲和父边权。Base 与 Enhanced 的树来源不同，但随后交给同一个 `WitnessUpperScheduler` 和同一个 `EvaluateWitnessTree`。

`AnchoredSingletonFuture` 同时持有标准 A1 row 与 ordinary 阶段需要的只读 top-two 索引。ordinary 结束后只释放索引缓存，随后把原 A1 row 移动到前向 A 容器；不会重新生成，也不会重复计数。

## 5. 逐文件与逐函数导读

### 5.1 `src/main.cpp`

`main` 只做四件事：解析最终模式和查询区间、加载输入、逐条调用求解器、打印结果。没有结果目录管理、RSS 线程、baseline 分派或诊断开关，因此计时边界可以直接从函数看出。

### 5.2 `src/common/`

| 函数或类型 | 作用 |
|---|---|
| `FastNumericReader` | 以 8 MiB 缓冲无分配扫描整数和浮点 token；`ReadDouble` 使用 C++17 `from_chars` |
| `BuildConnectedComponentIndex` | 一次遍历邻接表，建立稠密分量编号 |
| `LoadGraph` | 两遍构造图：保存原边和度数，再按精确容量建立双向邻接表 |
| `LoadQueries` | 从显式路径一次读取全部查询 |
| `GroupComponents` | 把一个组映射为去重、递增的分量集合 |
| `IsQueryFeasible` | 在非连通图上逐组求分量集合交；连通图直接返回 |
| `fp::Eq` | 只用于从浮点距离等式恢复真实路径；上下界闭合不用容差 |

### 5.3 `src/abhss/solver.cpp`

| 函数 | 作用 |
|---|---|
| `SolveTrivialQuery` | 处理空查询、单组查询和不存在共同可行分量的查询 |
| `BuildCommonA1` | 逻辑 A 正层存在时调用共同 A1；否则返回空 future 指针 |
| `FinishBase` | 把 A1 移入共同前向内核，并把前向 A 做到最高必需层 |
| `FinishEnhanced` | 用同一前向内核做到固定低层边界，再调用 H 完成高层 |
| `SolveOneQuery` | 唯一公开求解主线；公共阶段只写一次，末尾选择完成方式 |

最高锚定层直接由 `max(0, g / 2 - 1)` 得出。Enhanced 的低层边界在最高层为 0 时取 0，否则取 `max(1, highest_layer / 2)`。这两个值来自状态分解域，不是性能启发式。

### 5.4 `src/abhss/preprocess.cpp`

| 函数 | 作用 |
|---|---|
| `ComputeComponentCover` | 压缩零权连通分量并在组 mask 上做 set-cover DP，产生零代价判定与下界 |
| `BootstrapBoundedDistanceUpper` | 在统一距离—根职责内部，为有界距离构造真实 cutoff |
| `BuildGroupDistances` | 对每组运行多源 Dijkstra，形成有界或完整 `GroupRow` |
| `RootStarUpper` | 在共同根连接全部组；严格小于 cutoff 的判断阻止非精确占位更新上界 |
| `BuildDistanceRootInitialization` | 屏蔽两种距离表示差异，一次返回距离表、根和上界 |
| `BuildRootPathUnion` | 沿真实最短路边连接各组，按原 `edge_id` 去重计价 |
| `BuildWitnessFromEdges` | 把一组真实原边严格重根为无父指针环的树 |
| `BuildRootPathWitness` | 把共同路径并集转成 Base witness |
| `BuildDualWitness` | 把 Enhanced primal bitmap 转成相同格式的 witness |
| `TourLowerBound::Build` | 在组间松弛度量上预计算固定端点 Hamilton 路径 |
| `TourLowerBound::At` | 把当前顶点接到路径两端，返回可采纳 tour 下界 |
| `EvaluateWitnessTree` | 在真实 witness 上执行唯一一份 subset DP 上界求值 |
| `BuildPrimalFacilityUpper` | 在 Enhanced primal facilities 上构造支撑度量并做小型 DP 上界 |
| `FarthestRemaining` | 返回当前顶点到剩余组的最大单组距离 |
| `FutureBound` | Base 取 farthest/tour 最大值；Enhanced 再与 dual 取最大值 |
| `PrepareProblem` | 按固定顺序建立上述对象，并只构造当前模式的 witness |

`PrepareProblem` 是最长的预处理编排函数，按代码块依次阅读：零权闭合；距离—根合同；真实路径并集；锚组与 mask；farthest 缓存；tour；witness 分支；ordinary 容器初始化。只有 witness 块按模式分支，其余控制流共同。

### 5.5 `src/abhss/core.{h,cpp}`

| 函数或类型 | 作用 |
|---|---|
| `EstimateWitnessTreeDpWork` | 按 witness 顶点数和非锚组数计算共同 buy |
| `WitnessUpperScheduler::Account` | 让 A1 与 D 连续支付实际 queue/edge 工作，达到 buy 后调用树 DP |
| `AnchoredSingletonFuture::ValueWithLocator` | 二分读取精确 A1，或返回 cone 外正下界并编码 locator |
| `AnchoredSingletonFuture::Future` | 每顶点缓存最大、次大 singleton future，常见查询 O(1) |
| `BuildReusableAnchoredSingletonLayer` | 在 ordinary 前构造两种模式完全共用的 A1 |
| `ForEachRowBranchIntersection` | 在双指针与较小侧二分之间按估计工作量选择同一交集结果 |
| `ForEachCommonValue` | 统一 singleton 和多组 ordinary 的同顶点交集 |
| `ForEachPivotBranch` | 只让一个互补块走规范 branch，消除重复拆分 |
| `ForEachTriple` | 用三个 ordinary 块加锚组及时结算完整上界 |
| `BuildOrdinaryRows` | 按 mask 大小生成 ordinary D、做图闭包、标 branch、更新上界和 witness rent |

`BuildReusableAnchoredSingletonLayer` 的块顺序是：固定本轮 cutoff；逐 singleton 初始化精确 seed；以 farthest continuation 做 A* 闭包；在安全点向共同 scheduler 支付 rent；若树 DP 收紧上界则整轮重启；最终发布所有 A1 row；建立 top-two locator 缓存。函数体中不存在 `p.enhanced`。

`BuildOrdinaryRows` 的块顺序是：枚举规范 split；分阶段计算 dual、farthest、A1、tour 下界；多源闭包；写回有序 row 与 branch bitmap；登记状态；支付 witness rent；执行两块和三块完整完成式；清理 touched 工作区。

### 5.6 `src/abhss/forward.{h,cpp}`

| 函数或类型 | 作用 |
|---|---|
| `ForwardAnchoredPlan` | 只表达前向内核要做到哪一层、是否用隐式 A0 完成、末层是否保留 |
| `AnchoredAvailable` | 判断隐式 A0 或物化 A row 是否可读 |
| `ForEachAnchoredSum` | 枚举 A 与 ordinary branch 的同根 seed |
| `CompleteAnchoredRow` | 用 A 加至多两个 ordinary 块结算完整解 |
| `BuildForwardAnchoredRows` | 复用已有 A1，生成其余前向 A，并持续收紧上界 |

这里不读取 `p.enhanced`。公共代码直接查看 `GroupRow::bounded`：有界 singleton 必须检查 `IsExact`，完整表天然精确。A 的递推、闭包、稀疏 row 和完成式始终是同一实现。

### 5.7 `src/abhss/dual_cut.h`

`DualCutPotential` 只在 Enhanced 预处理中构造。`BuildChangedArcs` 按组建立有向割势和 residual；`MarkChangedArc` 记录本轮真正改变的弧；`RecoverPrimal` 在数值零 residual 支撑上恢复真实原图路径；`At` 与 `GroupAt` 读取可采纳势；`ReleaseResidual` 在 facility 与 witness 完成后释放 `2m` 临时数组。

该文件是 header-only 模块，因为热访问器需要内联。每个 lambda 都有紧邻中文注释；64-bit changed-arc 位图只是串行批处理结构，不表示 64 线程。

### 5.8 `src/abhss/adjoint.{h,cpp}`

| 函数 | 作用 |
|---|---|
| `AnchoredValue` | 统一读取低层 A；空 mask 映射为隐式锚组距离 |
| `ForEachBackwardBranchSum` | 合并 H 与一个 ordinary singleton 或规范 branch |
| `BuildTransposedTerminals` | 以 64 顶点块把 mask-major ordinary row 转成高层 H 终端 |
| `SolveHighAdjoint` | 按 mask 大小递减构造 H，并与低层 A 做边界结算 |

64 顶点块与一个 `uint64_t` membership word 对齐，只限制临时工作集；不是并行单元。H 的含义不是另一棵树 DP，而是“mask 外侧已经支付的代价”。

## 6. Base 与 Enhanced 的代码级差异

整个活动源码中，模式位只在下列位置产生算法效果：

| 位置 | Base | Enhanced | 关系 |
|---|---|---|---|
| `BuildDistanceRootInitialization` | 先构造 cutoff，保存有界组距离 | 保存完整组距离 | 同一距离—根职责的两种表示 |
| `PrepareProblem` witness 块 | root-path witness | dual/primal、facility 上界、dual-primal witness | witness 来源替换，并安全新增 dual/facility 证书 |
| `FutureBound` 与 ordinary `CanImprove` | farthest、A1、tour | 再与 dual 取最大值 | 可采纳下界安全新增 |
| `SolveOneQuery` 末尾 | 完整前向高层 A | 低层 A 后以 H 完成高层 | 同一完成职责的方向替换 |

有界或完整表示的后果由公共 `GroupRow::bounded`、`IsExact` 和 `ForEachExact` 消费，不再读取模式位。以下操作也明确没有模式分支：A1 的 seed、cone、fallback、图闭包和所有权移交；ordinary D 的状态定义、split 和 branch；witness rent 从 0 开始；buy 公式；`EvaluateWitnessTree`；稀疏 `Row`；前向 A 内核。审阅者可以直接搜索 `enhanced`，除入口解析和字段传递外，有算法效果的命中应只对应上表四行。

## 7. 状态数口径

`mask_vertex_states` 统计 D、A、H 每张逻辑 row 中首次进入工作区的不同顶点数。一个顶点在同一 row 内被改进多次只计一次；同一 `(mask,v)` 出现在不同状态族时分别计数。A1 在提前构造时计数，移动到前向容器后不重复。组距离、dual、tour、转置临时候选、队列过期项和完整解结算不计。

## 8. 人工 review 清单

1. 搜索 `enhanced`，确认模式差异没有越出第 6 节。
2. 搜索所有 lambda，确认紧邻中文注释说明枚举集合或缓存语义。
3. 检查写 `best` 的位置，确认候选可展开为真实原图边或精确 DP 状态。
4. 检查 singleton 消费者，确认 Base 的 bounded 位置经过 `IsExact`。
5. 检查所有剪枝，确认形式是“已付值 + 可采纳下界不可能严格优于真实上界”。
6. 检查 row 生命周期，确认构造完成后才设 `ready`，A1 移交不重复生成或计数。
7. 检查 H 的 mask 方向，确认 target、successor、boundary 三者的集合并恰好覆盖非锚全集。
8. 检查长函数的中文块注释是否仍与紧随代码一致；若移动代码块，应同时移动说明。

## 9. GitHub Markdown 注意

后续修改本文档或 `METHOD.md` 时，块公式统一使用 GitHub 支持的 `math` 围栏，不使用单独三行的 `$$`。不要使用 GitHub 曾拒绝的 `\operatorname`，用 `\mathrm{name}`；表格中的绝对值使用 `\lvert S\rvert`，避免裸竖线被当成列分隔符。行内公式开界前留空格，尤其不要写成中文标点后立即接 `$`。上传后应在 GitHub 网页逐段检查，确认没有黄色错误框、灰色公式源码回退或未解析的 `$`。
