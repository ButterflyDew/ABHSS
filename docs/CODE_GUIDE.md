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
            -> ComputeComponentCover       [zero-cost cover may close here]
            -> if g<=3: common BuildDistanceRootInitialization
                 BootstrappedBounded root-star package, then exact return
            -> otherwise: BuildDistanceRootInitialization [exactly once]
                 frozen profile selects BootstrappedBounded or CompletePotential
                 both return {group_distance, root, upper}
            -> root paths / tour / selected witness [only for open g>3 queries]
            -> BuildTripleSeededPathGrowthUpper [same real-edge upper bound for all profiles]
       -> DescribeConfiguration          [fixed add-or-replace profile]
       -> MakeAnchoredCompletionSchedule [derive logical A/H layer boundary]
       -> WitnessUpperScheduler          [both profiles start with rent = 0]
            buy = the same formula applied to this profile's witness size
       -> common A1 row                  [only when the logical grid contains A1]
            all configurations: same seed, farthest + endpoint-floor cone, positive fallback,
                                 top-two view and forward-A ownership transfer
            the A1 builder does not read enhancement flags or dual potentials
            queue/edge work pays rent; a tighter tree-DP buy restarts the A1 pass
       -> BuildOrdinaryWithProbe
            all configurations: farthest + tour + common A1 future
            DirectedCut/Enhanced: additionally max with dual potential
            -> BuildOrdinaryRows         [shared recurrence; materialize through schedule.ordinary_last_layer]
       -> RunForwardAnchoredStage        [Base / DirectedCutOnly: full A]
          or
          RunForwardAnchoredStage        [Enhanced with a nonempty H suffix: low A]
            -> SolveHighAdjoint           [H replaces high A; a two-bin capacity obstruction adds three-block terminals]
       -> SolveResult{best_weight, feasible, mask_vertex_states}
```

`src/main.cpp` 是二进制公共批处理入口。CMake 通过宏将同一入口编译为 `abhss`、`pruneddp`、`dpbf` 及可选的第三方 adapter，使图加载、查询分片、计时和输出格式一致。每个 `weights.txt` 查询行依次写 time、weight、query peak RSS 和 `mask_vertex_states`；正式 ABHSS/PrunedDP++ 返回非负计数，无统一口径的 adapter 写 `-1`。

`SolveOneQuery` 先通过 `IsValid`（内部读取 `DescribeConfiguration`）拒绝非法开关，再处理空查询、 $g>16$、无共同分量、单组等入口情形。非平凡查询建立 `Problem` 并完成预处理后，才保存本次执行所需的 `ConfigurationProfile` 和状态层计划。`Problem` 为避免改变热对象布局，仍只读保存原来的冻结 bit mask；`UsesBoundedGroupDistances`、`UsesDirectedCut` 和 `UsesAdjointCompletion` 与 profile 映射由同一配置回归共同约束。代码与论文共用这张“新增或替换”契约，不能再把位掩码单调误写成逐指令包含。

`PrepareProblem` 依次构建零权分量下界、距离—根初始化，再决定是否需要真实路径并集、锚组、tour 下界和当前配置自己的 witness。正权图的 `ComputeComponentCover` 直接使用加载期最小边权，只聚合查询触及的至多 $F$ 个单点分量；确实含零权边的图才扫描原边并建立并查集。对没有被零代价分量条件提前闭合的可行查询，外层始终只调用一次 `BuildDistanceRootInitialization`，并统一消费 `DistanceRootInitialization{group_distance, root, upper}`。 $g\le3$ 时全部配置统一选择 BootstrappedBounded root-star 基例并立即返回； $g>3$ 时才由冻结 profile 在 BootstrappedBounded 与 CompletePotential 之间实现同职责替换。前者在 realization 内用规范 SPT 边并集尝试启动 cutoff，再构造 bounded `GroupRow`；若非连通图的规范终端没有共同分量，bootstrap 可暂时为无穷，此时多源距离不截断，随后的共同 root-star 扫描仍会在已验证存在的公共分量中取得有限上界。后者构造完整距离势。两者都返回同一三元合同。最远组 oracle 只保存一个 byte/vertex 的全局 argmax：构造时用局部最大值做一次线性扫描，命中时 $O(1)$ 返回，未命中时严格扫描剩余 mask；完整势直接读取 dense payload，bounded 势仍走 `GroupRow` 合同。没有固定深度 top-k、查询统计或配置专属数学值。

当 $g\le3$ 时，任意三终端树在分叉点处分解可证明最优值恰为 $\min_v\sum_i d_i(v)$。BootstrappedBounded 的 cutoff 若严格大于最优值，则最优根的所有组距离都已精确保留；若等于最优值，则真实 cutoff 本身已经闭合。因此三个合法配置在进入任何 enhancement realization 前逐项执行同一个 bounded root-star 包并直接返回，不构造 complete potential、witness、dual、tour 或主状态。这是共同数学基例，不是按组数选择 Base/Enhanced。仅当 $g>3$ 仍未闭合时，公共外层才构造 root-path-union。规范 SPT 是 bounded 物理表示的内部 bootstrap，不是 Base-only 的外层调用阶段。Base 把共同边并集整理为 root-path witness；开启 `DirectedCut` 时，以 primal upper 与 dual-primal witness 实现相同的真实 witness 职责，facility 上界另作安全新增。随后所有配置调用同一个 `BuildTripleSeededPathGrowthUpper`：前两组建立种子路径，显式枚举第三组后再以真实 tight shortest paths 保持连通地接入最近未覆盖组，按 edge ID 去重计费，并用非负边权下费用单调不减的性质在不能严格改善 incumbent 时安全停止。实现对每个有序前两组只恢复一次种子路径，再把完全相同的真实种子边重放给各个第三组；`membership[v]` 同时承担路径终点组判断，避免每次恢复重复写入和清除一张全图 terminal 数组。恢复下一条路径前还用“已付真实树费用 + 当前树到目标组的最短距离”检查严格改善的必要条件；失败时跳过的 DFS 不可能产生更优候选。这些操作只消除重复恢复、组扫描与工作区，不改变任何可能严格改善 incumbent 的有序组三元组候选。当前 witness 已与公共 component-cover 下界闭合时，两种配置还会以同一精确条件在调用三元上界前返回；未闭合查询的图规模工作区在主状态搜索前释放。预处理到此为止：两边都不在这里无条件调用 `EvaluateWitnessTree`。

预处理返回后，`SolveOneQuery` 才构造唯一的 `WitnessUpperScheduler`，所以 Base、DirectedCutOnly 与 Enhanced 的 `rent` 都严格从 0 开始。初始阶段只把各自 witness 的真实顶点数代入同一个 `buy` 公式；公共 A1 与 ordinary $D$ 的 queue-pop/edge-relax 工作连续支付 rent，达到阈值且树 DP 有新输入时调用同一个 `EvaluateWitnessTree`。若 A1 中的购买真正收紧上界，A1 会以新的固定 cutoff 整轮重启；未收紧时继续当前轮。只有开启 DirectedCut 的配置可能在 ordinary 中另行购买 residual closure；购买后完成 residual 势并恢复真实 primal/facility 上界，若四元真实路径生长继续严格收紧上界，则把路径边与 primal 边合成 certificate support，调用 `RefreshCertificate` 按 support 顶点数切换确定性 buy 与 `EvaluateCertificateSupport`。路径上界已经直接写入 `best`，不再重建一棵后续无人消费的 witness 树。完整势和真实上界随后共同重滤已经物化的 D；删除条件仍是“精确 rooted 值 + 可采纳 future 不小于现有真实上界”。这是 closure 购买后的单调证书刷新，不改变初始 witness 的共同调度规则。

`MakeAnchoredCompletionSchedule` 从平衡证明得到完整锚定格的正层域 $\mathcal L_A=\{1,\ldots,q\}$，其中 $q=\max\{0,\lfloor g/2\rfloor-1\}$。代码只判断某个逻辑层是否属于该域，不含 `g >= 常数` 一类经验分段。 $g\le3$ 查询在进入层计划前已经由全部配置共同的精确恒等式闭包。对其余查询，域为空时完成式直接使用隐式 $A(\varnothing)$；域非空时 A1 是第一个成员，所有配置一律在 ordinary 前生成它。Enhanced 的前向边界为 $q=0$ 时 $\ell=0$，否则 $\ell=\max\{1,\lfloor q/2\rfloor\}$，所以 A1 总在前向前缀。该 row 形成 `AnchoredSingletonFuture`，在 ordinary 后按所有权移交给公共前向内核，既不重复闭包也不重复计数。

令 $k=g-1$， $h=\lfloor g/2\rfloor$。没有 H 后缀时 ordinary 完整物化到半格 $h$；存在 H 后缀时，最高逻辑锚定层 $q=h-1$ 本身仍有空前缀边界消费者，所以 Enhanced 固定物化到：

```math
r=\max\left\{q,\left\lceil\frac{k-(\ell+1)}{2}\right\rceil\right\}.
```

当前定义域中该式保留最高逻辑 ordinary 层，只省略半格 $h$。因此 H successor、所有低层 A 边界以及 $A(\varnothing)+D(q)+H(q)$ 都直接读取同一张完整 $D$ row，不存在另一个延迟 D realization。令最大转置外侧组数为 $c_{\max}=k-(\ell+1)$。组加权分隔点后的每个组件至多含 $r$ 个组；容量为 $r$ 的两个箱子最小不可装入整数分拆的总量为：

```math
\tau(r)=\left\lfloor\frac{3r}{2}\right\rfloor+2.
```

只有 $c_{\max}\ge\tau(r)$ 时，层计划才设置 `requires_three_block_terminal`，让 `SolveHighAdjoint` 补充第三个 rooted 块；否则单块和双块对整个逻辑定义域已经完备。这个开关来自两箱装箱证明，不比较图名、实际 $g$ 常数、row 密度、incumbent 或运行时间。关闭时实现不分配 `pair_best`、不扫描最大 entry 大小，也不在 pair 热循环调用空三块操作。

`BuildOrdinaryRows` 按 mask 大小生成普通 $D$，将同根 split seed 做图闭包，并标准化 branch；递推代码只有一份，调用者只传入计划推出的最高物化层。size 2 的唯一规范拆分保持单遍；更高层先把全部规范拆分聚合成精确的逐顶点最小 seed，再对每个顶点运行一次与拆分无关的统一 future。Base 的 flat cache 在首次存活候选后保存完整 farthest/A1/tour 最大值；开启 `DirectedCut` 后，同一个 `CanImprove` 使用 staged realization，打包的 `bound_state` 以高位保存 row epoch、低 3 bit 表示 dual、farthest、公共 A1、tour 已计算到哪一步，另用 2 bit 保存 `rejected-seen` 与 `rejected-frontier`。第 4 个低位只在 `GST_ENABLE_PROBE_DIAGNOSTICS` 构建中记录 exact-dual 命中，论文二进制把该写入完全编译掉；epoch 仍统一左移 6 bit，使两类构建采用相同布局。较小标签可从尚未完成的阶段继续，不能把先前候选的拒绝永久化。dual 的 certified interval 下端只能作为可采纳前缀；upper endpoint 放行后，任何以后还能越过后续缓存或真实 `distance` 的标签必然更小，因而也必然放行，所以可结束 dual 判定；未转置逐组和或区间歧义触发的逐组 exact fallback 则返回可直接复用的完整值，即使当前标签被拒绝也可把 dual 阶段标为完成。这个分流只读取预声明的 DirectedCut 位，因为没有 dual 的 Base 不存在可复用的第一阶段中途拒绝；它不读取图名、 $g$、row 密度或计时，也不改变 `double` 求和。staged 路径还在同一个 32-bit `bound_state` 中保存 `rejected-seen` 与 `rejected-frontier`。仓库限制 `g <= 16`，固定锚组后 ordinary mask 少于 `2^15`；epoch 左移 6 bit 后仍小于 `2^21`，所以高位 epoch 与低 6 bit 元数据在一条查询内不会重叠或回绕。单次缓存拒绝只登记 seen；一张 row 已观察到复用后，才把虚拟拒绝值写入既有 `distance` 并加入 `rejected` 清理表。stage 0 首次物化时从 `best - bound_cache[vertex]` 得到解析 cutoff，再用 `nextafter` 修正到原 `double` 谓词确实拒绝的首个可表示值；stage 1--4 保存实际拒绝值。虚拟值不入堆、不计状态，较小标签通过完整证书链时由 crossing 分支先加入 `touched` 并清除前沿位。删除任一环节都要重新证明 row 结束复位、状态计数与候选支配安全。通过完整 future 的初始标签用 `QueueNode` 的确定性全序线性 heapify；`CanImprove` 成功时已经把完整证书写入当前 epoch 的 cache，所以 heap key 直接读取该 cache，不再经过第二个只会命中的 accessor。图松弛仍执行相同精确闭包。所有配置都读取共同 A1 future；开启 `DirectedCut` 后，统一 future 栈在 A1 之外再与对偶势取最大，而不是替换、关闭或修改 A1。`BuildReusableAnchoredSingletonLayer` 不读取配置位或 dual，三个配置使用相同的 farthest + endpoint-floor cone 与正 fallback。DirectedCutOnly 与 Enhanced 随后都把已经生成的 A1 交给同一 `BuildForwardAnchoredRows` 内核； $H$ 只负责 A1 之后的高层后缀。补集转置按组加权分隔点定理枚举一块或两块；仅当 `requires_three_block_terminal` 的容量证明为真时再枚举第三块，补全未物化半格的 terminal。最高逻辑 ordinary 层已经完整物化，所以所有 A/H 边界直接读取公共 $D$ row。`complete_implicit_anchor` 仅表示完整正层域为空。

Base 和 DirectedCutOnly 经 `RunForwardAnchoredStage` 调用 `BuildForwardAnchoredRows`，生成完整的低/高层锚定 $A$。Enhanced 在 H 后缀非空时仍调用同一内核生成由平衡完成域确定的低层 $A$，再由 `SolveHighAdjoint` 以补集转置终端和递减 $H$ 代替未物化的高层 $A$；若 H 后缀为空，则直接复用完整前向入口，末层只消费而不保留，也不构造任何转置工作区。ordinary 截止保留最高逻辑层并只省略半格；切分、截止和是否需要第三块都来自递推消费者与容量证明，不读取数据集名或运行表现。

从论文和代码审计角度，配置关系必须按下表理解：

| 逻辑职责 | Base realization | Enhanced realization | 关系 |
|---|---|---|---|
| A1 与 ordinary A1 future | ordinary 前生成标准 A1，使用 farthest + endpoint-floor cone 与正 fallback，随后移交前向 A | 逐项执行同一 seed、cone、fallback、top-two 与移交；A1 内不读取 dual | 严格共同操作；不是替换，也没有增强专属分支 |
| $g>3$ 的距离—根初始化 | realization 内以真实 SPT 边并集启动 cutoff，构造 bounded `GroupRow`，再做共同根扫描 | 构造完整距离势 `GroupRow`，再做同一共同根扫描 | 外层只调用同一函数并接收 `{group_distance, root, upper}`；SPT/全距离扩展分别是两种表示的内部成本，不是 Base-only 阶段；低组基例在此之前共同闭包 |
| ordinary 的其他 future | flat realization：farthest、公共 A1、tour 的完整值首次存活后缓存 | staged realization：先增加 directed-cut，再依次复用 farthest、公共 A1、tour 的已算前缀 | 证书集合是安全新增；求值 realization 的共同输入、输出和严格拒绝职责相同，选择只由 DirectedCut 位决定 |
| witness realization 与条件式树 DP | root-path tree | primal upper + dual-primal tree | 对未被共同闭包的查询，树来源是同一真实 witness 职责的替换；两边预处理都只构造各自 witness，随后从 `rent=0` 进入同一调度器、同一 `buy` 公式和同一树 DP；共同的 root-star/root-path-union 仍由两边执行，facility 是额外安全上界 |
| A1 之后的高层锚定完成 | 完整 ordinary 半格依赖加前向高层 $A$ | 完整物化到最高逻辑层 $q$，仅省略半格 $h$；以单/双块及容量证明必要时的三块 separator terminal，加 A1/低层 $A$ 与高层 $H$ | 等价完成式的方向与半格 terminal realization 替换；A1 和最高逻辑 ordinary 都保留，不存在 Enhanced 少执行的独占职责 |
| 无 Base 对应物的工作 | 无 | directed-cut 可行证书、额外 facility 收紧、延迟 residual 证书刷新与单调 D 重滤 | 安全新增；只加强下界、真实上界或删去已不能严格改善的状态 |

`DirectedCutOnly` 采用表中的 DirectedCut 距离与 witness realization，增加 dual/facility 证书，但仍保留完整前向高层 $A$，只作为隔离 `AdjointCompletion` 的正确性/消融配置。表中不允许出现“Base 独有且 Enhanced 没有同职责替代物”的逻辑阶段。

## 5. ABHSS 源文件导读

| 文件 | 主要职责 | 阅读时需要抓住的不变式 |
|---|---|---|
| `src/abhss/abhss.h` | 公开 `SolveOptions`、增强位、`ConfigurationProfile`、`AnchoredCompletionSchedule` 和 `SolveResult` | 开关链只表达安全新增/同职责替换；层计划只表达平衡递推域及 A/H realization |
| `src/abhss/solver.cpp` | 单一 solver 入口与配置调度 | 只在这里选择完整前向或 adjoint 完成；不存在按查询 oracle |
| `src/abhss/pipeline.{h,cpp}` | 平凡/无解前置、预处理和 ordinary 的公共 probe 边界 | 诊断包装不改变算法语义 |
| `src/abhss/internal.h` | `Problem`、`Row`、`GroupRow`、witness、状态计数和热路枚举器的共同定义 | $D$、 $A$、 $H$ 共用一个有序稀疏 `Row`；每张 row 按首次进入工作区的顶点批量计数；`ready` 与空 payload 不能混淆；bounded `GroupRow` 的随机精确读取必须走 `ExactValueOrInf`，不得把 cutoff 当 singleton |
| `src/abhss/preprocess.cpp` | 正权 cover 快路径、零权 cover、组距离、 $g\le3$ 闭包、多种真实上界、tour、witness、统一 future | cutoff 不得当作精确状态；数学闭包必须对全部配置相同；`best` 只由真实可行子图收紧；tour 上包络只能跳过必被当前下界支配的求值，不能冒充新的下界 |
| `src/abhss/path_growth_upper.{h,cpp}` | 全部配置共同的有序组三元组一步前瞻真实路径生长上界 | 只恢复 `ExactValueOrInf` 有限位置的 tight paths；边 ID 去重；cutoff 只能作非负费用的单调安全终止 |
| `src/abhss/core.{h,cpp}` | A1 的 ordinary 前调度视图、ordinary $D$、逐顶点 split 聚合、flat/staged future realization、row 交集、规范 branch、共同 witness rent-or-buy | A1 构造不读取增强位或 dual；ordinary 的阶段缓存不能把候选专属拒绝永久化；Base 不支付无 dual 收益的 stage 热分支；树 DP 收紧上界时允许丢弃未完成的 A1 尝试并整轮重启，但最终只发布、移交和计数一份标准 `Row` |
| `src/abhss/forward.{h,cpp}` | 公共前向锚定 $A$ 递推与完整解结算 | 隐式 $A(0)$、提前 A1 的所有权交接和正常生成 row 都走同一完成函数 |
| `src/abhss/dual_cut.h` | `DirectedCut` 的 changed-arc 势、截断 potential cone、residual、exact fallback 与 primal 边恢复 | cone 只能跳过两端势都等于根 cap 的零梯度边；certified interval 不能冒充 exact；一次性构造保持非内联冷边界，避免 IPO 污染 Base 热布局；势只作下界，上界必须由原图真实边计价 |
| `src/abhss/adjoint.{h,cpp}` | ordinary 单/双及容量必要时的三块按顶点转置、高层 $H$ 递减、低层 $A$ 边界结算 | 第三块只由两箱容量反例条件启用；关闭时不支付三块准备工作；各组集合始终不交且并为全集 |
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
| `src/baselines/basic_plus.*` | PVLDB 2021 作者 header 的输入 adapter | 可选 correctness-only， $g\le14$ |
| `src/baselines/gpu4gst_pruneddp.*` | GPU4GST artifact 内 CPU PrunedDP++ header 的输入 adapter | 可选 artifact 核验，只接受非负整数边权，不在冻结性能矩阵 |

`basic_plus` 和 `gpu4gst_pruneddp_artifact` 只在所需 `third_party` header 已恢复时由 CMake 创建。SCIP-Jack 是独立外部二进制，由 Python runner 适配，不链接到本项目。

## 7. 测试与正确性门禁

| CTest | 覆盖范围 | 防止的回归 |
|---|---|---|
| `fast_graph_io_structure` | 零/小数/科学计数边权、原边和邻接顺序、自环双邻接项、连通分量、错误 token | 快速读取改变图语义或静默接受损坏输入 |
| `query_io_validation` | 合法多查询，以及负查询/组计数、空组、截断 payload 和声明查询后的多余 token | 批处理文件错位或静默截断 |
| `abhss_zero_weight_witness` | 用四个逻辑组保留历史零权父指针环反例的真实 witness 路径；另含 50,000 顶点逆序零权并查集链 | 低组闭包意外绕过 witness 回归，或递归 Find 在深链上爆栈 |
| `abhss_configuration_exactness` | 5,000 个 $2\le g\le10$ 确定性随机连通小图、500 个 $6\le g\le10$ 正权互异单终端实例和 160 个 $g=7..16$ omitted-half transpose 压力实例，逐例对照独立全子集 DP；另覆盖两箱容量边界的整数分拆穷举、三块 pair-union 等价性、 $g=2,3$ 零主状态共同闭包、逐弧势梯度与 residual、`DistanceRootInitialization`、`ExactValueOrInf`、非连通 fallback、配置合同、共同 witness `buy`、零起点调度、非法开关及 A/H/ordinary 层计划 | 配置重构或高层转置丢解、三块因子化漏候选、错误推广/配置化低组闭包、potential cone 漏边、重新暴露 Base-only SPT 调度、恢复 Base 预买、经验参数分派、“新增/替换”契约漂移、非法配置、零权错误或 epsilon 误闭合 |
| `mask_vertex_state_accounting` | 七点路径上 ABHSS Base/Enhanced 重复计数，以及 PrunedDP++ Hash/Dense 计数一致性和平凡查询零计数 | 状态数不稳定、A1 所有权交接后重复计数、误把 Dense 容量或辅助预处理当实际状态 |

本地 CTest 是每次改码必跑的快速门禁，不替代 `S1_steinlib_exactness_gate`。后者在 $11\le g\le16$ 的已知最优实例上同时比对 ABHSS、PrunedDP++-Safe、DPBF 以及已恢复的外部 correctness 方法。

## 8. 实验工具链

| 路径 | 职责 |
|---|---|
| `tools/data/build_published_workloads.py` | 从 MonoGST+/GPU4GST 作者输入生成 P1 接口与身份 manifest |
| `tools/data/build_gpu_query_panels.py` | 对已生成的 300 条 P2 候选按输入组大小分为五层；保留原 q1--q5，并从各层追加 q6--q10，共固定 10 条 |
| `tools/data/generate_controlled_queries.py` | 生成 DBLP/IMDb 的 $\langle g,f\rangle$ panel 并写实现后组大小 |
| `tools/data/build_query_feasibility_audit.py` | 重用图分量扫描并将每个矩阵 case 的可行性与当前矩阵哈希绑定 |
| `tools/experiments/validate_environment.py` | 在运行前检查矩阵总数、方法配置、路径、哈希和可行性审计 |
| `tools/experiments/validate_markdown.py` | 覆盖全部被 Git 跟踪的 Markdown，检查严格 UTF-8、围栏闭合，强制块公式使用 GitHub 官方 `math` 围栏，拒绝与中文标点或词内连字号相贴的行内公式开界，并拒绝未被 Git 跟踪或大小写不精确的本地链接目标 |
| `tools/experiments/run_experiments.py` | 稳定分片、断点续跑、逐查询 timeout、图加载 watchdog、一任务一 JSON 记录，并解析行末状态数 |
| `tools/experiments/run_parallel_campaign.py` | 在已验证不会显著扰动单进程时空结果的两枚固定 CPU 上，恢复 P1/P2 与受控探针；P2 先跑完整 Enhanced，再按冻结 frontier 规则运行较慢配置 |
| `tools/experiments/summarize_results.py` | 数据集/cell 汇总、PAR-2、共同完成时间/状态倍率、timeout 方向、目标值和可行性不一致 |
| `tools/experiments/plot_results.py` | 从冻结 supervisor JSON records 绘制 P2/S2 曲线，不重新挑选查询 |

一次正式运行不直接循环调用二进制，而是由 `run_experiments.py` 展开机器矩阵。runner 用 case/method/query 的稳定 key 分片，为每个任务写独立 JSON；同一 `run-dir` 下已完成 key 不会重跑。Linux 上保留 `PATH` 的大小写并为外部 solver 同步添加 `LD_LIBRARY_PATH`；Windows 上会合并大小写重复的 Path 环境项。

## 9. 修改算法时的最小安全流程

1. 先写明要保持的数学不变式：真实上界、可采纳下界、精确 row 还是 cutoff 证书。
2. 优先在公共 `Problem`/`Row`/future/交集函数中实现，不得恢复另一套 Base/Enhanced 数据结构。
3. 若是增强操作，从 `SolveOptions::Base()` 通过 `With` 增加，在 `DescribeConfiguration` 中明确登记为“安全新增”或“同职责替换”，并在 `IsValid` 中执行依赖检查；不得出现 Base 独有但 Enhanced 无同职责 realization 的阶段，也不得依据图名、 $g$、当前速度或内存自动开关。
4. 为最小反例增加 CTest，然后运行 `make release`。修改剪枝、闭合、零权边或 adjoint 时，三种合法配置都必须对照独立 DP。
5. 运行 SteinLib 已知最优 gate；任何目标值/可行性不一致都先当正确性错误，不能用“浮点容差”直接解释。
6. 只在正确性门禁通过后跑旧/新性能 panel；保留每个 panel 的权重序列和超时方向，不仅比较总时间。

## 10. GitHub 文档上传与渲染注意

后续 LLM 或人工修改 Markdown 时必须遵守以下仓库级约定。它们不是 LaTeX 数学语义限制，而是 GitHub 当前 Markdown 渲染器的兼容性边界。

1. 块公式统一使用带 `math` info string 的 fenced block，不使用“首尾各一行 `$$`”的三行式写法。标准形态为：

   ````markdown
   ```math
   E = mc^2
   ```
   ````

2. 不得在行内或块公式中使用 `\operatorname` 或 `\operatorname*`。截至 2026-07-24，GitHub 会显示 “The following macros are not allowed: operatorname”，并把公式源文回退成灰色代码块。普通命名使用 `\mathrm{name}`；例如 `\mathrm{OPT}`、`\mathrm{dist}` 和 `\mathrm{clamp}`。当前渲染器也曾把语法完整的 `\begin{cases}...\end{cases}` 报成 “Missing `\end{cases}`”；本仓库因此把分段函数拆成多个独立 `math` block，不再使用 `cases` 环境。
3. 不要把含下划线的代码标识符塞进数学文本命令，例如不要写 `$S\subseteq\texttt{full\_mask}$`。GitHub 曾把其中的 `_` 送到文本模式并报 “`'_' allowed only in math mode`”。代码名应留在公式外，用 Markdown 行内代码表示；若确实需要数学记号，则改写为 `$M_{\mathrm{full}}$` 这一类结构。
4. 表格单元格中的行内公式不能直接写竖线定界，如 `$|S|$`；使用 `$\lvert S\rvert$`，否则 Markdown 会先把竖线解释为列分隔符。
5. 行内公式的开界 `$` 前必须有安全边界。GitHub 已实测会把紧跟中文标点或词内连字号的后续公式留成原文：不要写 `$D$、$A$、$H$` 或 `fixed-$U_0$`，而应写成 $D$、 $A$、 $H$ 以及“固定的 $U_0$”。注意“定界符数量配对”不能发现这类问题，必须同时检查边界和上传后的实际 MathML 数量。
6. 每次提交前运行 `make validate-markdown` 或 `python3 tools/experiments/validate_markdown.py`。该门禁检查 UTF-8、围栏、行内定界符及安全左边界、表格公式、已确认的 GitHub 禁用宏，以及本地链接目标是否以精确大小写被 Git 跟踪；不能让一个只在 Windows 本地存在或仅靠大小写不敏感解析成功的路径通过。`make release` 已依赖该门禁。
7. 上传后不能只统计公式容器，因为失败公式同样会生成容器。必须在 GitHub 的实际渲染页面（或编辑器 **Preview**）检查所有含公式的文件，并确认每个 `.js-display-math` 和 `.js-inline-math` 都含实际 MathML `<math>` 子节点，任何 `math-renderer` 内均无可见 `.flash-error`、黄色错误框或灰色公式源码回退。不要把整页 `.flash-error` 数量当成判据：GitHub 页面可能自带隐藏的通用错误模板。错误文本既可能是 “The following macros are not allowed”，也可能是 “Missing ...” 或文本模式错误。若 GitHub 以后出现新失败模式，先改写公式，再把可静态识别的模式加入 `validate_markdown.py`。

语法依据见 [GitHub 数学表达式官方文档](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/writing-mathematical-expressions)；`\operatorname` 的实际限制见 [github/markup#1688](https://github.com/github/markup/issues/1688)。

## 11. 当前明确边界

- ABHSS 只支持无向、有限非负边权与 $g\le16$。
- 当前公开二进制输出精确权值和 feasibility，不输出最优树边集。
- 代码对解析后 `double` 边权做组合精确搜索；上下界闭合使用原始顺序比较，但跨实现结果报告仍使用 $10^{-6}$ 核验容差。
- PrunedDP++-Safe 是对公开论文路径的纠错重建，不能在 artifact 中写成“2016 原作者原码”。GPU4GST 2025 artifact 的 CPU 版本是独立核验对象。
- MonoGST+ 论文与作者 workload 目前是本地来源，不应假设已获得公开再分发许可。这是 artifact 发布问题，不改变本地哈希冻结的实验身份。
