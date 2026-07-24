# ABHSS：单线程精确 Group Steiner Tree 方法详述

本文档是论文方法章节的详细中文底稿。它描述当前代码实际实现的算法、证明责任和复杂度，不把尚未实现的设想写成既成事实。代码入口与逐文件导读见 [`CODE_GUIDE.md`](CODE_GUIDE.md)，正式实验口径见 [`EXPERIMENT_PLAN.md`](EXPERIMENT_PLAN.md)。

## 1. 问题定义、输出与适用范围

给定无向图 $G=(V,E,w)$，其中 $n=|V|$、$m=|E|$，每条边权 $w(e)\ge 0$。给定 $g$ 个非空顶点组 $\mathcal K=\{K_0,\ldots,K_{g-1}\}$，组之间允许重叠，每组允许包含多个候选顶点。一个可行解是与每个 $K_i$ 至少相交一次的连通子图；由于边权非负，任意可行连通子图均可删除环而不增代价，因此最优解可取为树。目标值记为：

$$
\operatorname{OPT}(G,\mathcal K)=
\min_{T\subseteq G\text{ connected}}
\left\{\sum_{e\in E(T)}w(e):V(T)\cap K_i\neq\varnothing,\ \forall i\right\}.
$$

当前二进制求精确最优权值，不序列化最终最优树的边集合；内部构造的真实树 witness 用于维持可行上界。若论文或 artifact 声称“输出树本身”，还需增加父决策保存或在最优值确定后的第二遍等式回溯。本文以下“精确”均指：对输入解析得到的 `double` 边权，返回与组合最优值一致的权值；实验用 $10^{-6}$ 做跨实现核验，但算法的上下界闭合和剪枝不使用该容差。

实现支持零权边、重边、自环、组重叠和非连通图，限制 $g\le16$。空查询的答案为 0；单组查询无需边，答案为 0；若不存在一个同时与所有组相交的连通分量，则返回 infeasible。所有正式实验均是一个计算线程；1 ms RSS 采样线程只读取进程内存，不参与搜索。

## 2. 设计主线

经典 rooted subset DP 为每个组子集 $S$ 和根 $v$ 维护“覆盖 $S$ 并在 $v$ 连通”的最优值。它的难点不是递推本身，而是 $2^g n$ 状态、同根的 $3^g$ 级拆分以及每层图闭包。ABHSS 的主线是保留这套精确语义，同时把真正进入内存和队列的区域压缩为“仍可能严格改善一个真实可行上界”的稀疏锥体：

1. 先构造真实可行树得到全局上界 $U$，并构造若干可采纳下界。
2. 固定一个永久锚组，只对其余 $k=g-1$ 个组编码 bit mask。
3. 先生成不含锚组、大小至多约一半的普通状态 $D$，并只发布规范 branch。
4. 用前向锚定状态 $A$ 表示包含永久锚组的公共状态；Base 以前向高层完成搜索。
5. 开启 `DirectedCut` 后，一方面安全增加对偶证书和额外可行上界，另一方面以完整距离势、dual future 和 dual-primal witness 替换 Base 中承担相同职责的 realization。
6. 再开启 `AdjointCompletion`，保留低层 $A$，用补集转置和递减的高层 $H$ 替代完整前向高层的等价求值。

这里的核心不是把两个实现包装成一个名字。`D` 的状态定义、稀疏 row、同根交集、图闭包、上界结算和统一 future 接口只有一份。增强开关只允许两种关系：增加 Base 没有的安全证书，或者以结构/功能对应的 realization 替换同一逻辑职责。禁止出现 Base 独自多执行、而 Enhanced 既不执行也没有同职责替代物的状态层。

全文围绕三个不变量展开。第一，`best` 始终由一棵可展开到原图边集的可行树给出，所以它只能从上方逼近最优值。第二，进入队列 key 的每个 future 都是不超过剩余代价的证书，所以只会删除无法严格改善 `best` 的状态。第三，任何没有被删除的 rooted DP 推导，要么在公共前向格中被枚举，要么在 adjoint 格中有一条一一对应的反向推导。工程优化只有在能保持这三个不变量时才进入正式实现。

### 2.1 一条查询的完整控制流

下面的伪代码对应当前 `SolveOneQuery`，顺序本身也是方法定义的一部分。`profile` 只解释预先冻结的增强位；`schedule` 只给出平衡递推需要的层区间。二者都不读取运行时间或中间状态规模。

```text
SolveOneQuery(G, K, options)
  1  检查 options 的依赖；处理空查询、单组、重叠零代价和不可行分量
  2  PrepareProblem:
       计算真实初始上界、组距离、组级下界、锚组和 witness
       若开启 DirectedCut，再增加 dual 证书与 facility 上界
       在产生主状态前完成所选配置的初始 witness 上界职责
  3  profile  <- DescribeConfiguration(options)
  4  q        <- max(0, floor(g/2)-1)
     ell      <- q                         (完整前向配置)
                 或 floor(q/2)            (AdjointCompletion)
  5  若 Base 的 ordinary-future realization 是 A1 cone，且层 1 属于 [1,q]:
       生成标准 A1 row；建立只读 top-two future 视图
  6  BuildOrdinaryRows:
       按 |S| 递增生成公共 D row，使用当前配置的 future
       同时更新规范 branch、真实 witness 上界和完整完成上界
  7  释放 A1 的只读查找缓存；标准 A1 row 本身仍保留
  8  若未开启 AdjointCompletion:
       把已有 A1 row（若有）移交给公共前向内核
       用 A 生成逻辑层 1..q 并完成答案
     否则:
       用同一前向内核生成低层 A(1..ell)
       转置 ordinary 终端，并用递减 H 实现高层 ell+1..q
  9  返回 best、feasible 和实际首次发现的 (mask,v) 数
```

第 5 步不是一条“组数够大才启用”的经验规则。`[1,q]` 是第 8 步精确递推本来就要覆盖的索引域；询问“层 1 是否属于该域”等价于询问一个循环是否有第一轮。Base 把这张本来就属于前向格的 row 提前，只为了在第 6 步复用；第 8 步接收同一对象而不是再生成一张。

### 2.2 逻辑状态、证书与物理缓存的边界

为了避免把工程对象误写成新的算法状态，本文统一区分三层：

| 层次 | 对象 | 能否决定精确答案 | 生命周期 |
|---|---|---|---|
| 逻辑状态 | $D(S,v)$、$A(S,v)$、$H(S,v)$ | 是；它们组成精确递推或其等价反向实现 | 由状态层计划决定 |
| 上下界证书 | `best`、farthest、tour、A1 cone、dual potential | 只能安全保留或拒绝逻辑状态，不能冒充 DP 值 | 可随更紧上界失效并重算 |
| 物理加速缓存 | epoch 数组、top-two bit/locator、64 顶点临时桶 | 否；删除它们只应改变常数 | 阶段结束立即释放或复用 |

特别地，A1 row 属于第一层；围绕它建立的 top-two locator 属于第三层。$H$ 属于第一层，因为它是高层前向依赖的等价反向状态；64 顶点桶只属于第三层。后文的正确性证明只依赖逻辑值与证书不变量，不依赖缓存宽度、容器容量或内存地址。

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

`AdjointCompletion` 依赖 `DirectedCut`，因为高层转置的 reduced value 和 prefix 剪枝读取 directed-cut 组势。仅开启 adjoint 的组合在进入求解前被拒绝。配置在一批查询开始前由命令行冻结；代码不会依据图名、$g$、组大小、row 密度、incumbent、时间或内存动态切换，也不允许逐查询选 Base/Enhanced 的较快值。论文中应称“ABHSS 的 Base 配置和开启全部增强的配置”，不能称为两个方法。

### 3.1 “新增”与“替换”的严格含义

位掩码满足集合关系：

$$
\varnothing
\subset
\{\text{DirectedCut}\}
\subset
\{\text{DirectedCut},\text{AdjointCompletion}\}.
$$

但这不等于 Enhanced 必须逐条重放 Base 的全部指令。代码中的 `ConfigurationProfile` 把差异分成两类：

1. **安全新增。** Enhanced 额外构造 directed-cut 对偶可行证书和 facility 可行上界。前者只以 `max` 加强可采纳下界，后者只以 `min` 收紧真实上界，因此不会排除最优解。代码中的 `AddedOperation` 只登记这类 Base 没有对应 realization 的工作。
2. **同职责替换。** 若 Enhanced 不执行 Base 的某段工作，必须存在结构或功能对应的 realization，且论文能够给出共同输入、共同输出语义及各自优势区间。替换不能凭数据集或运行时表现选择。

当前完整映射如下：

| 逻辑职责 | Base realization | Enhanced realization | 为什么属于替换 |
|---|---|---|---|
| 组距离 oracle | 规范 SPT 先给 cutoff，再构造 bounded `GroupRow` | 不需要 cutoff，直接构造完整距离势 `GroupRow` | 都实现“读取值 + `IsExact`”合同；完整表给出全部真实值，bounded 的非精确位置只给下界占位 |
| ordinary future | 提前 A1 cone | directed-cut potential | 都给出从当前根完成剩余组的可采纳下界；前者复用 A1，后者通常更强 |
| pre-D 与周期 witness | root-path tree，并在 D 前先做一次树 DP | primal upper + dual-primal tree，随后进入同一 rent-or-buy 调度 | 都只用原图真实边给出可行上界；共同 root-star/root-path-union 不属于差异，facility 上界另归安全新增 |
| 锚定 singleton 完成层 | 标准 A1 row 在 ordinary 前生成并移交 | 低层前向 $A$，或补集意义相同的高层 $H$ | 都承担锚组与一个非锚组进入完成格的第一层职责 |
| 高层锚定完成 | 前向高层 $A$ | 低层 $A$ 加反向高层 $H$ | 两者枚举同一平衡完成边界，只改变高层依赖方向 |

因此，不能把流程写成“Base-only early-A1 阶段”。A1 是完整锚定格的一层标准 `Row`。Base 提前求它只是为了让同一 row 兼任 ordinary future，之后 row 的所有权直接交给公共前向内核；Enhanced 以 dual potential 替换 future 职责，并由低层 $A$ 或高层 $H$ 实现完成职责。Base 可能因为证书较弱而留下更多数据相关状态，但没有 Enhanced 缺失的独立状态族。

### 3.2 由递推域推出层边界，而不是按参数分段

令

$$
h=\left\lfloor\frac g2\right\rfloor,
\qquad
q=\max\{0,h-1\}.
$$

$q$ 来自第 10 节的平衡分解定理，是完整锚定格必须覆盖的最高 mask 大小，不是经验阈值。正层的定义域是整数区间

$$
\mathcal L_A=\{s\in\mathbb Z:1\le s\le q\}.
$$

`MakeAnchoredCompletionSchedule` 只把这个定义域映射到 realization：

- Base 与 DirectedCutOnly：前向 $A$ 覆盖逻辑层 $1,\ldots,q$；
- Enhanced：令 $\ell=\lfloor q/2\rfloor$，前向 $A$ 覆盖 $1,\ldots,\ell$，$H$ 反向覆盖 $\ell+1,\ldots,q$；
- 空区间 $\mathcal L_A=\varnothing$ 时，完成式只读取隐式 $A(\varnothing)$，没有任何正层需要物化；
- 非空区间的第一个成员自然是层 1。Base 提前生成该标准 A1 row，不再出现 `g` 与某个调优常数比较、图名、row 密度或运行时间驱动的分派。

这里的 $q$ 与 $\ell$ 都是递推域边界，类似数组下标范围；它们不选择“哪种算法更快”。配置由查询批次开始前的增强位决定，状态层范围由同一平衡证明决定，二者共同形成唯一执行计划。换言之，代码可以询问 `ContainsLogicalLayer(1)`，却不能出现“观测到某个 $g$ 后决定要不要使用 A1”这种算法选择。

### 3.3 配置职责账本

为了能从代码审计“包含关系”而不是只相信文字，`DescribeConfiguration` 为一次运行生成不可变职责账本。四个 replacement 字段始终各选一个 realization；`added_operations` 只允许增加经过证明的证书位：

```text
Base
  added_operations = {}
  group_distance   = BoundedCutoff
  ordinary_future  = AnchoredSingletonCone
  upper_witness    = RootPathTree
  high_layer       = ForwardAnchoredA

DirectedCutOnly
  added_operations = {DirectedCutCertificate, FacilityUpperBound}
  group_distance   = CompletePotential
  ordinary_future  = DirectedCutPotential
  upper_witness    = DualPrimalTree
  high_layer       = ForwardAnchoredA

Enhanced
  与 DirectedCutOnly 相同，仅将
  high_layer       = AdjointH
```

因此，“关闭 Enhanced 的开关得到 Base”有两层含义。配置位上，关闭 `AdjointCompletion` 再关闭 `DirectedCut` 恰好得到空集合；执行语义上，每次关闭会把对应职责恢复为表中的前一个 realization，而不是留下一个只有旧路径才执行的孤立阶段。测试会同时断言位集合包含关系、四个 realization 映射、非法 adjoint-only 拒绝、$0\le g\le16$ 每个必需层恰由 A 或 H 覆盖一次，以及三种合法配置的精确目标值。

## 4. 图加载、可行性与计时边界

`graph.txt` 的首行是 $n,m$，随后恰好 $m$ 行 `u v w`。快速读取器以 8 MiB 块扫描数字，保留原边顺序、`edge_id` 和双向邻接插入顺序；读取完成后按邻接表一次性计算 `component_of[v]`。这次 $O(n+m)$ 连通分量扫描属于所有方法共享的图加载，发生在 `[Ready]` 之前，不进入逐查询计时。

对一条查询，若图连通，可行性检查只验证组非空和顶点范围；若图不连通，则把每组涉及的分量编号排序去重并逐组求交。设查询总组成员数为 $F=\sum_i |K_i|$，该步骤在连通图上为 $O(F)$，非连通图上为 $O(\sum_i |K_i|\log |K_i|)$，不再为每条查询重复扫描整图。手工构造且没有缓存的小测试图会回退执行一次局部分量扫描。

这一边界很重要：旧实现每条查询重新执行 $O(n+m)$ BFS，在 Orkut 的数百条查询上会把共同图性质误计成每种算法的搜索成本。当前图加载秒数和查询加载秒数单独写入结果 header，只作为 artifact 工程指标。

## 5. 记号与公共数据结构

预处理选择锚组 $K_a$。其余 $k=g-1$ 个组重新编号为 bit $0,\ldots,k-1$，映射由 `bit_to_group` 保存。对非锚组 mask $S\subseteq [k]$：

- $D(S,v)$：覆盖 $S$ 对应的所有非锚组并以 $v$ 为根的最小树代价；$D(\varnothing,v)=0$。
- $A(S,v)$：覆盖永久锚组 $K_a$、$S$ 并以 $v$ 为根的最小树代价。
- $H(S,v)$：adjoint 阶段的“外侧已付代价”；它覆盖 $[k]\setminus S$，等待与覆盖锚组及 $S$ 的前缀在 $v$ 处相接。

`D/A/H` 共用一种 `Row`：递增的顶点数组、对齐的值数组、ordinary 专用 branch bitmap、`branch_count` 和显式 `ready`。`ready` 区分“已经生成但为空”和“尚未生成”。不存在 Base/Enhanced 各一套 row，也不存在运行中在 dense/hash/bitmap DP 状态之间切换。

单组到全图的距离 $d_i(v)=\min_{t\in K_i}\operatorname{dist}(v,t)$ 存在 `GroupRow`，它不是 DP row。Base 可保存有界精确锥体；开启 `DirectedCut` 后保存完整 dense 距离。两种布局通过 `operator[]`、`IsExact` 和 `ForEachExact` 暴露同一语义。

### 5.1 `Row` 的逻辑值与物理 payload

一张 DP row 的逻辑键是 `(mask, vertex)`。物理上只存有限、仍可能改善 incumbent 的顶点：`vertex[j]` 严格递增，`value[j]` 是同一下标的精确 DP 值。读取不存在的顶点返回无穷。`ready=false` 表示依赖还没有计算；`ready=true` 且数组为空表示该 row 已被完整计算，但安全锥体为空。二者若混淆，高层会把“无候选”误认为“依赖缺失”，或者反过来读取尚未产生的值。

ordinary row 额外带一张按 payload 下标而非原顶点编号排列的 64-bit branch bitmap。第 $j$ 个 payload 是否为规范 branch，由 `branch_bits[j >> 6]` 的第 `j & 63` 位给出。$A$ 与 $H$ 沿用完全相同的有序 `vertex/value/ready` 布局，只是不解释 branch 位。这样，三种状态可以共用二分读取、双指针交集、诊断统计和所有权移动。

### 5.2 `GroupRow`、cutoff 与 `IsExact`

有界 `GroupRow` 对每个顶点暴露一个有限读取值，但该值有两种含义：

$$
\texttt{row}[v]=
\begin{cases}
d_i(v), & d_i(v)<U_0,\\
U_0, & d_i(v)\ge U_0\text{ 且未保存}.
\end{cases}
$$

因此 `IsExact(v)` 不是浮点精度标记，而是“这个位置保存的是否为真实多源最短距离”。cutoff 占位值可以作为不大于真实距离的拒绝证书，却不能作为 DP seed。若真实距离为 137、cutoff 为 100，把占位的 100 加进 split 会构造不存在的低成本树；所有 singleton 合并、完整结算与 witness 路径都必须先检查 `IsExact`。

有界 dense 布局以 `value[v] < cutoff` 判定精确性。稀疏布局用 `bits[v >> 6]` 判 membership，再通过该 64-bit word 之前的 `rank` 与 word 内低位 `popcount` 定位压紧的值数组。完整距离布局中每个位置都是真实值，故 `IsExact(v)` 恒真。这三个物理分支只实现一个读取契约，不产生三种算法状态。

### 5.3 两套 mask 编号为什么同时存在

查询原始组号使用 $g$ 位：`original_full_mask=(1<<g)-1`，锚组单独占 `anchor_bit`。删除锚组后，DP 只对 $k=g-1$ 位编码，`full_mask=(1<<k)-1`。`bit_to_group[b]` 把压缩 bit $b$ 还原为原组号；`original_mask[S]` 把整个压缩 mask 还原为原始 $g$ 位集合。二者不能混用：

- ordinary row、锚定 row、补集转置和 `popcount` 使用压缩 mask；
- 需要连同锚组调用全组 future、tour 或 dual 时，使用 `anchor_bit | original_mask[S]`；
- `full_mask ^ S` 只在已经确认 $S\subseteq\texttt{full\_mask}$ 时表示非锚补集；原始全组补集使用 `original_full_mask ^ original_mask[S]`。

选择永久锚后，所有上述数组一次性建立且查询内不再改变。这样可把普通状态数从 $2^g$ 降到 $2^{g-1}$，同时让 $A(\varnothing,v)=d_a(v)$ 作为隐式基例，不必为锚 singleton 再保存一张全图 row。

### 5.4 `Row` 的生命周期与所有权

一张 row 依次经历四种物理状态，但逻辑状态只计算一次：

1. `ready=false`：依赖尚未完成，任何消费者都必须跳过；
2. 构造中：局部 `distance/touched/settled` 保存候选，尚未对其他 mask 可见；
3. `ready=true`：`vertex/value` 已排序、对齐并只读，可为空；
4. 被消费或移动：末层可以只用于答案结算而不长期保留；提前 A1 则把同一个 `Row` 的所有权移动到前向容器。

`touched` 记录一张 row 内从无穷第一次变为有限候选的顶点，`settled` 记录真正从优先队列以当前最优值取出的顶点，最终 payload 还会按最新 `best` 再过滤。故三者大小可以不同。`mask_vertex_states` 对每张 row 累计 `touched` 的不同顶点数：状态族属于实际键，因此 D/A/H 中数值相同的 `(mask,v)` 是不同状态项。它描述实际进入主状态工作区的数量，不等于结束时 payload，也不等于队列弹出或松弛次数。A1 移交时不再次累计，这一所有权规则由状态计数回归测试约束。

## 6. 公共预处理：下界、上界与锚组

### 6.1 零权分量覆盖下界

先把所有零权边缩成免费连通分量。每个分量携带它命中的组集合 $C_j\subseteq[g]$。在这些集合上执行精确 set cover DP，令 $c_0$ 为覆盖全部查询组至少需要的零权分量数。若 $c_0=1$，某个零权分量已覆盖全部组，故 $\operatorname{OPT}=0$。

若 $c_0>1$，令 $w_+$ 为最小正边权。任何连通可行树在零权分量缩点图上至少连接 $c_0$ 个分量，因此至少使用 $c_0-1$ 条正权边：

$$
L_{\mathrm{cc}}=(c_0-1)w_+\le\operatorname{OPT}.
$$

若图没有正权边但查询可行，则必有 $c_0=1$。set cover DP 用 $O(3^g)$ 时间、$O(2^g+n)$ 空间，并返回若干代表顶点供上界构造使用。

### 6.2 Base 的规范 SPT 上界与有界组距离

Base 先从每组顶点数、最小顶点号的稳定顺序选规范终端作为候选根。对每个候选根运行 Dijkstra；当遇到尚未覆盖的组时，恢复根到该顶点的 SPT 路径，按原图 `edge_id` 去重计价。所得边并集是覆盖所有组的真实连通子图，因此其最小代价 $U_0$ 是合法上界。该步骤同时是 bounded 距离表示的 cutoff 构造器，而不是 Base 独有的一类逻辑状态；完整距离 realization 不需要 cutoff，因而以完整多源距离和后续共同/dual 上界职责替代这项表示准备。

随后对每个组做多源 Dijkstra。Base 只 settle 满足 $d_i(v)<U_0$ 的顶点。未保存位置返回 cutoff $U_0$，但被明确标记为“非精确”。这是安全的：任何严格优于 incumbent 的完成解中，单个必需连接代价不可能达到或超过 $U_0$。非精确 cutoff 只能参与下界拒绝，参加 DP 合并前必须通过 `IsExact`。

有界表按实际字节比较两种表示：

- dense bounded：长度 $n+1$ 的值数组，锥体外写 cutoff；
- ranked bitmap：递增精确顶点与值、membership 位图、每个 64-bit word 的 rank 前缀。

这只是同一 `GroupRow` 的确定性存储选择，不改变算法配置。开启 `DirectedCut` 时需要所有顶点的势，故关闭 bounded，保存 $g(n+1)$ 个完整距离。

### 6.3 共同根上界与真实路径并集

对任意根 $r$，分别连接到各组最近终端可得：

$$
U_{\mathrm{star}}(r)=\sum_{i=0}^{g-1}d_i(r).
$$

该和可能重复计算共享边，但仍是可行上界。实现从精确顶点数最少的组表驱动扫描，取最小共同根；随后沿满足最短路等式的真实边恢复根到各组的路径，按 `edge_id` 去重，得到通常更紧的 $U_{\mathrm{union}}$。路径等式使用 $10^{-9}$ 只为在浮点输入上找到真实边序列；即使选择了近似等式边，最终仍按原图边权对实际边并集计价，所以它只能影响上界强弱，不能产生虚假下界。

### 6.4 锚组选择

在当前最好共同根 $r$ 处，选择 $d_i(r)$ 最大的组作为永久锚组。直观上，把最远组固定到所有锚定状态中，可较早暴露长连接并增强 future 拒绝。该选择只影响状态组织，不影响可枚举解集合；并列时按组号稳定选择。

### 6.5 组间 tour 下界

定义组间松弛距离：

$$
\delta(i,j)=\min_{u\in K_i,v\in K_j}\operatorname{dist}(u,v).
$$

代码等价地在 $K_j$ 上取 $d_i$ 的最小值。有界距离返回的 cutoff 不大于被截断的真实距离，因此即使矩阵因截断而不完全对称，仍保持下界方向。

对每个组子集和一对固定端点，subset DP 预计算访问该子集中每个组一次的最短 Hamilton path。查询顶点 $v$ 与剩余组 mask $R$ 时，把 $v$ 分别接到路径两端；对每个被指定为端点的组取最小，再在端点组上取最大，最后除以 2，得到 $L_{\mathrm{tour}}(v,R)$。证明来自树的倍增：任意从 $v$ 出发覆盖 $R$ 的树，边倍增后存在长度至多两倍树权的闭合遍历；在组度量中 shortcut 并指定任一组作为首个端点不会增长。因此每个 fixed-endpoint 值除以 2 都不超过剩余树代价，最大值仍可采纳。

### 6.6 最远组与统一 future

最便宜的 future 是：

$$
L_{\mathrm{far}}(v,R)=\max_{i\in R} d_i(v).
$$

每个顶点缓存全体组中最远组；若该组仍在 $R$ 中可 $O(1)$ 返回，否则扫描 mask。Base 的统一下界为：

$$
L_{\mathrm{future}}(v,R)=\max\{L_{\mathrm{far}},L_{\mathrm{tour}}\}.
$$

开启 `DirectedCut` 后再取 directed-cut 下界 $L_{\mathrm{cut}}$ 的最大值。热路径总是先算便宜证书；一旦 `partial + lower >= U` 即拒绝，不支付更贵证书。最终值按 row epoch 缓存。

### 6.7 预处理顺序为何固定

当前顺序不是可交换的实现细节：零权分量下界先提供安全闭合条件；Base 的规范 SPT 先提供 bounded distance 所需 cutoff；组距离随后支持共同根、真实路径并集、锚组与 tour；最后才构造所选上界 witness。Base 会在任何 A/D 状态产生前，用尚只有隐式 singleton 的 ordinary 表购买一次 root-path witness DP。旧顺序把同一次购买放在 A1 之后，使 A1 使用更松的 incumbent；前移不会增加一种操作，只让后续所有状态看到同一个更紧上界。

DirectedCut 配置不重放这次 Base root-path 购买。共同的 root-star 与 root-path-union 上界仍由两种配置执行；差异只在随后承担 pre-D 与周期 witness 的 realization。DirectedCut 用 primal upper 及由同一 primal 边构造的 dual-primal tree 替换 Base 的 pre-D root-path-tree DP 与后续 witness 来源，facility 上界则是独立的安全新增；之后两边共用 ordinary 内的 rent-or-buy 调度器。这个顺序体现“同职责替换 + 安全新增”，但不能写成 Enhanced 删除了共同的 root-path-union 上界。

## 7. 普通状态 $D$ 与规范 branch

### 7.1 精确递推

对 singleton，$D(\{i\},v)=d_i(v)$。对 $|S|\ge2$，先在共同根合并两个真子集，再在图上做多源最短路闭包：

$$
B(S,v)=\min_{\varnothing\neq T\subsetneq S}\{D(T,v)+D(S\setminus T,v)\},
$$

$$
D(S,v)=\min_{u\in V}\{B(S,u)+\operatorname{dist}(u,v)\}.
$$

实现按 $|S|$ 递增，只生成到 $\lfloor g/2\rfloor$。size 2 直接求两个 singleton 的共同精确顶点；更高层固定 mask 的最低 bit 在 accumulator 侧，只枚举不含该 pivot 的 branch，从而消除左右对称和重复拆分。

### 7.2 A* 式稀疏图闭包

每个同根 split seed 的真实已付值为 $x$，队列 key 为：

$$
f=x+L_{\mathrm{future}}(v,[g]\setminus S).
$$

只有 $f<U$ 的候选进入或继续传播。因为 $L_{\mathrm{future}}$ 不超过任何完成代价，$f\ge U$ 的状态不可能导出严格优于已经存在的可行解；等于 $U$ 时也无需保留，因为同成本答案已存在。剪枝不会改变最优权值。

### 7.3 branch 的定义与完备性

实现同时保存图闭包前的 seed 值 $B(S,v)$。若最终 $D(S,v)<B(S,v)$，到达 $v$ 的最优实现必须从另一根经过至少一条边，代码把它标为 branch；若 $D(S,v)=B(S,v)$，则该状态在 $v$ 处仍有一个同根真子集拆分，不发布 branch。

这不会删掉解。考虑任意更高层在根 $v$ 使用一个未标 branch 的 $D(S,v)$：用实现其等值的两个真子状态替换，成本不变；反复替换后必到 singleton 或某个经过图边闭包的 branch。于是每个同根合并等价类至少保留一个规范分解。后续只要求“被接入的一侧”为 branch，accumulator 仍可是任意 ready 值，因此不会要求所有子块同时为 branch。

### 7.4 平衡完成式与三分完成式

当 ordinary 层达到半格时，代码用 $D(S,v)+D(\bar S,v)+d_a(v)$ 产生真实完整上界；在三块大小达到平衡点时，还枚举三个 ordinary 块与锚组共同相遇的完成式。它们只更新 $U$，不把某个启发式完成值当成精确状态，也不替代尚未生成的 row。

### 7.5 一张 ordinary row 如何生成

下面展开 `BuildOrdinaryRows` 对一个非 singleton mask 的实际顺序。singleton 的逻辑值直接来自 `GroupRow`，不复制成 `ordinary[bit]`；`OrdinaryValue` 和 `ForEachOrdinaryValue` 对调用者隐藏这一区别。

```text
for size = 2 .. h:
  for each mask S with |S| = size:
    split[v] <- infinity
    for each canonical split S = P disjoint-union B:
      枚举 D(P,v) 与 branch D(B,v) 的共同精确顶点
      split[v] <- min(split[v], D(P,v)+D(B,v))

    对每个有限 split seed x at v:
      lower <- max(farthest, tour, selected ordinary-future realization)
      仅当 x+lower < best 时放入最短路队列

    做非负边权图闭包；每次松弛前再次检查同一可采纳 future
    对 settled 顶点按编号排序、去重，并写入 D(S)
    若 D(S,v) 严格小于 split[v]，设置对应 branch bit
    累计本 row 的首次发现状态与 rent 工作量
    执行当前 size 能触发的两块/三块完整上界结算

  若累计 rent 达到 buy，才在真实 witness 树上重算一次上界
```

同根 split 与图闭包刻意分开保存。`split[v]` 用来判断规范 branch，`distance[v]` 是闭包后的精确 rooted 值；若只保留后者，就无法区分“在当前根仍可继续零边拆分”与“必须从别的根经图边到达”的状态。所有临时 dense 数组用 `touched` 或 epoch 恢复，而不是每张 row 清空 $n$ 个位置。

## 8. 真实 witness 与 rent-or-buy 上界

根路径并集或 directed-cut primal 会被整理为一棵以锚组终端为根的真实树。零权边上只允许严格距离改进来设置父指针；等距候选保留第一次父亲，避免 settled 祖先被重新挂到后代形成环。构造后还验证所有 witness 顶点可从锚根到达。

随着 ordinary row ready，可在该 witness 树上做独立 subset DP：树边只在子树确实携带非空 mask 时支付一次，节点处可以组合已经存在的 ordinary rooted 子树。任何有限结果都对应一棵原图可行树，所以只会收紧 $U$。一次求值的估计成本约为 witness 顶点数乘以 $3^k$ 合并量。普通 row 的 queue pop 和 edge relaxation 累计为 rent；只有 rent 达到一次 buy 估计才重新求值，避免每层重复扫描固定 witness。

Base 的零 rent 首次购买已前移到 A1 之前；DirectedCut 的 primal upper/witness 在预处理中替换该职责，facility 上界同时作为安全新增执行。进入 ordinary 后，两种配置只运行同一个 `WitnessUpperScheduler`：`buy` 由 witness 顶点数和子集拆分数确定，`rent` 只累计实际 queue pop 与邻接松弛，达到 `buy` 后重新执行公共树 DP 并清零 rent。它不依据墙钟、图名或某条查询的历史快慢作决定。

## 9. 公共 A1 层的调度与 ordinary future 替换

### 9.1 A1 的状态语义

对每个非锚 singleton $i$，公共锚定 singleton 状态定义为：

$$
A(\{i\},v)=\min_u\{d_a(u)+d_i(u)+\operatorname{dist}(u,v)\}.
$$

式中 $d_a(u)+d_i(u)$ 是锚组与组 $i$ 在共同根 $u$ 合并的 seed，外层最短路闭包把根移动到 $v$。所以 A1 不是辅助启发式，而是完整前向 $A$ 递推的第一张真实逻辑 row。Base 提前生成它，ordinary 结束后把同一批标准 `Row` 移交给 `BuildForwardAnchoredRows`；前向阶段只按更新后的 incumbent 重滤并完成结算，不再次运行闭包。

### 9.2 有界 A1 cone 及其下界证明

设 A1 构造开始时的真实可行上界为 $U_0$。对组 $i$ 定义尚未由 A1 覆盖的非锚组集合 $R_i$，并令

$$
C_i(v)=\max_{j\in R_i} d_j(v).
$$

Base 只保存满足 $A(\{i\},v)<U_0$ 且 $A(\{i\},v)+C_i(v)<U_0$ 的精确位置。这里 $A(\{i\},\cdot)$ 是多源最短路值，$C_i$ 是若干最短距离函数的最大值；二者沿任意边 $(x,y)$ 都满足一致性关系。特别地，若 $x$ 是到 $y$ 的一条最短 A1 路径上的前驱，则

$$
A(\{i\},x)+C_i(x)
\le A(\{i\},y)+C_i(y).
$$

因此，只要目标 $v$ 满足 $A(\{i\},v)+C_i(v)<U_0$，其规范最短路径上的全部前缀也满足严格 cone 条件，Dijkstra 不会在到达 $v$ 前被剪掉。反过来，若 $v$ 没有保存，则必有 $A(\{i\},v)+C_i(v)\ge U_0$，从而可安全返回：

$$
\underline A_i(v)=\max\{0,U_0-C_i(v)\}.
$$

若位置在 row 内，`Value` 返回精确 $A(\{i\},v)$；否则返回上述 cone 外下界。两者统一记为 $\underline A_i(v)$。对一个 ordinary 状态 $D(S,v)$，尚未覆盖的每个组 $i$ 都必须进入包含锚组的完成部分，因此

$$
L_{A1}(v,[k]\setminus S)
=\max_{i\in[k]\setminus S}\underline A_i(v)
$$

是相应规范完成的可采纳 future。代码把它再与 farthest 和 tour 取最大。

### 9.3 不含经验组数阈值的统一调度

调度只读取第 3.2 节的逻辑层域 $\mathcal L_A=\{1,\ldots,q\}$。域为空时，求解直接用隐式 $A(\varnothing)$ 完成；域非空时，A1 正是它的第一个成员，Base 对所有查询都在 ordinary 前生成这一成员。不存在“组数较小时延后、组数较大时提前”或任何等价的隐藏阈值。

每个顶点第一次查询 $L_{A1}$ 时，代码扫描所有 A1 singleton，缓存最大和次大的组 bit，以及对应精确值在 `row.value` 中的 32-bit 下标。若该 top 值来自 cone 外，则 locator 的最高位记录“直接重算 $\max\{0,U_0-C_i(v)\}$”。两个 locator 压在一个 64-bit 项中，只在该顶点第一次真正查询 future 时写入；后续若最大 bit 尚未被 $S$ 覆盖就 O(1) 读取，否则优先读取次大值，只有两者都被覆盖时才扫描剩余 bit。ordinary 结束后立即释放 bit/locator 查找缓存，只保留需要移交的 A1 row。这项优化不舍入数值，也不改变任何 future 或状态集合。

### 9.4 Enhanced 如何覆盖 A1 的两个职责

A1 在 Base 中同时承担“ordinary future”和“锚定完成第一层”两个职责。开启 `DirectedCut` 后，这两个职责分别覆盖：

1. ordinary future 由 $L_{\mathrm{cut}}$ 替换，不支付 Base 的 A1-before-D 固定成本；
2. DirectedCutOnly 的完整前向格仍在 size 1 生成标准 A1；Enhanced 中，若层 1 属于低层域 $\{1,\ldots,\ell\}$，也由公共前向内核生成，若它属于高层域 $\{\ell+1,\ldots,q\}$，则由补集 $H$ 承担同一完成边界；若层 1 不属于 $\mathcal L_A$，任何配置都不存在该逻辑层。

这不是“Base 有 A1、Enhanced 没有 A1”的不对称，也不是声称 $H(S,v)$ 与 $A(S,v)$ 数值相等。准确含义是：在完整解推导中，本来跨过 A1/高层 A 的那段依赖，要么正向物化为 $A$，要么通过第 12.4 节的双向映射反向物化为 $H$。两者覆盖相同的完整推导集合。

### 9.5 A1 的缓存正确性与所有权移交

`BuildReusableAnchoredSingletonLayer` 为每个非锚 bit 建立一张 `ready` 的标准 row；即使安全 cone 为空，`ready=true` 也表示“该层已经完整处理”，而不是缺失依赖。构造时固定的 $U_0$ 只用于定义 cone 与 fallback。后来 `best` 进一步下降不会让旧 fallback 失效，因为它证明的是 $\underline A_i(v)\le A(\{i\},v)$，与当前上界数值无关；更紧的 `best` 只会让更多已保存位置在移交时被重新过滤。

对固定顶点，设全部 singleton future 按值降序为 $x_1,x_2,\ldots$。查询剩余 mask 时：若 $x_1$ 的 bit 仍在 mask 中，答案必为 $x_1$；否则若 $x_2$ 仍在，答案必为 $x_2$；只有两者都不在时才需扫描 mask。故缓存最大和次大足以覆盖所有 O(1) 情形。并列值按稳定 bit 顺序选择不影响最大值。locator 只保存精确 `row.value` 的下标，不保存近似数值；最高位作为 fallback 标志。顶点编号和 payload 下标均受 32-bit 有符号图接口约束，故有效下标不会占用该标志位。

ordinary 完成后，`first/second/locator` 立即释放。随后 `std::move(singleton_future.row)` 把 row 容器交给 `RunForwardAnchoredStage`。前向内核看到 size 1 已 `ready` 时不会重新执行合并或图闭包，只按最新上界重滤、运行正常的完整解结算，并跳过第二次状态累计。这保证“提前供 future 使用”和“属于公共 A 格”是同一物理对象的两个阶段，而不是 Base 私有 row 加上一张公共 row。

## 10. 前向锚定状态 $A$

### 10.1 递推与图闭包

锚定递推为：

$$
A(S,v)=\min_{u\in V}\left\{
\min_{\varnothing\neq T\subseteq S}
A(S\setminus T,u)+D(T,u)+\operatorname{dist}(u,v)
\right\},
$$

其中 $A(\varnothing,v)=d_a(v)$ 为隐式 row，不为全图单独物化。对固定 $S$，内层枚举一个非空 ordinary 块 $T$，要求 $D(T)$ 已 ready，且 $A(S\setminus T)$ 已 ready 或为空 mask。合并只在同一根 $u$ 发生，随后从所有有限 seed 做一次多源 Dijkstra 闭包。和 ordinary 一样，只有 `value + future < best` 的候选进入队列。

`ForEachAnchoredSum` 对 singleton 使用 `GroupRow` 的精确 membership；对多组 ordinary 块只消费规范 branch；对锚定侧读取完整值。这样，A 的每个同根合并都能展开为一个合法 ordinary 分块，同时沿用第 7.3 节的规范化完备性。seed 合并、图闭包、future 剪枝和稀疏 row 写回都由 `forward.cpp` 的一份实现完成。

### 10.2 每张 row 的上界结算

每个 settled $A(S,v)$ 会执行两种只收紧上界的操作。第一，把每个未覆盖 singleton 的组距离直接接到 $v$：

$$
U_{\mathrm{root}}=A(S,v)+
\sum_{i\in[k]\setminus S}d_i(v).
$$

虽然不同最短路可能重复边，该和仍对应若干真实路径的并，因此是可行上界。第二，`CompleteAnchoredRow` 把剩余 mask $R=[k]\setminus S$ 分成至多两个 ordinary 块 $L$ 与 $R\setminus L$，并在共同根结算：

$$
U_{\mathrm{split}}=
A(S,v)+D(L,v)+D(R\setminus L,v).
$$

实现从实际可枚举值最少的一侧驱动交集，singleton 先检查 `IsExact`，多组块要求 ready 并遵守 branch 规范。结算产生的是完整可行树候选，不写入新的 full-mask DP row。

### 10.3 为什么只到 $q=h-1$

令 $h=\lfloor g/2\rfloor$。Base 生成到 $|S|=h-1$；最后层只消费和结算，不保存 payload。其充分性来自平衡分解：去掉永久锚组后，任意完整规范分解都可选择一个包含锚的块，使其非锚组数不超过 $h-1$，余下组可分成至多两个大小不超过 $h$ 的 ordinary 块。所有这样的 left/right 子 mask 都在 `CompleteAnchoredRow` 中枚举。当 $q=\max\{0,h-1\}=0$ 时，直接用隐式 $A(\varnothing)$ 完成。

更具体地，把完整规范推导在最上层看作若干以同一根相接的最大子块，并选含锚组的块为锚定侧。若它包含超过 $h-1$ 个非锚组，则其余非锚组少于 $h$，可以沿锚定递推向下移动一次分界；反复平衡后，可令锚定侧大小不超过 $h-1$，而外侧至多拆成两个大小不超过 $h$ 的 ordinary 块。ordinary 已生成到 $h$，前向格只需生成到 $q=h-1$。最后一层只为结算存在，所以 `retain_last_layer=false` 时不保留 payload，不影响完备性。

### 10.4 公共前向内核的三种进入方式

`BuildForwardAnchoredRows` 只有一份实现，差别只在输入计划和初始 row 所有权：

| 调用者 | `last_size` | 初始 row | 末层是否保留 | 后续阶段 |
|---|---:|---|---|---|
| Base | $q$ | 已提前生成的 A1（若该层存在） | 否 | 直接返回最优值 |
| DirectedCutOnly | $q$ | 空 | 否 | 直接返回最优值 |
| Enhanced | $\ell$ | 空 | 是 | 把低层边界交给 $H$ |

对每个 size，内核先检查初始容器中该 row 是否已经 `ready`。复用 A1 时，它仍执行与普通 A row 相同的 `CompleteAnchoredRow` 和上界重滤；未复用时才枚举 $A(S\setminus T)+D(T)$ seed 并做闭包。`last_size=0` 只表示本次前向正层循环为空：若完整域 $\mathcal L_A$ 本身为空，计划另设 `complete_implicit_anchor=true`，直接以隐式 $A(\varnothing)$ 做最终结算；若只是 Enhanced 的低层前缀为空而高层域非空，该位为 false，答案必须交给 H 边界完成。两种情况都不会创建假的 size-0 payload，读取 $A(\varnothing,v)$ 时统一退回 $d_a(v)$。因此 Base、消融配置和 Enhanced 的低层不是三套递推，只是同一递推在不同已证明层边界上的调用。

## 11. `DirectedCut` 增强

### 11.1 对偶势

把每条无向边表示为两个同容量的有向弧。各组按根到该组的原始距离递减处理。第一个组直接使用距离势；后续组只从先前真正改变过 residual 的弧检查 Bellman 违反，再执行必要的最短路传播。每组势在根距离处截断，并从对应有向弧 residual 中扣除势差，始终截到非负。

对每个组 $i$ 得到势 $\pi_i(v)$。非负 residual 是可核验的对偶可行性证书：一棵从 $v$ 连到剩余每个组的树，在有向展开中必须跨过各组的一个有效割；顺序容量扣减保证不同组势对同一弧的总收费不超过该弧容量。因此：

$$
L_{\mathrm{cut}}(v,R)=\sum_{i\in R}\pi_i(v)
\le \text{从 }v\text{ 完成 }R\text{ 的最小代价}.
$$

changed-arc 只减少每轮重新检查的弧，不改变最终 residual 最短路条件；处理顺序和并列规则固定，不依据查询运行表现切换。

### 11.2 primal 与 facility 上界

全部组处理后，代码从根出发，在 residual 不超过固定数值容差 $10^{-10}\max\{1,w(e)\}$ 的有向弧上逐次连接尚未覆盖的组；每次仍以原边权运行 Dijkstra，把真实路径边写入 bitmap，并按真实新增路径成本计价。该容差只扩大 primal 候选支撑，不进入 dual 下界或上下界闭合；即使纳入一条并非数学零 residual 的弧，恢复结果仍由原图真实边计价，因而只可能改变上界强弱。若恢复成功，得到 primal 上界和 witness 树。

此外，取 primal 边涉及的顶点为 facilities。在同一数值零 residual 弧与 primal 树边构成的支撑图上计算 facility 间真实最短路，并做小规模 subset DP，得到第二个可行上界。所有数值最终都由真实原图路径组成；residual 只限制候选支撑，不直接充当答案。完成这两项后立即释放 $2m$ residual，只保留 $gn$ 组势和 primal edge bitmap。

只开 `DirectedCut` 时，后续仍运行完整前向 $A$。这一中间配置只用于 correctness/ablation，以隔离第二项增强，不是第四套算法。

## 12. `AdjointCompletion` 增强

### 12.1 为什么需要反向高层

完整前向格在高层不断用相似 ordinary row 扩展许多 $A(S,\cdot)$。对一个完整解，真正重要的是低层锚定前缀与“其余组已经付出的外侧代价”在边界相遇。adjoint 把高层依赖按补集转置，使同一个 ordinary 值在一个顶点处一次参与多个高层目标。

固定：

$$
h_{\max}=\lfloor g/2\rfloor-1,
\qquad
h_{\mathrm{low}}=\left\lfloor h_{\max}/2\right\rfloor.
$$

公共前向内核只物化 $|S|\le h_{\mathrm{low}}$ 的 $A$，并保留边界层。该式是对最高逻辑层的固定 meet-in-the-middle 切分，不含“某个组数以上才启用”的经验分类，也不看 row 密度、图名或耗时。

### 12.2 按顶点补集转置

普通 row 按 mask 存储：`mask -> [(vertex,value)]`；转置需要相反访问方向：`vertex -> [(mask,value)]`。若给全部 $n$ 个顶点各建一个临时容器，大图会产生 $O(n)$ 个 vector 对象。实现因此每次只处理 64 个连续顶点，用 `array<vector<TerminalEntry>,64>` 聚合当前块内可用的 $D(B,v)$。

多组 ordinary row 的顶点已递增，每张 row 维护一个单调 cursor，只扫描落入当前块的 payload；singleton 直接从完整组距离读取当前 64 个位置。块内用 `vertex & 63` 定位桶，处理完即清空复用。64 与 membership/branch bitmap 的一个 `uint64_t` word 自然对齐，并限制临时工作集；它不是 64 线程、SIMD/GPU warp 或图压缩参数。替换为其他固定块宽不会改变候选集合，只影响常数和缓存行为。

一个或两个互不相交 ordinary 块的并集为 $Q$ 时，产生目标：

$$
S=[k]\setminus Q,
\qquad
H(S,v)\leftarrow \sum_j D(B_j,v).
$$

这里 $H(S,v)$ 已支付 $S$ 外的组，等待锚定前缀覆盖 $S$。只保留 $h_{\mathrm{low}}<|S|\le h_{\max}$ 的目标。一个或两个块足够，是因为平衡完成边界的外侧在规范分解中至多由两个大小不超过 $h$ 的 ordinary 块组成；更多同根块可按第 7.3 节递归规范化进已有 branch。

每个顶点先计算 subset 势 $\Pi(B,v)=\sum_{i\in B}\pi_i(v)$；ordinary 值减去对应势得到 reduced value。由于完整解至少支付全组势，只有 reduced 值之和不超过预算 `best - full_potential` 的组合可能改善 incumbent。代码比较两种等价枚举的确定性工作量：按 reduced value 排序后枚举可行 pair，或对已有 mask 枚举互补 submask。选择只改变同一候选集合的遍历常数。

终端写入前还计算包含锚组和目标 $S$ 的 prefix：farthest、tour 和 directed-cut 三者取最大。只有 `terminal + prefix < U` 才保存。输出按目标 mask 分组、顶点递增，可直接装载到图闭包。

### 12.3 递减 $H$ 与边界结算

从 $h_{\max}$ 递减到 $h_{\mathrm{low}}+1$。对目标 $S$，除转置终端外，还可从更大的 successor $S\cup B$ 加一个 ordinary branch：

$$
H(S,v)=\operatorname{closure}\left(
\min_{B\subseteq [k]\setminus S}
H(S\cup B,v)+D(B,v)
\right).
$$

每张 $H(S)$ 完成后，枚举低层锚定 mask $L\subseteq S$，令 $B=S\setminus L$，在共同根结算：

$$
A(L,v)+D(B,v)+H(S,v).
$$

三部分分别覆盖锚组及 $L$、边界块 $B$、以及 $[k]\setminus S$，恰好覆盖全部查询组。

### 12.4 与完整前向格的对应

取完整前向 $A$ 的任一规范推导，沿依赖从高层向低层找到第一次跨过 $h_{\mathrm{low}}$ 的边。设跨界前锚定 mask 为 $L$，跨界后为 $S$。新加入的外侧 ordinary 块以及最终剩余外侧块，在转置中产生 $H(S)$ terminal；完整前向中此后每一次“给 A 加一个 ordinary branch”，在递减 $H$ 中反向成为“从更大 successor 去掉同一 ordinary branch”；最后 $A(L)+D(S\setminus L)+H(S)$ 恰好在边界结算相遇。

反向也成立：任取一个转置 terminal，把其一个或两个 ordinary 块作为完整前向推导的外侧；再把每一步 $H(S\cup B)+D(B)$ 按相反顺序加回锚定侧；最后接上低层 $A(L)$。每一步都使用同一 ordinary branch 和同一根，因此得到一条合法完整前向推导，且边权和不变。由此得到高层前向推导与“terminal + 递减 H + 边界结算”的双射。Adjoint 改变的是依赖求值方向和物化层数，不改变状态语义覆盖的完整解集合。

### 12.5 一张 $H$ row 的构造顺序

`H(S)` 与 `A(S)` 的数值含义不同，不能逐项比较；可比较的是它们在完整推导中的边界职责。一张 $H$ row 按以下顺序产生：

```text
for size = q down to ell+1:
  for each target S with |S| = size:
    从转置得到的 terminal[S] 装载外侧已付候选
    for each nonempty B subset of full_mask xor S:
      successor <- S union B
      若 H(successor) ready:
        只用 ordinary singleton 或规范 branch D(B) 做同根松弛
    以 max(farthest, tour, directed-cut prefix) 为可采纳前缀做图闭包
    写回顶点递增的 H(S) 并累计首次发现状态
    枚举 L subseteq S, |L|<=ell:
      用 A(L)+D(S xor L)+H(S) 更新真实完整上界
```

递减顺序保证读取 `H(successor)` 时 successor 已经 ready。`S xor L` 等于集合差仅因为先检查了 $L\subseteq S$；这与第 5.3 节的补集约束一致。图闭包传播的是已经真实支付的外侧边权，prefix 只用于拒绝，因此有限 $H$ 值始终可展开为若干 ordinary 子树和原图路径。边界结算同时读取低层 A、一个中间 ordinary 块和 H，三者的组集合互不重叠且并为全集。

## 13. 完整正确性论证

下面给出当前实现必须同时满足的证明链。

**引理 1（上界真实性）。** `best` 只由规范 SPT 边并集、共同根真实路径并集、directed-cut primal、facility 支撑路径、witness-tree DP、root-star 或完整状态结算更新。每一项都能展开为原图上的连通覆盖，因此 `best` 始终满足 $best\ge\operatorname{OPT}$。

**引理 2（基础 future 可采纳）。** $L_{\mathrm{cc}}$ 来自零权缩点后必须连接的分量数；$L_{\mathrm{far}}$ 是任一剩余组不可回避的单组距离；$L_{\mathrm{tour}}$ 来自可行树倍增、组度量 shortcut 和除以 2。三者分别不超过其声明的剩余代价，因此任意最大组合仍可采纳。

**引理 3（有界距离安全）。** Base 中未保存的 $d_i(v)$ 至少为构造时 cutoff。它只能作为拒绝证书；任何需要精确距离的 split、完成式或 witness 都检查 `IsExact`。因此 cutoff 不会被当作一棵虚构的低成本子树。

**引理 4（A1 cone 安全）。** 对固定 $i$，$C_i$ 是一致的最短距离最大值。若 $A(\{i\},v)+C_i(v)<U_0$，一条最短 A1 路径上的每个前缀也满足该不等式，所以有界闭包必然保存真实 $A(\{i\},v)$。未保存时 $A(\{i\},v)\ge U_0-C_i(v)$，故 `Value` 返回的精确值或 fallback 都不大于真实 A1；对剩余 singleton 取最大仍是可采纳 future。

**引理 5（directed-cut future 可采纳）。** 每轮势差只从相应方向的非负 residual 容量扣除，全部组在任一有向弧上的累计收费不超过原容量。任何从当前根连接指定剩余组的树都必须支付这些割势，因此 $L_{\mathrm{cut}}$ 不超过剩余代价。与引理 2 的证书取最大仍安全。

**引理 6（稀疏图闭包精确）。** 对固定 mask，所有同根 seed 都是真实子树之和。Dijkstra 只在 `value + admissible_future < best` 的区域传播；区域外不可能导出严格优于已有上界的完整解。区域内每次松弛使用真实边权，过期队列项只被忽略，故写入 row 的值等于完整 rooted DP 在该安全锥体内的精确值。

**引理 7（普通格规范化完备）。** 完整 rooted subset DP 的任意同根拆分可通过最低 pivot 唯一选定 accumulator/branch 方向。未发布的非 branch 状态存在一个等成本同根真子集拆分；递归展开最终到达 singleton 或经过图边闭包的 branch。因此只在被接入侧要求 branch 不会删掉一个分解等价类的最后代表。

**引理 8（前向完成完备）。** 平衡分解保证任意完整规范树可表示为一个大小至多 $q=h-1$ 的锚定块和至多两个大小至多 $h$ 的 ordinary 块。前向 $A$ 枚举锚定块的每次合法增长，`CompleteAnchoredRow` 枚举余下至多两块，因此 Base 与 DirectedCutOnly 不遗漏完整规范推导。

**引理 9（Adjoint 等价）。** 第 12.4 节给出完整高层 $A$ 推导与“低层 $A$ + 转置 terminal + 递减 $H$ + 边界结算”的双向映射；映射保持普通 branch、共同根与代价和。转置 budget、prefix 和 H 闭包只使用引理 2、5 的可采纳下界。因此 Enhanced 与完整前向格枚举相同的可改善完整推导。

**引理 10（配置覆盖）。** `ConfigurationProfile` 的每个差异要么只增加引理 1 或引理 5 类型的安全证书，要么替换同一职责：bounded/complete 组距离保持读取契约，A1/dual 保持 future 契约，root-path/dual-primal 保持真实上界契约，前向高层 A/adjoint H 由引理 9 对应。第 3.2 节的计划保证每个必需逻辑层恰由一个 realization 覆盖，没有 Base-only 的未替代状态族。

**定理（ABHSS 精确性）。** 对任意合法配置、无向非负边权输入及 $g\le16$ 的查询：若共同分量不存在，算法正确返回 infeasible；否则算法返回 $\operatorname{OPT}(G,\mathcal K)$。证明如下：引理 1 保证任意时刻 `best` 不低于最优值；引理 2–6 保证距离截断、future 与严格上界剪枝不删除任何代价低于当前 `best` 的完整推导；引理 7 与引理 8（Base/DirectedCutOnly）或引理 9（Enhanced）保证至少一条最优规范推导仍被枚举。搜索耗尽时不存在低于 `best` 的未枚举可行解，故 `best <= OPT`；与引理 1 的 `best >= OPT` 合并得到 `best = OPT`。所有上下界闭合使用原始 `double` 顺序 `best <= lower`，不以 epsilon 把正 gap 当作 0。

### 13.1 证明责任如何落到代码接口

正确性不是由一个最终断言集中保证，而是由接口边界逐层约束：

| 证明责任 | 代码边界 | 必须成立的可检查条件 |
|---|---|---|
| 上界真实性 | `PrepareProblem`、`EvaluateWitnessTree`、各完成式 | 每次写 `best` 的有限值都能展开为原图真实边或 rooted DP 值 |
| bounded 距离不冒充状态 | `GroupRow::IsExact`、`ForEachExact` | 所有作为 seed 的 singleton 必须精确；cutoff 只能进入下界比较 |
| future 可采纳 | `FutureBound`、A1 `Value`、dual `At` | 不同来源的证书只用 `max` 合并；只有 residual 收费等已经证明互不超容量的内部项才可求和 |
| row 依赖完备 | `ready`、按 size 递增/递减循环 | 消费者只读已经完成的依赖；空 row 与未生成 row 不混淆 |
| 规范拆分完备 | branch bit 与 pivot 规则 | 每个等价拆分类至少有一个可被高层消费的代表 |
| A/H 覆盖相同边界 | `AnchoredCompletionSchedule` | $1..q$ 的每个逻辑职责由前向 A 或 adjoint H 恰好覆盖一次 |
| 严格剪枝 | 所有 `candidate + lower < best` | 等于上界的状态可以删除，因为已有同成本真实解；正 gap 不能闭合 |

`ConfigurationProfile` 不参与数学数值计算，但它把最后一项变成机器可测合同：安全新增位只能单调增加，四个同职责字段必须落在已证明 realization 中。若以后加入第三种 future 或高层实现，必须先扩展该枚举、上述表格和独立 DP 回归，不能只在某个函数中增加数据相关分支。

### 13.2 边界输入为何不构成证明例外

- **重叠组。** 一个顶点可同时命中多个组。零权分量 cover、组距离和 full-mask 更新都按 bit 并集处理，不要求每组选择不同终端；若一个零成本连通分量覆盖全部组，答案直接为 0。
- **零权边。** Dijkstra 仍适用，但 witness 父指针只在严格距离下降时改写，防止等距重挂形成环。DP 的严格上界剪枝针对完整成本，不要求边权严格为正。
- **重边与自环。** 邻接表保留各自 `edge_id`；真实路径并集按边 ID 去重。非负自环不能改善最短路，因而不会影响最优值。
- **非连通图。** 只有当某个连通分量与每个组相交时查询才可行；一旦选定共同分量，任何跨分量状态都不可能进入一棵可行树。
- **空 row。** `ready=true` 且 payload 为空表示在当前严格上界锥体中没有可改善状态；这不同于数学值处处无穷，也不同于依赖未计算。证明只需要“不遗漏低于 best 的推导”。
- **浮点输入。** 最短路和 DP 使用输入 `double` 的确定性比较；容差只用于跨程序报告以及恢复一条数值等式路径，恢复后仍按真实边权计价。核心闭合不用 epsilon。

工程证据不是数学证明的替代，但用于防止实现偏离上述引理：历史零权父指针反例；包含零权、重叠组和多终端的确定性随机图，三种合法配置逐例对照独立全子集 DP；高 $g$ SteinLib 已知最优值；非法配置、不可行图、平凡查询、输入格式和小于 $10^{-9}$ 正 gap 的闭合回归。

## 14. 复杂度分析

### 14.1 最坏界

令 $k=g-1$，$h=\lfloor g/2\rfloor$，$F=\sum_i|K_i|$，并令 $r$ 为 directed-cut primal 涉及的不同 facility 顶点数；Base 中取 $r=0$。忽略只改善常数的稀疏性，并把当前二叉堆、真实路径恢复和 facility 上界都计入，一个输出敏感的保守总时间上界可写为：

$$
O\!\left(
(g+r+2^g)(m+n)\log(n+m)
+g^2(m+n)+2^g(g^3+r^2)+3^g(n+r)+F\log(F+1)
\right),
$$

其中 $g^2(m+n)$ 保守覆盖至多 $O(g)$ 个候选根、每根至多 $g$ 条真实最短路的恢复；$r$ 相关三项来自 facility 支撑图。若只讨论 facility 之后的主状态搜索，常用简写才是 $O(3^g n+2^g(m+n)\log(n+m))$。这里按当前 `std::priority_queue` 的重复入堆二叉堆实现计每次 push/pop 的 $O(\log(n+m))$，不借用 decrease-key/Fibonacci heap 的 $O(m+n\log n)$ 界；在简单图上该对数项可等价写成 $O(\log n)$，但本实现允许重边。tour 的固定端点表使用 $O(2^g g^2)$ 空间、$O(2^g g^3)$ 时间；零权 cover 的组维度 DP 为 $O(3^g)$，另有当前逐查询零边扫描/并查集的图维度成本。因为当前 $g\le16$，纯组维度表可控，实际瓶颈通常是图维度 row、闭包和 Enhanced 的 facility 数 $r$；论文不能把后者从最坏界中省略。

输出敏感的最坏空间为 $O(2^g n+gn+2^g g^2+m+r^2+F)$；其中 facility DP 的 $O(2^g r)$ 已被 $r\le n$ 下的 $O(2^g n)$ 覆盖。Base 的组距离仍可能退化到 $O(gn)$，但通常只保存 cutoff 内精确值；Enhanced 为 directed-cut 明确支付 dense $O(gn)$，并在 facility 阶段临时支付 $O(r^2)$ 度量矩阵。

逐阶段的保守边界如下：

| 阶段 | 最坏时间 | 主要空间 | 说明 |
|---|---:|---:|---|
| 连通性与零权 cover | 保守 $O((n+m)\log(n+1)+F\log(F+1)+3^g)$ | $O(n+2^g+F)$ | 普通连通分量索引在加载期共享；当前零权 cover 按查询扫描边并使用仅路径压缩的并查集 |
| 组距离 | $O(g(m+n)\log(n+m))$ | Base 至多 $O(gn)$；Enhanced 为 $O(gn)$ | Base 可在 cutoff 处停止并选 dense/sparse 物理布局 |
| 组 tour | $O(2^g g^3)$ | $O(2^g g^2)$ | 只含组维度，不含图顶点维度 |
| Base A1 | $O(k(m+n)\log(n+m))$ | row 至多 $O(kn)$，查找缓存 $O(n)$ | 层 1 属于 $\mathcal L_A$ 时存在；缓存 ordinary 后释放 |
| ordinary $D$ | 保守 $O(3^g n+2^g(m+n)\log(n+m))$ | $O(2^g n)$ | 实际只到 size $h$ 且为稀疏 row |
| 完整前向 $A$ | 同阶保守上界 | $O(2^g n)$ | 实际只到 size $q=h-1$，末层可只消费 |
| directed-cut | $O(gm+g(m+n)\log(n+m))$ | $O(gn+m)$ | changed-arc 只改善实际扫描量；边容量更新仍逐组扫描边 |
| facility 上界 | $O(r(m+n)\log(n+m)+2^g r^2+3^g r)$ | $O(n+r^2+2^g r)$ | 仅 DirectedCut/Enhanced；$r$ 是 primal 涉及的不同顶点数 |
| adjoint 转置与 $H$ | 保守不超过高层前向指数阶 | $O(2^g n+gn)$ | 只物化 $\ell<\lvert S\rvert\le q$ 的稀疏 $H$ |

表中的 Base A1 条件不是参数调优分支：它只是询问层 1 是否属于前向递推的定义域 $\mathcal L_A$；属于时该阶段无条件存在，不属于时没有这张逻辑 row。A1 top-two 使用两个初始化为 255 的 byte bit 数组，因而固定触及约 $2(n+1)$ 字节；两个 32-bit locator 再压入一个 64-bit 数组，只有真正查询该顶点 future 时才写入相应 locator 页面。最坏虚拟容量约每顶点 10 字节，典型物理增量更准确地写成约 $2n$ 字节加已触及的 locator 页面，而不是声称全部 10 字节都按需驻留。两类缓存均在 ordinary 后释放；locator 既能无损定位精确 double，也能标记 cone 外公式。复杂度按 $O(n)$ 计。

### 14.2 以实际 payload 表示的实现成本

令 $Z_D,Z_A,Z_H$ 为阶段结束时实际保存的状态标量数，$R_D,R_A,R_H$ 为图闭包实际检查的邻接项数，$M_D,M_A,M_H$ 为同根交集/拆分候选数，则主搜索更贴近实际的成本为：

$$
O\left(M_D+M_A+M_H+(R_D+R_A+R_H)\log(n+m)\right),
$$

若 $Z_{A1}^{\mathrm{work}}$ 表示 Base 提前 A1 在构造 cone 中曾被接纳的不同状态，则其构造工作还包括相应 queue/邻接扫描；它可能大于 ordinary 结束后重滤所保留的 A1 payload。空间分别为 Base 的 $O(Z_D+Z_A+n)$、DirectedCutOnly 的 $O(Z_D+Z_A+gn)$、Enhanced 的 $O(Z_D+Z_A+Z_H+gn)$。转置 terminal 和 residual 是阶段临时量；residual 在 ordinary 前释放，高层 $H$ 仅保存高层区间中实际生成的稀疏 row，不物化完整 dense 状态网格。

实现为每条查询额外报告 `mask_vertex_states`。其口径是累计的“首次发现状态项数”，不是结束时 payload、队列弹出数或峰值空间：状态族属于实际键，对每张 $D$、$A$、$H$ 逻辑 row，顶点第一次从无穷变为有限候选时计一次，同一 row 内后续改进不重复；D/A/H 中数值相同的 `(mask,v)` 分别计数。Base 提前生成的 A1 在生成时计入，所有权转交给公共 $A$ 内核后不再计；DirectedCut/Enhanced 若正常生成 A1 也只计一次，若由 $H$ 替换则只统计实际 $H$ 状态。最后只消费而不保留的 $A$ 层仍计入，因为这些状态已经实际生成。组距离、tour、directed-cut 势、转置前的终端候选、完整解结算及队列过期项均排除。记该累计数为 $C_{\mathrm{ABHSS}}$，则它可用于解释搜索工作量，但通常 $C_{\mathrm{ABHSS}}\ge Z_D+Z_A(+Z_H)$，不能替代峰值内存指标。

PrunedDP++ 的对应值是主 `StateStore` 首次插入的不同 `(mask,v)` 数：Hash 后端直接读取实际容器大小，Dense 后端只统计 `present` 项而不是 $2^g(n+1)$ 预分配容量；状态 reopen 不重复，组距离和 route DP 同样排除，full-mask 完成候选只更新 incumbent 而不进入表。两边的统计都描述各自算法实际主状态域，不能把它解释成完全相同的单步成本；应与时间、边扫描/合并工作和 RSS 联合分析。

见证树一次 buy 若含 $t$ 个节点，最坏 $O(t3^k)$ 时间、$O(t2^k)$ 工作空间；rent-or-buy 控制调用次数。directed-cut changed-arc 构造最坏 $O(gm+g(m+n)\log(n+m))$，空间 $O(gn+m)$。若 primal 含 $r$ 个 facility，facility 上界的保守界包括 $O(r(m+n)\log(n+m))$ 的 $r$ 次二叉堆支撑图最短路和 $O(2^g r^2+3^g r)$ 的小图 DP；它是 Enhanced 的预处理固定成本，也是小图上可能不占优的原因之一。

## 15. 实现细节为何存在

| 实现细节 | 作用 | 若删除或写错的风险 |
|---|---|---|
| `ready` 与空 payload 分离 | 区分“已生成空 row”和“依赖尚未生成” | 高层会错误跳过合法依赖或读取未初始化状态 |
| 顶点递增 row | 允许双指针、较小侧驱动二分和稳定输出 | Hash 随机访问会放大常数并破坏确定性 |
| branch bitmap | 只发布规范的不可继续同根拆分状态 | 不影响值但会产生大量重复组合；定义错误则可能丢解 |
| `IsExact` | 阻止 bounded cutoff 冒充 DP 值 | 会构造不存在的低成本状态，直接破坏精确性 |
| epoch/stamp 缓存 | 避免每张 row 清零 $O(n)$ 下界数组 | 大图上清零成本可能超过实际稀疏搜索 |
| A1 top-two bit/locator 缓存 | 跨 ordinary row O(1) 复用最大两个 singleton future，保持原 double 值 | 只缓存 bit 会反复二分稀疏 A1；缓存两个 double 则会放大大图 RSS |
| 64 顶点转置块 | 把 mask-major 稀疏 row 分块转成 vertex-major terminal，与位图 word 对齐 | 全图按顶点物化 ordinary 列会产生 $O(n)$ 个容器并放大内存 |
| 严格父指针改进 | 零权等距时维持无环 witness | 可能形成 2-cycle，导致见证遍历错误或不终止 |
| 原始 `best <= lower` | 只有真正闭合才提前返回 | epsilon 闭合可能返回相差极小但非最优的值 |
| 加载期 component cache | 把共同图性质从逐查询计时移出 | 大图每条查询重复 $O(n+m)$，实验含义失真 |
| 固定配置与稳定并列规则 | 可复现且禁止 per-query oracle | 性能曲线无法解释，审稿人可质疑方法选择偏置 |

## 16. 论文表述边界与当前限制

- 可以声称：单线程、精确最优权值、无向非负边权、零权边安全、$g\le16$，以及从 Base 通过预声明开关得到的“安全新增或同职责替换”配置链。
- 不应声称：当前二进制已经输出最终最优边集合；它目前输出精确权值和 feasibility。
- 不应把 Dijkstra、subset DP、A* 下界、directed-cut、inside/outside、位图或 rent-or-buy 单独表述为原创。贡献应聚焦于它们在 exact GST 中的状态组织、证书复用和高层完成机制。
- “增强位集合单调”不表示 Enhanced 逐指令执行 Base 的所有操作。允许的差异只有安全新增和第 3.1 节表格中的同职责替换；特别地，A1 是共同逻辑层，Base 提前并移交标准 row，Enhanced 由低层 A 或高层 H 覆盖，不能表述成 Base 独有的 EarlyA 结构。
- 不得把 A1 调度写成任何 `g >= 常数` 的经验规则。唯一条件是第 3.2 节由平衡分解推出的逻辑层是否存在；代码与文档都必须从 $q$ 的状态域解释。
- Base 与 Enhanced 的两条实验曲线来自同一二进制的预声明配置，不能取逐查询最小值组成 `ABHSS-best`。
- `double` 输入上的“精确”是组合结构精确而非任意精度实数计算。上下界逻辑不用容差；跨实现报告仍需说明目标值核验容差。
