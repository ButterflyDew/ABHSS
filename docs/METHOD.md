# ABHSS：单线程精确 Group Steiner Tree 方法详述

本文档是论文方法章节的详细中文底稿。它描述当前代码实际实现的算法、证明责任和复杂度，不把尚未实现的设想写成既成事实。代码入口与逐文件导读见 [`CODE_GUIDE.md`](CODE_GUIDE.md)，正式实验口径见 [`EXPERIMENT_PLAN.md`](EXPERIMENT_PLAN.md)。

## 1. 问题定义、输出与适用范围

给定无向图 $G=(V,E,w)$，其中 $n=|V|$、 $m=|E|$，每条边权 $w(e)\ge 0$。给定 $g$ 个非空顶点组 $\mathcal K=\{K_0,\ldots,K_{g-1}\}$，组之间允许重叠，每组允许包含多个候选顶点。一个可行解是与每个 $K_i$ 至少相交一次的连通子图；由于边权非负，任意可行连通子图均可删除环而不增代价，因此最优解可取为树。目标值记为：

```math
\mathrm{OPT}(G,\mathcal K)=
\min_{T\subseteq G\text{ connected}}
\left\{\sum_{e\in E(T)}w(e):V(T)\cap K_i\neq\varnothing,\ \forall i\right\}.
```

当前二进制求精确最优权值，不序列化最终最优树的边集合；内部构造的真实树 witness 用于维持可行上界。若论文或 artifact 声称“输出树本身”，还需增加父决策保存或在最优值确定后的第二遍等式回溯。本文的“精确”首先是组合算法声称：在实数算术模型下，所有上界都可展开为真实可行子图，所有下界都由可行松弛给出，状态格不作近似截断。实现使用 IEEE 754 `double`，不以 epsilon 提前闭合；实验用 $10^{-6}$ 做跨实现结果核验。它不是任意精度或区间算术实现，因此论文不得把该声称写成对每一种浮点舍入轨迹的形式化认证。

实现支持零权边、重边、自环、组重叠和非连通图，限制 $g\le16$。空查询的答案为 0；单组查询无需边，答案为 0；若不存在一个同时与所有组相交的连通分量，则返回 infeasible。所有正式实验均是一个计算线程；1 ms RSS 采样线程只读取进程内存，不参与搜索。

## 2. 设计主线

经典 rooted subset DP 为每个组子集 $S$ 和根 $v$ 维护“覆盖 $S$ 并在 $v$ 连通”的最优值。它的难点不是递推本身，而是 $2^g n$ 状态、同根的 $3^g$ 级拆分以及每层图闭包。ABHSS 的主线是保留这套精确语义，同时把真正进入内存和队列的区域压缩为“仍可能严格改善一个真实可行上界”的稀疏锥体：

1. 先构造真实可行树得到全局上界 $U$，并构造若干可采纳下界；当 $g\le3$ 时由共同 root-star 恒等式直接精确闭包。
2. 固定一个永久锚组，只对其余 $k=g-1$ 个组编码 bit mask。
3. 先生成不含锚组、大小至多约一半的普通状态 $D$，并只发布规范 branch。
4. 用前向锚定状态 $A$ 表示包含永久锚组的公共状态；Base 以前向高层完成搜索。
5. 开启 `DirectedCut` 后，安全增加 ordinary/adjoint 使用的对偶证书与额外可行上界，并以完整距离势、dual-primal witness 替换 Base 中相同职责的 realization；搜索工作支付固定购买价时，再完成 residual 势、恢复真实 primal/support 上界并以更紧的上下界重滤已物化 ordinary。公共 A1 不被替换，也不读取 directed-cut 开关。
6. 再开启 `AdjointCompletion`，保留低层 $A$，用补集转置和递减的高层 $H$ 替代完整前向高层的等价求值。

这里的核心不是把两个实现包装成一个名字。`D` 的状态定义、稀疏 row、同根交集、图闭包、上界结算和统一 future 接口只有一份。增强开关只允许两种关系：增加 Base 没有的安全证书，或者以结构/功能对应的 realization 替换同一逻辑职责。禁止出现 Base 独自多执行、而 Enhanced 既不执行也没有同职责替代物的状态层。

全文围绕三个不变量展开。第一，`best` 始终由一棵可展开到原图边集的可行树给出，所以它只能从上方逼近最优值。第二，进入队列 key 的每个 future 都是不超过剩余代价的证书，所以只会删除无法严格改善 `best` 的状态。第三，任何没有被删除的 rooted DP 推导，要么在公共前向格中被枚举，要么在 adjoint 格中有一条保持组覆盖与代价的反向实现。工程优化只有在能保持这三个不变量时才进入正式实现。

### 2.1 一条查询的完整控制流

下面的伪代码对应当前 `SolveOneQuery`，顺序本身也是方法定义的一部分。`profile` 只解释预先冻结的增强位；`schedule` 只给出平衡递推需要的层区间。二者都不读取运行时间或中间状态规模。

```text
SolveOneQuery(G, K, options)
  1  检查 options 的依赖；处理空查询、单组和不可行分量
  2  PrepareProblem:
       先计算零权分量 cover；若一个零代价分量覆盖全部组则返回 0
       若 g<=3，则运行共同 bounded root-star 包并返回精确值
       否则通过当前配置的距离—根 realization 取得同一三元合同
       继续计算组级下界、锚组和当前配置的 witness
       若开启 DirectedCut，再增加 dual 证书与 facility 上界
       只构造当前配置的 witness；不无条件运行 witness-tree DP
       两边共同运行有序组三元组一步前瞻路径生长，以真实边并集收紧上界
  3  profile  <- DescribeConfiguration(options)
  4  h        <- floor(g/2), q <- max(0,h-1)
     ell      <- q                         (完整前向配置)
                 或 (q=0 ? 0 : max(1,floor(q/2)))  (AdjointCompletion)
     D_last   <- h                         (没有 H 后缀)
                 或 q                       (存在 H 后缀)
     H_last   <- q                         (没有 H 后缀，不物化 H)
                 或 h                       (存在 H 后缀，含辅助半格 H(h))
  5  scheduler <- WitnessUpperScheduler(witness), rent <- 0
  6  若层 1 属于 [1,q]，所有合法配置均:
       生成标准 A1 row；建立 top-two 加完整 byte tail 的分级只读 future 视图
       A1 的 queue-pop/edge-relax 向共同 scheduler 支付 rent
       若条件式树 DP 收紧 best，则以新 cutoff 整轮重启 A1
  7  BuildOrdinaryRows:
       按 |S| 递增物化到计划层 D_last，使用当前配置的 future
       继续向同一个 scheduler 支付 rent，并更新完整完成上界
       DirectedCut 的独立 rent 达到结构 buy 且已有足够 D payload 时：
         完成 residual 势，恢复 primal/facility 与四元路径 support
         以更紧的下界和真实上界单调重滤已完成 D；若 support 更新则切换 evaluator
  8  释放 A1 的只读查找缓存；标准 A1 row 本身仍保留
  9  若未开启 AdjointCompletion:
       把已有 A1 row（若有）移交给公共前向内核
       用 A 生成逻辑层 1..q 并完成答案
     否则:
       把已有 A1 row 移交给同一前向内核，继续生成低层 A(2..ell)
       用一/两张 ordinary row 转置播种辅助 H(h)，精确实现被省略的 D(h)
       从 h 递减 H；ell+1..q 替换高层 A，全部边界读取已物化到 q 的 ordinary
 10  返回 best、feasible 和实际首次发现的 (mask,v) 数
```

第 5 步不是一条“组数够大才启用”的经验规则。`[1,q]` 是第 8 步精确递推本来就要覆盖的索引域；询问“层 1 是否属于该域”等价于询问一个循环是否有第一轮。所有配置都把这张本来属于前向格的 row 提前，只为了在第 6 步复用；第 8 步接收同一对象而不是再生成一张。A1 构造函数不接收 `profile`，因此 Enhanced 不能在该层额外启用 dual、切换 fallback 或采用另一套闭包。

### 2.2 逻辑状态、证书与物理缓存的边界

为了避免把工程对象误写成新的算法状态，本文统一区分三层：

| 层次 | 对象 | 能否决定精确答案 | 生命周期 |
|---|---|---|---|
| 逻辑状态 | $D(S,v)$、 $A(S,v)$、 $H(S,v)$ | 是；它们组成精确递推或其等价反向实现 | 由状态层计划决定 |
| 上下界证书 | `best`、farthest、tour、A1 cone、dual potential | 只能安全保留或拒绝逻辑状态，不能冒充 DP 值 | 可随更紧上界失效并重算 |
| 物理加速缓存 | epoch 数组、A1 top-two bit/locator 与完整 byte tail、64 顶点临时桶 | 否；删除它们只应改变常数 | 阶段结束立即释放或复用 |

特别地，A1 row 属于第一层；围绕它建立的 top-two locator 与完整 byte 排名都属于第三层。后者不保存近似值，只决定下一次精确读取哪个 singleton row。 $H$ 属于第一层，因为它是高层前向依赖的等价反向状态；64 顶点桶只属于第三层。后文的正确性证明只依赖逻辑值与证书不变量，不依赖缓存宽度、容器容量或内存地址。

## 3. 单一算法与冻结配置链

公开入口为：

```cpp
SolveResult SolveOneQuery(const Graph& graph,
                          const Query& query,
                          const SolveOptions& options);
```

合法配置形成固定偏序：

```text
Base {}
  + DirectedCut
DirectedCutOnly {DirectedCut}
  + AdjointCompletion
Enhanced {DirectedCut, AdjointCompletion}
```

`AdjointCompletion` 依赖 `DirectedCut`，因为高层转置的 reduced value 和 prefix 剪枝读取 directed-cut 组势。仅开启 adjoint 的组合在进入求解前被拒绝。配置在一批查询开始前由命令行冻结；代码不会依据图名、 $g$、组大小、row 密度、incumbent、时间或内存动态选择 Base/Enhanced，也不允许逐查询选两条曲线的较快值。代码中关于 $g$ 的条件只表达已证明的递推定义域或数学基例，例如 $g\le3$ 的 root-star 精确闭包；它们对所有配置逐项相同，不是经验调参或配置分派。论文中应称“ABHSS 的 Base 配置和开启全部增强的配置”，不能称为两个方法。

### 3.1 “新增”与“替换”的严格含义

位掩码满足集合关系：

```math
\varnothing
\subset
\{\text{DirectedCut}\}
\subset
\{\text{DirectedCut},\text{AdjointCompletion}\}.
```

但这不等于 Enhanced 必须逐条重放 Base 的全部指令。代码中的 `ConfigurationProfile` 把差异分成两类：

1. **安全新增。** Enhanced 额外构造 directed-cut 对偶可行证书、facility 可行上界和一次延迟 residual 证书刷新。刷新形成一个单调证书闭环：增加可采纳下界，只以真实边支持图收紧上界，再删除在新上下界下已不能严格改善的状态。代码中的 `AddedOperation` 只登记这类 Base 没有对应 realization 的工作。
2. **同职责替换。** 若 Enhanced 不执行 Base 的某段工作，必须存在结构或功能对应的 realization，且论文能够给出共同输入、共同输出语义及各自优势区间。替换不能凭数据集或运行时表现选择。

当前完整映射如下：

| 逻辑职责 | Base realization | Enhanced realization | 为什么属于替换 |
|---|---|---|---|
| $g>3$ 的距离—根初始化 | bootstrapped-bounded：在 realization 内取得真实 cutoff，再构造 bounded `GroupRow` 并选择共同根 | complete-potential：构造完整距离势 `GroupRow` 并选择共同根 | 都一次返回 `DistanceRootInitialization{group_distance, root, upper}`；调用者及全部下游只读取同一三元合同，SPT bootstrap 不是独立逻辑阶段； $g\le3$ 在进入该替换职责前由共同 bounded 基例闭包 |
| witness realization | root-path tree | primal upper + dual-primal tree | 都构造原图真实 witness；预处理均不无条件运行树 DP，随后把各自树大小代入同一 `buy` 公式，并从 `rent=0` 调用同一调度器和树 DP；共同 root-star/root-path-union 不属于差异，facility 上界另归安全新增 |
| 高层锚定完成 | 完整 ordinary 半格依赖加前向高层 $A$ | ordinary 保留到最高逻辑层 $q$；辅助 $H(h)$ 精确转置实现省略的 $D(h)$，再以低层 $A$ 加递减 $H$ 完成 | $H(h)$ 与 $D(h)$ 是同一递推职责的正反 realization；逻辑 $H(\ell+1,\ldots,q)$ 再替换高层 $A$ |

A1 不在替换表中，因为它现在是三个合法配置的逐项共同操作。只要正层域非空，Base、DirectedCutOnly 与 Enhanced 都在 ordinary 前运行同一个 `BuildReusableAnchoredSingletonLayer`，使用相同 seed、farthest + endpoint-floor cone、非负 fallback、标准 `Row`、分级精确 future 视图和所有权交接。该视图先构造 top-two bit/locator，并精确因子化各 remaining mask 的 tail 租金；必要时再购买 top-two 之后的完整 byte 排名。两级表示都不保存近似 double。该函数不读取 `UsesDirectedCut()`、`UsesAdjointCompletion()` 或 `ConfigurationProfile`。开启 `DirectedCut` 只在 A1 之外新增证书：ordinary 的统一 future 栈额外取 $L_{\mathrm{cut}}$，adjoint 继续使用其 reduced/prefix 证书；它不改变 A1 搜索域或 A1 值。

因此，不能把流程写成“Base-only early-A1 阶段”，也不能再写成“A1 future 被 dual 替换”或“A1 完成层被 H 替换”。A1 是完整锚定格的一层标准 `Row`，所有配置都提前求它，使同一 row 兼任 ordinary future，之后把所有权直接交给公共前向内核。真正的状态层替换只发生在 A1 之后的高层前向 $A$ 与 adjoint $H$ 之间。

### 3.2 由递推域推出层边界，而不是按参数分段

令

```math
h=\left\lfloor\frac g2\right\rfloor,
\qquad
q=\max\{0,h-1\}.
```

$q$ 来自第 10 节的平衡分解定理，是完整锚定格必须覆盖的最高 mask 大小，不是经验阈值。正层定义域为

```math
\mathcal L_A=\{s\in\mathbb Z:1\le s\le q\}.
```

`MakeAnchoredCompletionSchedule` 只把这个定义域映射到 realization：

- Base 与 DirectedCutOnly 的前向 $A$ 覆盖逻辑层 $1,\ldots,q$；
- Enhanced 在 $q=0$ 时令 $\ell=0$，否则令 $\ell=\max\{1,\lfloor q/2\rfloor\}$。前向 $A$ 覆盖 $1,\ldots,\ell$，逻辑 $H$ 覆盖 $\ell+1,\ldots,q$；
- 空区间 $\mathcal L_A=\varnothing$ 时，完成式只读取隐式 $A(\varnothing)$；非空时层 1 自然属于定义域，三个配置都提前生成并移交同一张标准 A1 row；
- 若 $\ell=q$，不存在 H 后缀，Enhanced 直接复用完整前向入口，不构造任何转置工作区。

没有 H 后缀时，ordinary 与完整前向配置一样物化到半格 $h$。存在 H 后缀时，Enhanced 物化 $D$ 到最高逻辑层 $q=h-1$，并把 adjoint 的物理层扩到 $h$：

```math
D:1,\ldots,q,
\qquad
H:h,h-1,\ldots,\ell+1.
```

其中 $H(h)$ 是一个辅助半格层，不替代任何额外的前向 $A$；它精确转置实现被省略的 $D(h)$。当 $g=2h$ 时，固定 $|S|=h$ 后的补集大小为 $h-1=q$，已有一张 ordinary row 即可播种 $H(S)$；当 $g=2h+1$ 时补集大小为 $h$，转置枚举两个互斥、大小均不超过 $q$ 的 ordinary 块，恰好给出普通 $D(h)$ 的同根 split seed，随后执行相同的图闭包。因此 $H(h)$ 不是经验补丁，而是被省略半格递推的反向 realization。

从 $H(h)$ 递减时， $H(S\cup B)+D(B)$ 与 ordinary 从补集 $[k]\setminus(S\cup B)$ 扩为 $[k]\setminus S$ 的规范转移逐项对应。于是辅助层以下的 $H(S)$ 继续表示 $D([k]\setminus S)$ 的补集转置；逻辑区间 $\ell+1,\ldots,q$ 才承担替换高层 $A$ 的职责。该边界不需要树 separator、两箱容量或三块 terminal，也不读取图名、实际状态数、incumbent、时间或内存。

$q$、 $\ell$ 与 $h$ 都是递推数组边界，不选择“哪种算法更快”。代码可以询问 `ContainsLogicalLayer(1)` 或 `UsesAdjointH(h)`，却不能出现“观测到某个 $g$ 后决定是否使用某项操作”的经验分派。

### 3.3 配置职责账本

为了能从代码审计“包含关系”而不是只相信文字，`DescribeConfiguration` 为一次运行生成不可变职责账本。距离—根初始化、上界 witness 和高层完成三个 replacement 字段始终各选一个 realization；`ordinary_future` 固定登记共同 A1；`added_operations` 只允许增加经过证明的证书位：

```text
Base
  added_operations = {}
  distance_root    = BootstrappedBounded
  ordinary_future  = AnchoredSingletonCone
  upper_witness    = RootPathTree
  high_layer       = ForwardAnchoredA

DirectedCutOnly
  added_operations = {DirectedCutCertificate, FacilityUpperBound,
                      ResidualCertificateRefresh}
  distance_root    = CompletePotential
  ordinary_future  = AnchoredSingletonCone
  upper_witness    = DualPrimalTree
  high_layer       = ForwardAnchoredA

Enhanced
  与 DirectedCutOnly 相同，仅将
  high_layer       = AdjointH
```

因此，“关闭 Enhanced 的开关得到 Base”有两层含义。配置位上，关闭 `AdjointCompletion` 再关闭 `DirectedCut` 恰好得到空集合；执行语义上，每次关闭会移除安全新增证书或把相应替换职责恢复为表中的前一个 realization，而不会留下一个只有旧路径才执行的孤立阶段。测试会同时断言位集合包含关系、共同 A1 realization、其余 replacement 映射、非法 adjoint-only 拒绝、 $0\le g\le16$ 每个必需层恰由 A 或 H 覆盖一次、ordinary 物化截止、辅助 H 半格与逻辑 A/H 覆盖符合递推公式，以及三种合法配置的精确目标值。

## 4. 图加载、可行性与计时边界

`graph.txt` 的首行是 $n,m$，随后恰好 $m$ 行 `u v w`。快速读取器以 8 MiB 块扫描数字，保留原边顺序、`edge_id` 和双向邻接插入顺序；读取完成后按邻接表一次性计算 `component_of[v]`。这次 $O(n+m)$ 连通分量扫描属于所有方法共享的图加载，发生在 `[Ready]` 之前，不进入逐查询计时。

对一条查询，若图连通，可行性检查只验证组非空和顶点范围；若图不连通，则把每组涉及的分量编号排序去重并逐组求交。设查询总组成员数为 $F=\sum_i |K_i|$，该步骤在连通图上为 $O(F)$，非连通图上为 $O(\sum_i |K_i|\log |K_i|)$，不再为每条查询重复扫描整图。手工构造且没有缓存的小测试图会回退执行一次局部分量扫描。

这一边界很重要：旧实现每条查询重新执行 $O(n+m)$ BFS，在 Orkut 的数百条查询上会把共同图性质误计成每种算法的搜索成本。当前图加载秒数和查询加载秒数单独写入结果 header，只作为 artifact 工程指标。

## 5. 记号与公共数据结构

预处理选择锚组 $K_a$。其余 $k=g-1$ 个组重新编号为 bit $0,\ldots,k-1$，映射由 `bit_to_group` 保存。对非锚组 mask $S\subseteq [k]$：

- $D(S,v)$：覆盖 $S$ 对应的所有非锚组并以 $v$ 为根的最小树代价； $D(\varnothing,v)=0$。
- $A(S,v)$：覆盖永久锚组 $K_a$、 $S$ 并以 $v$ 为根的最小树代价。
- $H(S,v)$：adjoint 阶段的“外侧已付代价”；它覆盖 $[k]\setminus S$，等待与覆盖锚组及 $S$ 的前缀在 $v$ 处相接。

`D/A/H` 共用一种 `Row`：递增的顶点数组、对齐的值数组、ordinary 专用 branch bitmap、`branch_count` 和显式 `ready`。对需要跨阶段判断可用性的 ordinary、提前 A1 与 $H$ row，`ready` 区分“已经发布但为空”和“尚未生成”。普通 forward $A$ 的构造内核有一个局部省写规则：严格层序已经处理、且从未产生有限标签的空 row 可以同时保持空 payload 与 `ready=false`；同一次前向构造及后续 adjoint 只按 payload 读取它，结果恒为无穷，不把该位当作生命周期判据。不存在 Base/Enhanced 各一套 row，也不存在运行中在 dense/hash/bitmap DP 状态之间切换。

单组到全图的距离  $d_i(v)=\min_{t\in K_i}\mathrm{dist}(v,t)$ 存在 `GroupRow`，它不是 DP row。Base 可保存有界精确锥体；开启 `DirectedCut` 后保存完整 dense 距离。两种布局通过 `operator[]`、`ExactValueOrInf` 和 `ForEachExact` 暴露同一语义。

### 5.1 `Row` 的逻辑值与物理 payload

一张 DP row 的逻辑键是 `(mask, vertex)`。物理上只存有限、仍可能改善 incumbent 的顶点：`vertex[j]` 严格递增，`value[j]` 是同一下标的精确 DP 值。读取不存在的顶点返回无穷。凡是消费者需要询问生命周期，`ready=false` 表示依赖还没有发布，`ready=true` 且数组为空表示该 row 已被完整计算但安全锥体为空；二者不能混淆。唯一不以 `ready` 暴露生命周期的是普通 forward $A$ 内部的零标签行：严格 size 层序已经证明它被处理，空 payload 又逐点等价于无穷，因此省去一次布尔写入不会让任何消费者误判依赖。

ordinary row 额外带一张按 payload 下标而非原顶点编号排列的 64-bit branch bitmap。第 $j$ 个 payload 是否为规范 branch，由 `branch_bits[j >> 6]` 的第 `j & 63` 位给出。 $A$ 与 $H$ 沿用完全相同的有序 `vertex/value/ready` 布局，只是不解释 branch 位。这样，三种状态可以共用二分读取、双指针交集、诊断统计和所有权移动。

### 5.2 `GroupRow`、cutoff 与单次精确读取

有界 `GroupRow` 对每个顶点暴露一个有限读取值，但该值有两种含义：

```math
\texttt{row}[v]=d_i(v),
\qquad d_i(v)<U_0.
```

```math
\texttt{row}[v]=U_0,
\qquad d_i(v)\ge U_0\text{ 且精确值未保存}.
```

这里的 membership 不是浮点精度标记，而是“这个位置是否保存了真实多源最短距离”。cutoff 占位值可以作为不大于真实距离的拒绝证书，却不能作为 DP seed。若真实距离为 137、cutoff 为 100，把占位的 100 加进 split 会构造不存在的低成本树。需要随机读取真实 singleton 的消费者统一调用 `ExactValueOrInf(v)`：它只做一次 dense/bitmap 定位，精确位置返回真实距离，非精确位置直接返回正无穷。旧的“先判断 membership、再用另一入口重读同一位置”已被该单次合同严格替代，避免重复定位和漏检 membership。

有界 dense 布局以 `value[v] < cutoff` 判定精确 membership。稀疏布局用 `bits[v >> 6]` 判 membership，再通过该 64-bit word 之前的 `rank` 与 word 内低位 `popcount` 定位压紧的值数组。完整距离布局中每个位置都是真实值，`ExactValueOrInf(v)` 直接返回 dense payload。这三个物理分支只实现一个读取契约，不产生三种算法状态。枚举消费者使用 `ForEachExact`，需要下界占位的消费者才使用 `operator[]`。多源 Dijkstra 只在标签从无穷首次变为有限时把顶点加入 `touched`，其后只允许严格改进，因此同一组内的 `touched` 天然无重复。完整或 bounded-dense 布局直接取得 dense 数组，不再排序；ranked-bitmap 布局只为递增 payload 排序一次，不再执行结果必为空的 `unique`。这一减空不依赖边权为整数，也不改变堆并列、距离值或顶点顺序。

### 5.3 两套 mask 编号为什么同时存在

查询原始组号使用 $g$ 位：`original_full_mask=(1<<g)-1`，锚组单独占 `anchor_bit`。删除锚组后，DP 只对 $k=g-1$ 位编码，`full_mask=(1<<k)-1`。`bit_to_group[b]` 把压缩 bit $b$ 还原为原组号；`original_mask[S]` 把整个压缩 mask 还原为原始 $g$ 位集合。二者不能混用：

- ordinary row、锚定 row、补集转置和 `popcount` 使用压缩 mask；
- 需要连同锚组调用全组 future、tour 或 dual 时，使用 `anchor_bit | original_mask[S]`；
- `full_mask ^ S` 只在已经确认 $S$ 是压缩全集 `full_mask` 的子集时表示非锚补集；原始全组补集使用 `original_full_mask ^ original_mask[S]`。

选择永久锚后，所有上述数组一次性建立且查询内不再改变。这样可把普通状态数从 $2^g$ 降到 $2^{g-1}$，同时让 $A(\varnothing,v)=d_a(v)$ 作为隐式基例，不必为锚 singleton 再保存一张全图 row。

### 5.4 `Row` 的生命周期与所有权

一张需要显式发布的 row 依次经历四种物理状态，但逻辑状态只计算一次：

1. `ready=false`：依赖尚未完成，任何消费者都必须跳过；
2. 构造中：局部 `distance/touched/settled` 保存候选，尚未对其他 mask 可见；
3. `ready=true`：`vertex/value` 已排序、对齐并只读，可为空；
4. 被消费或移动：末层可以只用于答案结算而不长期保留；提前 A1 则把同一个 `Row` 的所有权移动到前向容器。

普通 forward $A$ 的零标签行采用更窄的构造内协议。对固定 size，mask 按确定次序逐一处理；后继 $A(S)$ 只读取真子集 $A(S\setminus T)$，所以读取发生时该真子集的构造已经结束。若该真子集的 `touched` 为空，它没有任何有限 `(mask,v)` 值，`RowValue` 对空 payload 在所有顶点都返回无穷。实现因此直接进入下一个 mask，不写 `ready=true`；同一前向内核和 adjoint 边界都直接读取 payload，而不会把这一位提交给 `OrdinaryAvailable`、A1 owner 交接或跨阶段调度。这个省写不适用于 ordinary、提前 A1 或 $H$：这些 row 的外部消费者确实使用 `ready`，即使为空也必须显式发布。

`touched` 记录一张 row 内从无穷第一次变为有限候选的顶点，`settled` 记录真正从优先队列以当前最优值取出的顶点，最终 payload 还会按最新 `best` 再过滤。故三者大小可以不同。`mask_vertex_states` 对每张 row 累计 `touched` 的不同顶点数：状态族属于实际键，因此 D/A/H 中数值相同的 `(mask,v)` 是不同状态项。它描述实际进入主状态工作区的数量，不等于结束时 payload，也不等于队列弹出或松弛次数。A1 移交时不再次累计，这一所有权规则由状态计数回归测试约束。

## 6. 公共预处理：下界、上界与锚组

### 6.1 零权分量覆盖下界

先把所有零权边缩成免费连通分量。每个分量携带它命中的组集合 $C_j\subseteq[g]$。在这些集合上执行精确 set cover DP，令 $c_0$ 为覆盖全部查询组至少需要的零权分量数。若 $c_0=1$，某个零权分量已覆盖全部组，故 $\mathrm{OPT}=0$。

若 $c_0>1$，令 $w_+$ 为最小正边权。任何连通可行树在零权分量缩点图上至少连接 $c_0$ 个分量，因此至少使用 $c_0-1$ 条正权边：

```math
L_{\mathrm{cc}}=(c_0-1)w_+\le\mathrm{OPT}.
```

若图没有正权边但查询可行，则必有 $c_0=1$。图加载器已经缓存全图最小边权。若该值严格为正，查询预处理可立即知道不存在零权边和 $w_+$，不再扫描 $m$ 条边，也不再分配长度 $n+1$ 的并查集；它只排序并聚合查询实际出现的 $F$ 个单点分量记录。只有图确实含零权边时，才扫描原边、建立零权并查集并同时求 $w_+$。两条路径都只为查询触及的分量保存 mask 和代表点，不再分配两张长度 $n+1$ 的分量数组。排序的第二关键字和聚合后的复排恢复查询首次触及顺序，因此 set-cover 并列根与旧实现一致。set cover DP 本身使用 $O(3^g)$ 时间、 $O(2^g)$ 空间，并返回若干代表顶点供上界构造使用。

### 6.2 距离—根初始化的共同合同

对已通过共同分量检查且未被零代价分量条件闭合的查询，所有合法配置都只调用一次 `BuildDistanceRootInitialization`。 $g\le3$ 的数学基例统一传入 Bootstrapped-bounded 并在返回后结束；仅对 $g>3$ 的非基例，才按冻结配置选择本节后述的两种 realization。无论走哪条路径，函数都返回同一个结构：

```math
\mathcal I_{\mathrm{dist}}
=
(\mathcal D,r,U_0),
```

其中 $\mathcal D$ 是由 `GroupRow` 组成的距离 oracle， $r$ 是后续路径并集与锚组选择使用的候选根， $U_0$ 是可由原图真实边展开的可行上界。`PrepareProblem` 不再分别调用“Base 的 SPT”或“Enhanced 的距离阶段”；它只消费这一共同三元合同。两种 realization 的差异完全封装在构造函数内部：

- **Bootstrapped-bounded。** 先按每组顶点数、最小顶点号的稳定顺序选规范终端作为候选根。对每个候选根运行 Dijkstra；遇到尚未覆盖的组时恢复根到该顶点的 SPT 路径，并按原图 `edge_id` 去重。若某个候选根能够覆盖全部组，最小真实边并集给出有限 cutoff，随后每组多源 Dijkstra 只 settle $d_i(v)<U_0$ 的顶点。非连通图中，各组规范最小终端可能没有共同分量；此时 bootstrap 暂为无穷，多源搜索自然保留所有有限标签，最后同一个共同根扫描会在前置检查已经证明存在的公共分量中取得有限的 $r,U_0$。
- **Complete-potential。** 每组多源 Dijkstra 保存所有顶点的真实距离，再由完全相同的共同根扫描得到 $r,U_0$。它不需要在完整距离之前另求 cutoff，但为此支付完整的 $g(n+1)$ 距离存储和全搜索工作；这些距离还供 directed-cut 势使用。

因此，规范 SPT 只是在第一种 realization 内使 bounded 表成为可能的 bootstrap，地位与第二种 realization 内“把所有距离扩展到底”相同：二者都是各自物理表示的内部成本，不是只有 Base 才拥有、而 Enhanced 没有对应职责的论文阶段。强迫 Complete-potential 再运行 SPT 不会改变其输出合同，只会增加至多 $g$ 次单源 Dijkstra，故当前实现不做这种冗余逐指令包含。

Bootstrapped-bounded 的候选根搜索复用一组 `distance`、`parent_edge` 和 edge bitmap：每轮顺序重置距离与位图，父边只在本轮已发现顶点上读取，因此无需清零；bounded 多源距离在最终选择稀疏 `GroupRow` 时也复用同一 dense scratch，只重置该组触及的顶点。复用不改变堆序、父边、cutoff、精确 membership 或输出 row，只消除同一查询内重复的大块申请；它属于相应 realization 内部的物理优化，不形成 Base 独占的外层逻辑职责。

bounded `GroupRow` 的未保存位置由 `operator[]` 返回 cutoff $U_0$，但 `ExactValueOrInf` 对同一位置返回正无穷。任何严格优于 incumbent 的完成解中，单个必需连接代价不可能达到或超过 $U_0$；因此 cutoff 可作拒绝下界，却不能作为 DP singleton。枚举型 DP 消费者使用 `ForEachExact`，随机读取型消费者使用 `ExactValueOrInf`。有界表再按实际字节在两种等价布局中确定性选择：

- dense bounded：长度 $n+1$ 的值数组，锥体外写 cutoff；
- ranked bitmap：递增精确顶点与值、membership 位图、每个 64-bit word 的 rank 前缀。

### 6.3 初始化包后的真实路径并集

对任意根 $r$，分别连接到各组最近终端可得：

```math
U_{\mathrm{star}}(r)=\sum_{i=0}^{g-1}d_i(r).
```

该和可能重复计算共享边，但仍是可行上界。两种 realization 都在初始化包内部从精确顶点数最少的组表驱动同一个扫描，并把最小值及其根写入 $(U_0,r)$。若 $g>3$，包返回后公共外层再沿满足最短路等式的真实边恢复根到各组的路径，按 `edge_id` 去重，得到通常更紧的 $U_{\mathrm{union}}$。路径等式使用 $10^{-9}$ 只为在浮点输入上找到真实边序列；即使选择了近似等式边，最终仍按原图边权对实际边并集计价，所以它只能影响上界强弱，不能产生虚假下界。

### 6.3.1 共同的有序组三元组一步前瞻路径生长上界

对每个互异的有序组三元组 $(i,j,k)$，先在 $K_j$ 中选取到 $K_i$ 距离最小的规范终端，并在 $d_i$ 的 tight-edge 子图中恢复种子路径；随后显式把第三组 $K_k$ 以距当前树最近的真实最短路接入。完成这一次有限前瞻后，每轮选择当前树到未覆盖组的全局最短距离并继续生长。枚举全部 $k$ 严格包含旧的 pair-seeded 贪心：其中一个 $k$ 恰是旧规则第一次选择的组；其余 $k$ 提供不同的早期共享结构，而不是由数据驱动选择分支。物理实现把候选枚举按有序对 $(i,j)$ 因子化：种子 tight path 只恢复一次，保存其真实 `edge_id`，然后在每个 $k$ 开始时重放同一批种子边。因此候选集合、并列规则、计价和终止条件与逐三元组重建完全一致，只少做重复的路径恢复。路径触及的全部组同时标记为覆盖；原图边按 `edge_id` 去重并按输入权值计费。

恢复过程只读取 `ExactValueOrInf` 返回有限值的距离位置；每个 DFS 顶点先缓存当前 tight distance，再扫描邻边，避免对同一稀疏 membership/rank 位置重复定位；bounded 表缺失的路径只会使当前起点失效，不会把 cutoff 当成边权。DFS 使用访问 epoch，因此零权 tight-edge 环不会造成恢复环。每条成功路径与已有树相交于起点，故中间结构始终连通；覆盖全部组时即得到真实可行子图。边权非负且边只会加入，所以已选边费用单调不减。若当前真实树为 $T$、已付费用为 $w(T)$，则任何继续覆盖组 $K_i$ 的连通扩展至少再付 $d(T,K_i)=\min_{v\in V(T)}d_i(v)$：把 $T$ 收缩为零代价起点，取扩展路径最后一次离开 $T$ 的位置即可。因此代码在 tight-path DFS 前先检查 $w(T)+d(T,K_i)$ 是否仍严格小于 incumbent；失败时省略的恢复不可能产生更优候选。种子、显式第三组、购买后的第四组和后续最近组使用同一必要条件。该短路不含阈值，也不改变任何可能严格改善 incumbent 的候选、并列规则或最终上界。一旦已付费用本身不再严格小于调用前的 `best` 或本模块已找到的更优值，该起点同样可以无参数地安全终止。三个合法配置在各自 witness 构造后调用同一函数；Enhanced 较早取得的 dual/facility incumbent 只会让同一条件更早成立。

### 6.4 至多三组的共同精确闭包

对任意两个组，连接它们的最优代价等于两组间最短距离，也等于 $\min_v\sum_i d_i(v)$。对三个组，任取一棵最优树及其命中的三个终端；三终端最小子树存在一个分叉点或退化分叉点 $v$，从 $v$ 到三个终端的树内路径边不重不漏地覆盖该子树。因此：

```math
\min_{v\in V}\sum_{i=0}^{g-1}d_i(v)
\le \mathrm{OPT},
\qquad g\le 3.
```

反向地，对任意 $v$，取 $v$ 到每组最近终端的最短路并集即可得到一张连通可行子图，其去重代价不超过距离和，所以：

```math
\mathrm{OPT}
\le \min_{v\in V}\sum_{i=0}^{g-1}d_i(v).
```

两式合并得到精确恒等式。Bootstrapped-bounded 以真实可行 cutoff $U_0$ 初始化：若最优距离和严格小于 $U_0$，其每一项都严格小于 cutoff，故最优根在全部组表中均为精确位置并必被扫描；若最优值等于 $U_0$，已有真实上界已经等于最优值。因此这一 realization 已足以精确求出低组基例。

`PrepareProblem` 因此让 Base、DirectedCutOnly 与 Enhanced 都运行同一个 Bootstrapped-bounded root-star 包，并在同一位置返回，不再构造 complete-potential 表、root-path/dual-primal witness、tour、directed-cut、A1 或指数状态表。这里没有“Enhanced 临时改成 Base”的配置选择：完整算法在数学基例处尚未进入任何 enhancement realization，三个配置逐项执行同一闭包。 $g=0,1$ 仍由更早的入口平凡条件处理。

### 6.5 锚组选择

在当前最好共同根 $r$ 处，选择 $d_i(r)$ 最大的组作为永久锚组。直观上，把最远组固定到所有锚定状态中，可较早暴露长连接并增强 future 拒绝。该选择只影响状态组织，不影响可枚举解集合；并列时按组号稳定选择。

### 6.6 组间 tour 下界

定义组间松弛距离：

```math
\delta(i,j)=\min_{u\in K_i,v\in K_j}\mathrm{dist}(u,v).
```

代码等价地在 $K_j$ 上取 $d_i$ 的最小值。有界距离返回的 cutoff 不大于被截断的真实距离，因此即使矩阵因截断而不完全对称，仍保持下界方向。

对每个组子集和一对固定端点，subset DP 预计算访问该子集中每个组一次的最短 Hamilton path。查询顶点 $v$ 与剩余组 mask $R$ 时，把 $v$ 分别接到路径两端；对每个被指定为端点的组取最小，再在端点组上取最大，最后除以 2，得到 $L_{\mathrm{tour}}(v,R)$。证明来自树的倍增：任意从 $v$ 出发覆盖 $R$ 的树，边倍增后存在长度至多两倍树权的闭合遍历；在组度量中 shortcut 并指定任一组作为首个端点不会增长。因此每个 fixed-endpoint 值除以 2 都不超过剩余树代价，最大值仍可采纳。

实现还为每个非 singleton mask 保存“固定起点、终点自由”的最短 Hamilton path 值，并取这些值在起点组上的最大值 $P(R)$。若 $M=L_{\mathrm{far}}(v,R)$，则完整 rooted tour 的每个固定起点项都至多为 $2M+P(R)$，所以

```math
L_{\mathrm{tour}}(v,R)\le M+\frac{P(R)}{2}.
```

右侧是完整 tour 求值的常数时间上包络，不是新的剪枝下界。若当前已知的可采纳下界已经不小于该上包络，继续枚举端点不可能改变各证书的最大值，因而可等价跳过 `TourLowerBound::At`。该判断不读取图名、组数阈值、状态量或时间；它只消除结果必定被已有下界支配的求值，队列 key、保留状态和浮点精度均不改变。

### 6.7 最远组与统一 future

最便宜的 future 是：

```math
L_{\mathrm{far}}(v,R)=\max_{i\in R} d_i(v).
```

每个顶点只缓存全体组中唯一的最远组；预处理在一次组扫描中用局部最大值同时构造该 argmax，不保存固定深度排名。若该组仍在 $R$ 中可 $O(1)$ 返回，否则严格扫描 $R$。完整势 realization 的 `GroupRow` 已知是 dense，miss 路径直接读连续值数组；bounded realization 仍通过共同布局 oracle 读取 cutoff 证书。D、A、H 都调用同一个 `FarthestRemaining`，所以这只是同一数学 oracle 的布局专门化，不增加空间、不改变并列规则或比较顺序，也不存在经验 top-k 深度。Base 的统一下界为：

```math
L_{\mathrm{future}}(v,R)=\max\{L_{\mathrm{far}},L_{\mathrm{tour}}\}.
```

开启 `DirectedCut` 后再取 directed-cut 下界 $L_{\mathrm{cut}}$ 的最大值。Base 的 farthest、公共 A1 与 tour 是一个廉价的 flat realization：首次存活候选把完整 future 写入 row-epoch cache，后续标签直接复用。开启 `DirectedCut` 后，最前面的 dual certified interval 可能在尚未计算后续证书时拒绝当前标签；为复用这种安全早停，DirectedCutOnly 与 Enhanced 采用同职责的 staged realization，把统一 future 拆成 directed-cut、farthest、公共 A1 和 tour 四个单调阶段。每个顶点按 row epoch 记录已经完成到的阶段及其当前最大下界。若某个较大标签在中间阶段被拒绝，后续更小标签先用已缓存下界复查，不足以拒绝时从下一未完成阶段继续，而不是错误地把一次拒绝永久化；只有执行到最后阶段后，缓存才是完整 future。

directed-cut 阶段还区分“安全区间值”和“完整证书值”。residual closure 前，证书按剩余组逐项精确求和；closure 后，代码先用预计算全势减去已覆盖组势，并以非负浮点求和的标准误差界得到一个向下/向上包围区间。区间下端足以拒绝当前标签时，只缓存该可采纳下端，不把阶段标为完成，因为更小标签可能需要更强值。若区间上端已经严格小于当前剩余预算，则当前标签可以放行；若它随后在更强阶段被拒绝，新下界会拦住所有不小于它的标签，若它最终被接纳则真实 `distance` 做同一件事。因此以后还能重新进入尚未完成阶段的标签必然更小，也必然能通过 dual；代码可以用缓存下端作为后续阶段的可采纳起点并结束 dual 判定职责，而不声称下端等于完整势。区间无法判定时才执行逐组 `double` 求和；若这个完整值仍拒绝当前标签，`exact=true` 使代码也能结束 dual 阶段，以后越过缓存拒绝边界的更小标签直接复用同一精确和，不再重复扫描剩余组。closure 前本来就逐组求和，同样返回 `exact=true`。这里复用的是候选无关证书或由缓存/真实距离顺序共同保证的单调放行事实，不是把前一个候选的拒绝结论永久化；没有压缩精度、改变求和顺序或用区间下端冒充 exact 值。两种 realization 的输入都是 `(S,v,value,best)`，输出都是“同一证书最大值是否允许严格改善”；差异由预声明的 DirectedCut 安全新增位唯一决定，不读取图名、组数、时间、内存或 row 统计。

staged realization 还维护一个 row-local 单调拒绝前沿。单次缓存拒绝只登记 `rejected-seen`；同一 row 已经观察到复用后，才把被拒绝标签写入既有 `distance` 工作区并登记 `rejected-frontier`。这是确定性的 break-even：没有第二个消费者时不支付写入与 row 末清理，有复用时才用一次虚拟写入替代后续证书入口；它不读取图名、组数、组大小、状态量或墙钟。虚拟值不入堆、不进入 settled row，也不计为 `(mask, vertex)` 状态。更小标签若通过完整证书链，crossing 分支先把顶点登记到 `touched` 并清除前沿位，再由调用者写入真实距离；row 结束时 `rejected` 列表把未 crossing 的虚拟位置恢复为无穷。

stage 0 表示 directed-cut interval 尚未完成 exact/upper 判定。在该阶段首次物化前沿时，代码不只保存偶然遇到的拒绝标签，而是从 `best - bound_cache[vertex]` 构造解析 cutoff，并用 `nextafter` 向上修正到按原来 `double` 加法顺序确实满足 `!(x + lower < best)` 的首个可表示非负值。浮点加法对非负标签单调，所以不小于 cutoff 的标签都必被原谓词拒绝；上界以后只会下降、缓存下界只会保持或加强，旧 cutoff 至多变弱而不会失效。stage 1--4 已经完成 dual 职责，仍只保存实际拒绝值，避免让 P1 中占主导的普通拒绝支付 eager cutoff 成本。该分类来自证书状态机语义，不是对参数的经验分段。

### 6.8 预处理顺序为何固定

当前顺序不是可交换的实现细节：零权分量下界先提供安全闭合条件；若尚未闭合且 $g\le3$，所有配置调用同一个 Bootstrapped-bounded 距离—根初始化包，并由第 6.4 节恒等式直接返回。仅对 $g>3$ 查询，外层才按冻结 profile 选择距离 realization：Bootstrapped-bounded 先尝试得到 cutoff 再构造距离，Complete-potential 则完成全部距离；两者都一次返回距离 oracle、候选根和初始真实上界。公共外层随后构造真实路径并集、锚组与 tour，最后构造所选 witness，再共同执行第 6.3.1 节的有序组三元组一步前瞻路径生长上界。Base 将共同 root-path union 整理为 root-path tree；DirectedCut 配置由 primal 边整理 dual-primal tree，并额外得到 primal/facility 可行上界。两边的预处理都在树构造完成后返回，不在这里无条件调用 `EvaluateWitnessTree`。

`SolveOneQuery` 在预处理返回后才构造一个 `WitnessUpperScheduler`。因此所有配置的 `rent` 都从 0 开始，而不是让 Base 预付一次树 DP、Enhanced 从零开始。root-path tree 与 dual-primal tree 是同一 witness 输入职责的 realization 替换；共同调度器、共同 `buy` 公式和共同树 DP 则是逐项相同的后续操作。root-star 由全部配置执行；对未被第 6.4 节闭包的查询，root-path-union 也仍由全部配置执行。facility 上界仍是独立安全新增，不能把这些事实改写成 Enhanced 删除了共同上界。

## 7. 普通状态 $D$ 与规范 branch

### 7.1 精确递推

对 singleton， $D(\{i\},v)=d_i(v)$。对 $|S|\ge2$，先在共同根合并两个真子集，再在图上做多源最短路闭包：

```math
B(S,v)=\min_{\varnothing\neq T\subsetneq S}\{D(T,v)+D(S\setminus T,v)\},
```

```math
D(S,v)=\min_{u\in V}\{B(S,u)+\mathrm{dist}(u,v)\}.
```

实现按 $|S|$ 递增，只生成到层计划要求的 ordinary 边界。size 2 只有一个规范拆分，直接求两个 singleton 的共同精确顶点；更高层固定 mask 的最低 bit 在 accumulator 侧，只枚举不含该 pivot 的 branch，从而消除左右对称和重复拆分。固定 $(S,v)$ 的 future 只依赖最终的 $S$ 与 $v$，不依赖产生 seed 的拆分 $T$。因此高层先精确聚合全部规范拆分的逐顶点最小值 $B(S,v)$，再对每个有限顶点运行一次统一 future；这与逐拆分先筛选再取最小完全等价，却不会为随后被更优拆分覆盖的候选重复求证书。size 2 保留单遍路径，避免为唯一拆分支付二次遍历。

### 7.2 A* 式稀疏图闭包

每个同根 split seed 的真实已付值为 $x$，队列 key 为：

```math
f=x+L_{\mathrm{future}}(v,[g]\setminus S).
```

只有 $f<U$ 的候选进入或继续传播。因为 $L_{\mathrm{future}}$ 不超过任何完成代价， $f\ge U$ 的状态不可能导出严格优于已经存在的可行解；等于 $U$ 时也无需保留，因为同成本答案已存在。剪枝不会改变最优权值。

### 7.3 branch 的定义与完备性

实现同时保存图闭包前的 seed 值 $B(S,v)$。若最终 $D(S,v)<B(S,v)$，到达 $v$ 的最优实现必须从另一根经过至少一条边，代码把它标为 branch；若 $D(S,v)=B(S,v)$，则该状态在 $v$ 处仍有一个同根真子集拆分，不发布 branch。

这不会删掉解。考虑任意更高层在根 $v$ 使用一个未标 branch 的 $D(S,v)$：用实现其等值的两个真子状态替换，成本不变；反复替换后必到 singleton 或某个经过图边闭包的 branch。于是每个同根合并等价类至少保留一个规范分解。后续只要求“被接入的一侧”为 branch，accumulator 仍可是任意 ready 值，因此不会要求所有子块同时为 branch。

### 7.4 平衡完成式与三分完成式

当 ordinary 层达到半格时，代码用 $D(S,v)+D(\bar S,v)+d_a(v)$ 产生真实完整上界；在三块大小达到平衡点时，还枚举三个 ordinary 块与锚组共同相遇的完成式。它们只更新 $U$，不把某个启发式完成值当成精确状态，也不替代尚未生成的 row。

### 7.5 一张 ordinary row 如何生成

下面展开 `BuildOrdinaryRows` 对一个非 singleton mask 的实际顺序。singleton 的逻辑值直接来自 `GroupRow`，不复制成 `ordinary[bit]`；`OrdinaryValue` 和 `ForEachOrdinaryValue` 对调用者隐藏这一区别。公开入口只根据冻结的 `DirectedCut` 位分派一次 flat/staged 编译期实例，mask 递推和下述闭包代码仍只有一份；这只是避免在每个候选处重复读取查询配置的机器实现，不是新的证书、数据相关选择或论文算法步骤。

```text
for size = 2 .. h:
  for each mask S with |S| = size:
    split[v] <- infinity
    for each canonical split S = P disjoint-union B:
      枚举 D(P,v) 与 branch D(B,v) 的共同精确顶点
      split[v] <- min(split[v], D(P,v)+D(B,v))

    对每个有限 split seed x at v:
      Base 复用完整 flat future；DirectedCut 配置复用 staged future 前缀
      仅当 x+完整 lower < best 时放入最短路队列

    对全部初始 seed 线性 heapify，再做非负边权图闭包
    每次松弛前用该标签自己的值检查当前配置的同职责 future realization
    对 settled 顶点按编号排序、去重，并写入 D(S)
    若 D(S,v) 严格小于 split[v]，设置对应 branch bit
    累计本 row 的首次发现状态
    执行当前 size 能触发的两块/三块完整上界结算
    将 queue-pop/edge-relax 工作支付给 A1 延续下来的共同 scheduler
    若 rent 达到 buy 且 ordinary 输入修订已变化，调用共同树 DP
```

同根 split 与图闭包刻意分开保存。`split[v]` 用来判断规范 branch，`distance[v]` 是闭包后的精确 rooted 值；若只保留后者，就无法区分“在当前根仍可继续零边拆分”与“必须从别的根经图边到达”的状态。高层聚合时 `touched` 先登记所有有限 split，future 筛选后在原数组内压紧为实际初始标签；被拒绝位置立即把 `split` 恢复为无穷，使它后来若仅由图闭包到达，仍正确分类为 branch。所有临时 dense 数组用这份触及列表或 epoch 恢复，而不是每张 row 清空 $n$ 个位置。

## 8. 真实 witness 与共同 rent-or-buy 上界

### 8.1 两种树来源、一个树 DP 接口

根路径并集或 directed-cut primal 会被整理为一棵以锚组终端为根的真实树。零权边上只允许严格距离改进来设置父指针；等距候选保留第一次父亲，避免 settled 祖先被重新挂到后代形成环。构造后还验证所有 witness 顶点可从锚根到达。Base 只构造 root-path witness，DirectedCutOnly/Enhanced 只构造 dual-primal witness；两边都不在预处理中无条件求树 DP。

`EvaluateWitnessTree` 是唯一的树 DP 实现。它给真实 witness 增加一个虚拟超根，在每个真实树顶点 $x$ 上把已经可用的 rooted 块组合为局部值，再依次合并子树。singleton 块读取精确组距离；多组块只读取已经 `ready` 的 ordinary row；缺失块保持无穷。合并一个孩子 $c$ 时，对当前 mask $S$ 枚举交给孩子的 $Q\subseteq S$，只有 $Q$ 非空才支付真实树边 $xc$ 的权重。故任何有限的超根 full-mask 值都能展开为 witness 边与若干真实 rooted 子树的连通并，只能作为可行上界收紧 $U$。

### 8.2 初始 witness 的共同 `buy` 与购买后的 support `buy`

令 $k=g-1$，当前配置的初始 witness 含 $t$ 个真实顶点。一个真实顶点处，固定最低 bit 去除左右对称后，全部非空局部拆分数为 $(3^k-1)/2$；一条父子关系上枚举全部 mask/submask 的总数为 $3^k$。每个真实顶点恰对应一次局部处理和一条通向父亲的关系，其中 witness 根通向虚拟超根。因此初始阶段的共同购买成本为：

```math
B_{\mathrm{wit}}(T,k)=t\left(\frac{3^k-1}{2}+3^k\right),
\qquad t=\lvert V(T)\rvert.
```

Base 与 Enhanced 的公式、整数运算和调用位置完全相同；配置只通过各自合法 witness 的 $t$ 进入公式。一次实际求值调用同一个 `EvaluateWitnessTree`，最坏时间为 $O(t3^k)$，工作空间为 $O(t2^k)$。公式没有图名、墙钟、经验组数阈值、历史查询速度或配置专属常数；它是与实现循环次数对应的确定性工作估计，不声称是精确 CPU 周期模型。

只有开启 DirectedCut 的配置在后续购买 residual closure 且取得更紧四元真实路径证书后，才把该路径边与 primal 边合并成 certificate support。设并图含 $s$ 个不同顶点。support evaluator 先做 $s$ 点 Floyd 闭包，再在每个 mask 上执行规范同根拆分与 support metric 闭包；其确定性购买成本为：

```math
B_{\mathrm{sup}}(s,k)=
s\frac{3^k-2^{k+1}+1}{2}
+(2^k-1)s^2+s^3+(2^k-1)s.
```

每一项分别对应非平凡规范拆分、各 mask 的 metric 闭包、Floyd 和初值写入。调度器只在证书真正替换后清零 rent、锁定当前 ordinary 修订并切换到这个公式；下一张 D row 提供新输入后才能再次购买。形式化地，support Floyd 的每个有限 metric 都对应 support 内一条真实原图路径；DP 初值要么是精确 singleton 路径，要么是已经可展开的 rooted D 子树；同根合并取连通并，metric 闭包再接一条真实 support 路径。归纳可得每个有限 DP 项都是连通真实子图，最终只在 support 中的锚组终端读取 full mask。因此 `EvaluateCertificateSupport` 仍然只是安全增加的可行上界。路径证书本身已直接写入 `best`；代码不再把同一批路径边额外重建成无人消费的 `witness_tree`，后续 evaluator 只读取路径边与 primal 边的并图。它不是 Base 初始 witness 的另一套隐式规则，也不改变 A1 或 ordinary 的状态语义。

### 8.3 从共同零点连续累计 A1 与 D 的 rent

预处理完成后才创建 `WitnessUpperScheduler`，构造时统一令 $R=0$。公共 A1 和 ordinary $D$ 都把实际 queue pop 与检查过的邻接项计为工作 $W$：

```math
R\leftarrow R+W.
```

初始证书下，当 $R\ge B_{\mathrm{wit}}$ 且树 DP 的输入修订相对上次购买已经变化时，调度器调用同一个 `EvaluateWitnessTree`，用返回的真实可行值收紧 `best`，随后令 $R=0$。第一次购买允许只使用隐式 singleton；此后每完成一张 ordinary row 都推进输入修订。若 residual closure 后切换为 support 证书，阈值改为 $B_{\mathrm{sup}}$，消费函数改为 `EvaluateCertificateSupport`，但“必须有新 ordinary 修订”与 rent 清零规则不变。若尚未有新 ordinary 信息，即使 rent 再次达到阈值也只保留累计值，等下一张 D row `ready` 后再购买，避免对完全相同的 DP 输入重复求值。

A1 结束时未消费的 rent 不清零，而是交给 D 继续累计；D 也不创建第二个调度器。这样 Base 与 Enhanced 的差异只剩 witness 大小及条件式树 DP 本身可能得到的上界强弱，不存在“Base 先无条件购买、Enhanced 不购买”的不对称路径。

### 8.4 A1 中收紧上界时为何必须整轮重启

A1 cone 与 cone 外 fallback 的证明要求一轮内共享固定 cutoff $U_0$。调度器可在 A1 的 queue-pop 安全检查点达到 buy；若树 DP 没有严格收紧 `best`， $U_0$ 未变，可以继续当前 row。若它把上界收紧为 $U_1<U_0$，代码先清空当前临时距离，再丢弃本轮此前的全部部分 A1 row，以 $U_1$ 从第一个 singleton mask 重启。调度器已经记录同一 ordinary 修订被求值过，所以重启轮不会再次购买；最终被接纳的一整轮始终使用一个固定 cutoff。

不能只保留购买前的 A1 行并让购买后的行使用新上界：后续队列会多出一种由 $U_1$ 触发的拒绝原因，而已有 fallback 仍由 $U_0$ 定义，这会破坏“同一轮固定 cone”这一证明接口。整轮重启把条件式早期购买与第 9 节证明重新对齐。被丢弃的尝试仍已支付调度 rent，但不属于最终发布的逻辑 `(mask,v)` 状态；状态统计只登记最终接纳并移交的一份 A1 row。

## 9. 严格公共的提前 A1 层

### 9.1 A1 的共同状态语义

对每个非锚 singleton $i$，公共锚定 singleton 状态定义为：

```math
A(\{i\},v)=\min_u\{d_a(u)+d_i(u)+\mathrm{dist}(u,v)\}.
```

式中 $d_a(u)+d_i(u)$ 是锚组与组 $i$ 在共同根 $u$ 合并的 seed，外层最短路闭包把根移动到 $v$。所以 A1 不是辅助启发式，而是完整前向 $A$ 递推的第一张真实逻辑 row。只要逻辑正层域非空，三个合法配置都在 ordinary 前调用同一个 `BuildReusableAnchoredSingletonLayer`；ordinary 结束后又把同一批 `Row` 移交给 `BuildForwardAnchoredRows`。前向阶段只按更新后的 incumbent 重滤并完成结算，不再次运行闭包。

共同操作具体包括：同一组 seed、同一 `distance`/stamp 工作区、同一 farthest + endpoint-floor queue key、同一 `Row` 布局、同一 top-two/完整 tail 分级 future 视图、同一精确 mask-rent 表、同一所有权移交和同一“不重复计数”规则。Enhanced 没有另一张 A1，也不会把 A1 交给 $H$。

“操作相同”不要求不同配置最终保存的 payload 逐字节相同。Base 与 Enhanced 在进入 A1 前可以通过第 3.1 节允许的组距离/上界 realization 得到不同但都合法的 `best`，所以严格 cone 的实际边界和状态数可以不同；bounded 与 complete `GroupRow` 的物理枚举范围也不同。但任何被接受的 seed 都由相同精确距离定义，后续执行同一闭包与 fallback 公式，且代码没有根据配置选择第二套 A1 规则。论文应把这种差异写成“共同 A1 内核作用于各配置已证明等价的预处理接口”，不能误称为两种 A1 算法，也不能反过来声称三者状态数必然相等。

### 9.2 三种配置完全相同的 A1 cone 与非负 fallback

设 A1 构造开始时的真实可行上界为 $U_0$。对组 $i$ 定义尚未由 A1 覆盖的非锚组集合 $R_i$。共同 continuation 由最远组下界和轻量端点路径下界取最大。

当 $|R|\ge 2$ 时，对任意起点组 $l\in R$，令 $P_R(l,r)$ 表示在组间松弛度量上从 $l$ 到 $r$ 且访问 $R$ 中每组一次的最短 Hamilton 路径，并定义终点自由路径值：

```math
h_l(R)=\min_{r\in R\setminus\{l\}}P_R(l,r).
```

预处理对每个 $R$ 固定选择使 $h_l(R)$ 最大的起点组；并列时选择组号最小者。记该组为 $l^*(R)$，则轻量端点路径下界和共同 continuation 分别为：

```math
C^{\mathrm{path}}(v,R)=\frac{d_{l^*(R)}(v)+h_{l^*(R)}(R)}{2},
```

```math
C(v,R)=\max\left\{\max_{j\in R}d_j(v),\ C^{\mathrm{path}}(v,R)\right\}.
```

当 $|R|=1$ 时，代码直接令 endpoint-floor 等于唯一的组距离 $d_l(v)$；它与 farthest 相同，因此共同 continuation 仍为 $d_l(v)$。空集合返回 0。这样 `EndpointFloorAt` 对空集、单组和多组的三个分支都与定义一致，也避免把单组代入没有终点 $r$ 的 Hamilton 路径公式。

这里没有可调端点数、组数阈值或数据集分支。固定 $R$ 后， $l^*(R)$ 与 $h_{l^*(R)}(R)$ 都是预处理常量；A1 热路径除原有 farthest 外只增加一次组距离读取。

先证明可采纳性。取任意从 $v$ 出发覆盖 $R$ 的可行树 $T$，并在每组选择树中实际命中的终端。固定一个起点组 $l$，从该组终端开始把树边倍增遍历，并以 $v$ 为最终终点；总长度为 $2w(T)-\mathrm{dist}_T(l,v)$。截去最后一个被访问组到 $v$ 的尾段，再在组度量中 shortcut，得到某个终点组 $r$，满足 $P_R(l,r)\le 2w(T)-\mathrm{dist}_T(l,v)$。又有 $d_l(v)\le\mathrm{dist}_T(l,v)$，因此 $d_l(v)+h_l(R)\le2w(T)$。该结论对每个 $l$ 成立，所以预处理选择其中最大者仍然安全。

再证明 cone 与 fallback 可以共用它。截断组距离  $\min\{d_l(v),U_0}$ 仍是 1-Lipschitz；乘以 $1/2$ 后， $C^{\mathrm{path}}$ 沿边至多下降半条边权。最远组下界是若干 1-Lipschitz 距离的最大值；二者再取最大仍是 1-Lipschitz。若 $x$ 是到 $y$ 的一条最短 A1 路径上的前驱，则：

```math
A(\{i\},x)+C(x,R_i)\le A(\{i\},y)+C(y,R_i).
```

因此三种配置都只保存满足 $A(\{i\},v)<U_0$ 且 $A(\{i\},v)+C(v,R_i)<U_0$ 的精确位置，不会在到达合法目标前剪掉其规范最短路径前缀。若 row 没有保存 $v$，同一个拒绝式给出非负 fallback：

```math
\underline A_i(v)=\max\{0,U_0-C(v,R_i)\}.
```

row 内返回精确 $A(\{i\},v)$，row 外返回该下界。cone 与 fallback 都只调用 `AnchoredSingletonContinuation`，不存在“用更强条件剪枝、却用较弱条件解释缺项”的证明裂缝。Base、DirectedCutOnly 与 Enhanced 逐项执行同一公式和同一代码路径。

### 9.3 为什么不把 DirectedCut 接入 A1 内核

从纯正确性看， $L_{\mathrm{cut}}(v,R_i)$ 也是可采纳 continuation，因而可以拒绝某些 A1 标签；但它与上一节的共同 continuation 只是功能相同，并非结构相同。farthest 和 endpoint-floor 都由最短距离函数与预计算常量组成，直接满足统一的 1-Lipschitz cone 和非负 fallback 证明；directed-cut 则是多组势的容量可行和。若只让 Enhanced 在 A1 内额外使用后者，row 外缺项便有两种配置相关的原因，不能再由同一个 $U_0-C(v,R_i)$ 解释。

让 Base 也构造 directed-cut 势虽然形式统一，却会把 $O(gn)$ 完整势、residual 处理和随机势读取变成 Base 的必付成本，消解 `DirectedCut` 作为可关闭增强的边界。它也不是轻量 endpoint-floor 的替代品：后者复用全部配置已经构造的 tour endpoint 表，只新增 $O(g^2 2^g)$ 预处理扫描和 $O(2^g)$ 存储，A1 每次 continuation 查询为常数额外工作。

当前方案因此保持严格边界：`BuildReusableAnchoredSingletonLayer` 不读取配置或 dual，只调用共同的 `AnchoredSingletonContinuation=max(farthest, endpoint-floor)`；DirectedCut 仍在 ordinary 的统一 future 中与 farthest、完整 tour、A1 future 取最大，并在 adjoint 中承担 reduced/prefix 证书。实验不得把 endpoint-floor 描述成 Enhanced 专属操作，也不得按图名或 $g$ 为它增加开关。

### 9.4 不含经验组数阈值的统一调度

调度只读取第 3.2 节的逻辑层域 $\mathcal L_A=\{1,\ldots,q\}$。域为空时，所有配置直接用隐式 $A(\varnothing)$ 完成；域非空时，A1 正是第一个成员，所有配置都在 ordinary 前生成它。Enhanced 的前向边界定义为 $\ell=\max\{1,\lfloor q/2\rfloor\}$，所以 A1 永远属于共同前向前缀，只有层 $2,\ldots,q$ 中的高层后缀才可能由 $H$ 替换。不存在“组数较小时延后、组数较大时提前”或任何等价隐藏阈值。

每个顶点第一次查询 A1 future 时，lazy 路径扫描所有 A1 singleton，缓存最大和次大的组 bit，以及对应精确值在 `row.value` 中的 32-bit 下标。若该值来自 cone 外，locator 的最高位统一记录上一节的非负 fallback。两个 locator 压在一个 64-bit 项中；后续若最大 bit 仍未覆盖就常数时间读取，否则优先读取次大值，只有两者都已覆盖时才进入 tail。

纯 lazy 路径在 future 只触及少量顶点时最省工作与物理页面，但在大图密集 A1 row 上，每个新顶点都对每张稀疏 row 做二分会形成明显的随机访问。代码因此先对 top-two 使用一次结构性 rent-or-buy。设第 $i$ 张已发布 singleton row 的 payload 顶点集合为 $Z_i$，令 $b_i$ 是在长度 $|Z_i|$ 的递增数组中二分一次的保守比较数。第一层购买式为：

```math
B_{\mathrm{scan}}=\sum_i\left(2n+(n-|Z_i|)(k+2)\right).
```

```math
R_{\mathrm{lazy}}=\sum_i(b_i+1),\qquad
\tau=\left\lceil\frac{B_{\mathrm{scan}}}{R_{\mathrm{lazy}}}\right\rceil.
```

$B_{\mathrm{scan}}$ 覆盖逐 row 顺序推进及缺项 continuation 的结构工作， $R_{\mathrm{lazy}}$ 是每个首次触及顶点必付的逐 row 二分工作；fallback 的额外成本不计入 rent，因此购买不会因把可选成本虚增为既付租金而提前。前 $\tau-1$ 个新顶点走 lazy 路径，第 $\tau$ 个新顶点完成后，代码按顶点编号和 singleton bit 同时递增扫描全部 row，并在已有的 `first/second/cached_locator_pair` 中填入同一结果。

top-two 已经物化后，若两者都不属于当前 remaining mask，旧路径仍需逐个剩余 bit 二分。令 $t=\max\{0,k-2\}$。实现从此时才累计真实支付的 tail 查找工作；一次 tail 查询的租金为：

```math
R_{\mathrm{tail}}(R)=\sum_{i\in R}(b_i+1).
```

这里每个 $b_i$ 只由已经发布的 row 长度决定，与顶点和运行历史无关。top-two 购买时，代码一次性构造整数表 $C$：

```math
C[0]=0,\qquad C[M]=C[M\setminus\{j\}]+b_j+1,
\quad j=\mathrm{lsb}(M).
```

对子集大小归纳立即得到 $C[M]=\sum_{i\in M}(b_i+1)$，所以后续一次表读取与原来的逐 bit 租金累加严格相等，第二级购买发生在完全相同的调用之后。该因子化支付 $O(2^k)$ 的一次性整数时间和空间，不改变 A1 值、浮点轨迹或 rent-or-buy 公式。


第二层购买成本为：

```math
B_{\mathrm{rank}}=B_{\mathrm{scan}}+
n\left(\frac{t(t-1)}{2}+t\right).
```

其中第一项保守覆盖再次顺扫 singleton row 与缺项 fallback，第二项覆盖每个顶点对 $t$ 个 tail bit 的稳定插入排序比较及 byte 写入。累计 tail rent 达到 $B_{\mathrm{rank}}$ 后，`MaterializeRankedTail` 为每个顶点保存 top-two 之外全部 bit 的完整非增次序。它不是固定深度 top-k：所有 $t$ 个 bit 都存在，每项只占一个 byte。查询按 byte 次序找到第一个仍在 remaining mask 中的 bit，再通过原 `Value` 路径读取一次精确 double；排名本身不保存、量化或替换数值。

A1 只在正层域包含第一层时构造；由 $\lfloor g/2\rfloor-1\ge1$ 可得 $g\ge4$、 $k=g-1\ge3$，而可行图有 $n\ge1$。因此 $B_{\mathrm{scan}}>0$，进而 $B_{\mathrm{rank}}>0$。第二级热路径只需比较累计 rent 是否达到 buy，不重复检查 buy 是否为正；这删除的是由逻辑域和购买式共同保证的恒真条件，不改变购买调用点。

两个购买式只读取 $n,k,|Z_i|$ 和实际已经执行的二分工作，不读取数据集名、查询编号、运行时间、配置位、状态数或经验组数阈值。第二层也不会在第一层尚未购买时记租：否则无法触发的计数只会给小查询增加热路径成本。两级调度都只是同一精确视图的物理 realization，不是新的下界或算法配置。两个全图物化函数在一条查询中各至多执行一次；实现把它们保留为非内联冷边界，避免 IPO 将一次性购买代码复制进逐状态调用的 `Future`。这只约束机器码布局，不是第三层购买条件或数据相关开关。

### 9.5 缓存正确性、精确因子化与生命周期

`BuildReusableAnchoredSingletonLayer` 为每个非锚 bit 建立一张 `ready` 的标准 row；即使安全 cone 为空，`ready=true` 也表示“该层已经处理完毕”，而不是缺失依赖。构造时固定的 $U_0$ 只定义本轮安全搜索域。若条件式树 DP 在本轮中把 `best` 降低，函数按第 8.4 节丢弃尚未发布的部分轮并用新 $U_0$ 重启；只有内层完整遍历全部 singleton bit 后，外层才会退出并初始化 future 视图。因此该初始化点本身就是发布屏障：所有 row（包括空 payload）均已 `ready`。只读 future 在屏障之后直接遍历完整 bit 域，不再对每次读取重复检查 `ready`；ordinary、H、forward 所有权交接等仍可能区分“未生成”与“已发布空”的生命周期不受影响。最终发布的一整轮只含一个 cutoff。A1 完成后 `best` 再下降，只会让更多已保存位置在移交时被重新过滤，不会使旧下界失效。

对固定顶点，设全部 singleton future 按值非增排列为 $x_1,x_2,\ldots,x_k$。若 $x_1$ 的 bit 仍在 remaining mask 中，答案为 $x_1$；否则若 $x_2$ 的 bit 仍在，答案为 $x_2$。两者都不在时，完整 tail 排名中的第一个 surviving bit 恰好实现剩余集合的最大值。lazy、top-two 顺序物化和完整 tail 三条路径读取同一 row 内 double 或同一非负 fallback；并列值只影响选中哪个等值 bit，不影响返回值。

精确租金表也不改变上述视图：由递推归纳，任意 mask 的表项恰等于旧路径按相同 bit 顺序累计的整数工作量，因此 `ranked_rent_work`、购买调用点和购买后的完整次序逐项不变。`Future` 在所有生命周期都返回“row 内真实 A1、row 外非负 fallback”这一统一视图的精确最大值，不把 fallback 冒充真实 A1，也不依赖调用点传入另一个下界。

ordinary 完成后，`first/second/cached_locator_pair/ranked_tail` 一并释放。随后 `std::move(singleton_future.row)` 把 row 容器交给 `RunForwardAnchoredStage`。前向内核看到 size 1 已 `ready` 时不会重新执行合并或图闭包，只按最新上界重滤、运行正常完整解结算，并跳过第二次状态累计。因此“提前供 future 使用”和“属于公共 A 格”是同一物理对象的两个生命周期阶段。A1 的 tentative 顶点在首次进入工作区时计入 `mask_vertex_states`；移交不重复计数。Directed-cut 势只在后续 ordinary/adjoint 证书中读取，其读取本身也不计为 DP 状态。

## 10. 前向锚定状态 $A$

### 10.1 递推与图闭包

锚定递推为：

```math
A(S,v)=\min_{u\in V}\left\{
\min_{\varnothing\neq T\subseteq S}
A(S\setminus T,u)+D(T,u)+\mathrm{dist}(u,v)
\right\},
```

其中 $A(\varnothing,v)=d_a(v)$ 为隐式 row，不为全图单独物化。对固定 $S$，内层枚举一个非空 ordinary 块 $T$，要求 $D(T)$ 已 ready； $A(S\setminus T)$ 要么是隐式空 mask，要么是严格较低 size、因而已经由同一前向层序处理。若较低 A row 没有有限标签，其空 payload 直接使交集为空。合并只在同一根 $u$ 发生，随后从所有有限 seed 做一次多源 Dijkstra 闭包。和 ordinary 一样，只有 `value + future < best` 的候选进入队列。

`ForEachAnchoredSum` 对 singleton 使用 `GroupRow` 的精确 membership；对多组 ordinary 块只消费规范 branch；对锚定侧读取完整值。这样，A 的每个同根合并都能展开为一个合法 ordinary 分块，同时沿用第 7.3 节的规范化完备性。seed 合并、图闭包、future 剪枝和稀疏 row 写回都由 `forward.cpp` 的一份实现完成。

### 10.2 每张 row 的上界结算

每个 settled $A(S,v)$ 会执行两种只收紧上界的操作。第一，把每个未覆盖 singleton 的组距离直接接到 $v$：

```math
U_{\mathrm{root}}=A(S,v)+
\sum_{i\in[k]\setminus S}d_i(v).
```

虽然不同最短路可能重复边，该和仍对应若干真实路径的并，因此是可行上界。第二，`CompleteAnchoredRow` 把剩余 mask $R=[k]\setminus S$ 分成至多两个 ordinary 块 $L$ 与 $R\setminus L$，并在共同根结算：

```math
U_{\mathrm{split}}=
A(S,v)+D(L,v)+D(R\setminus L,v).
```

实现从实际可枚举值最少的一侧驱动交集，singleton 通过 `ExactValueOrInf` 做一次定位并拒绝非精确位置，多组块要求 ready 并遵守 branch 规范。结算产生的是完整可行树候选，不写入新的 full-mask DP row。

### 10.3 为什么只到 $q=h-1$

令 $h=\lfloor g/2\rfloor$。Base 生成到 $|S|=h-1$；最后层只消费和结算，不保存 payload。其充分性来自平衡分解：去掉永久锚组后，任意完整规范分解都可选择一个包含锚的块，使其非锚组数不超过 $h-1$，余下组可分成至多两个大小不超过 $h$ 的 ordinary 块。所有这样的 left/right 子 mask 都在 `CompleteAnchoredRow` 中枚举。当 $q=\max\{0,h-1\}=0$ 时，直接用隐式 $A(\varnothing)$ 完成。

更具体地，把完整规范推导在最上层看作若干以同一根相接的最大子块，并选含锚组的块为锚定侧。若它包含超过 $h-1$ 个非锚组，则其余非锚组少于 $h$，可以沿锚定递推向下移动一次分界；反复平衡后，可令锚定侧大小不超过 $h-1$，而外侧至多拆成两个大小不超过 $h$ 的 ordinary 块。ordinary 已生成到 $h$，前向格只需生成到 $q=h-1$。最后一层只为结算存在，所以 `retain_last_layer=false` 时不保留 payload，不影响完备性。

### 10.4 公共前向内核的三种进入方式

`BuildForwardAnchoredRows` 只有一份实现，差别只在输入计划和初始 row 所有权：

| 调用者 | `last_size` | 初始 row | 末层是否保留 | 后续阶段 |
|---|---:|---|---|---|
| Base | $q$ | 已提前生成的 A1（若该层存在） | 否 | 直接返回最优值 |
| DirectedCutOnly | $q$ | 已提前生成的 A1（若该层存在） | 否 | 直接返回最优值 |
| Enhanced | $\ell$ | 已提前生成的 A1（若该层存在） | 是 | 把 A1 之后的低层边界交给 $H$ |

对每个 size，内核先检查初始容器中该 row 是否已经 `ready`。复用 A1 时，它仍执行与普通 A row 相同的 `CompleteAnchoredRow` 和上界重滤；只有尚未生成的后续层才枚举 $A(S\setminus T)+D(T)$ seed 并做闭包。`last_size=0` 只可能表示完整域 $\mathcal L_A$ 本身为空，此时计划设置 `complete_implicit_anchor=true`，直接以隐式 $A(\varnothing)$ 做最终结算。只要完整域非空，Enhanced 的定义就保证 $\ell\ge1$，不会出现“高层 H 存在但共同 A1 前缀为空”的特殊路径。所有情况都不会创建假的 size-0 payload，读取 $A(\varnothing,v)$ 时统一退回 $d_a(v)$。因此 Base、消融配置和 Enhanced 的低层不是三套递推，只是同一递推在不同已证明层边界上的调用。

## 11. `DirectedCut` 增强

### 11.1 对偶势

把每条无向边表示为两个同容量的有向弧。各组按根到该组的原始距离递减处理。第一个组直接使用距离势；后续组只从先前真正改变过 residual 的弧检查 Bellman 违反，再执行必要的最短路传播。每组势在根距离处截断，并从对应有向弧 residual 中扣除势差，始终截到非负。

对每个组 $i$ 得到势 $\pi_i(v)$。非负 residual 是可核验的对偶可行性证书：一棵从 $v$ 连到剩余每个组的树，在有向展开中必须跨过各组的一个有效割；顺序容量扣减保证不同组势对同一弧的总收费不超过该弧容量。因此：

```math
L_{\mathrm{cut}}(v,R)=\sum_{i\in R}\pi_i(v)
\le \text{从 }v\text{ 完成 }R\text{ 的最小代价}.
```

changed-arc 只减少每轮重新检查的弧，不改变最终 residual 最短路条件。得到根距离  $c_i=\pi_i(r)$ 后，代码把严格满足 $\pi_i(v)<c_i$ 的顶点记为本组 potential cone。cone 外所有顶点的截断势都逐位等于 $c_i$，所以两端均在 cone 外的边势差严格为 0，无需执行 residual 扣减。可能非零的边集合恰为至少一个端点在 cone 内的边。对任一无向边，两个有向势梯度不可能同时为正：若两端势不等，只有从高势端指向低势端的梯度为正；若相等，两向都为零。初始 cone 扣减与购买后的 residual 补全因此共用 `SubtractPositiveGradient`，每条被枚举边只标记并回写唯一可能为正的有向弧，零方向不再执行空的 `max` 回写。该替换与原来的双向 `max` 逐位等价，不改变浮点减法、residual、changed-arc 集或 primal 支撑。

为了不让 cone 很大时退化，代码先累计 cone 顶点的邻接项数。若该数小于 $m$，从 cone 邻接表枚举候选边，并让 cone 内边只在原边记录的 `u` 端处理一次、跨界边在唯一 cone 端处理一次；否则扫描原边数组，但立即跳过两端均不在 cone 的边。两种物理遍历执行完全相同的势差、changed-arc 标记和 residual 更新，检查的邻接/原边项数不超过原来的 $m$ 次全边扫描。这个选择只比较两种方式枚举同一数学支撑集所需的确定性项数，不读取图名、 $g$、时间、配置或证书强弱，也不改变势、residual、primal 或后续状态。

因此，当前构造可写成“changed-arc 最短路修复 + 截断 potential cone 容量更新”。前者缩小需要重新传播的区域，后者缩小需要扣减容量的边支撑；处理顺序和并列规则固定，不依据查询运行表现切换算法。整个一次性 dual 构造还有明确的跨编译器非内联边界，防止 Release IPO 把这一大段 Enhanced 冷路径并入 `PrepareProblem`，进而仅因指令布局拖慢未开启 `DirectedCut` 的 Base。该边界不改变任一配置执行的语句和证书，只约束机器码布局。

### 11.2 primal 与 facility 上界

全部组处理后，代码从根出发，在 residual 不超过固定数值容差 $10^{-10}\max\{1,w(e)\}$ 的有向弧上逐次连接尚未覆盖的组；每次仍以原边权运行 Dijkstra，把真实路径边写入 bitmap，并按真实新增路径成本计价。该容差只扩大 primal 候选支撑，不进入 dual 下界或上下界闭合；即使纳入一条并非数学零 residual 的弧，恢复结果仍由原图真实边计价，因而只可能改变上界强弱。若恢复成功，得到 primal 上界和 witness 树。

此外，取 primal 边涉及的顶点为 facilities。在同一数值零 residual 弧与 primal 树边构成的支撑图上计算 facility 间真实最短路，并做小规模 subset DP，得到第二个可行上界。所有数值最终都由真实原图路径组成；residual 只限制候选支撑，不直接充当答案。完成这两项后立即释放 $2m$ residual，只保留 $gn$ 组势和 primal edge bitmap。

### 11.3 residual closure 的 rent-or-buy 与证书刷新

初始 directed-cut 只保存截断势、changed-arc bitmap、primal edge bitmap 和恢复闭包所需的计数，随即释放 $2m$ residual。`ResidualClosureScheduler` 不按时间或图名决定是否重建，而是同时要求两个确定性条件：

1. A1 与 ordinary 已支付的 queue-pop/邻接检查 rent 达到 residual 重放、全势补全和 primal 恢复的静态结构成本；
2. 已完成 ordinary row 的累计 payload 至少为 $n$，避免在状态需求仍很稀疏时为 dense 全势过早付费。

静态购买价取 $2m+g(2m+n)$ 与 changed-arc/bitmap 重放、每组补全、原边扫描及顶点扫描之和的较大者。初始 dual 热循环不再维护一份已经无人消费的重复工作计数。两个条件都满足后只购买一次：按原顺序重放初始势扣减，以逆序补全剩余 residual 势，恢复新的 primal 与 facility 上界，然后再次释放 residual。

完整势随后转置为 vertex-major 连续布局。对“除已覆盖组外的全势和”，代码缓存每个顶点的全组和，并用标准浮点求和误差界构造保守区间：区间下端已不能改善时安全拒绝，上端仍可改善时安全接受；只有区间跨过 incumbent 时才回退逐组精确求和。该布局和区间只改变读取成本，不改变 lower bound 数值的安全方向。

若购买后的四元路径生长得到严格更紧的真实上界，代码把路径边与 primal 边合并为第 8.2 节的 certificate support，并让同一个上界 scheduler 按 support 结构成本等待下一次有新输入的购买。无论路径 support 是否替换，完整势、primal/facility 上界或路径上界都可能加强当前证书；代码因此统一重滤已物化 D。

这一步的删行条件为：

```math
D(S,v)+L_{mathrm{new}}(v,[g]\setminus S)\ge U_{mathrm{new}}.
```

$L_{mathrm{new}}$ 是可采纳下界， $U_{mathrm{new}}$ 是真实可行上界；所以被删状态的任一完整扩展代价至少为 $U_{mathrm{new}}$，而同成本可行解已经存在。未被删除的顶点和值保持原相对次序；branch 位只随对应项压紧，不重新分类。一个 branch 的真假描述它在原 ordinary 闭包中是否严格优于同根 split，这一事实不随上下界改变；被删除的非 branch 等价展开本身也满足同一个拒绝式，故不会丢失可改善拆分类。由此形成统一的 $U$—$L$—$U$ 闭环：真实路径给出初始 $U$，容量可行势提高 $L$，延迟购买把 residual/primal 结构转成新的真实 $U$，最后只按更紧的 $(L,U)$ 单调缩小既有锥体。路径生长、support metric 和 subset DP 的每个有限结果都由原图真实边或精确 D 值组成；这一整段是 DirectedCut 的安全新增证书，不是按查询切换 Base 与 Enhanced。

只开 `DirectedCut` 时，后续仍运行完整前向 $A$。这一中间配置只用于 correctness/ablation，以隔离第二项增强，不是第四套算法。

## 12. `AdjointCompletion` 增强

### 12.1 为什么需要反向高层

完整前向格在高层不断用相似 ordinary row 扩展许多 $A(S,\cdot)$。对一个完整解，真正重要的是低层锚定前缀与“其余组已经付出的外侧代价”在边界相遇。adjoint 把高层依赖按补集转置，使同一个 ordinary 值在一个顶点处一次参与多个高层目标。

固定：

```math
h_{\max}=\lfloor g/2\rfloor-1,
```

当 $h_{\max}=0$ 时令 $h_{\mathrm{low}}=0$；当 $h_{\max}>0$ 时令：

```math
h_{\mathrm{low}}=
\max\left\{1,\left\lfloor h_{\max}/2\right\rfloor\right\}.
```

存在非空 H 后缀时，公共前向内核只物化 $|S|\le h_{\mathrm{low}}$ 的 $A$ 并保留边界层；外层的 1 保证共同 A1 始终位于前向前缀。层计划同时令 ordinary 物化到 $q=h-1$，并令 adjoint 从辅助半格 $h$ 递减到 $h_{\mathrm{low}}+1$。辅助 $H(h)$ 精确转置实现省略的 $D(h)$，其余 H 层再承担高层 A 的替换职责。若前向边界已覆盖完整逻辑域，则直接复用完整前向入口，不构造转置工作区。这些都是递推索引边界，不含经验组数、row 密度、图名、incumbent 或耗时分派。

### 12.2 按顶点补集转置

普通 row 按 mask 存储：`mask -> [(vertex,value)]`；转置需要相反访问方向：`vertex -> [(mask,value)]`。若给全部 $n$ 个顶点各建一个临时容器，大图会产生 $O(n)$ 个 vector 对象。实现因此每次只处理 64 个连续顶点，用 `array<vector<TerminalEntry>,64>` 聚合当前块内可用的 $D(B,v)$。多组 ordinary row 的顶点已递增，每张 row 维护一个单调 cursor，只扫描落入当前块的 payload；singleton 直接读取当前 64 个位置。块内用 `vertex & 63` 定位桶，处理完即清空复用。64 与 membership/branch bitmap 的一个 `uint64_t` word 对齐，只是缓存块宽，不是 64 线程、SIMD/GPU warp 或图压缩参数。

令 $k=g-1$， $h=\lfloor g/2\rfloor$， $q=h-1$，前向边界为 $\ell$。对任一待物化的 $H(S)$，记补集 $Q=[k]\setminus S$。一个或两个互不相交 ordinary 块可以产生直接终端：

```math
Q=\bigcup_{j=1}^{b}B_j,
\qquad
B_1\cap B_2=\varnothing,
\qquad
H(S,v)\leftarrow\sum_{j=1}^{b}D(B_j,v),
\qquad
1\le b\le2.
```

这里 $H(S,v)$ 已支付 $S$ 外的组，等待覆盖锚组和 $S$ 的前缀。当前实现为全部物化目标 $\ell<|S|\le h$ 准备必要的直接终端，而不是只播种最高辅助层：

- 若完整的 $D(Q)$ 已在 ordinary 阶段发布，则单块终端直接装载该精确值；同目标任意双块和都是两棵可行 rooted 子树的并，不小于 $D(Q,v)$，因此被单块和它已经执行的图闭包支配。某个顶点没有保留 $D(Q,v)$ 时，ordinary 的可采纳 future 已证明它及从它出发的传播不能严格改善当前真实上界，pair 也不可能绕过该证明。
- 若 $D(Q)$ 未物化，而某个 ordinary 规范拆分的两侧都不超过 $q$，双块终端的候选集合包含该 split，并按目标、顶点取最小值。集合中额外的非规范 pair 仍是可行 rooted 子树之并，不会产生虚假的较低值。
- 若至少一侧超过 $q$，该拆分不在转置阶段强行装成三块或更多块；它由第 12.3 节的 successor 递推承担。

辅助层 $|S|=h$ 给出省略半格的精确基例。若 $g=2h$，则 $k=2h-1$ 且 $|Q|=h-1=q$，直接装载已有 $D(Q)$。若 $g=2h+1$，则 $k=2h$ 且 $|Q|=h$，ordinary 中没有 $D(h)$；任一二分的两侧都至多为 $q$，双块终端包含全部规范 split，随后执行与 ordinary 相同的图闭包。因此在仍可能严格改善 incumbent 的锥体内：

```math
H(S,v)=D([k]\setminus S,v),
\qquad
|S|=h.
```

较低 H 层同样可能需要直接双块终端。原因是 H 只物化到最高层 $h$：当一个目标的普通 split 两侧都太大，任一侧加回目标后都可能越过 H 的物理上界；如果两侧又都已经不超过 ordinary 边界 $q$，直接 pair 正是唯一缺失的结构化入口。把这批终端误写成“被 successor 严格支配”会漏掉平衡 split。当前代码只枚举上述必要的单/双块集合，不引入固定三块装箱、数据相关开关或经验阈值。

每个顶点先计算 subset 势 $\Pi(B,v)=\sum_{i\in B}\pi_i(v)$；ordinary 值减去对应势得到 reduced value。完整解至少支付全组势，所以只有 reduced 值之和不超过 `best - full_potential` 的组合可能改善 incumbent。代码在“按 reduced value 排序枚举 pair”和“枚举已有 mask 的互补 submask”之间比较确定的循环项数；两条路径遍历同一候选集合，只改变常数。terminal 写入前还计算包含锚组和目标 $S$ 的 prefix：farthest、tour 和 directed-cut 三者取最大。只有 `terminal + prefix < best` 才保存。它们都是可采纳下界，因此只截去无法严格改善现有真实上界的值。

### 12.3 递减 H、全值 successor 与边界结算

从辅助层 $h$ 递减到 $\ell+1$。对目标 $S$，种子来自第 12.2 节的直接终端，或来自更大的 successor：

```math
H(S,v)=\mathrm{closure}\left(
\min\left\{
T(S,v),
\min_{\varnothing\ne B\subseteq[k]\setminus S}
\bigl(H(S\cup B,v)+D(B,v)\bigr)
\right\}
\right),
```

其中 $T(S,v)$ 是不存在时取无穷的单/双块直接终端。successor 合并读取新增 ordinary 块的全部精确值，而不是只读取它的 branch 位。这个区别是补集转置的核心：普通规范 split 的 branch 可能位于 $H(S\cup B)$ 所代表的补集一侧；若再次要求新增块也是 branch，就会把“branch 已在 successor 中”的合法拆分删除。放宽为全部值仍然安全，因为 $H$ 与 $D(B)$ 覆盖互斥组集，每个额外同根和都是两棵真实 rooted 子树的并，只可能增加等价或较差的可行推导。

下面逐个普通规范 split 证明覆盖。固定 $Q=[k]\setminus S$，把它的两侧记为 $X,Y$：

1. 若 $D(Q)$ 已物化，单块终端直接给出精确值；被 ordinary future 安全拒绝的位置及其向外传播同样不能改善 incumbent。
2. 否则若 $|X|,|Y|\le q$，直接双块终端包含该 split。
3. 否则设较大一侧超过 $q$。因为 $|Q|=k-|S|$ 且 H 的最高层为 $h$，另一侧的大小至多为 $h-|S|$（偶数 $g$ 时还可再小一），所以把较小一侧加到 $S$ 后仍落在已完成的 H successor 区间。successor 表示较大一侧，新增 ordinary 侧读取全部值，因而无论较小一侧在原规范 split 中是 accumulator 还是 branch 都被覆盖。

三类互斥地覆盖每个规范 split。对 size 从大到小归纳，并在每层执行同一非负边图闭包，可得：

```math
H(S,v)=D([k]\setminus S,v),
\qquad
\ell<|S|\le h,
```

同样只需在当前严格上界锥体内保持。若 terminal 或 successor 状态被 prefix 拒绝，其外侧已付值加连接锚组与 $S$ 的可采纳下界不小于当前真实上界；以后上界只会下降，因此不会删去严格改善解。

每张 $H(S)$ 完成后枚举低层锚定 mask $L\subseteq S$，令 $B=S\setminus L$，在共同根结算：

```math
A(L,v)+D(B,v)+H(S,v).
```

这里 $D(B)$ 仍只读取规范 branch，因为它承担的是前向 $A$ 递推从 $L$ 跨到 $S$ 的 ordinary 一侧；这与 successor 的补集职责不同，不能共用同一个 branch 限制。三部分分别覆盖锚组及 $L$、边界块 $B$、以及 $[k]\setminus S$，组集合互不相交且并为全集。

还需处理一个严格由层边界推出的平衡完成式。当 $k=2h$ 时，即查询组数为奇数，最高辅助层的 $S$ 与其补集都恰有 $h$ 个非锚组；两侧 $D(h)$ 都由辅助 $H(h)$ realization 承担，ordinary 中没有可供 $L=0$ 边界读取的 $D(h)$。因此互补 H row 均完成后，代码额外结算：

```math
d_a(v)+H(S,v)+H([k]\setminus S,v).
```

它逐项对应完整 ordinary 半格的 $d_a(v)+D(S,v)+D([k]\setminus S,v)$，每对只在后一个 mask 发布时检查一次。条件只检查集合大小和 row 生命周期，不读取图名、查询统计、耗时或经验组数阈值。若 $k=2h-1$，两侧大小为 $h$ 与 $h-1$，同一责任会在较低 H 边界由已有 $D(h-1)$ 自然完成，不需要另一条分支。


### 12.4 为什么保留最高逻辑 ordinary 层

设 $q=h-1$。完整前向与 adjoint 边界都可能直接读取

```math
d_a(v)+D(S,v)+H(S,v),
\qquad
|S|=q.
```

低层前缀为空时不能把 $S$ 再缩小后转交给非空 $A(L)$； $D(S)$ 又包含规范 split seed 后的图闭包，不能只在少数目标顶点用较小 D 的和代替。因此 Base 与 Enhanced 都调用同一 `BuildOrdinaryRows` 完整生成所有 $|S|\le q$ 的 row。Enhanced 只把更高的 $D(h)$ 交给辅助 $H(h)$ 的同递推转置。

把 ordinary 再降到 $q-1$ 的旧探针曾使 Orkut 精确值 32 错成 33。另一次错误实现保留 $D(q)$，却让 H 直接从 $q$ 启动；P1 的 Orkut 和 LiveJournal 多条查询因此高报 1–2。两者都省略了被后续递推消费的完整图闭包职责。当前方案分别以公共 $D(q)$ 和辅助 $H(h)\equiv D(h)$ 明确承担它们，不再依赖装箱论证。

### 12.5 与完整前向格的对应

完整前向实现需要 ordinary 半格 $D(h)$。Enhanced 不物化该 D row，而是先由第 12.2 节构造数值语义相同的辅助 $H(h)$。对任一较低目标，普通规范 split 要么由已有单块 $D(Q)$ 直接转置，要么由两侧都不超过 $q$ 的双块 terminal 转置，要么由“较大侧 successor + 较小侧全值 ordinary”递推。第 12.3 节的分类覆盖表明，不存在第四类 split；因此对每个 $\ell<|S|\le h$， $H(S)$ 精确代表完整前向推导中外侧的 $D([k]\setminus S)$。

任取一条完整前向规范推导，在第一次跨过前向边界 $\ell$ 时，把锚定侧写成低层 $A(L)$、规范 ordinary 块 $D(S\setminus L)$，并把剩余外侧写成 $D([k]\setminus S)$。前两项由公共前向/ordinary row 完整物化，第三项由 $H(S)$ 等价替代；前向闭包在跨界根之后移动的路径可以并入外侧 D 的图闭包，所以同代价推导在 adjoint 边界被枚举。当 $k=2h$ 且跨界前缀为空时，两个大小均为 $h$ 的 ordinary 半格都由互补 $H(h)$ 代替，并由专门的互补完成式逐项恢复。反向上，每个直接 terminal、successor 同根和、H 图闭包、普通 A/H 边界和互补 H 完成式都由互斥组集的真实 rooted 子树与原图路径组成；展开后必是合法完整推导。两边可能存在重复表示，但最小可行代价集合相同。

这个对应不使用“删除一棵完整树后把组件装入固定数量箱子”的前提。该前提只能说明组件大小可以分组，不能保证经过图闭包的 D 状态可在同一根处被较小状态代数替换。当前证明直接对 ordinary 的规范 split、补集侧的三类入口及每层图闭包归纳，证明责任与代码中的 terminal、successor、互补完成和 H Dijkstra 逐项对应。

### 12.6 一张 $H$ row 的构造顺序

$H(S)$ 与 $A(S)$ 的数值含义不同，不能逐项比较；可比较的是它们在完整推导中的边界职责。当前实现顺序如下：

```text
按 64 顶点块转置全部已发布 ordinary row:
  为每个 ell < |S| <= h 登记可用的单块 D(full xor S)
  为两侧都不超过 q 的规范 split 超集登记双块 terminal
  用 reduced budget 与 prefix 删除不能严格改善 incumbent 的候选

for size = h down to ell+1:
  start layer timer
  for each target S with |S| = size:
    从 terminal[S] 装载直接 seed
    for each nonempty B subset of full_mask xor S:
      successor <- S union B
      若 |successor| <= h:
        用 H(successor) 与全部精确 ordinary D(B) 做同根松弛
    以 max(farthest, tour, directed-cut prefix) 为可采纳前缀做图闭包
    写回顶点递增的 H(S) 并累计首次发现状态
    若 S 与补集同属辅助半层且补集 row 已发布:
      用 anchor distance + H(S) + H(full xor S) 更新真实上界
    枚举 L subseteq S, |L|<=ell:
      用 A(L) + branch-D(S xor L) + H(S) 更新真实上界
  输出该 H 层耗时、row/scalar 数和当前 best
```

递减顺序保证读取 H successor 时该 row 已经发布。successor 合并读取全部 ordinary 值，是因为规范 branch 可能位于 successor 所代表的一侧；最终 A/H 边界仍读取 branch，因为它对应前向 A 的规范跨界。转置阶段和 H 闭包传播的每个有限值都能展开为真实子树与原图路径，prefix 只参与拒绝。互补辅助半层完成只在两个 mask 大小相等且前一 row 已发布时触发，不是按奇偶值硬编码的算法开关；诊断构建会在它严格收紧 `best` 时输出 `adjoint_complementary_half_upper`，并为每个 `adjoint_layer` 输出独立秒数，便于大图长尾审计。



## 13. 完整正确性论证

下面给出当前实现必须同时满足的证明链。

**引理 1（上界真实性）。** `best` 只由规范 SPT 边并集、共同根真实路径并集、有序组三元组/购买后四元路径生长的真实边并集、directed-cut primal、facility 支撑路径、witness-tree DP、certificate-support DP、root-star 或完整状态结算更新。每一项都能展开为原图真实边与精确 rooted 子树的连通覆盖，因此 `best` 始终满足 $best\ge\mathrm{OPT}$。

**引理 2（基础 future 可采纳）。** $L_{\mathrm{cc}}$ 来自零权缩点后必须连接的分量数； $L_{\mathrm{far}}$ 是任一剩余组不可回避的单组距离； $L_{\mathrm{tour}}$ 来自可行树倍增、组度量 shortcut 和除以 2。三者分别不超过其声明的剩余代价，因此任意最大组合仍可采纳。

**引理 3（距离—根初始化合同与有界距离安全）。** 两种 realization 返回的 $U_0$ 都来自真实 SPT 边并集或共同根连接，故是可行上界；返回的 $r$ 只改变后续状态组织。Bootstrapped-bounded 中未保存的 $d_i(v)$ 至少为构造时 cutoff。它只能作为拒绝证书；任何需要精确距离的 split、完成式或 witness 都通过 `ExactValueOrInf` 或 `ForEachExact` 拒绝非精确位置。Complete-potential 的每个位置都精确。因此共同调用者只依赖这一 `GroupRow` 合同，cutoff 不会被当作一棵虚构的低成本子树。

**推论（至多三组的共同闭包）。** 当 $g\le3$ 时，任意可行树的三个命中终端在树内有一个分叉点 $v$，其分支总长不小于 $\sum_i d_i(v)$；反向取任意 $v$ 到各组的最短路并集，真实去重代价不超过该距离和。因此 $\mathrm{OPT}=\min_v\sum_i d_i(v)$。Bootstrapped-bounded 若最优值低于 cutoff，则最优根的所有组距离均是精确位置，若等于 cutoff，则已有真实上界已闭合。故全部配置可共同运行这一初始化包并直接返回精确值，不需要 complete potential 或任何配置专属证书。

**引理 4（共同 A1 cone 与条件式重启安全）。** 对固定 $i$，共同 continuation $C_i$ 是 farthest 与 endpoint-floor 的最大值。前者是 1-Lipschitz；endpoint-floor 在多组时是 1/2-Lipschitz，在单组时等于组距离而是 1-Lipschitz。故 $C_i$ 始终是 1-Lipschitz。若 $A(\{i\},v)+C_i(v)<U_0$，一条最短 A1 路径上的每个前缀也满足该不等式，所以共同闭包不会漏掉该精确值。任一配置未保存 $v$ 时都有 $A(\{i\},v)\ge U_0-C_i(v)$，故使用同一个 continuation 的非负 fallback 安全。树 DP 未收紧上界时 $U_0$ 不变；收紧时全部部分 row 被丢弃，并在同一输入修订不再购买的条件下用新上界整轮重建。因此最终发布的每一轮都满足同一个固定的 $U_0$ 证明。三个配置调用同一不读取增强位的构造和同一调度器，row 内精确值与 row 外证书遵守同一证明；对剩余 singleton 取最大仍是可采纳 future。

**引理 5（directed-cut future 可采纳）。** 每轮势差只从相应方向的非负 residual 容量扣除，全部组在任一有向弧上的累计收费不超过原容量。截断 cone 外的势值都等于同一个根 cap，所以跳过两端均在 cone 外的边只省略严格为 0 的梯度；稀疏邻接与稠密原边遍历对其余每条边恰好更新一次。任何从当前根连接指定剩余组的树都必须支付这些割势，因此 $L_{\mathrm{cut}}$ 不超过剩余代价。与引理 2 的证书取最大仍安全。

**引理 6（拆分因子化与稀疏图闭包精确）。** 对固定 mask，所有同根 seed 都是真实子树之和。对固定 $(S,v)$，可采纳 future 与规范拆分无关，故先取全部规范 seed 的精确最小值再检查 future，与逐 seed 检查后取最小保留相同的可改善值。Base flat cache 保存完整的 farthest/A1/tour 最大值；DirectedCut staged cache 只保存已经计算出的可采纳下界最大值。这个状态机不假设候选天然按大小到达：若标签 $x$ 在某阶段被当前缓存 $L$ 拒绝，则任何 $y\ge x$ 也被同一比较拒绝，只有满足 $y+L<best$ 的更小标签才会进入下一未完成阶段；若 $x$ 通过当前阶段却在后续阶段被拒绝，后续阶段写回的更强缓存又建立同一不变量；若 $x$ 最终被接纳，真实 `distance` 直接阻止不小于 $x$ 的标签再次进入。特别地，directed-cut 的 certified lower endpoint 只作为已知可采纳前缀；若 upper endpoint 已让当前标签严格通过，所有随后能够越过其余缓存边界的标签都更小，因而也通过 dual，无需再判 dual；若逐组 exact fallback 已求出，则固定 $(S,v)$ 的剩余组 mask 在整张 row 中不变，后续复用和重新逐组求和返回逐位相同的 `double`，无论当前标签通过还是被拒绝都可结束 dual 阶段。因此 staged cache 既不把一次区间下端拒绝永久化，也不会用区间近似替代精确证书。Dijkstra 只在 `value + admissible_future < best` 的区域传播；区域外不可能导出严格优于已有上界的完整解。区域内每次松弛使用真实边权，过期队列项只被忽略，故写入 row 的值等于完整 rooted DP 在该安全锥体内的精确值。初始容器的线性 heapify 与逐项 push 具有同一 `QueueNode` 全序，改变的只是建堆成本。

拒绝前沿也不改变该闭包，而且同样不要求候选按大小到达。固定真实上界与缓存下界时，非负 `double` 加法关于标签单调；stage-0 cutoff 又用原比较式逐位验证为一个已拒绝值，所以无论何时到达，任何不小于它的标签都必被原 `CanImprove` 拒绝。查询过程中 `best` 只会下降，缓存下界只会保持或加强，已经安全的前沿不会失效。虚拟 cutoff 不进入 queue、settled row 或状态计数；只有更小标签通过完整证书链后，crossing 才把同一顶点恢复为普通工作项。因此该前沿等价于省略一批返回值必为 `false` 的候选调用，不删除任何可改善 rooted 状态。

**推论（证书刷新后的重滤安全）。** residual closure 只把引理 5 的容量可行势补全，并把 primal、facility、四元路径及 support DP 的真实可行值写入 `best`。对已物化状态，若新的精确 rooted 值与可采纳 future 之和不小于新的真实上界，则它不存在严格改善 incumbent 的完整扩展。删除该项并同步压紧原 branch 位不会改变剩余项的数值、次序或 branch 定义，也不会删去一个可改善规范拆分类的最后代表。

**引理 7（普通格规范化完备）。** 完整 rooted subset DP 的任意同根拆分可通过最低 pivot 唯一选定 accumulator/branch 方向。未发布的非 branch 状态存在一个等成本同根真子集拆分；递归展开最终到达 singleton 或经过图边闭包的 branch。因此只在被接入侧要求 branch 不会删掉一个分解等价类的最后代表。

**引理 8（前向完成完备）。** 平衡分解保证任意完整规范树可表示为一个大小至多 $q=h-1$ 的锚定块和至多两个大小至多 $h$ 的 ordinary 块。前向 $A$ 枚举锚定块的每次合法增长，`CompleteAnchoredRow` 枚举余下至多两块，因此 Base 与 DirectedCutOnly 不遗漏完整规范推导。

**引理 9（Adjoint 等价）。** Enhanced 完整物化所有 $|S|\le q=h-1$ 的 ordinary row。辅助层 $H(h)$ 在 $g=2h$ 时直接转置 $D(q)$，在 $g=2h+1$ 时枚举 $D(h)$ 的全部双块 split seed并执行同一图闭包。对任一较低目标的普通规范 split：完整 $D(Q)$ 已有时走单块 terminal；两侧都不超过 $q$ 时走双块 terminal；否则较小侧加入目标后仍位于 H 物理上界内，走 successor 加较小侧的全部精确 ordinary 值。三类覆盖全部 split，且每个额外候选仍是可行 rooted 子树之并，故归纳得到 $H(S,v)=D([k]\setminus S,v)$ 对 $\ell<|S|\le h$ 成立。普通边界 $A(L)+D(S\setminus L)+H(S)$ 恢复第一次跨过前向边界的推导；当 $k=2h$ 时，互补 $H(h)$ 完成式恢复缺失的 $d_a+D(h)+D(h)$ 平衡情形。转置 budget、prefix 与 H 闭包只使用引理 2、5 的可采纳下界。因此 Enhanced 与完整前向格枚举相同的可改善完整推导。

**引理 10（配置覆盖）。** `ConfigurationProfile` 的每个差异要么只增加引理 1、引理 5 或重滤推论类型的安全证书，要么替换同一职责：bootstrapped-bounded/complete-potential 都返回引理 3 的距离—根合同，root-path/dual-primal 保持真实 witness 契约，前向高层 A/adjoint H 由引理 9 对应。两种 witness 都只在预处理中构造，随后从零 rent 进入同一购买公式与同一树 DP；不存在 Base-only 的无条件求值。A1 是三个配置共同生成并移交的逻辑层。Enhanced 与 Base 还调用同一 ordinary 递推到最高逻辑层 $q$；省略的半格 $D(h)$ 由同职责的辅助 $H(h)$ 精确转置，而不是由未经闭包的同根块和近似替代。因而不存在 Base-only 而 Enhanced 没有对应消费者或 realization 的状态职责。

**定理（ABHSS 精确性）。** 对任意合法配置、无向非负边权输入及 $g\le16$ 的查询：若共同分量不存在，算法正确返回 infeasible；否则在实数算术模型下返回 $\mathrm{OPT}(G,\mathcal K)$。当 $g\le3$ 时结论直接由上述共同闭包推论成立。其余查询中，引理 1 保证任意时刻 `best` 不低于最优值；引理 2–6 与证书重滤推论保证距离截断、future、重滤与严格上界剪枝不删除任何代价低于当前 `best` 的完整推导；引理 7 与引理 8（Base/DirectedCutOnly）或引理 9（Enhanced）保证至少一条最优规范推导仍被枚举。搜索耗尽时不存在低于 `best` 的未枚举可行解，故 `best <= OPT`；与引理 1 的 `best >= OPT` 合并得到 `best = OPT`。实现的上下界闭合使用原始 `double` 顺序 `best <= lower`，不以 epsilon 把正 gap 当作 0；其浮点声称边界见第 1 节与第 13.2 节。

### 13.1 证明责任如何落到代码接口

正确性不是由一个最终断言集中保证，而是由接口边界逐层约束：

| 证明责任 | 代码边界 | 必须成立的可检查条件 |
|---|---|---|
| 上界真实性 | `PrepareProblem`、`EvaluateWitnessTree`、`EvaluateCertificateSupport`、各完成式 | 每次写 `best` 的有限值都能展开为原图真实边或 rooted DP 值 |
| bounded 距离不冒充状态 | `GroupRow::ExactValueOrInf`、`ForEachExact` | 所有作为 seed 的 singleton 必须精确；cutoff 只能经 `operator[]` 进入下界比较 |
| future 可采纳 | `FutureBound`、A1 `Value`、dual `At` | 不同来源的证书只用 `max` 合并；只有 residual 收费等已经证明互不超容量的内部项才可求和 |
| row 依赖完备 | `ready`、按 size 递增/递减循环 | 跨阶段消费者只读显式 ready 的依赖；普通 forward 内部由严格层序证明已处理的零标签行只读空 payload |
| 规范拆分完备 | branch bit 与 pivot 规则 | 每个等价拆分类至少有一个可被高层消费的代表 |
| 证书升级后重滤 | `RefilterOrdinaryAfterCertificateUpgrade` | 只删除 `value + admissible future >= feasible best` 的项；保留项的 branch 位按原下标同步压紧 |
| A/H 与 ordinary 消费边界完备 | `AnchoredCompletionSchedule`、`BuildTransposedTerminals`、`SolveHighAdjoint` | $1..q$ 的每个锚定职责由前向 A 或逻辑 H 恰好覆盖；ordinary 完整物化到 $q$；每个补集 split 由完整 D 直接终端、双块直接终端或 successor 加全值 ordinary 覆盖；非锚组二等分时由互补 $H(h)$ 恢复平衡完成式 |
| 严格剪枝 | 所有 `candidate + lower < best` | 等于上界的状态可以删除，因为已有同成本真实解；正 gap 不能闭合 |

`ConfigurationProfile` 不参与数学数值计算，但它把最后一项变成机器可测合同：安全新增位只能单调增加，三个替换字段必须落在已证明 realization 中，ordinary future 在三个配置中固定为共同 A1。若以后加入另一种 future 或高层实现，必须先扩展该枚举、上述表格和独立 DP 回归，不能只在某个函数中增加数据相关分支。

### 13.2 边界输入为何不构成证明例外

- **重叠组。** 一个顶点可同时命中多个组。零权分量 cover、组距离和 full-mask 更新都按 bit 并集处理，不要求每组选择不同终端；若一个零成本连通分量覆盖全部组，答案直接为 0。
- **零权边。** Dijkstra 仍适用，但 witness 父指针只在严格距离下降时改写，防止等距重挂形成环。DP 的严格上界剪枝针对完整成本，不要求边权严格为正。
- **重边与自环。** 邻接表保留各自 `edge_id`；真实路径并集按边 ID 去重。非负自环不能改善最短路，因而不会影响最优值。
- **非连通图。** 只有当某个连通分量与每个组相交时查询才可行；一旦选定共同分量，任何跨分量状态都不可能进入一棵可行树。
- **空 row。** 对显式发布的 ordinary、提前 A1 与 $H$，`ready=true` 且 payload 为空表示在当前严格上界锥体中没有可改善状态；这不同于依赖未计算。普通 forward 内部零标签行虽省去 ready 写入，但严格层序证明它已经处理，且所有内部读取只见空 payload。两种物理形式的证明都只要求“不遗漏低于 best 的推导”。
- **浮点输入。** 数学证明在实数算术模型下成立。实现用 IEEE 754 `double` 做确定性比较；容差只用于跨程序报告以及恢复一条数值等式路径，恢复后仍按真实边权计价，核心闭合不用 epsilon。当前 artifact 没有用任意精度或区间算术逐次认证舍入方向，因此“精确”不得解释成与实数模型无条件逐 bit 等价。

工程证据不是数学证明的替代，但用于防止实现偏离上述引理：用四个逻辑组保留的历史零权 witness 父指针反例，以及 50,000 顶点逆序零权并查集链；包含零权、重叠组和多终端的确定性随机图，三种合法配置逐例对照独立全子集 DP；显式 $g=2,3$ 非零最优实例断言三个配置都在零主状态处闭包；逐弧从全部组势独立复算 residual，断言 potential cone 没有漏减非零梯度；高 $g$ SteinLib 已知最优值；非法配置、不可行图、平凡查询、输入格式和小于 $10^{-9}$ 正 gap 的闭合回归。

## 14. 复杂度分析

### 14.1 最坏界

令 $k=g-1$， $h=\lfloor g/2\rfloor$， $F=\sum_i|K_i|$，并令 $\rho$ 为 directed-cut primal 涉及的不同 facility 顶点数、 $s$ 为购买后 certificate support 涉及的不同顶点数；Base 中取 $\rho=s=0$。Adjoint 转置为全部物化 H 目标枚举必要的一块或两块 ordinary 状态：已有完整 $D(Q)$ 时只取单块，两侧都位于 ordinary 边界内时取双块；其稠密组维度仍为 $O(3^g)$。其余 split 由 successor 加全值 ordinary 侧覆盖，互补辅助半层只做一次稀疏 row 交集，因此都不改变同阶最坏界。把当前二叉堆、真实路径恢复、facility 与 support 上界都计入，一个输出敏感的保守总时间上界可写为：

```math
O\!\left(
(g+\rho+2^g)(m+n)\log(n+m)
+g^4(m+n)+2^g(g^3+\rho^2+s^2)+3^g(n+\rho+s)+s^3+F\log(F+1)
\right),
```

其中 $g^4(m+n)$ 保守覆盖 $g(g-1)(g-2)$ 个有序组三元组起点、每个起点至多 $g-1$ 次 tight-edge 路径恢复；公共根路径并集包含在该项内。实际实现受当前真实上界的单调截断，通常不会达到此界。 $\rho$ 相关项来自 facility 支撑图； $s^3+2^g s^2+3^g s$ 分别覆盖 support Floyd、各 mask 的 metric 闭包和规范拆分。若只讨论这些证书之后的主状态搜索，常用简写才是 $O(3^g n+2^g(m+n)\log(n+m))$。这里按当前 `std::priority_queue` 的重复入堆二叉堆实现计每次 push/pop 的 $O(\log(n+m))$，不借用 decrease-key/Fibonacci heap 的 $O(m+n\log n)$ 界；在简单图上该对数项可等价写成 $O(\log n)$，但本实现允许重边。tour 的固定端点表使用 $O(2^g g^2)$ 空间、 $O(2^g g^3)$ 时间；零权 cover 的组维度 DP 为 $O(3^g)$。正权图只额外支付 $O(F\log(F+1))$ 的查询分量聚合；含零权边时才支付保守的 $O((n+m)\log(n+1))$ 路径压缩并查集成本。因为当前 $g\le16$，纯组维度表可控，实际瓶颈通常是图维度 row、闭包以及 Enhanced 的 facility/support 规模；论文不能把它们从最坏界中省略。

输出敏感的最坏空间为 $O(2^g n+gn+2^g g^2+m+\rho^2+s^2+F)$；facility/support DP 的 $O(2^g\rho+2^g s)$ 已被 $\rho,s\le n$ 下的 $O(2^g n)$ 覆盖。Base 的组距离仍可能退化到 $O(gn)$，但通常只保存 cutoff 内精确值；Enhanced 为 directed-cut 明确支付 dense $O(gn)$，并在 facility 与 support 阶段分别临时支付 $O(\rho^2)$、 $O(s^2)$ 度量矩阵。ordinary 的 `distance`、`split`、`bound_cache` 和 Base stamp 各为线性工作区；DirectedCut 配置用一个 32-bit `bound_state` 替代 stamp/stage/exact 并行数组。拒绝前沿复用 `distance`，只让 `rejected` 保存至多 $n$ 个待复位顶点，仍为 $O(n)$ 临时空间；它不增加任何持久 row payload。

逐阶段的保守边界如下：

| 阶段 | 最坏时间 | 主要空间 | 说明 |
|---|---:|---:|---|
| 连通性与零权 cover | 正权图 $O(F\log(F+1)+3^g)$；含零权边时保守再加 $O((n+m)\log(n+1))$，另加非连通可行性检查 | 正权图 $O(F+2^g)$；含零权边时 $O(n+F+2^g)$ | 普通连通分量索引和最小边权在加载期共享；当前并查集使用迭代路径压缩但不按秩合并，分量 mask 始终只为查询触及项保存 |
| 距离—根初始化 | $O(g(m+n)\log(n+m))$ | Bootstrapped-bounded 至多 $O(gn)$；Complete-potential 为 $O(gn)$ | 前者把候选根 SPT bootstrap 与截断多源搜索封装为一个 realization；后者把多源搜索扩展到全图；两边返回相同三元合同 |
| $g\le3$ 精确闭包 | 一次共同 Bootstrapped-bounded 距离—根初始化 | 不增加渐近空间 | 所有配置逐项相同；不构造 complete potential、witness、dual、tour 或指数状态表 |
| 共同有序组三元组一步前瞻路径生长 | $O(g^4(m+n))$ | $O(m+n+F)$ 临时空间 | 全部配置调用同一函数；每个有序前两组只恢复一次种子路径并向全部第三组重放；真实边去重，既有上界只作单调安全终止 |
| 组 tour | $O(2^g g^3)$ | $O(2^g g^2)$ | 只含组维度，不含图顶点维度 |
| 共同 A1 | $O(k(m+n)\log(n+m)+nk^2+2^k)$ | row 至多 $O(kn)$，分级查找缓存 $O(kn+2^k)$ | 层 1 属于 $\mathcal L_A$ 时所有配置执行同一 cone；top-two 由 lazy 切换为顺序物化并构造精确租金表，tail 达到独立结构购买点后物化完整 byte 排名；不读取 dual 或增强位 |
| ordinary $D$ | 保守 $O(3^g n+2^g(m+n)\log(n+m))$ | $O(2^g n)$ | 无 H 后缀时物化到半格 $h$；存在 H 后缀时 Enhanced 物化到 $q=h-1$，全部逻辑边界直接读取这些公共 row，省略的 $D(h)$ 由辅助 H 同递推转置 |
| 完整前向 $A$ | 同阶保守上界 | $O(2^g n)$ | 实际只到 size $q=h-1$，末层可只消费 |
| directed-cut | $O(gm+g(m+n)\log(n+m))$ | $O(gn+m)$ | 最坏界不变；第 $i$ 轮容量更新实际枚举 $\min\{m,\sum_{v\in C_i}\deg(v)\}$ 个原边/邻接项， $C_i$ 为截断势 cone |
| facility 上界 | $O(\rho(m+n)\log(n+m)+2^g\rho^2+3^g\rho)$ | $O(n+\rho^2+2^g\rho)$ | 仅 DirectedCut/Enhanced； $\rho$ 是 primal 涉及的不同顶点数 |
| certificate-support 上界 | $O(s^3+2^g s^2+3^g s)$ | $O(s^2+2^g s)$ | 仅在 closure 购买后路径证书严格改善时登记； $s$ 是路径边与 primal 边并图的顶点数 |
| adjoint 转置与 $H$ | 保守 $O(3^g n+2^g(m+n)\log(n+m))$ | $O(2^g n+gn)$ | 单块精确 D 优先；必要双块 terminal 与 successor 全值交集覆盖全部 split； $k=2h$ 时互补辅助 H 做一次同根完成 |

表中的共同 A1 条件不是参数调优分支：它只是询问层 1 是否属于前向递推定义域 $\mathcal L_A$；属于时三个配置都执行同一逻辑 A1，不属于时没有这张 row。top-two 使用两个 byte bit 数组和一个压入两个 32-bit locator 的 64-bit 数组，全部购买后固定容量约为 $10(n+1)$ 字节；同时构造含 $2^k$ 个 32-bit 整数的精确租金表。完整 tail 再为每个顶点保存 $t=k-2$ 个 byte，因此两级缓存最坏容量约为 $(k+8)(n+1)+4\cdot2^k$ 字节，即 $O(kn+2^k)$；它不增加任何 double 数组。未购买时只触及约 $2n$ 字节的 bit 数组与实际查询顶点对应的 locator 页面，top-two 购买后触及约 $10n+4\cdot2^k$ 字节，tail 购买后才达到上述最坏容量。locator 无损定位原 double 或统一非负 fallback，ranked tail 只保存 bit 次序。稳定插入排序及缺项 continuation 的保守最坏工作为 $O(nk^2)$，租金表构造为 $O(2^k)$，均已计入表中。所有缓存在 ordinary 后释放。若 A1 内条件式 witness 购买收紧上界，至多丢弃当前输入修订上的一个部分 pass；同修订购买保护使这一重启只增加常数因子。witness、top-two 和完整 tail 是职责互异的三个结构购买式，不能混写成按数据选择算法。

### 14.2 以实际 payload 表示的实现成本

令 $Z_D,Z_A,Z_H$ 为阶段结束时实际保存的状态标量数， $R_D,R_A,R_H$ 为图闭包实际检查的邻接项数， $M_D,M_A,M_H$ 为同根交集、转置 pair 与拆分候选数。则主搜索更贴近实际的成本为：

```math
O\left(M_D+M_A+M_H+(R_D+R_A+R_H)\log(n+m)\right),
```

若 $Z_{A1}^{\mathrm{work}}$ 表示共同 A1 在构造 cone 中曾被接纳的不同状态，则其构造工作还包括相应 queue/邻接扫描；它可能大于 ordinary 结束后重滤所保留的 A1 payload。A1 本身不读取 DirectedCut 的 $O(gn)$ 单组势；这些势只用于 ordinary/adjoint 的独立证书。计入分级 A1 缓存后，阶段空间分别为 Base 的 $O(Z_D+Z_A+kn+2^k)$、DirectedCutOnly 的 $O(Z_D+Z_A+gn+kn+2^k)$、Enhanced 的 $O(Z_D+Z_A+Z_H+gn+kn+2^k)$；因 $k=g-1$，租金表与主 subset 状态同阶，A1 byte 缓存不改变后二者的 $O(gn)$ 线性项，但都必须在实际 RSS 中报告。转置 terminal 和 residual 是阶段临时量；residual 在 ordinary 前释放，H 只保存从辅助半格到低层边界之间实际生成的稀疏 row。

实现为每条查询额外报告 `mask_vertex_states`。其口径是累计的“首次发现状态项数”，不是结束时 payload、队列弹出数或峰值空间：状态族属于实际键，对每张完整物化的 $D$、 $A$、 $H$ 逻辑 row，顶点第一次从无穷变为有限候选时计一次；同一 row 内后续改进不重复，D/A/H 中数值相同的 `(mask,v)` 分别计数。所有配置提前生成的共同 A1 在生成时计入，所有权转交给公共 $A$ 内核后不再计。最后只消费而不保留的 $A$ 层仍计入，因为这些状态已经实际生成。组距离、tour、directed-cut 势读取、转置前终端候选、完整解结算及队列过期项均排除。记该累计数为 $C_{\mathrm{ABHSS}}$，则它可用于解释搜索工作量，但通常 $C_{\mathrm{ABHSS}}\ge Z_D+Z_A(+Z_H)$，不能替代峰值内存指标。

PrunedDP++ 的对应值是主 `StateStore` 首次插入的不同 `(mask,v)` 数：Hash 后端直接读取实际容器大小，Dense 后端只统计 `present` 项而不是 $2^g(n+1)$ 预分配容量；状态 reopen 不重复，组距离和 route DP 同样排除，full-mask 完成候选只更新 incumbent 而不进入表。两边的统计都描述各自算法实际主状态域，不能把它解释成完全相同的单步成本；应与时间、边扫描/合并工作和 RSS 联合分析。

初始见证树一次 buy 若含 $t$ 个节点，确定性阈值为 $t((3^k-1)/2+3^k)$，实际求值的最坏时间为 $O(t3^k)$、工作空间为 $O(t2^k)$；support 刷新后的阈值与成本则为第 8.2 节的 $B_{\mathrm{sup}}(s,k)$。共同修订保护避免对同一输入重复购买。directed-cut changed-arc 构造最坏 $O(gm+g(m+n)\log(n+m))$，空间 $O(gn+m)$；potential cone 不改变这一最坏界，但把第 $i$ 轮全边容量更新替换为不超过 $m$ 项的同支撑枚举。若 primal 含 $\rho$ 个 facility，facility 上界的保守界包括 $O(\rho(m+n)\log(n+m))$ 的 $\rho$ 次二叉堆支撑图最短路和 $O(2^g\rho^2+3^g\rho)$ 的小图 DP；support 另支付 $O(s^3+2^g s^2+3^g s)$。它们是 Enhanced 可能支付的证书成本，也是小图上需要实测固定开销的原因之一。

## 15. 实现细节为何存在

| 实现细节 | 作用 | 若删除或写错的风险 |
|---|---|---|
| 对跨阶段 row 分离 `ready` 与空 payload | 区分“已发布空 row”和“依赖尚未生成”；forward 内部零标签行由层序单独证明 | 外部调度会错误跳过合法依赖或读取未初始化状态 |
| 顶点递增 row | 允许双指针、较小侧驱动二分和稳定输出 | Hash 随机访问会放大常数并破坏确定性 |
| branch bitmap | 只发布规范的不可继续同根拆分状态 | 不影响值但会产生大量重复组合；定义错误则可能丢解 |
| `ExactValueOrInf` 单次精确读取 | 一次定位并阻止 bounded cutoff 冒充 DP 值 | 若改用 `operator[]` 作为 seed，会构造不存在的低成本状态，直接破坏精确性 |
| 正权 cover 快路径与触及项工作区复用 | 避免每条查询重复扫描全边、分配全图分量数组或清零未访问距离 | 大图固定预处理会掩盖主搜索优势；复用若漏重置则会跨组污染距离或边并集 |
| $g\le3$ 共同 root-star 闭包 | 以证明过的精确恒等式跳过已经无必要的后续阶段 | 若只给某个配置启用会破坏包含关系；若推广到 $g\ge4$ 则恒等式不再成立 |
| directed-cut potential cone 与唯一正梯度回写 | 只枚举可能具有非零截断势差的边；每条无向边只标记并扣除唯一可能为正的方向 | 漏掉跨 cone 边会少扣 residual；同时回写两向虽数值等价，但会恢复被严格支配的零方向操作 |
| dual 构造非内联冷边界 | 隔离一次性 Enhanced 预处理与 Base 热控制流的机器码布局 | IPO 可把未执行的大分支并入 `PrepareProblem`，造成与状态无关的 Base 退化 |
| epoch/stamp 缓存 | 避免每张 row 清零 $O(n)$ 下界数组 | 大图上清零成本可能超过实际稀疏搜索 |
| flat/staged future 与单调拒绝前沿 | Base 一次缓存完整公共证书；DirectedCut 配置复用新增 dual 的中途拒绝，并允许较小标签继续尚未计算的阶段；stage 0 只在 row 已证明复用后物化按原谓词验证的解析 cutoff | 强迫 Base 支付无 dual 收益的 stage 热分支会损害小组实验；把区间下端当作 exact、把一次拒绝永久化或让虚拟 cutoff 入堆都会破坏精确性 |
| 逐顶点 split 聚合与线性 heapify | 先得到精确 $B(S,v)$，再对每个顶点求一次 candidate-independent future，并以同一全序批量建初始堆 | 逐拆分求 future 会重复工作；若聚合跨越图闭包或改变全序则会破坏精确 row/branch 语义 |
| A1 分级 bit/locator/ranked-tail 缓存与结构购买 | 先复用最大两个 singleton future；top-two 购买时用精确子集递推因子化 tail 租金，累计实际二分成本达到购买式后再用完整 byte 排名把多次二分化为一次精确读取；两个购买函数保持一次性冷代码边界 | 无条件全图物化会让小查询支付固定成本；固定 top-k 缺少理论边界；保存 double 排名会放大 RSS；若租金表不与逐 bit 和严格相等会改变购买时机 |
| A1 内核不读取增强位或 dual | 保证 Base/Enhanced 的 seed、cone、fallback 与交接逐项相同；dual 留在 ordinary/adjoint 证书栈 | 在 A1 内接 dual 会产生不同缺项原因和 fallback；强制 Base 也建 dual 又会消解可关闭增强 |
| 单一零起点 witness scheduler | 两边只代入各自树大小；A1 与 D 连续支付 rent，达到共同 buy 才调用同一树 DP | Base 无条件预买会形成 Base-only 操作；A1 中直接改变 cutoff 而不重启会混合两套 cone 证明 |
| 辅助 H 半格与结构完备 terminal | 用完整 D 直接终端、必要双块终端、successor 加全值 ordinary 及互补 H 完成式精确实现被省略的 $D(h)$ 并向下递减；只减去被同目标完整 D 逐值支配的 pair | 若只给辅助层播种，会漏掉较低层的平衡 split；若 successor 误读 branch 会漏掉 branch 位于补集侧的推导；若缺少互补 H，会漏掉锚组加两个半格的完成式；恢复被完整 D 支配的 pair 则只增加重复候选和固定成本 |
| 64 顶点转置块 | 把 mask-major 稀疏 row 分块转成 vertex-major terminal，与位图 word 对齐 | 全图按顶点物化 ordinary 列会产生 $O(n)$ 个容器并放大内存 |
| 保留最高逻辑 ordinary 层 | Base 与 Enhanced 对所有 $\lvert S\rvert\le q$ 调用同一 ordinary 递推，空前缀和全部 H/A 边界读取同一标准 row | 把该层改成目标驱动的局部替代会破坏共同职责，并曾在 Orkut 反例上把精确值 32 错成 33；只有更高的半格 terminal 交给转置 |
| 严格父指针改进 | 零权等距时维持无环 witness | 可能形成 2-cycle，导致见证遍历错误或不终止 |
| 原始 `best <= lower` | 只有真正闭合才提前返回 | epsilon 闭合可能返回相差极小但非最优的值 |
| 加载期 component cache | 把共同图性质从逐查询计时移出 | 大图每条查询重复 $O(n+m)$，实验含义失真 |
| 固定配置与稳定并列规则 | 可复现且禁止 per-query oracle | 性能曲线无法解释，审稿人可质疑方法选择偏置 |

## 16. 论文表述边界与当前限制

- 可以声称：单线程、精确最优权值、无向非负边权、零权边安全、 $g\le16$，以及从 Base 通过预声明开关得到的“安全新增或同职责替换”配置链。
- 不应声称：当前二进制已经输出最终最优边集合；它目前输出精确权值和 feasibility。
- 不应把 Dijkstra、subset DP、A* 下界、directed-cut、inside/outside、位图或 rent-or-buy 单独表述为原创。贡献应聚焦于它们在 exact GST 中的状态组织、证书复用和高层完成机制。
- “增强位集合单调”不表示 Enhanced 逐指令执行 Base 的所有操作。允许的差异只有安全新增和第 3.1 节表格中的同职责替换；特别地，A1 是共同逻辑层，三个配置都提前并移交同一标准 row，不能表述成 Base 独有的 EarlyA、被 dual 替换的 future，或由 H 替换的第一层。
- 不得把 A1 调度写成任何 `g >= 常数` 的经验规则。A1 的唯一条件是第 3.2 节由平衡分解推出的逻辑层是否存在；代码与文档都必须从 $q$ 的状态域解释。第 6.4 节的 $g\le3$ 判断是全部配置共有的已证明精确基例，发生在 A1 计划之前，不能混写成 A1 性能分派。
- Base 与 Enhanced 的两条实验曲线来自同一二进制的预声明配置，不能取逐查询最小值组成 `ABHSS-best`。
- `double` 输入上的“精确”是组合结构精确而非任意精度实数计算。上下界逻辑不用容差；跨实现报告仍需说明目标值核验容差。
