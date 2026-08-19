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
            after a DirectedCut support refresh: persist support metric/subset DP
       -> common A1 row                  [only when the logical grid contains A1]
            all configurations: same seed, farthest + endpoint-floor cone, nonnegative fallback,
                                 hierarchical exact A1 view: lazy/linear top-two, complete byte-ranked tail, and forward-A ownership transfer
            the A1 builder does not read enhancement flags or dual potentials
            queue/edge work pays rent; a tighter tree-DP buy restarts the A1 pass
       -> BuildOrdinaryWithProbe
            all configurations: farthest + tour + common A1 future
            DirectedCut/Enhanced: additionally max with dual potential
            -> BuildOrdinaryRows         [shared recurrence; materialize through schedule.ordinary_last_layer]
       -> RunForwardAnchoredStage        [Base / DirectedCutOnly: full A]
          or
          RunForwardAnchoredStage        [Enhanced with a nonempty H suffix: low A]
            -> SolveHighAdjoint           [auxiliary H(h) realizes omitted D(h), then descending H replaces high A]
       -> SolveResult{best_weight, feasible, mask_vertex_states}
```

`src/main.cpp` 是二进制公共批处理入口。CMake 通过宏将同一入口编译为 `abhss`、`pruneddp`、`dpbf` 及可选的第三方 adapter，使图加载、查询分片、计时和输出格式一致。每个 `weights.txt` 查询行依次写 time、weight、query peak RSS 和 `mask_vertex_states`；正式 ABHSS/PrunedDP++ 返回非负计数，无统一口径的 adapter 写 `-1`。

`SolveOneQuery` 先通过 `IsValid`（内部读取 `DescribeConfiguration`）拒绝非法开关，再处理空查询、 $g>16$、无共同分量、单组等入口情形。非平凡查询建立 `Problem` 并完成预处理后，才保存本次执行所需的 `ConfigurationProfile` 和状态层计划。`Problem` 为避免改变热对象布局，仍只读保存原来的冻结 bit mask；`UsesBoundedGroupDistances`、`UsesDirectedCut` 和 `UsesAdjointCompletion` 与 profile 映射由同一配置回归共同约束。代码与论文共用这张“新增或替换”契约，不能再把位掩码单调误写成逐指令包含。

`PrepareProblem` 依次构建零权分量下界、距离—根初始化，再决定是否需要真实路径并集、锚组、tour 下界和当前配置自己的 witness。正权图的 `ComputeComponentCover` 直接使用加载期最小边权，只聚合查询触及的至多 $F$ 个单点分量；确实含零权边的图才扫描原边并建立并查集。对没有被零代价分量条件提前闭合的可行查询，外层始终只调用一次 `BuildDistanceRootInitialization`，并统一消费 `DistanceRootInitialization{group_distance, root, upper}`。 $g\le3$ 时全部配置统一选择 BootstrappedBounded root-star 基例并立即返回； $g>3$ 时才由冻结 profile 在 BootstrappedBounded 与 CompletePotential 之间实现同职责替换。前者在 realization 内用规范 SPT 边并集尝试启动 cutoff，再构造 bounded `GroupRow`；若非连通图的规范终端没有共同分量，bootstrap 可暂时为无穷，此时多源距离不截断，随后的共同 root-star 扫描仍会在已验证存在的公共分量中取得有限上界。后者构造完整距离势。两者都返回同一三元合同。最远组 oracle 只保存一个 byte/vertex 的全局 argmax：构造时用局部最大值做一次线性扫描，命中时 $O(1)$ 返回，未命中时严格扫描剩余 mask；完整势直接读取 dense payload，bounded 势仍走 `GroupRow` 合同。没有固定深度 top-k、查询统计或配置专属数学值。

当 $g\le3$ 时，任意三终端树在分叉点处分解可证明最优值恰为 $\min_v\sum_i d_i(v)$。BootstrappedBounded 的 cutoff 若严格大于最优值，则最优根的所有组距离都已精确保留；若等于最优值，则真实 cutoff 本身已经闭合。因此三个合法配置在进入任何 enhancement realization 前逐项执行同一个 bounded root-star 包并直接返回，不构造 complete potential、witness、dual、tour 或主状态。这是共同数学基例，不是按组数选择 Base/Enhanced。仅当 $g>3$ 仍未闭合时，公共外层才构造 root-path-union。规范 SPT 是 bounded 物理表示的内部 bootstrap，不是 Base-only 的外层调用阶段。Base 把共同边并集整理为 root-path witness；开启 `DirectedCut` 时，以 primal upper 与 dual-primal witness 实现相同的真实 witness 职责，facility 上界另作安全新增。随后所有配置调用同一个 `BuildTripleSeededPathGrowthUpper`：前两组建立种子路径，显式枚举第三组后再以真实 tight shortest paths 保持连通地接入最近未覆盖组，按 edge ID 去重计费，并用非负边权下费用单调不减的性质在不能严格改善 incumbent 时安全停止。实现对每个有序前两组只恢复一次种子路径，再把完全相同的真实种子边重放给各个第三组；`membership[v]` 同时承担路径终点组判断，避免每次恢复重复写入和清除一张全图 terminal 数组。恢复下一条路径前还用“已付真实树费用 + 当前树到目标组的最短距离”检查严格改善的必要条件；失败时跳过的 DFS 不可能产生更优候选。这些操作只消除重复恢复、组扫描与工作区，不改变任何可能严格改善 incumbent 的有序组三元组候选。当前 witness 已与公共 component-cover 下界闭合时，两种配置还会以同一精确条件在调用三元上界前返回；未闭合查询的图规模工作区在主状态搜索前释放。预处理到此为止：两边都不在这里无条件调用 `EvaluateWitnessTree`。

预处理返回后，`SolveOneQuery` 才构造唯一的 `WitnessUpperScheduler`，所以 Base、DirectedCutOnly 与 Enhanced 的 `rent` 都严格从 0 开始。初始阶段只把各自 witness 的真实顶点数代入同一个 `buy` 公式；公共 A1 与 ordinary $D$ 的 queue-pop/edge-relax 工作连续支付 rent，达到阈值且树 DP 有新输入时调用同一个 `EvaluateWitnessTree`。若 A1 中的购买真正收紧上界，A1 会以新的固定 cutoff 整轮重启；未收紧时继续当前轮。只有开启 DirectedCut 的配置可能在 ordinary 中另行购买 residual closure；购买后完成 residual 势并恢复真实 primal/facility 上界，若四元真实路径生长继续严格收紧上界，则把路径边与 primal 边合成 certificate support，调用 `RefreshCertificate` 按 support 顶点数切换确定性 buy，并把后续消费函数切换为 `CertificateSupportDpCache::Evaluate`。路径上界已经直接写入 `best`，不再重建一棵后续无人消费的 witness 树。完整势和真实上界随后共同重滤已经物化的 D；删除条件仍是“精确 rooted 值 + 可采纳 future 不小于现有真实上界”。这是 closure 购买后的单调证书刷新，不改变初始 witness 的共同调度规则。

`CertificateSupportDpCache` 只属于上述 DirectedCut 新增证书：Base 的 `BuildOrdinaryRowsImpl<false>` 在编译期不调用 `PublishOrdinaryMask`，也不包含每行的空指针判断。缓存第一次购买时执行与独立 `EvaluateCertificateSupport` 相同的 Floyd 与全 mask subset DP；support 边集不变时保留 metric 和 DP。新发布一张 $D(M)$ 后，`PublishOrdinaryMask` 只登记 $M$，下一次购买仅重算所有 $X\supseteq M$ 的 mask，并继续按基数递增、数值 submask 次序运行原递推。若购买收紧上界并 destructive-refilter 已有 D，`Reset` 令下一次购买保守地全表重建；若 residual closure 刷新 support，`RefreshCertificate` 直接建立新缓存。因此缓存不修改 `rent`、`buy`、购买位置、候选次序、状态数或浮点运算，只消除同一 support 上重复求值的被支配工作。

`MakeAnchoredCompletionSchedule` 从平衡证明得到完整锚定格的正层域 $\mathcal L_A=\{1,\ldots,q\}$，其中 $q=\max\{0,\lfloor g/2\rfloor-1\}$。代码只判断某个逻辑层是否属于该域，不含 `g >= 常数` 一类经验分段。 $g\le3$ 查询在进入层计划前已经由全部配置共同的精确恒等式闭包。对其余查询，域为空时完成式直接使用隐式 $A(\varnothing)$；域非空时 A1 是第一个成员，所有配置一律在 ordinary 前生成它。Enhanced 的前向边界固定为 $\ell=\min\{1,q\}$：有正层时只保留共同 A1，逻辑层 $2,\ldots,q$ 全由 H 实现。该 row 形成 `AnchoredSingletonFuture`，在 ordinary 后按所有权移交给公共前向内核，既不重复闭包也不重复计数； $q\le1$ 时 H 后缀为空，直接复用完整前向入口。

A1 的重启循环只有在全部 singleton bit 都写入 `ready` 后才初始化 `AnchoredSingletonFuture` 的查找计划；空 payload 也属于已发布 row。因此 future 只读阶段把该初始化点当作发布屏障，直接遍历完整 bit 域，不在每次读取时重复检查 `ready`。同理，A1 域推出 $g\ge4$、 $k\ge3$，两级购买工作严格为正；ranked-tail 热路径只比较 rent 与 buy。上述两项是共同生命周期不变量的减空，不是按组数启用的算法开关。其他仍可能观察未生成 row 的模块继续使用 `ready`。

令 $k=g-1$， $h=\lfloor g/2\rfloor$， $q=h-1$。没有 H 后缀时，ordinary 与完整前向配置一样物化到半格 $h$。存在 H 后缀时，Enhanced 与 Base 仍用同一 `BuildOrdinaryRows` 完整生成 $D(1),\ldots,D(q)$；Enhanced 不物化更高的 $D(h)$，而把 adjoint 的物理区间扩为：

```math
H(h),H(h-1),\ldots,H(2).
```

$H(h)$ 是辅助层，不对应额外的前向 A 层。若 $g=2h$，其补集大小为 $q$，单张已有 $D(q)$ 直接播种；同目标任意 pair 之和不小于 $D(q)$，因此只在这一目标上完全跳过 pair。若 $g=2h+1$，其补集大小为 $h$，两个互斥且大小不超过 $q$ 的 ordinary 块覆盖全部规范 split。随后执行与 ordinary 相同的图闭包，因此 $H(h)$ 精确实现被省略的 $D(h)$。较低 H 层按 ordinary split 的结构分为三类：完整 $D(Q)$ 已物化时直接转置；两侧都不超过 $q$ 时转置双块 terminal；至少一侧超过 $q$ 时，由已完成的 H successor 加回较小 ordinary 侧。ordinary 发布域按块大小向下闭合；若本次 H 区间所需的最小单块 cover 层都不可用，则所有更大单块 cover 也不可用，代码直接跳过严格为空的单块扫描，但不跳过双块 terminal。successor 必须读取新增块的全部精确值，因为原规范 branch 可能位于 successor 所代表的一侧。若非锚组恰能二等分，互补的两个 $H(h)$ 还共同恢复锚组加两张 $D(h)$ 的平衡完成式。调度器只使用由 $g$ 推出的 $h,q,\ell$、已发布层域和集合大小，不读取图名、row 密度、incumbent、时间、内存或经验组数阈值。

`BuildOrdinaryRows` 按 mask 大小生成普通 $D$，将同根 split seed 做图闭包，并标准化 branch；递推代码只有一份，调用者只传入计划推出的最高物化层。公开入口只按冻结的 `DirectedCut` 位分派一次 `BuildOrdinaryRowsImpl<false/true>`，使 flat/staged 热循环不再重复读取配置；两个实例来自同一份模板递推，这项编译期特化不改变候选、证书或状态，也不是经验算法选择。size 2 的唯一规范拆分保持单遍；更高层先把全部规范拆分聚合成精确的逐顶点最小 seed，再对每个顶点运行一次与拆分无关的统一 future。Base 的 flat cache 在首次存活候选后保存完整 farthest/A1/tour 最大值；开启 `DirectedCut` 后，同一个 `CanImprove` 使用 staged realization，打包的 `bound_state` 以高位保存 row epoch、低 3 bit 表示 dual、farthest、公共 A1、tour 已计算到哪一步，另用 2 bit 保存 `rejected-seen` 与 `rejected-frontier`。第 4 个低位只在 `GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS` 构建中记录 exact-dual 命中，论文二进制把该写入完全编译掉；epoch 仍统一左移 6 bit，使两类构建采用相同布局。较小标签可从尚未完成的阶段继续，不能把先前候选的拒绝永久化。dual 的 certified interval 下端只能作为可采纳前缀；upper endpoint 放行后，任何以后还能越过后续缓存或真实 `distance` 的标签必然更小，因而也必然放行，所以可结束 dual 判定；未转置逐组和或区间歧义触发的逐组 exact fallback 则返回可直接复用的完整值，即使当前标签被拒绝也可把 dual 阶段标为完成。这个分流只读取预声明的 DirectedCut 位，因为没有 dual 的 Base 不存在可复用的第一阶段中途拒绝；它不读取图名、 $g$、row 密度或计时，也不改变 `double` 求和。staged 路径还在同一个 32-bit `bound_state` 中保存 `rejected-seen` 与 `rejected-frontier`。仓库限制 `g <= 16`，固定锚组后 ordinary mask 少于 `2^15`；epoch 左移 6 bit 后仍小于 `2^21`，所以高位 epoch 与低 6 bit 元数据在一条查询内不会重叠或回绕。单次缓存拒绝只登记 seen；一张 row 已观察到复用后，才把虚拟拒绝值写入既有 `distance` 并加入 `rejected` 清理表。stage 0 首次物化时从 `best - bound_cache[vertex]` 得到解析 cutoff，再用 `nextafter` 修正到原 `double` 谓词确实拒绝的首个可表示值；stage 1--4 保存实际拒绝值。虚拟值不入堆、不计状态，较小标签通过完整证书链时由 crossing 分支先加入 `touched` 并清除前沿位。删除任一环节都要重新证明 row 结束复位、状态计数与候选支配安全。通过完整 future 的初始标签用 `QueueNode` 的确定性全序线性 heapify；`CanImprove` 成功时已经把完整证书写入当前 epoch 的 cache，所以 heap key 直接读取该 cache，不再经过第二个只会命中的 accessor。图松弛仍执行相同精确闭包。所有配置都读取共同 A1 future；开启 `DirectedCut` 后，统一 future 栈在 A1 之外再与对偶势取最大，而不是替换、关闭或修改 A1。`BuildReusableAnchoredSingletonLayer` 不读取配置位或 dual，三个配置使用相同的 farthest + endpoint-floor cone 与非负 fallback。DirectedCutOnly 与 Enhanced 随后都把已经生成的 A1 交给同一 `BuildForwardAnchoredRows` 内核； $H$ 只负责 A1 之后的高层后缀。补集转置扫描全部已物化 ordinary row，并为每个 H 目标生成结构上必要的单块或双块直接终端；完整 $D(Q)$ 存在时，它逐值支配同一目标的 pair。其余 split 由更大 H successor 加回一张 ordinary row 的全部精确值，最终 A/H 边界才按前向规范化要求只读 branch。非锚组二等分时，互补辅助 H row 还恢复没有 ordinary $D(h)$ 可读的平衡完成式。转置 pair 在排序枚举与互补 submask 两种等价循环中选择确定项数较少者；这一选择只改变遍历方式，不改变候选集。最高逻辑 ordinary 层已经完整物化，所以所有 A/H 边界直接读取公共 $D$ row；不再存在 separator 装箱、三块 terminal 或数据相关结构位。`complete_implicit_anchor` 仅表示完整正层域为空。

Base 和 DirectedCutOnly 经 `RunForwardAnchoredStage` 调用 `BuildForwardAnchoredRows`，生成完整的低/高层锚定 $A$。Enhanced 在 H 后缀非空时仍调用同一内核生成由平衡完成域确定的低层 $A$，再由 `SolveHighAdjoint` 以补集转置终端和递减 $H$ 代替未物化的高层 $A$；若 H 后缀为空，则直接复用完整前向入口，末层只消费而不保留，也不构造任何转置工作区。ordinary 截止保留最高逻辑层并只省略半格；辅助 $H(h)$ 先承担该半格的同递推转置，之后的 H 后缀才替换高层 A。全部边界来自递推索引，不读取数据集名或运行表现。

从论文和代码审计角度，配置关系必须按下表理解：

| 逻辑职责 | Base realization | Enhanced realization | 关系 |
|---|---|---|---|
| A1 与 ordinary A1 future | ordinary 前生成标准 A1，使用 farthest + endpoint-floor cone 与非负 fallback；先按结构购买 top-two，并用精确子集递推因子化 tail rent，再按已付 tail 查找工作购买完整 byte 排名 | 逐项执行同一 seed、cone、fallback、两级购买、精确租金表与移交；A1 内不读取 dual | 严格共同操作；完整排名不是固定 top-k，不改变原 double；购买式不读取配置、图名、计时或经验组数阈值 |
| $g>3$ 的距离—根初始化 | realization 内以真实 SPT 边并集启动 cutoff，构造 bounded `GroupRow`，再做共同根扫描 | 构造完整距离势 `GroupRow`，再做同一共同根扫描 | 外层只调用同一函数并接收 `{group_distance, root, upper}`；SPT/全距离扩展分别是两种表示的内部成本，不是 Base-only 阶段；低组基例在此之前共同闭包 |
| ordinary 的其他 future | flat realization：farthest、公共 A1、tour 的完整值首次存活后缓存 | staged realization：先增加 directed-cut，再依次复用 farthest、公共 A1、tour 的已算前缀 | 证书集合是安全新增；求值 realization 的共同输入、输出和严格拒绝职责相同，选择只由 DirectedCut 位决定 |
| witness realization 与条件式树 DP | root-path tree | primal upper + dual-primal tree | 对未被共同闭包的查询，树来源是同一真实 witness 职责的替换；两边预处理都只构造各自 witness，随后从 `rent=0` 进入同一调度器、同一 `buy` 公式和同一树 DP；共同的 root-star/root-path-union 仍由两边执行，facility 是额外安全上界 |
| A1 之后的高层锚定完成 | 完整 ordinary 半格 $D(h)$ 加前向高层 $A$ | 完整物化到最高逻辑层 $q$；以必要的单/双 ordinary 终端、全值 successor 和互补半格完成精确构造 $H(h),\ldots,H(2)$，再与共同 A1 结算 | $H(h)$ 与 $D(h)$ 是同一 rooted 递推职责的正反 realization；逻辑 $H(2),\ldots,H(q)$ 替换 A1 之后的前向层。A1 与 $D(1..q)$ 都是共同操作；只减去被完整 D 逐值支配的同目标 pair |
| 无 Base 对应物的工作 | 无 | directed-cut 可行证书、额外 facility 收紧、延迟 residual 证书刷新与单调 D 重滤 | 安全新增；只加强下界、真实上界或删去已不能严格改善的状态 |

`DirectedCutOnly` 采用表中的 DirectedCut 距离与 witness realization，增加 dual/facility 证书，但仍保留完整前向高层 $A$，只作为隔离 `AdjointCompletion` 的正确性/消融配置。表中不允许出现“Base 独有且 Enhanced 没有同职责替代物”的逻辑阶段。

## 5. ABHSS 源文件导读

| 文件 | 主要职责 | 阅读时需要抓住的不变式 |
|---|---|---|
| `src/abhss/abhss.h` | 公开 `SolveOptions`、增强位、`ConfigurationProfile`、`AnchoredCompletionSchedule` 和 `SolveResult` | 开关链只表达安全新增/同职责替换；层计划只表达平衡递推域及 A/H realization |
| `src/abhss/solver.cpp` | 单一 solver 入口与配置调度 | 只在这里选择完整前向或 adjoint 完成；不存在按查询 oracle |
| `src/abhss/pipeline.{h,cpp}` | 平凡/无解前置、预处理和 ordinary 的公共 probe 边界 | 诊断包装不改变算法语义 |
| `src/abhss/internal.h` | `Problem`、`Row`、`GroupRow`、witness、support-DP 缓存接口、状态计数和热路枚举器的共同定义 | $D$、 $A$、 $H$ 共用一个有序稀疏 `Row`；每张 row 按首次进入工作区的顶点批量计数；ordinary、提前 A1 与 H 的构建期 `ready` 不能和空 payload 混淆，A1 future 只在完整发布屏障后省略重复读取；普通 forward 内部的零标签行则由严格层序证明已处理并省写 ready；bounded `GroupRow` 的随机精确读取必须走 `ExactValueOrInf`，不得把 cutoff 当 singleton |
| `src/abhss/preprocess.cpp` | 正权 cover 快路径、零权 cover、组距离、 $g\le3$ 闭包、多种真实上界、tour、witness、统一 future、certificate-support subset-DP 缓存 | cutoff 不得当作精确状态；数学闭包必须对全部配置相同；`best` 只由真实可行子图收紧；support 增量失效域必须覆盖新 D mask 的全部超集；tour 上包络只能跳过必被当前下界支配的求值，不能冒充新的下界 |
| `src/abhss/path_growth_upper.{h,cpp}` | 全部配置共同的有序组三元组一步前瞻真实路径生长上界 | 只恢复 `ExactValueOrInf` 有限位置的 tight paths；边 ID 去重；cutoff 只能作非负费用的单调安全终止 |
| `src/abhss/core.{h,cpp}` | A1 的 ordinary 前调度视图、ordinary $D$、逐顶点 split 聚合、flat/staged future realization、row 交集、规范 branch、共同 witness rent-or-buy、support-DP 缓存所有权 | A1 构造不读取增强位或 dual；完整 singleton pass 是 future 的发布屏障，屏障后不重复检查 row `ready`；`InitializeLookupPlan` 只按 singleton row 形状确定严格为正的两级购买式，`MaterializeAllTopTwo` 保持稳定 bit/locator 并构造精确 mask-rent 表，`MaterializeRankedTail` 只保存 top-two 之后的完整 byte 次序；两个一次性物化函数保持冷机器码边界，逐状态 `Future` 始终返回同一精确 row/非负 fallback 视图的最大值；ordinary 阶段缓存不能把候选专属拒绝永久化；Base 不支付无 dual 收益的 stage 热分支或 support 通知；树 DP 收紧上界时允许丢弃未完成的 A1 尝试并整轮重启，但最终只发布、移交和计数一份标准 `Row` |
| `src/abhss/forward.{h,cpp}` | 公共前向锚定 $A$ 递推与完整解结算 | 隐式 $A(0)$、提前 A1 的所有权交接和正常非空 row 都走同一完成函数；从未产生标签的普通 A row 由严格层序证明已处理，保留空 payload 且不额外写 ready |
| `src/abhss/dual_cut.h` | `DirectedCut` 的 changed-arc 势、截断 potential cone、residual、exact fallback 与 primal 边恢复 | cone 只能跳过两端势都等于根 cap 的零梯度边；certified interval 不能冒充 exact；一次性构造保持非内联冷边界，避免 IPO 污染 Base 热布局；势只作下界，上界必须由原图真实边计价 |
| `src/abhss/adjoint.{h,cpp}` | ordinary 单/双块补集转置、辅助 $H(h)$、高层 $H$ 递减、互补半格与低层 $A$ 边界结算 | 每个规范 split 必须落入“完整 D 直接终端、双块直接终端、successor 加全值 ordinary”之一；最小单块 cover 层不可用时，由 ordinary 层域向下闭合可知整段单块扫描为空，但双块和 successor 不能删；只有最终 A/H 边界读取 branch；互补 H 只恢复缺失的平衡完成式；各组集合始终不交且并为全集 |
| `src/abhss/diagnostics.h` | 编译期可关闭的稀疏 phase 诊断、support 脏域计数和 ordinary 累计进度；详细宏自动包含普通 probe 宏 | 正式构建不因诊断改变状态或配置；逐候选计数只在 `GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS` 中存在；计时器与事件在正式构建中编译为空 |

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
| `abhss_configuration_exactness` | 5,000 个 $2\le g\le10$ 确定性随机连通小图、500 个 $6\le g\le10$ 正权互异单终端实例和 160 个 $g=7..16$ omitted-half transpose 压力实例，逐例对照独立全子集 DP；另含真值 5.75、旧错误值 6.25 的 12 点辅助半层固定反例，并覆盖全部受支持组数的 adjoint 三类 split 结构、互补半格域、稀疏 row 全值交集三种遍历、A1 lazy/top-two/ranked-tail 三种精确视图、全部 remaining mask 的租金表等价性、 $g=2,3$ 共同闭包、逐弧势梯度与 residual、距离—根合同、精确 singleton 读取、非连通 fallback、配置合同、共同 witness `buy`、零起点调度、非法开关及 A/H/ordinary 层计划 | A1 物化或租金因子化改变最大值/购买点、配置重构、省略 $D(h)$、缺失较低直接 terminal、successor 错读 branch、遗漏互补 H、row 交集值错配、错误推广/配置化低组闭包、potential cone 漏边、重新暴露 Base-only SPT 调度、恢复 Base 预买、经验参数分派、“新增/替换”契约漂移、非法配置、零权错误或 epsilon 误闭合 |
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

## 10. 文档与 GitHub 渲染检查

块公式统一使用独占行的 fenced math block，行内公式使用单美元定界；不要使用 GitHub 已拒绝的宏、`cases` 环境或表格中的裸竖线数学定界。提交前运行：

```bash
make validate-markdown
```

验证器检查 UTF-8、围栏、公式边界、禁用宏和精确大小写链接。推送后还需人工查看 GitHub 页面，确认公式生成实际 MathML 而非错误框或灰色源码。已知故障模式和网页检查细节见[归档说明](archive/README.md#history-github-rendering-maintenance-20260820)。

## 11. 当前明确边界

- ABHSS 只支持无向、有限非负边权与 $g\le16$。
- 当前公开二进制输出精确权值和 feasibility，不输出最优树边集。
- 代码对解析后 `double` 边权做组合精确搜索；上下界闭合使用原始顺序比较，但跨实现结果报告仍使用 $10^{-6}$ 核验容差。
- PrunedDP++-Safe 是对公开论文路径的纠错重建，不能在 artifact 中写成“2016 原作者原码”。GPU4GST 2025 artifact 的 CPU 版本是独立核验对象。
- MonoGST+ 论文与作者 workload 目前是本地来源，不应假设已获得公开再分发许可。这是 artifact 发布问题，不改变本地哈希冻结的实验身份。
