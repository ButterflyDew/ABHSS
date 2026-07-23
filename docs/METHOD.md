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
4. Base 配置用前向锚定状态 $A$ 完成搜索。
5. 在同一 Base 上开启 `DirectedCut`，增加更强的组势下界和 primal witness。
6. 再开启 `AdjointCompletion`，保留低层 $A$，用补集转置和递减的高层 $H$ 替代完整前向高层的重复求值。

这里的核心不是把两个实现包装成一个名字。`D` 的状态定义、稀疏 row、同根交集、图闭包、上界结算和统一 future 接口只有一份。增强开关只增加证书或替换同一完成式的求值顺序。

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

## 6. 公共预处理：下界、上界与锚组

### 6.1 零权分量覆盖下界

先把所有零权边缩成免费连通分量。每个分量携带它命中的组集合 $C_j\subseteq[g]$。在这些集合上执行精确 set cover DP，得到覆盖全部查询组至少需要的零权分量数 $q$。若 $q=1$，某个零权分量已覆盖全部组，故 $\operatorname{OPT}=0$。

若 $q>1$，令 $w_+$ 为最小正边权。任何连通可行树在零权分量缩点图上至少连接 $q$ 个分量，因此至少使用 $q-1$ 条正权边：

$$
L_{\mathrm{cc}}=(q-1)w_+\le\operatorname{OPT}.
$$

若图没有正权边但查询可行，则必有 $q=1$。set cover DP 用 $O(3^g)$ 时间、$O(2^g+n)$ 空间，并返回若干代表顶点供上界构造使用。

### 6.2 Base 的规范 SPT 上界与有界组距离

Base 先从每组顶点数、最小顶点号的稳定顺序选规范终端作为候选根。对每个候选根运行 Dijkstra；当遇到尚未覆盖的组时，恢复根到该顶点的 SPT 路径，按原图 `edge_id` 去重计价。所得边并集是覆盖所有组的真实连通子图，因此其最小代价 $U_0$ 是合法上界。

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

## 8. 真实 witness 与 rent-or-buy 上界

根路径并集或 directed-cut primal 会被整理为一棵以锚组终端为根的真实树。零权边上只允许严格距离改进来设置父指针；等距候选保留第一次父亲，避免 settled 祖先被重新挂到后代形成环。构造后还验证所有 witness 顶点可从锚根到达。

随着 ordinary row ready，可在该 witness 树上做独立 subset DP：树边只在子树确实携带非空 mask 时支付一次，节点处可以组合已经存在的 ordinary rooted 子树。任何有限结果都对应一棵原图可行树，所以只会收紧 $U$。一次求值的估计成本约为 witness 顶点数乘以 $3^k$ 合并量。普通 row 的 queue pop 和 edge relaxation 累计为 rent；只有 rent 达到一次 buy 估计才重新求值，避免每层重复扫描固定 witness。Base 在 ordinary 前先买一次，因为早期上界会直接缩小 bounded cone；DirectedCut 已在预处理购买 primal/facility 上界，后续使用相同调度器。

## 9. Base 的 early-A1 证书

对每个非锚 singleton $i$，Base 在 ordinary size 2 前生成：

$$
A(\{i\},v)=\min_u\{d_a(u)+d_i(u)+\operatorname{dist}(u,v)\}.
$$

只保留满足 $A(\{i\},v)<U_0$ 且 $A(\{i\},v)+C_i(v)<U$ 的精确 cone，其中 $C_i$ 是其余组的 farthest continuation。若某位置未保存而真实 $A$ 小于 $U_0-C_i(v)$，它本应通过同一闭包条件进入 cone，矛盾；因此 cone 外可安全返回：

$$
\underline A_i(v)=\max\{0,U_0-C_i(v)\}.
$$

ordinary 状态需要完成锚组和若干剩余 singleton 时，取相应 $\underline A_i(v)$ 的最大值作为额外 future。每个顶点懒缓存最大和次大组 bit；若最大组已被当前 mask 覆盖，可 $O(1)$ 退到次大值。early-A1 在 ordinary 结束后按所有权移动给公共前向内核，绝不重复闭包。

## 10. 前向锚定状态 $A$

锚定递推为：

$$
A(S,v)=\min_{u\in V}\left\{
\min_{\varnothing\neq T\subseteq S}
A(S\setminus T,u)+D(T,u)+\operatorname{dist}(u,v)
\right\},
$$

其中 $A(\varnothing,v)=d_a(v)$ 为隐式 row，不为全图单独物化。seed 合并、图闭包、future 剪枝和稀疏 row 写回都由 `forward.cpp` 的同一实现完成。每个 settled $A(S,v)$ 还会把未覆盖 singleton 直接接到 $v$ 形成 root-star 上界，并与至多两张 ordinary row 做完整结算。

令 $h=\lfloor g/2\rfloor$。Base 生成到 $|S|=h-1$；最后层只消费和结算，不保存 payload。其充分性来自平衡分解：去掉永久锚组后，任意完整规范分解都可选择一个包含锚的块，使其非锚组数不超过 $h-1$，余下组可分成至多两个大小不超过 $h$ 的 ordinary 块。所有这样的 left/right 子 mask 都在 `CompleteAnchoredRow` 中枚举。$g=2,3$ 时直接用隐式 $A(\varnothing)$ 完成。

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

全部组处理后，代码从根出发在零 residual 有向弧上逐次连接尚未覆盖的组；每次仍以原边权运行 Dijkstra，把真实路径边写入 bitmap，并按真实新增路径成本计价。若恢复成功，得到 primal 上界和 witness 树。

此外，取 primal 边涉及的顶点为 facilities。在 residual 可达弧与 primal 树边构成的支撑图上计算 facility 间真实最短路，并做小规模 subset DP，得到第二个可行上界。所有数值最终都由真实原图路径组成；residual 只限制候选支撑，不直接充当答案。完成这两项后立即释放 $2m$ residual，只保留 $gn$ 组势和 primal edge bitmap。

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

公共前向内核只物化 $|S|\le h_{\mathrm{low}}$ 的 $A$，并保留边界层。切分点只由 $g$ 决定，不看 row 密度或耗时。

### 12.2 按顶点补集转置

转置阶段以 64 个连续顶点为块，把该块内可用的 $D(B,v)$ 聚到顶点侧。一个或两个互不相交 ordinary 块的并集为 $U$ 时，产生目标：

$$
S=[k]\setminus U,
\qquad
H(S,v)\leftarrow \sum_j D(B_j,v).
$$

这里 $H(S,v)$ 已支付 $S$ 外的组，等待锚定前缀覆盖 $S$。只保留 $h_{\mathrm{low}}<|S|\le h_{\max}$ 的目标。每个顶点先计算 subset 势；ordinary 值减去其组势得到 reduced value，用全局预算 $U-\sum_i\pi_i(v)$ 排除不可能改善 incumbent 的组合。代码比较 pair enumeration 与 submask enumeration 的确定性工作量，选择较小者；两条路径产生完全相同的候选集合。

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

取完整前向 $A$ 的任一规范推导，沿依赖从高层向低层找到第一次跨过 $h_{\mathrm{low}}$ 的边。跨界外侧由一个或两个 ordinary 块开始，故在转置阶段产生相应 $H$ 终端；此后完整前向中继续加入 ordinary branch 的每一步，在递减 $H$ 中反向出现；最后剩余的低层锚定依赖由边界结算读取。反之，每个转置终端、$H+D$ 递推和边界结算都能反向拼成一条合法完整前向推导。因此 adjoint 只改变高层依赖的求值方向，不增加或删除组合解。

## 13. 完整正确性论证

下面给出当前实现必须同时满足的证明链。

**引理 1（上界真实性）。** `best` 只由规范 SPT 边并集、共同根真实路径并集、directed-cut primal、facility 支撑路径、witness-tree DP、root-star 或完整状态结算更新。每一项都能展开为原图上的连通覆盖，因此 `best` 始终满足 $best\ge\operatorname{OPT}$。

**引理 2（future 可采纳）。** $L_{\mathrm{cc}}$、$L_{\mathrm{far}}$、$L_{\mathrm{tour}}$、early-A1 cone 外证书和 $L_{\mathrm{cut}}$ 分别由零权缩点连接数、单组必要距离、树倍增度量松弛、闭包反证和 directed-cut 对偶可行性得到，均不超过相应剩余代价；它们的最大值仍可采纳。

**引理 3（有界距离安全）。** Base 中未保存的 $d_i(v)$ 至少为构造时 cutoff。它只能作为拒绝证书；任何需要精确距离的 split、完成式或 witness 都检查 `IsExact`。因此 cutoff 不会被当作一棵虚构的低成本子树。

**引理 4（普通格完备）。** 完整 rooted subset DP 的任意同根拆分可通过 pivot 唯一选择枚举。未发布的非 branch 状态可等价递归展开为 singleton/branch，故规范 branch 不删除最后一种分解。图闭包对所有满足严格改进条件的 seed 执行 Dijkstra，得到稀疏锥体内的精确 $D$。

**引理 5（剪枝安全）。** 若某部分状态值 $x$ 与可采纳 future $L$ 满足 $x+L\ge best$，任何完成解代价都不小于已有可行上界；删除它不改变最优权值。所有上下界闭合使用原始 `double` 顺序 `best <= lower`，不使用 epsilon 把正 gap 当作 0。

**引理 6（Base 完成式完备）。** 平衡分解保证任意完整规范树可表示为一个大小至多 $h-1$ 的锚定块和至多两个大小至多 $h$ 的 ordinary 块。完整前向 $A$ 和 `CompleteAnchoredRow` 枚举所有这类分解。

**引理 7（Adjoint 等价）。** 第 12.4 节给出完整高层 $A$ 推导与“低层 $A$+转置终端+递减 $H$+边界结算”的双向映射；所有转置/prefix 剪枝只使用引理 2 的下界。因此 Enhanced 与完整前向格返回同一最优权值。

**定理（ABHSS 精确性）。** 对任意合法配置、无向非负边权输入及 $g\le16$ 的查询：若共同分量不存在，算法正确返回 infeasible；否则算法返回 $\operatorname{OPT}(G,\mathcal K)$。证明：引理 1 保证返回值不低于最优值；引理 2、3、5 保证剪枝不删除任何严格优于 incumbent 的完成；引理 4 与引理 6（Base/DirectedCutOnly）或引理 7（Enhanced）保证仍有一条最优推导被枚举。当队列与完成格耗尽时，不存在低于 `best` 的未枚举解，故 $\texttt{best}\le\operatorname{OPT}$，结合上界真实性得等号。

工程证据不是数学证明的替代，但用于防止实现偏离上述引理：历史零权父指针反例；包含零权、重叠组和多终端的确定性随机图，三种合法配置逐例对照独立全子集 DP；高 $g$ SteinLib 已知最优值；非法配置、不可行图、平凡查询、输入格式和小于 $10^{-9}$ 正 gap 的闭合回归。

## 14. 复杂度分析

### 14.1 最坏界

令 $k=g-1$，$h=\lfloor g/2\rfloor$。忽略只改善常数的稀疏性，ABHSS 仍属于经典 subset DP 的指数族。一个保守总时间上界可写为：

$$
O\!\left(
g(m+n\log n)+3^g+2^g g^3+
3^g n+2^g(m+n\log n)
\right),
$$

即通常简写为 $O(3^g n+2^g(m+n\log n))$，另加组距离与 tour 预处理。tour 的固定端点表使用 $O(2^g g^2)$ 空间、$O(2^g g^3)$ 时间；零权 cover 为 $O(3^g)$。因为当前 $g\le16$，这些纯组维度表可控，真正瓶颈是图维度 row 和闭包。

最坏空间为 $O(2^g n+gn+2^g g^2+m)$。Base 的组距离仍可能退化到 $O(gn)$，但通常只保存 cutoff 内精确值；Enhanced 为 directed-cut 明确支付 dense $O(gn)$。

### 14.2 以实际 payload 表示的实现成本

令 $Z_D,Z_A,Z_H$ 为实际保存的状态标量数，$R_D,R_A,R_H$ 为图闭包实际检查的邻接项数，$M_D,M_A,M_H$ 为同根交集/拆分候选数，则主搜索更贴近实际的成本为：

$$
O\left(M_D+M_A+M_H+(R_D+R_A+R_H)\log n\right),
$$

空间分别为 Base 的 $O(Z_D+Z_A)$、DirectedCutOnly 的 $O(Z_D+Z_A+gn)$、Enhanced 的 $O(Z_D+Z_A+Z_H+gn)$。转置终端和 residual 是阶段临时量；residual 在 ordinary 前释放，高层 $H$ 仅保存高层区间中实际生成的稀疏 row，不物化完整的 dense 状态网格。

见证树一次 buy 若含 $t$ 个节点，最坏 $O(t3^k)$ 时间、$O(t2^k)$ 工作空间；rent-or-buy 控制调用次数。directed-cut changed-arc 构造最坏 $O(gm+g(n\log n+m))$，空间 $O(gn+m)$。若 primal 含 $q$ 个 facility，facility 上界的保守界包括 $q$ 次支撑图最短路和 $O(2^g q^2+3^g q)$ 的小图 DP；它是 Enhanced 的预处理固定成本，也是小图上可能不占优的原因之一。

## 15. 实现细节为何存在

| 实现细节 | 作用 | 若删除或写错的风险 |
|---|---|---|
| `ready` 与空 payload 分离 | 区分“已生成空 row”和“依赖尚未生成” | 高层会错误跳过合法依赖或读取未初始化状态 |
| 顶点递增 row | 允许双指针、较小侧驱动二分和稳定输出 | Hash 随机访问会放大常数并破坏确定性 |
| branch bitmap | 只发布规范的不可继续同根拆分状态 | 不影响值但会产生大量重复组合；定义错误则可能丢解 |
| `IsExact` | 阻止 bounded cutoff 冒充 DP 值 | 会构造不存在的低成本状态，直接破坏精确性 |
| epoch/stamp 缓存 | 避免每张 row 清零 $O(n)$ 下界数组 | 大图上清零成本可能超过实际稀疏搜索 |
| 64 顶点转置块 | 与 branch/membership 位图 word 对齐并限制临时内存 | 全图按顶点物化 ordinary 列会爆内存 |
| 严格父指针改进 | 零权等距时维持无环 witness | 可能形成 2-cycle，导致见证遍历错误或不终止 |
| 原始 `best <= lower` | 只有真正闭合才提前返回 | epsilon 闭合可能返回相差极小但非最优的值 |
| 加载期 component cache | 把共同图性质从逐查询计时移出 | 大图每条查询重复 $O(n+m)$，实验含义失真 |
| 固定配置与稳定并列规则 | 可复现且禁止 per-query oracle | 性能曲线无法解释，审稿人可质疑方法选择偏置 |

## 16. 论文表述边界与当前限制

- 可以声称：单线程、精确最优权值、无向非负边权、零权边安全、$g\le16$、Base 上依次增加两个固定增强。
- 不应声称：当前二进制已经输出最终最优边集合；它目前输出精确权值和 feasibility。
- 不应把 Dijkstra、subset DP、A* 下界、directed-cut、inside/outside、位图或 rent-or-buy 单独表述为原创。贡献应聚焦于它们在 exact GST 中的状态组织、证书复用和高层完成机制。
- “增强集合单调”不表示 Enhanced 逐指令执行 Base 的所有操作。DirectedCut 用完整势和 primal 替换 Base 的 bounded/early 固定成本；Adjoint 用等价的反向高层替换完整前向高层。
- Base 与 Enhanced 的两条实验曲线来自同一二进制的预声明配置，不能取逐查询最小值组成 `ABHSS-best`。
- `double` 输入上的“精确”是组合结构精确而非任意精度实数计算。上下界逻辑不用容差；跨实现报告仍需说明目标值核验容差。
