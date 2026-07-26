# ABHSS：单线程精确 Group Steiner Tree 方法详述

本文档是当前 review 分支对应的论文方法章节中文底稿。它只描述代码已经实现的 Base 与 Enhanced 两种最终模式，不包含 baseline、中间消融态、实验框架或历史设计。代码入口和逐函数定位见 [`CODE_GUIDE.md`](CODE_GUIDE.md)。

## 1. 问题、输出与适用域

给定无向图 $G=(V,E,w)$，其中 $n=\lvert V\rvert$、 $m=\lvert E\rvert$，且每条边满足 $w(e)\ge 0$。查询含 $g$ 个非空顶点组 $\mathcal K=\{K_0,\ldots,K_{g-1}\}$；组之间允许重叠，每组也可含多个候选顶点。可行解是同时与每个组相交的连通子图。由于边权非负，删除环不会增加代价，因此至少存在一棵最优树。

```math
\mathrm{OPT}(G,\mathcal K)
=\min_{T\subseteq G}\sum_{e\in E(T)}w(e)
\quad
\mathrm{s.t.}\quad
T\ \mathrm{connected},\qquad V(T)\cap K_i\ne\varnothing\ \text{for every }i.
```

程序返回精确最优权值、可行性标志和实际主状态数，不保存最终最优树的决策链。内部构造的树只用于证明和收紧可行上界。这里的“精确”指对解析后的 `double` 边权执行组合精确搜索；上下界闭合使用原始顺序比较，不用 epsilon 把很小的正 gap 当作 0。数值容差只用于从浮点最短距离等式中恢复一条真实原图路径，恢复后的候选仍按原边权计价。

当前方法支持零权边、重边、自环、重叠组和非连通图，限制 $g\le16$。空查询与单组查询答案为 0；若不存在一个同时命中全部组的连通分量，则查询不可行。图与查询文件由 review 入口按既定格式直接信任，不把输入损坏检测写进算法主线。

## 2. 方法主线与三个不变量

经典 rooted subset DP 为组子集和根维护精确值，主要成本来自指数状态、同根拆分和每张 row 的图闭包。ABHSS 保留该精确语义，但只物化仍可能严格改善当前真实上界的稀疏状态：

1. 构造可展开到原图边的可行解，维护全局 incumbent `best`。
2. 构造最远组、组间 tour、A1 cone；Enhanced 再加入有向割对偶势。
3. 固定一个永久锚组，只对其余 $k=g-1$ 个组编码 mask。
4. 按子集大小构造普通状态 D，并发布规范 branch。
5. 提前构造 Base 与 Enhanced 完全共用的第一层锚定状态 A1，让 ordinary D 复用为 future。
6. Base 用前向 A 完成全部锚定高层；Enhanced 保留同一低层 A 前缀，并用补集转置的反向 H 完成高层。

所有优化都围绕三个不变量：

- `best` 只由真实原图边或精确 DP 状态组成的可行解更新，所以始终不小于最优值。
- 任意 future 都不大于对应剩余完成代价，所以删除 `paid + future >= best` 的状态不会删除严格更优解。
- 未被剪枝的规范完整推导，要么由 Base 的完整前向格枚举，要么与 Enhanced 的“低层 A + 高层 H”推导一一对应。

## 3. Base 与 Enhanced 是同一算法的两个最终模式

公开入口只有一个布尔位：

```cpp
SolveResult SolveOneQuery(const Graph& graph, const Query& query, bool enhanced);
```

该位在查询开始前固定，运行中不观察数据集名、 $g$、平均组大小、row 密度、时间或内存。两种模式共用 `Problem`、稀疏 `Row`、A1、ordinary D、witness 调度器、树 DP 和前向 A 内核。差异只允许两类：

1. **安全新增。** Enhanced 增加可采纳 dual 下界以及真实 primal/facility 上界。
2. **同职责替换。** 两侧输入和输出语义相同，只采用不同物理表示或求值方向。

| 逻辑职责 | Base | Enhanced | 关系 |
|---|---|---|---|
| 距离—候选根初始化 | 真实 cutoff 加有界 `GroupRow` | 完整距离势 `GroupRow` | 同一返回合同的表示替换 |
| ordinary 的基础 future | farthest、A1、tour | 相同三项，再与 dual 取最大值 | 安全新增证书 |
| 上界 witness | root-path witness | dual-primal witness | 同一真实 witness 输入职责的替换 |
| 额外上界 | 共同 root-star、root-path union、条件式树 DP | 相同共同上界，再加 primal/facility | 安全新增证书 |
| 高层锚定完成 | 完整前向 A | 低层 A 加高层 H | 同一完整推导职责的方向替换 |

A1 不属于替换项。只要第一层存在，两种模式逐项执行同一个 seed、farthest cone、正 fallback、图闭包、top-two 视图、状态计数和所有权移交；A1 构造函数不读取模式位或 dual。

## 4. 状态记号与统一物理结构

### 4.1 锚组与两套 mask

预处理选择锚组 $K_a$。其余 $k=g-1$ 个组重新编号为 bit $0,\ldots,k-1$。压缩 mask 用于 D、A、H；原始 $g$ 位 mask 用于需要连同锚组读取 tour 或 dual 的位置。

- `bit_to_group[b]` 把压缩 bit 还原为原查询组号。
- `original_mask[S]` 把压缩 mask 还原为原始组 mask。
- `full_mask=(1<<k)-1` 是非锚组全集。
- `original_full_mask=(1<<g)-1` 是全部查询组全集。
- `anchor_bit=1<<anchor_group` 单独表示永久锚组。

永久锚组使 $A(\varnothing,v)$ 可直接取锚组距离，不必物化一张全图 singleton row，同时把普通状态空间从 $2^g$ 降到 $2^{g-1}$。

### 4.2 三个逻辑状态族

对非锚组 mask $S$ 和顶点 $v$：

- $D(S,v)$：覆盖 $S$ 中全部组并以 $v$ 为根的最小树代价； $D(\varnothing,v)=0$。
- $A(S,v)$：覆盖锚组和 $S$ 中全部组并以 $v$ 为根的最小树代价。
- $H(S,v)$：Enhanced 高层的外侧已付代价；它覆盖非锚全集减去 $S$，等待与覆盖锚组及 $S$ 的低层前缀相接。

D、A、H 共用同一个 `Row`：递增的 `vertex`，与其对齐的精确 `value`，ordinary D 专用的 `branch_bits` 和显式 `ready`。`ready=false` 表示依赖未生成；`ready=true` 且 payload 为空表示该 row 已经处理完，但当前严格上界锥体为空。

### 4.3 `GroupRow` 与精确 membership

令单组距离为：

```math
d_i(v)=\min_{t\in K_i}\mathrm{dist}(v,t).
```

`GroupRow` 不是 DP row，而是该函数的物理存储。Enhanced 保存完整 dense 距离。Base 只需保留安全 cutoff $U_0$ 内满足 $d_i(v)<U_0$ 的精确值；其余位置读取为 $U_0$，但 `IsExact(v)=false`。

因此有界位置具有两种语义：

```math
\texttt{row}[v]=d_i(v)\quad\text{and}\quad \texttt{IsExact}(v)=\mathrm{true},
```

或：

```math
\texttt{row}[v]=U_0\le d_i(v)\quad\text{and}\quad \texttt{IsExact}(v)=\mathrm{false}.
```

第二种值可作下界证书，不能作为一棵真实 singleton 子树。所有 seed、同根合并、完成式和 witness 读取都必须通过 `IsExact` 或 `ForEachExact`。Base 在 dense cutoff 数组和 ranked membership bitmap 之间按实际字节数选择，二者不改变逻辑接口。

## 5. 公共预处理

### 5.1 查询可行性与零权 cover

读图时一次建立无向连通分量编号。一条查询只有在某个分量同时与每个组相交时才可行；非连通图上把每组命中的分量集合排序去重后逐组求交即可。

随后仅沿零权边做并查集压缩。每个零权分量携带其覆盖的组 mask。对这些 mask 做精确 set-cover DP，令 $c_0$ 为覆盖全部查询组至少需要的零权分量数。若 $c_0=1$，存在零代价可行连通分量，答案立即为 0。若 $c_0>1$，令 $w_+$ 为最小正边权，则任何连通解至少连接 $c_0$ 个免费分量：

```math
L_{\mathrm{cc}}=(c_0-1)w_+\le\mathrm{OPT}.
```

cover DP 还返回代表顶点，供后续真实路径并集尝试不同候选根。

### 5.2 统一的距离—根合同

两种模式都只调用一次 `BuildDistanceRootInitialization`，并接收相同结构：

```math
\mathcal I_{\mathrm{dist}}=(\mathcal D,r,U_0).
```

$\mathcal D$ 是 `GroupRow` 表， $r$ 是候选根， $U_0$ 是由原图真实路径支持的可行上界。差异封装在函数内部：

- Base 先按组大小和稳定顶点顺序尝试候选根。从根做 Dijkstra，首次遇到新组时恢复 SPT 路径，并按原 `edge_id` 对边并集去重计价。最好的可行并集给出 cutoff，随后每组多源 Dijkstra 只扩展严格小于该 cutoff 的标签。
- Enhanced 对每组执行完整多源 Dijkstra，并保留全图距离，为后续有向割势提供完整 potential。

若 Base 的启动候选暂时没有共同可行分量，cutoff 保持无穷，此次多源搜索自然不截断；共同 root 扫描仍会在先前已证明存在的可行分量中找到有限上界。两种模式最后都运行同一个 `RootStarUpper`：

```math
U_{\mathrm{star}}(r)=\sum_{i=0}^{g-1}d_i(r).
```

对每组各取一条从 $r$ 到最近终端的路径，它们的并是可行连通子图，真实并权不超过上式，故该值是安全上界。扫描由精确顶点最少的 `GroupRow` 驱动，其他组通过统一接口读取。Base 把当前最优值初始化为 cutoff；任何非精确位置都返回 cutoff，含这类位置的非负距离和不可能严格小于当前值，因此只有所有组距离均精确的根才能更新上界。

### 5.3 真实路径并集与锚组

合同返回后，两种模式共同调用 `BuildRootPathUnion`。它沿满足最短路等式的真实原边恢复根到各组的路径，按 `edge_id` 去重并以原边权计价，得到通常更紧的 $U_{\mathrm{union}}$。等式匹配允许 $10^{-9}$ 数值容差；即使它选择了另一条近似等式边，最终候选仍由真实原图边组成，只可能使上界变弱，不会制造虚假下界。

在当前候选根处，选择组距离最大的组作为永久锚组。直观上，最远组固定进入所有 A 状态，可更早暴露长连接。该选择只重排状态，不改变可行解集合；并列时按原组顺序稳定选择。

### 5.4 组间 tour 下界

定义组间松弛距离：

```math
\delta(i,j)=\min_{u\in K_i,\,v\in K_j}\mathrm{dist}(u,v).
```

代码在 $K_j$ 上取 $d_i$ 的最小值。对每个组子集和固定的两个端点组，subset DP 预计算最短 Hamilton path。查询顶点 $v$ 与剩余组 mask $R$ 时，把 $v$ 接到该路径两端，对端点选择取最优，再对强制端点取最大，最终除以 2，得到 $L_{\mathrm{tour}}(v,R)$。

可采纳性来自树倍增。任意从 $v$ 覆盖 $R$ 的树，边倍增后给出长度至多两倍树权的闭合遍历；在组间松弛度量中 shortcut 不增加长度。因此每个固定端点路径除以 2 不超过剩余树代价，若干此类下界的最大值仍安全。

### 5.5 farthest 与统一 future

最便宜的剩余代价证书是：

```math
L_{\mathrm{far}}(v,R)=\max_{i\in R}d_i(v).
```

每个顶点缓存全体组中的最远组；若该组仍在 $R$ 中即可 O(1) 返回，否则扫描 mask。Base 的公共 future 为：

```math
L_{\mathrm{base}}(v,R)=\max\{L_{\mathrm{far}}(v,R),L_{\mathrm{tour}}(v,R)\}.
```

Enhanced 再与第 10 节的 $L_{\mathrm{cut}}$ 取最大。热循环按照“dual、farthest、A1、tour”或其适用子序列逐级检查；一旦某个便宜下界已经证明不能改善 `best`，就不计算后面的昂贵证书。

## 6. 普通状态 D 与规范 branch

### 6.1 精确递推

singleton 直接读取组距离。对 $\lvert S\rvert\ge2$，先在同一根合并两个真子集，再做图闭包：

```math
B(S,v)=\min_{\varnothing\ne T\subsetneq S}\bigl(D(T,v)+D(S\setminus T,v)\bigr),
```

```math
D(S,v)=\min_{u\in V}\bigl(B(S,u)+\mathrm{dist}(u,v)\bigr).
```

实现按 $\lvert S\rvert$ 递增，普通格只生成到：

```math
h=\left\lfloor\frac g2\right\rfloor.
```

size 2 直接相交两个 singleton 的精确 support。更高层固定 mask 的最低 bit 在 accumulator 一侧，另一侧只枚举不含该 pivot 的子 mask，从而去掉左右对称。

### 6.2 稀疏 A* 式图闭包

一个同根 seed 已付代价为 $x$，尚未覆盖原始组集合为 $R$，队列 key 为：

```math
f=x+L_{\mathrm{future}}(v,R).
```

只有 $f<\texttt{best}$ 的候选进入和继续传播。因为 future 不超过任意完成代价， $f\ge\texttt{best}$ 的候选不可能产生严格更优解；等于上界的候选也可删除，因为已经存在同成本可行解。

每张 row 复用长度 $n+1$ 的 `distance`、`split`、bound cache 和 epoch 数组。`touched` 记录本 row 首次由无穷变为有限的顶点，便于只清理真正写过的位置；`settled` 记录以当前最优距离弹出的顶点，排序去重后形成递增 payload。

### 6.3 branch 定义与完备性

`split[v]` 保存闭包前的 $B(S,v)$，`distance[v]` 保存闭包后的 $D(S,v)$。若：

```math
D(S,v)<B(S,v),
```

则该状态必须从其他根经过至少一条图边到达，代码把它标为规范 branch。若二者相等，状态在当前根仍存在等价的同根真子集拆分，不发布 branch。

这不会丢解。任意更高层若使用一个未标 branch 的状态，可以把它替换为实现等值 split 的两个真子状态；反复替换后必然到达 singleton 或一个严格经图边闭包的 branch。后续只要求被接入的一侧为 branch，另一 accumulator 仍可取任意 ready 值，所以每个等价分解类至少保留一个代表。

### 6.4 ordinary 完成式

当 ordinary 达到半格时，代码用：

```math
D(S,v)+D(\overline S,v)+d_a(v)
```

产生完整可行上界。到达三块平衡点时，还枚举三个 ordinary 块与锚组在同一根相遇。完成式只收紧 `best`，不写入新的精确 row，也不替代尚未生成的依赖。

## 7. 两种 witness 与共同 rent-or-buy

### 7.1 witness 的来源

Base 把共同 root-path union 重根为一棵真实 `WitnessTree`。Enhanced 从有向割 primal 边 bitmap 构造同格式树。零权边上父指针只接受严格距离改善，等距候选保留第一次父亲，避免已 settled 的祖先被重新挂到后代而形成父指针环。

两种模式的预处理都只构造 witness，不无条件执行树 DP。预处理返回后才建立同一个 `WitnessUpperScheduler`，所以两边的 rent 都严格从 0 开始。

### 7.2 唯一树 DP

`EvaluateWitnessTree` 给 witness 根增加虚拟超根。在每个真实树顶点，把当前可用的 singleton 和 ordinary rooted 块合并为局部 subset 值，再依次卷积孩子子树。把非空 mask 交给孩子时才支付真实父边权。

任何有限的 full-mask 结果都能展开为 witness 边与若干真实 rooted 子树的连通并，因此只可能形成可行上界。Base 与 Enhanced 调用完全相同的函数；树来源不会改变 DP 语义。

### 7.3 buy 公式

令 $k=g-1$，当前 witness 含 $t$ 个真实顶点。固定最低 bit 去掉左右拆分对称后，一个顶点的全部非空局部拆分数为 $(3^k-1)/2$；一条父子关系的全部 mask/submask 卷积数为 $3^k$。每个真实顶点有一次局部处理和一条通向父亲的关系，因此：

```math
B_{\mathrm{wit}}(t,k)
=t\left(\frac{3^k-1}{2}+3^k\right).
```

两种模式只把各自 witness 的 $t$ 代入同一整数公式。它不读取图名、墙钟、状态密度或模式专属常数。

### 7.4 rent 的连续累计

A1 与 ordinary D 把实际 queue pop 和检查过的邻接项作为 rent 工作。累计值达到 buy，且 relative to 上次购买已有新的 ordinary row 时，调度器调用树 DP、令 `best` 取更小可行值，并清零 rent。第一次购买可以只使用 singleton；以后只有 ordinary 修订变化才值得重复购买。

A1 结束时未消费的 rent 直接交给 D，D 不创建新调度器。这样不存在 Base 预买一次而 Enhanced 不买、或两边从不同 rent 起点开始的不对称路径。

## 8. Base 与 Enhanced 完全共用的提前 A1

### 8.1 逻辑值

对一个非锚 singleton $i$：

```math
A(\{i\},v)=\min_{u\in V}\bigl(d_a(u)+d_i(u)+\mathrm{dist}(u,v)\bigr).
```

$d_a(u)+d_i(u)$ 是锚组与组 $i$ 在根 $u$ 合并的 seed，多源 Dijkstra 把根移动到 $v$。因此 A1 是标准前向 A 的第一层，不是启发式辅助表。

完整锚定格的最高层为：

```math
q=\max\left\{0,\left\lfloor\frac g2\right\rfloor-1\right\}.
```

只有 $q=0$ 时不存在任何正层；否则 A1 必然存在，两种模式都在 ordinary D 之前生成它。这里没有 `g >= 常数` 的经验规则。

### 8.2 共同 cone 与正 fallback

设本轮 A1 开始时的真实上界为 $U_0$。对组 $i$，令 $R_i$ 为 A1 尚未覆盖的非锚组，定义：

```math
C_i(v)=\max_{j\in R_i}d_j(v).
```

两种模式只保留满足以下严格条件的精确位置：

```math
A(\{i\},v)<U_0,
\qquad
A(\{i\},v)+C_i(v)<U_0.
```

A1 值与每个单组距离都是图度量上的 1-Lipschitz 函数，有限个单组距离的最大值仍具有一致性。若目标顶点满足上述 cone 条件，其任意规范最短 A1 路径前缀也满足，因此 Dijkstra 不会在到达目标前错误截断。

若顶点不在 row 中，则它至少因 $A\ge U_0$ 或 $A+C_i\ge U_0$ 被拒绝，于是总有：

```math
A(\{i\},v)\ge\max\{0,U_0-C_i(v)\}.
```

row 内返回精确 A1；row 外返回右侧正下界。ordinary 状态查询剩余 singleton future 时，对相应 A1 下界取最大仍可采纳。

### 8.3 为什么 A1 内不接 dual

dual 也是安全 continuation，但与 farthest cone 的结构不同。若只让 Enhanced 在 A1 中额外用 dual 拒绝标签，缺失位置便有两种原因，Base 与 Enhanced 不能继续共享同一个 $U_0-C_i(v)$ fallback。让 Base 也构造完整 dual 又会把 Enhanced 的主要额外成本强制加入 Base。

因此当前方案把 dual 放在 ordinary 与 H 的独立 future 中，A1 只读取两边本来就拥有的 farthest continuation。`BuildReusableAnchoredSingletonLayer` 不接收模式参数，代码层无法在 A1 内分叉。

### 8.4 条件式树 DP 收紧后的整轮重启

一轮 A1 的 cone 与 fallback 必须共享同一 $U_0$。调度器可能在某张 A1 row 的安全 queue-pop 点购买树 DP。若 `best` 没有严格下降，可以继续；若下降到 $U_1<U_0$，此前部分 row 的 fallback 仍绑定旧上界，不能与后续新 cone 混用。

代码因此丢弃本轮尚未发布的全部 A1 row，清空临时距离，并以 $U_1$ 从第一个 singleton 整轮重建。调度器记住同一 ordinary 修订已经购买过，重启轮不会再次购买。最终发布的一组 A1 row 总是由单一固定 cutoff 证明。

### 8.5 top-two 缓存与所有权

ordinary 会反复询问“剩余 singleton 中最大的 A1 future”。每个顶点第一次被查询时，扫描全部 A1 singleton，保存最大和次大 bit；精确值的位置保存为 32-bit payload locator，cone 外值用 locator 高位表示 fallback。两个 locator 压成一个按需触页的 64-bit 项。

若最大 bit 仍在剩余 mask，答案直接命中；否则尝试次大；只有两者都被覆盖才扫描剩余 bit。ordinary 结束后释放 bit/locator 缓存，但保留标准 A1 row，并通过 `std::move` 把同一对象交给前向内核。前向阶段不会重做 A1 图闭包，也不会重复累计状态数。

## 9. 前向锚定状态 A

### 9.1 递推

隐式基例为 $A(\varnothing,v)=d_a(v)$。对非空 $S$：

```math
A(S,v)=\min_{u\in V}\min_{\varnothing\ne T\subseteq S}
\bigl(A(S\setminus T,u)+D(T,u)+\mathrm{dist}(u,v)\bigr).
```

`ForEachAnchoredSum` 只在共同根合并 A 与 ordinary branch。Base 的 singleton 来自 bounded `GroupRow`，必须检查精确 membership；Enhanced 的完整表天然精确。多组 ordinary 一律只读规范 branch。全部有限 seed 再通过和 D 相同的多源 Dijkstra 做图闭包，并以可采纳 future 严格剪枝。

### 9.2 完整解结算

一张 A row 生成后有两类上界。首先把每个剩余 singleton 从同根接入：

```math
U_{\mathrm{root}}=A(S,v)+\sum_{i\in[k]\setminus S}d_i(v).
```

其次把剩余 mask 分成至多两个 ordinary 块 $L$ 与 $R$：

```math
U_{\mathrm{split}}=A(S,v)+D(L,v)+D(R,v).
```

`CompleteAnchoredRow` 先用每张 row 的最小值做廉价拒绝，再从候选最少的一侧驱动同根交集。两类值都表示真实 rooted 子树与真实最短路径的并，因此只收紧上界，不写新的 full-mask DP row。

### 9.3 为什么最高层是 q

令 $h=\lfloor g/2\rfloor$。任意完整规范推导都能选择一个含锚块，使其非锚组数不超过 $h-1$，余下组分成至多两个大小不超过 $h$ 的 ordinary 块。ordinary 已生成到 $h$，所以前向 A 只需生成到：

```math
q=\max\{0,h-1\}.
```

当 $q=0$ 时，直接用隐式 $A(\varnothing)$ 与 ordinary 完成式结算。当 $q>0$ 时，A1 是第一层。Base 的前向计划做到 $q$，最后层只需要结算，可以不长期保留 payload。

## 10. Enhanced 的有向割证书

### 10.1 dual 势

把每条无向边看成两个容量均为原边权的有向弧。查询组按根到组的距离递减处理。每轮在当前 residual 容量下构造到该组的截断距离势 $\pi_i(v)$，并从相应方向的 residual 扣除势差；容量始终截到非负。

后一组只需从此前真正改变过的弧检查 Bellman 违反，再从违反点继续传播。changed-arc 位图减少重复扫描，但不改变最终 residual 最短路条件。

对剩余组集合 $R$：

```math
L_{\mathrm{cut}}(v,R)=\sum_{i\in R}\pi_i(v).
```

顺序 residual 扣减保证全部组在任一有向弧上的累计收费不超过原容量。任何从 $v$ 连接 $R$ 中全部组的树都必须跨越相应割，因此该和不超过剩余连接代价。Enhanced 可与 farthest、A1、tour 取最大；Base 不构造或读取该对象。

### 10.2 primal 与 facility 上界

dual 构造后，代码从根出发，仅在数值零 residual 的有向弧支撑上寻找尚未覆盖的组，但路径仍按原边权运行 Dijkstra，并把真实边写入 bitmap。零 residual 判断使用 $10^{-10}\max\{1,w(e)\}$ 的固定容差；容差只扩大候选支撑，最终权重仍由真实边给出，因此不会产生虚假上界。

primal 边涉及的顶点作为 facilities。代码在“数值零 residual 弧 + primal 树边”的支撑图上计算 facility 间真实最短路，再做小规模 subset DP，得到额外可行上界。完成 facility DP 和 dual witness 后立即释放 `2m` residual，只保留组势和 primal 边 bitmap。

## 11. Enhanced 的高层 H

### 11.1 固定层边界

Enhanced 仍用共同前向内核生成低层 A。令完整最高层为 $q$。若 $q=0$，令 $\ell=0$；否则：

```math
\ell=\max\left\{1,\left\lfloor\frac q2\right\rfloor\right\}.
```

前向 A 覆盖层 $1,\ldots,\ell$，H 覆盖层 $\ell+1,\ldots,q$。外层的 1 保证只要正层存在，共同 A1 永远留在前向前缀。该边界是对递推域的固定 meet-in-the-middle 切分，不观察运行表现。

### 11.2 64 顶点块转置

ordinary row 以 mask 为主序保存，而高层终端需要在同一顶点查看许多 mask。给每个图顶点永久建立一个容器会产生 $O(n)$ 个小 vector。实现每次只处理 64 个连续顶点，以 `array<vector<TerminalEntry>,64>` 暂存当前块内的 D 值；每张递增 ordinary row 保持单调 cursor，处理完块后立即复用容器。

64 只是与一个 `uint64_t` membership word 对齐的串行块宽，不代表线程、SIMD lane 或 GPU warp。改变该常数只影响缓存行为，不改变终端集合。

### 11.3 补集终端

一个或两个互不相交 ordinary 块的并为 $Q$ 时，产生目标：

```math
S=[k]\setminus Q,
\qquad
H(S,v)\leftarrow\sum_j D(B_j,v).
```

H 已支付 $S$ 外的组，等待低层锚定前缀覆盖 $S$。只保存 $\ell<\lvert S\rvert\le q$ 的目标。每个顶点先计算 subset potential；ordinary 值减去对应势得到 reduced value。若若干 reduced value 加全组 potential 已不可能低于 `best`，该组合可安全跳过。

候选写入前，再对将来需要覆盖的锚组与 target 计算 farthest、tour、dual prefix。只有 `terminal + prefix < best` 的终端进入 H。输出按 target mask 分组并保持顶点递增。

### 11.4 递减 H 与边界结算

H 按 mask 大小从 $q$ 递减到 $\ell+1$。除转置终端外，还可从更大的 successor 加一个 ordinary branch：

```math
H(S,v)=\mathrm{closure}\left(
\min_{B\subseteq[k]\setminus S}
H(S\cup B,v)+D(B,v)
\right).
```

递减顺序保证读取 successor 时依赖已 ready。图闭包传播已经真实支付的外侧代价，prefix 只负责拒绝。每张 H 完成后，枚举低层 $L\subseteq S$，令 $B=S\setminus L$，在共同根结算：

```math
A(L,v)+D(B,v)+H(S,v).
```

三部分分别覆盖锚组及 $L$、中间块 $B$、以及 $[k]\setminus S$，互不重叠且并为全部查询组。

### 11.5 与完整前向高层的等价性

取完整前向 A 的任一规范推导，沿依赖找到第一次跨过低层边界 $\ell$ 的位置。跨界前的锚定 mask 作为低层 $L$；跨界后到最终完成之间加入的 ordinary 块按相反顺序放入 H。最外侧一个或两个平衡块产生转置 terminal，随后每次“给 A 加一个 ordinary branch”反向成为“从更大的 H successor 去掉同一 branch”，最终在 $A(L)+D(S\setminus L)+H(S)$ 相遇。

反过来，任意 H terminal、递减转移和边界结算都可把 ordinary 块按相反顺序加回前向锚定侧。每步使用同一根、同一规范 branch 和相同边权和。因此完整高层 A 推导与“低层 A + terminal + H + 边界结算”之间存在保成本双向映射。

## 12. 完整正确性论证

**引理 1（上界真实性）。** `best` 只由 root-star、真实路径并集、primal、facility 路径、witness-tree DP 或 D/A/H 完整完成式更新。每一项都可展开为原图真实边的连通覆盖，所以任意时刻 `best >= OPT`。

**引理 2（基础下界可采纳）。** 零权 cover 必须连接的免费分量数给出 $L_{\mathrm{cc}}$；任一剩余组不可回避的单组距离给出 $L_{\mathrm{far}}$；树倍增和组度量 shortcut 给出 $L_{\mathrm{tour}}$。它们分别不超过所声明的剩余代价，最大组合仍可采纳。

**引理 3（有界距离安全）。** Base 的 cutoff 来自真实可行上界。未保存位置满足 $d_i(v)\ge U_0$，并明确标记为非精确；任何需要真实 singleton 的位置都检查 `IsExact`。因此 cutoff 只能加强拒绝，不能被当成虚构低成本子树。Enhanced 的完整表在每个位置精确。

**引理 4（A1 cone 安全）。** A1 值和 farthest continuation 具有路径一致性，故 cone 内目标的一条最短路径前缀不会被提前剪掉。cone 外统一返回 $\max\{0,U_0-C_i(v)\}$，不超过真实 A1。若树 DP 收紧上界，整轮重启恢复固定 cutoff 前提。因此两种模式的 A1 精确值和 row 外 future 都安全。

**引理 5（有向割势可采纳）。** Enhanced 逐组只从非负 residual 扣除可行势差，任一弧的累计收费不超过原容量。覆盖剩余各组的树必须支付相应割收费，故 $L_{\mathrm{cut}}$ 不超过剩余树代价。

**引理 6（稀疏图闭包精确）。** 固定 mask 的 seed 都是真实子状态之和；Dijkstra 松弛只加真实非负边权。`paid + admissible_future >= best` 的区域不可能导出严格改善 incumbent 的完整解，区域内保留的值与完整 rooted DP 相同。

**引理 7（branch 规范化完备）。** 任意未标 branch 的等值同根状态都能展开为更小真子集；递归展开最终到 singleton 或严格图闭包 branch。固定 pivot 并只要求接入侧为 branch 去掉重复表示，但不会删掉任何分解等价类的全部代表。

**引理 8（Base 前向完成完备）。** 平衡分解保证任意完整规范推导可写成大小至多 $q$ 的锚定块，加至多两个大小至多 $h$ 的 ordinary 块。Base 生成所有所需 A 层，`CompleteAnchoredRow` 枚举剩余两块，因此至少保留一条最优规范推导。

**引理 9（Enhanced 高层等价）。** 第 11.5 节给出完整高层 A 与低层 A、补集 terminal、递减 H、边界结算之间的保成本双向映射。转置 budget、prefix 和 H 闭包只使用前述可采纳下界，所以不会删除严格更优推导。

**定理（精确性）。** 若不存在共同连通分量，算法正确返回不可行；否则 Base 与 Enhanced 都返回 $\mathrm{OPT}(G,\mathcal K)$。由引理 1，最终 `best` 不小于最优值；由引理 2 至 7，距离截断、规范化和严格剪枝不会删除代价低于 incumbent 的全部最优推导；由引理 8 或 9，相应模式至少枚举一条最优完整推导，故最终 `best` 不大于最优值。两边合并得到相等。

### 12.1 边界输入

- 重叠组按 bit 并集处理，一个顶点可同时覆盖多个组。
- 零权边不破坏 Dijkstra；witness 父指针只接受严格改进，避免等距环。
- 重边由不同 `edge_id` 区分；自环非负，不能改善最短路。
- 非连通图先找共同可行分量；不存在时不进入指数搜索。
- 空 row 通过 `ready` 与未生成依赖区分，只表示严格上界锥体中无可改善状态。
- 浮点容差不参与上下界闭合；路径恢复后的候选始终按真实原边计价。

## 13. 复杂度

令 $k=g-1$、 $h=\lfloor g/2\rfloor$、 $F=\sum_i\lvert K_i\rvert$，Enhanced primal 涉及 $r$ 个 facility。忽略稀疏性时，主 subset 搜索保持经典指数上界：

```math
O\left(3^g n+2^g(m+n)\log(n+m)\right)
```

和空间：

```math
O(2^g n).
```

该式只描述 D/A/H 主格。把当前预处理完整计入，一个保守总界为：

```math
O\left(
(g+r+2^g)(m+n)\log(n+m)
+g^2(m+n)
+2^g(g^3+r^2)
+3^g(n+r)
+F\log(F+1)
\right).
```

主要阶段如下：

| 阶段 | 保守时间 | 主要空间 |
|---|---:|---:|
| 图连通分量加载 | $O(n+m)$ | $O(n)$ |
| 零权 cover | $O(n+m+3^g)$ | $O(n+2^g)$ |
| 组距离 | $O(g(m+n)\log(n+m))$ | Base 最坏 $O(gn)$，Enhanced 固定 $O(gn)$ |
| 候选根路径 bootstrap 与恢复 | 保守 $O(g^2(m+n)\log(n+m))$ | $O(n+m)$ |
| group tour | $O(2^g g^3)$ | $O(2^g g^2)$ |
| 共同 A1 | $O(k(m+n)\log(n+m))$ | 最坏 $O(kn+n)$ |
| ordinary D 与前向 A | $O(3^g n+2^g(m+n)\log(n+m))$ | $O(2^g n)$ |
| directed cut | $O(gm+g(m+n)\log(n+m))$ | $O(gn+m)$ |
| facility 上界 | $O(r(m+n)\log(n+m)+2^g r^2+3^g r)$ | $O(n+r^2+2^g r)$ |
| 高层 H | 最坏不超过被替换前向高层的指数阶 | $O(2^g n+gn)$ |

当前二叉堆没有 decrease-key，故把重复 push/pop 计为 $O(\log(n+m))$；允许重边时不直接借用简单图专用的更紧写法。

### 13.1 输出敏感成本

令 $M_D,M_A,M_H$ 为实际检查的同根组合数， $R_D,R_A,R_H$ 为图闭包实际扫描的邻接项数，则主搜索更贴近实现的时间是：

```math
O\left(M_D+M_A+M_H+(R_D+R_A+R_H)\log(n+m)\right).
```

令 $Z_D,Z_A,Z_H$ 为最终保存的稀疏 payload 数。Base 主状态空间为 $O(Z_D+Z_A+n)$；Enhanced 为 $O(Z_D+Z_A+Z_H+gn)$，另有阶段性的 residual 和 facility 空间。

程序报告的 `mask_vertex_states` 不是最终 payload 数。对每张 D、A、H row，某顶点第一次从无穷进入工作区时计一次；同一 row 内后续改进不重复。A1 提前构造时计数，移动到前向容器后不再计。组距离、tour、dual、转置临时终端、完整解结算和过期队列项不计。该量可解释搜索域，但不等同于 CPU 指令或峰值内存。

## 14. 实现细节及其目的

| 细节 | 目的 | 若写错的后果 |
|---|---|---|
| 8 MiB 数字读取缓冲 | 降低大图 token 解析开销 | 图加载可能主导总墙钟 |
| 先统计度数再建邻接 | 每个邻接 vector 只分配一次 | 大图反复扩容与复制 |
| `ready` 独立于 payload | 区分空 row 与未生成依赖 | 高层错误跳过或读取状态 |
| 递增 `vertex/value` | 双指针交集、较小侧二分、稳定访问 | hash 随机访问放大常数 |
| `branch_bits` | 保留规范拆分代表 | 位定义错误可能丢解或重复爆炸 |
| `IsExact` | 阻止 cutoff 冒充真实 singleton | 直接产生虚假低成本状态 |
| epoch 与 `touched` | 不为每张稀疏 row 清零全图数组 | 大图 O(n) 清零掩盖稀疏收益 |
| A1 top-two locator | ordinary 中常见 future 查询 O(1) | 反复二分 A1 row 或放大 RSS |
| A1 不读取模式位 | 保证两种模式的 A1 真正相同 | 出现两套 cone 与 fallback 证明 |
| 单一零起点 scheduler | A1 与 D 连续支付同一 rent | Base/Enhanced 出现不可解释的不对称购买 |
| A1 收紧后整轮重启 | 一轮 row 共用固定 cutoff 证明 | 混合两套 cone 与 fallback |
| 64 顶点转置块 | 限制 vertex-major 临时容器并对齐位图 word | 全图建立 O(n) 小容器 |
| 严格父指针改进 | 零权等距时维持无环 witness | 父指针环或重复遍历 |
| 原始 `best <= lower` | 只有真正闭合才提前返回 | epsilon 可能把正 gap 误判为最优 |

所有 lambda 都紧邻中文注释，说明它枚举的集合、缓存的量或更新的不变量。长函数按上述阶段加块注释；修改代码块时必须同步修改注释和本章对应段落。

## 15. 论文表述边界

- 可以声称：单线程、无向非负边权、支持零权边、 $g\le16$、输出精确最优权值。
- 不应声称当前二进制已经输出最终最优边集；它只维护内部 witness 和最优权值。
- Base 与 Enhanced 是同一公开入口的预声明模式，不能逐查询取两者较快值组成第三条曲线。
- Enhanced 相对 Base 的关系必须按“安全新增或同职责替换”表述，不能写成两个互不相关的方法。
- A1 是两种模式的共同逻辑层，不是 Base 独占操作，也不由 H 替换。
- A1 是否存在只由 $q=\max\{0,\lfloor g/2\rfloor-1\}$ 的逻辑域决定，不能写成经验性的 `g >= 常数`。
- Dijkstra、subset DP、A* 下界、有向割对偶、inside/outside、位图和 rent-or-buy 本身都不是独立原创点；贡献应聚焦于精确 GST 中的状态组织、证书组合、共同 A1 复用和高层完成方向。
- `double` 上的精确是组合结构精确，不是任意精度实数运算；论文实验仍应说明跨实现目标值核验容差。
