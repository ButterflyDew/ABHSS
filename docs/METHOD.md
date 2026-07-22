# ABHSS 方法说明：分层预算下的锚定子集动态规划

更新时间：2026-07-21。

## 1. 方法目标与两个版本

ABHSS 面向精确 Group Steiner Tree（GST）查询。给定带非负边权的无向图和 `g` 个顶点组，目标是寻找一棵至少命中每个组一个顶点的最小权连通子图。

ABHSS 不在运行中猜测应该使用哪套方法，而是提供两个独立入口：

- **ABHSS-Light**：控制固定预处理和内存，用有界组距离、提前锚定 singleton、根路径见证树和完整前向 `A` 格求值。
- **ABHSS-Heavy**：先支付完整 directed-cut 预处理，再用 dual/primal 见证树、低层前向 `A`、高层伴随 `H` 和按根转置终端求值。

二者共享相同的普通状态定义、不可拆分分支、下界接口和唯一的稀疏 row 格式，但 **Heavy 不是 Light 的严格超集**。它们是针对不同状态压力的两套冻结算法，调用者应分别报告结果，不能按数据集名称、固定 `g`、query id、状态密度或运行时间在内部切换。

## 2. 统一记号与永久锚组

首先构造一个共同根可行解，并选择其根 `r`。在 `r` 上距离最远的组被固定为锚组 `T_a`，其余组记为 `K`，令 `k=|K|=g-1`、`h=floor(g/2)`。锚组一旦选定便不再改变。

对每个原始组 `i`，多源最短路距离为

```text
d_i(v) = min(t in T_i) dist_G(v,t).
```

后续所有掩码只编码 `K` 中的组；需要调用全组下界时，再把掩码映回原始组编号。

## 3. 两个版本共享的主线

一次查询按以下顺序执行：

1. 检查查询组是否位于同一可达分量，并计算零权分量覆盖下界。
2. 计算组距离、共同根可行上界、根到各组最短路并集上界，选择永久锚组。
3. 构造固定端点 group-tour 下界，并缓存每个顶点距离最远的组编号。
4. 生成所有 `|S|<=h` 的普通状态 `D(S,v)`。
5. Light 生成完整前向锚定状态 `A`；Heavy 只生成低层 `A`，再从补集终端反向生成高层 `H`。
6. 每次拼出真实可行树时收紧 incumbent `B`；只有严格可能得到小于 `B` 的状态才被保留。

整个求解只维护一个确定主线。不存在失败后从头调用另一框架，也不存在运行时回退。

## 4. 公共上下界

### 4.1 零权分量覆盖下界

删除全部正权边后得到若干零权连通分量。若至少需要 `c` 个这样的分量才能覆盖全部查询组，而最小正边权为 `w_min`，则任何可行树至少需要连接这些分量：

```text
L_component = (c-1) w_min.
```

若一个零权分量已经覆盖全部组，最优值立即为零。若某个可行上界等于该下界，也可以直接结束。

### 4.2 共同根和根路径并集上界

对任意根 `r`，把它分别沿最短路接到每个组可得上界 `sum_i d_i(r)`。算法进一步恢复这些路径，并按原图 edge id 去重。路径并集连通且覆盖全部组，因此其真实边权和仍是合法上界；这个并集同时为 Light 提供见证树。

### 4.3 最远组与固定端点 tour 下界

设当前状态尚未覆盖的原始组集合为 `R`。最远组下界为

```text
L_far(v,R) = max(i in R) d_i(v).
```

算法还在组间最短路度量上预计算 Hamiltonian path。对 `R` 中每个固定端点组，求所有以该组为端点的路径完成代价下界，再对端点取最大值并除以二，得到 `L_tour(v,R)`。固定端点版本支配只在所有端点对中取一次全局最小值的旧公式。

Light 使用 `max(L_far,L_tour)`；Heavy 再加入 directed-cut 下界 `L_cut`。

## 5. 普通状态 `D`

### 5.1 状态语义

```text
D(S,v) = 连接非锚组集合 S，且以 v 为根的最小树代价。
```

单组状态直接由组距离给出。多组状态先在同一个根合并较小状态，再执行一次多源最短路闭包：

```text
D(S,v) = min_u { M_S(u) + dist_G(u,v) }.
```

状态按 `|S|` 递增生成到 `h`。每张 row 的闭包使用 `value + future` 作为优先队列键；若该键不能严格改善 incumbent，则状态及其后继均被剪去。

### 5.2 不可继续同根拆分的 branch

直接枚举所有二分会让同一棵树以多种结合顺序反复出现。ABHSS 固定 `S` 中编号最小的组位于累计侧，另一侧只能读取 **不可继续同根拆分的 branch**。

具体地，row 闭包前记录同根 split seed。若闭包后的 `D(S,v)` 严格小于该 seed，则它必须经过至少一条图边后才达到根 `v`，可作为不可拆分 branch；单组状态天然是 branch。递归展开任意被排除的可拆状态最终都会到达单组或不可拆分状态，因此该规范化只删除等价生成顺序，不删除最优树。

### 5.3 唯一的稀疏 row

ABHSS 暂时删除逐 row 的 sparse、dense、ranked-bitmap 三选一。每张 `D/A/H` row 都保存：

```text
vertex[]   严格递增的有限根编号
value[]    对应的 double 代价
branchBits 仅 D 使用，每个 value 对应一个 bit
```

row 交集通过有序列表归并完成，单点读取使用二分查找。实现会在“线性归并”“扫描 branch bit 并二分另一行”“扫描较短值行并二分 branch 行”之间按当次确定的操作数选择，但三者读取的仍是**同一种物理 row**。这样做不改变状态语义和渐进复杂度，但会放弃高密度 row 的位图交集与 dense 直接访问收益。当前精简发行版的性能数字必须与历史三布局研究版分开报告。

**组距离表 `d_i` 不是 DP 状态 row。** Heavy 的完整 `d_i` 固定使用 dense。Light 的有界 `d_i` 可能覆盖几乎全图，也可能只结算少量顶点；因此每个组在 Dijkstra 结束后，比较 `(n+1)sizeof(double)` 与“有序精确值 + membership/rank 位图”的实际字节数，保留较小者。这个选择不读取数据集、`g`、行密度阈值或运行时刻，也不恢复 `D/A/H` 的三布局。

## 6. Light：提前锚定 singleton 与完整 `A`

### 6.1 有界组距离

Light 先从每个查询组的规范终端运行 SPT，得到真实可行上界 `B0`。随后每次组多源最短路只结算严格小于 `B0` 的距离。未保存位置满足 `d_i(v)>=B0`，不可能参与成本小于 `B0` 的最终解。

### 6.2 提前锚定 singleton

对每个非锚组 `i`，正式锚定格中的第一层为

```text
A_i(v) = min_u { d_a(u) + d_i(u) + dist_G(u,v) }.
```

Light 在普通 `D2` 前提前生成这些 exact row。令 `R_i=K-{i}`，并定义 continuation 下界

```text
h_i(v) = max(j in R_i) d_j(v).
```

只物化满足 `A_i(v)+h_i(v)<B0` 的闭包区域。对缺失位置，由 `A_i(v)+h_i(v)>=B0` 得到合法证书

```text
A_i(v) >= max(0, B0-h_i(v)).
```

普通状态若尚未覆盖 `i`，任何锚定完成都必须至少支付 `A_i(v)`。因此对剩余组取 `max_i A_i(v)` 是合法的 anchor-aware future。每个顶点缓存最大的两个组编号；若它们均已覆盖，再扫描剩余组。缓存只改变读取顺序，不改变数值。

### 6.3 完整前向锚定格

```text
A(S,v) = 连接锚组和 S，且以 v 为根的最小树代价。
```

种子由一个较小的 `A` row 与一个普通不可拆分 branch 在同根相加，随后执行图闭包。提前生成的 `A_i` 在第一层直接复用，不重复运行闭包。每张完成的 `A` row 立即与至多两张普通 `D` row 在同根结算；当全部非锚组都被覆盖时得到真实可行解。

Light 生成到 `|S|=h-1`。最后一层只消费并更新答案，不再保存。

### 6.4 根路径见证树上界

根路径并集被整理为一棵以锚终端为根的真实树。随着普通 `D` row 出现，可以把某个非锚组块作为一棵真实 `D(S,v)` 子树接到见证树节点，也可以沿见证树边共享路径。树上的 subset DP 因此始终构造真实可行解。

ABHSS 只累计 incumbent 收紧后可能节省的 ordinary queue pop 和 edge relaxation；累计工作足以支付一次树 DP 时才重新求值。该 rent-or-buy 预算来自实际算法工作量，不读取数据集、层号或运行时间。

## 7. Heavy：directed-cut、低层 `A` 与高层 `H`

### 7.1 eager changed-arc directed-cut

Heavy 完整计算每个组的距离后，按固定顺序分配有向边容量，构造组势函数 `pi_i(v)`，满足

```text
sum_i max(0, pi_i(u)-pi_i(v)) <= w(u,v).
```

因此

```text
L_cut(v,R) = sum(i in R) pi_i(v)
```

不超过从 `v` 连接剩余组的真实代价。后续组的 residual Dijkstra 只从此前被势函数改写过、且真正违反新三角不等式的弧启动，但传播仍读取完整邻接表；该 changed-arc 初始化与全弧初始化得到相同势。

Heavy 不执行 residual packing，也不构造 junction。directed-cut 完成后恢复出的原图边形成一棵覆盖全部组的 primal 可行树；算法用它产生初始上界和逐层见证树上界，随后释放 residual。

### 7.2 低层前向 `A`

令最终所需锚定深度为 `a_max=h-1`，切分点为

```text
c = floor(a_max/2).
```

Heavy 只正向物化 `|S|<=c` 的 `A(S,v)`。递推、图闭包和完成结算与 Light 相同，只是 future 额外取 `L_cut`。

### 7.3 按根转置补集终端

高层锚定状态最终需要与一张或两张 ordinary row 在同根相遇。直接对每个高层目标重新枚举普通分块会反复扫描相同数据。Heavy 改为按根收集当前可用的 `D(S,v)`，一次产生所有高层目标的补集终端：

```text
terminal(T,v) = min D(L,v) + D(R,v),
T = K - (L union R),  L intersection R = empty.
```

directed-cut 势按组可加。把每个普通值减去其组势后，同一根上的全部目标共享统一 reduced budget。实现根据本次根上的精确候选数量，在全局有序双指针枚举与补集子掩码枚举之间选择工作量较小者；该选择比较的是当前操作数，不是经验阈值。

### 7.4 高层伴随 row `H`

`H(T,v)` 表示：从高层锚定目标 `T` 出发，沿原 `A` 依赖图反向走到一个补集终端所需的最小后缀代价。它从 `terminal(T,v)` 启动，也可从更大的 successor `H(T union Q,v)` 吸收一个 ordinary branch `Q`，然后执行图闭包。

高层掩码按大小递减处理，因此所有 successor 已经完成。每张 `H` row 完成后，立即与所有可兼容的低层 `A` 边界相交并更新 incumbent。所有跨切分的 `A` 依赖边都由“低层边界 + 第一个高层 successor”唯一覆盖；补集终端则覆盖原前向格的完成操作，所以反向求值与完整前向 `A` 格等价。

## 8. 正确性结论

ABHSS 的精确性由以下不变量组成：

1. `D(S,v)` 与 `A(S,v)` 都是对应 rooted GST 子问题的精确值；图闭包是多源最短路的标准 min-plus 闭包。
2. 不可拆分 branch 只规范化同根分解顺序，不删除任何标准递推树。
3. `L_component`、`L_far`、固定端点 `L_tour`、Light 的缺失 A1 证书和 Heavy 的 `L_cut` 都是不超过真实剩余成本的下界。
4. 共同根、根路径并集、SPT、primal tree 和见证树 DP 都由原图真实边与真实 rooted 子树组成，只能产生合法上界。
5. Light 完整枚举平衡 `A+D+D` 完成；Heavy 的低层 `A`、第一条跨切分依赖、递减 `H` 和补集终端与完整前向格之间存在完备对应。
6. 所有剪枝只删除不能严格改善已知可行解的状态，因此最终 incumbent 等于最优值。

## 9. 复杂度

令 `n=|V|`、`m=|E|`、`k=g-1`。忽略稀疏剪枝后的实际状态数，两版仍保持经典半状态 GST 动态规划的最坏上界：

```text
时间 O(3^g n + 2^g (m+n log n))
```

固定端点 tour 预计算使用 `O(2^g g^2)` 空间和至多 `O(2^g g^3)` 时间；`g<=16` 时它被主 DP 上界覆盖。Light 的有界最短路只会减少实际图扫描。Heavy 额外支付 `g` 轮 residual 最短路、primal facility 和转置终端；这些阶段仍不超过方法声明的指数主界。

若以 `Z_D`、`Z_A`、`Z_H` 表示实际保留的稀疏 row 值数，则 DP payload 为 `O(Z_D+Z_A)` 或 `O(Z_D+Z_A+Z_H)`。Heavy 还需要 `O(gn+m)` 的组距离、势和临时 residual；Light 的有界组距离在最坏情况下仍为 `O(gn)`，稀疏行时则按实际 settle 数与 membership/rank 位图保存。删除 DP 三布局后，高密度查询的实际空间可能高于 ranked-bitmap DP row 版本，这属于本次冻结版本的明确取舍。

## 10. 文献来源与贡献边界

rooted subset recurrence 源自 [Dreyfus and Wagner, 1971](https://doi.org/10.1002/net.3230010302)。平衡三分解和半状态 meet-in-the-middle 见 [Iwata--Shigemura, 2019](https://ojs.aaai.org/index.php/AAAI/article/download/3965/3843)。rooted 状态搜索与 admissible future cost 可参照 [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492) 和 [DS*](https://arxiv.org/abs/2011.04593)。directed-cut dual-ascent 路线源自 [Wong, 1984](https://doi.org/10.1007/BF02612335)。GST 的直接 CPU/GPU 对照分别来自 [PrunedDP++](https://doi.org/10.1145/2882903.2915217) 与 [GPU4GST](https://doi.org/10.1145/3769792)。一般 inside/outside 或伴随递推可参照 [Goodman, 1999](https://aclanthology.org/J99-4004.pdf)、[Li--Eisner, 2009](https://aclanthology.org/D09-1005.pdf) 和 [Gildea, 2020](https://aclanthology.org/2020.cl-4.2.pdf)。

ABHSS 不把 Dijkstra、subset DP、位图、directed-cut、inside/outside 或 rent-or-buy 原语单独宣称为原创。可供论文讨论的是这些构件在 exact GST 中形成的整体组织：永久组锚定、普通不可拆分分支、Light 的有界 anchor-aware future 与完整前向格，以及 Heavy 的 dual/primal 见证、按根补集转置和 closure-aware 高层伴随求值。最终原创性表述仍应以投稿前的完整相关工作审计为准。
