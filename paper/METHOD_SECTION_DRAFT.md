# ABHSS 论文方法章节初稿与版面设计

本文档是面向 SIGMOD/VLDB 完整论文的方法章节中文初稿。它从 [`docs/METHOD.md`](../docs/METHOD.md) 的实现级证明中提炼正文主线，不取代后者。本文只描述当前冻结算法；历史事故、负结果和旧二进制门禁留在 [`docs/archive`](../docs/archive/README.md)。ABHSS 的英文全称尚需作者最终确认，初稿不擅自扩写缩写。

## 1. 正文目标与篇幅预算

方法章节建议控制在 4.5–5 页，正文只回答四件事：搜索状态是什么、哪些证书安全删状态、为何 Adjoint 能替代高层前向格、整个算法为何仍然精确。实现级缓存布局和完整实验门禁不进入主线。

| 建议小节 | 目标篇幅 | 正文必须留下的内容 |
|---|---:|---|
| 预备定义与挑战 | 0.4 页 | GST、rooted subset DP、指数状态爆炸 |
| 总览与稀疏 ordinary 格 | 0.9 页 | 一个算法、统一 row、规范 branch、图闭包 |
| 共同 A1 与上界调度 | 0.9 页 | A1 cone/fallback、真实 witness、rent-or-buy |
| DirectedCut 证书 | 0.7 页 | 对偶可行下界、真实 primal/facility 上界、单调重滤 |
| AdjointCompletion | 1.4 页 | 补集语义、辅助半格、三类 split、边界完成式 |
| 正确性与复杂度 | 0.7 页 | 主定理、证明链、最坏界与输出敏感解释 |

正文最多放 3 段伪代码和 1 张结构图。A1 的分级 top-two/ranked-tail 缓存、certificate-support 增量 subset DP、浮点区间读取和所有负结果放附录或 artifact 文档；正文只说明它们保持同一证书值与状态域。

## 2. 论文叙事主线

现有精确 GST 动态规划的困难不是缺少一个新的指数递推，而是完整 subset–vertex 格会在图维度和组维度同时膨胀。ABHSS 的叙事应按以下顺序展开：

1. 以 rooted ordinary 状态 $D(S,v)$ 作为唯一公共底座，用真实上界与可采纳 future 只物化仍可能改善 incumbent 的稀疏 row。
2. 固定一个锚组，把完整解组织成“锚定侧 + ordinary 块”。所有配置共同提前构造第一张锚定层 A1，使它既是标准前向 row，又能作为 ordinary 搜索的 future。
3. `DirectedCut` 增加容量可行的下界和由真实边恢复的上界，形成“上界—下界—新上界—安全重滤”的单调证书闭环。
4. `AdjointCompletion` 不再正向枚举 A1 之后的全部高层 $A$；它先用辅助 $H(h)$ 精确实现被省略的 ordinary 半格 $D(h)$，再递减维护补集状态 $H(S,v)=D([k]\setminus S,v)$。每个普通 split 恰落入单块终端、双块终端或 successor 加 ordinary 三类之一。
5. 由真实上界、可采纳下界和前向/反向格完备性得到精确性。Base 与 Enhanced 是同一算法的预声明配置：差异只能是安全新增证书或同一职责的 realization 替换。

建议把贡献排序写成“稀疏证书化状态格 → 公共锚定 future → 反向高层完成”，把缓存和工程减空放在实现段。不要把 Dijkstra、subset DP、A*、directed-cut 或位图单独包装成贡献。

## 3. 方法正文草案

### 3.1 问题、记号与基础状态

给定无向非负边权图 $G=(V,E,w)$ 和非空顶点组 $K_0,\ldots,K_{g-1}$，目标是求一棵与每个组至少相交一次的最小权连通子图。非负边权保证任意可行连通子图都可删环为树而不增代价。

选择锚组 $K_a$，其余 $k=g-1$ 个组用 bit mask 表示，并记 $[k]=\{1,\ldots,k\}$。对 $S\subseteq[k]$ 和 $v\in V$，ordinary 状态定义为：

```math
D(S,v)=\text{覆盖 }S\text{ 中各组并以 }v\text{ 为根的最小子树代价}.
```

完整 rooted DP 在同一根合并两个互斥子集，并沿原图边做非负权闭包。ABHSS 不稠密保存全部 $2^k\lvert V\rvert$ 项；每个 row 只保存有限且仍可能严格改善当前真实上界 $U$ 的 `(vertex,value)` 对。缺失项不是近似值：它要么确实为无穷，要么由该 row 的可采纳拒绝证书解释。

为避免同一拆分的对称重复，令每个有限 ordinary 状态记录规范 branch。最低 pivot 唯一确定拆分方向；任意非 branch 推导都可递归展开到一个等价 branch。因此合并时只要求被接入的一侧为 branch，仍保留每个拆分类的至少一个代表。

### 3.2 总体算法与配置关系

令：

```math
h=\left\lfloor\frac g2\right\rfloor,
\qquad
q=\max\{0,h-1\}.
```

$q$ 是平衡完成式要求的最高逻辑锚定层，不是经验参数。所有配置先执行共同可行性、真实上界和低组基例。距离初始化虽然可由 bounded 或 complete 两种物理表示实现，但都只返回同一合同：组距离视图、共同根和真实上界；bounded 中未保存的位置只能作 cutoff 拒绝证书，任何需要真实子树值的消费者必须读取精确项或无穷。当 $g\le3$ 时，任一最优树在分叉点分为至多三条组路径，反向又可合并共同根到各组的最短路，所以最优值等于该距离和的最小值，算法精确返回而不进入指数搜索。

对其余查询，Base 与 Enhanced 都构造共同 A1 和所有 $\lvert S\rvert\le q$ 的 ordinary row。Base 还以 ordinary 半格和完整前向 $A$ 完成；Enhanced 增加 DirectedCut 证书，并以辅助半格 $H(h)$ 与递减 $H$ 替换 Base 的 $D(h)$ 及 A1 之后的高层前向职责。

```text
Algorithm 1  ABHSS(G, K, profile)
1:  检查共同连通分量；若不存在则返回 infeasible
2:  计算零权 component-cover 下界和距离—根初始化合同
3:  若 g <= 3，则用共同 root-star 恒等式精确返回
4:  构造真实初始上界、锚组、tour 表和当前 profile 的 witness；若开启 DirectedCut，同时构造初始势与 primal
5:  两种 profile 都令 witness scheduler 的 rent <- 0
6:  若逻辑层 1 存在，则用同一 cone/fallback 构造公共 A1
7:  按 mask 大小构造共同 ordinary row D(S)，直至 q
8:  若 DirectedCut 的确定性 rent 达到购买价，则补全 residual，并刷新真实上界证书
9:  若 profile 不含 AdjointCompletion：
10:     构造 ordinary 半格 D(h)，并运行完整前向 A
11: 否则：
12:     从已有 D 构造辅助 H(h)，再令 H 从 h 递减到 2
13:     用公共低层 A、ordinary D 与 H 的边界式完成
14: 返回当前真实上界 U
```

算法 1 的 `profile` 在查询批次开始前冻结。Enhanced 不按图名、组数区间、状态量或运行时间选择另一条路径。配置差异仅有两类：DirectedCut/facility 是安全新增证书；bounded/complete 距离表示、root-path/dual-primal witness、完整前向/Adjoint 是共享输入输出职责的 realization 替换。A1 和 $D(1),\ldots,D(q)$ 始终共同。

建议正文配一张层格图：横轴为 mask 大小，底部画共同 ordinary $D(1..q)$，左上画共同 A1；Base 继续到 $D(h)$ 和高层 $A$，Enhanced 从 $H(h)$ 反向递减并在 A/H 边界汇合。图中用实线表示共同层、虚线表示安全新增证书、双箭头表示同职责替换。

### 3.3 可采纳 future 下的稀疏 row

对 row $S$，先聚合所有规范同根 split 的精确 seed，再统一调用 future。令 $L(S,v)$ 为连接剩余组的可采纳下界。若：

```math
D(S,v)+L(S,v)\ge U,
```

则任何经该状态完成的树都不优于已知真实可行解，可安全拒绝。保留 seed 后，使用 key `value + future` 做非负边 Dijkstra 闭包。这样每个有限输出值都仍是完整 rooted DP 的精确值，而 row 外位置由拒绝式解释。

```text
Algorithm 2  BuildSparseRow(S, dependencies, U)
1:  对每个规范拆分 X + (S \setminus X)：
2:      双指针相交两张有序稀疏 row
3:      只从规范 branch 一侧接入，更新逐顶点最小 seed
4:  对每个有限 seed (v,x)：
5:      若 x + Future(S,v) < U，则加入优先队列
6:  while 队列非空：
7:      弹出最小 key 的 (v,x)；忽略过期项
8:      对每条边 (v,u)：
9:          y <- x + w(v,u)
10:         若 y 改善 u 且 y + Future(S,u) < U，则松弛
11: 把有限距离按顶点递增写为 row，并标记规范 branch
12: 返回 row
```

Base 的 `Future` 由 component-cover、farthest、tour 和公共 A1 组成。Enhanced 在同一接口中再取 DirectedCut 势的最大值。实现可分阶段缓存已经计算的下界，但缓存只能保存可采纳前缀，不能把某个候选的一次拒绝永久解释成 row 值；候选变小时必须能继续计算尚未完成的证书链。

### 3.4 公共 A1 与真实上界调度

A1 是逻辑层 $A(\{i\},v)$ 的标准精确 row。所有配置在 ordinary 之前用同一 seed、同一图闭包和同一 continuation 构造它，之后把同一 row 的所有权移交公共前向内核，不重复闭包或计数。

对尚未覆盖组集合 $R$，定义最远组下界和 endpoint-floor 的最大值为 $C(v,R)$。endpoint-floor 从组间 Hamilton path 表中选择终点自由路径值最大的起点组 $l^{*}(R)$：

```math
C^{\mathrm{path}}(v,R)
=
\frac{d_{l^{*}(R)}(v)+h_{l^{*}(R)}(R)}{2}.
```

```math
C(v,R)=\max\left\{\max_{j\in R}d_j(v),C^{\mathrm{path}}(v,R)\right\}.
```

当 $R$ 只有一组时两项都退化为该组距离。 $C$ 可采纳且沿边为 1-Lipschitz，因此若某个 A1 目标满足 `A1 + C < U`，其规范最短路径前缀也满足该式。row 内返回精确 A1；row 外统一返回：

```math
\underline A_i(v)=\max\{0,U-C(v,R_i)\}.
```

这使“用 cone 删除位置”和“用缺项作为 future”共享同一证明。DirectedCut 不进入 A1：否则 Base 与 Enhanced 的缺项会由不同证书解释，破坏公共 row 接口；让 Base 也支付完整对偶势又会消解增强边界。

初始上界来自真实原图路径组成的 witness。Base 使用 root-path tree，Enhanced 使用 dual-primal tree；二者都只构造 witness，不在预处理中无条件运行树 DP。随后 A1 与 ordinary 的 queue-pop/edge-relax 工作共同支付 rent。设 witness 有 $t$ 个顶点，则购买阈值为：

```math
B_{\mathrm{wit}}(t,k)
=
t\left(\frac{3^k-1}{2}+3^k\right).
```

达到阈值且 witness 输入修订发生变化时，两种配置调用同一个树 DP。若购买在 A1 中严格收紧 $U$，当前未发布的部分 A1 pass 必须丢弃，并在新的固定 $U$ 下整轮重建；否则一张 row 会混合两套 cone 解释。

A1 future 的 lazy、top-two 和完整 ranked-tail 是同一精确最大值视图的三种物理表示。共同表示的是规则、代码路径和 row 合同；不同 profile 在进入 A1 前可能持有不同但合法的真实上界和距离 realization，因此不声称其最终 payload 或状态数逐项相同。正文只需说明分级 rent-or-buy 不改变 double 值或触发顺序；具体整数购买式和 byte/locator 布局放附录。

### 3.5 DirectedCut：单调证书闭环

Enhanced 将每条无向边视为两条同容量有向弧，依次构造各组势 $\pi_i(v)$。每轮只从 residual 容量扣除非负势差，并始终保持 residual 非负。因此对剩余组 $R$：

```math
L_{\mathrm{cut}}(v,R)=\sum_{i\in R}\pi_i(v)
```

不超过从 $v$ 完成 $R$ 的最小附加代价。changed-arc 修复和截断 potential cone 只省略势差严格为 0 的弧，不改变对偶值。

对偶势只能作为下界。算法沿零 residual 支撑恢复 primal，并按原图真实边重新计价，得到可行上界和 dual-primal witness；facility DP 同样只组合可展开到真实边的路径。ordinary 搜索积累足够确定性工作后，可一次性购买 residual closure，补全势并恢复更强 primal/facility 上界。若新的真实路径证书继续改善 $U$，算法只删除满足以下条件的已物化状态：

```math
D(S,v)+L_{\mathrm{new}}(v,[k]\setminus S)\ge U_{\mathrm{new}}.
```

于是 DirectedCut 形成闭环：真实路径给初始 $U$，容量可行势提高 $L$，购买后真实边恢复新的 $U$，最后按新的 $(L,U)$ 单调收缩已有 row。这个闭环是 Enhanced 相对 Base 的安全新增，不承担 A1 的替换职责。

### 3.6 AdjointCompletion：从补集侧完成高层

完整前向格需要 ordinary 半格 $D(h)$，并正向生成锚定层 $A(1),\ldots,A(q)$。Enhanced 保留共同 A1 与全部 $D(1..q)$，但不物化 $D(h)$，也不正向生成 A1 之后的高层 $A$。它定义：

```math
H(S,v)=D([k]\setminus S,v),
\qquad
2\le\lvert S\rvert\le h.
```

这里 $S$ 表示已经放在锚定/外侧的组， $H$ 保存其补集 ordinary 子树。关键不是名称转置，而是必须恢复被省略 ordinary row 的全部同根 split 和图闭包。

辅助层 $H(h)$ 提供精确基例。若 $g=2h$，补集大小为 $q$，已有完整 $D(q)$ 可直接转置；若 $g=2h+1$，补集大小为 $h$，其任意规范二分两侧都不超过 $q$，双块终端枚举全部 split seed，再执行与 ordinary 相同的图闭包。因此 $H(h)$ 精确实现被省略的 $D(h)$。

对较低目标 $S$，令 $Q=[k]\setminus S$。 $D(Q)$ 的每个规范 split 恰落入三类之一：

1. 完整 $D(Q)$ 已物化，使用单块终端；
2. 两侧大小都不超过 $q$，使用双块终端；
3. 至少一侧超过 $q$，较大侧由已完成的 H successor 表示，较小侧从 ordinary 读取全部精确值。

第三类不能只读 ordinary branch，因为原规范 branch 可能已经位于 successor 所代表的一侧。三类覆盖是 Adjoint 正确性的核心，也是正文必须重点解释的地方。

```text
Algorithm 3  AdjointCompletion(D, A1, U)
1:  转置所有已发布 ordinary row，建立必要单块/双块终端
2:  for size <- h downto 2：
3:      for 每个大小为 size 的目标 S：
4:          seed <- 该目标的单块/双块终端
5:          for 每个非空且 $D(B)$ 已发布的 $B\subseteq[k]\setminus S$：
6:              若 successor $S\cup B$ 位于已完成的 H 区间：
7:                  用 $H(S\cup B)$ + 全值 $D(B)$ 松弛 seed
8:          用可采纳 prefix 做 ordinary 同构的图闭包，发布 H(S)
9:          用低层 $A(L)$ + branch-$D(S\setminus L)$ + $H(S)$ 更新 $U$
10:         若 S 与其补集同属最高辅助半格：
11:             用锚距离 + $H(S)$ + $H([k]\setminus S)$ 更新 $U$
12: 返回 U
```

从 size 大到小归纳可得所有已发布 $H(S)$ 都等于补集 ordinary 值。边界式中的三部分覆盖互斥组集且并为全集。组数为奇数时，最高辅助半格的两侧同为 $h$，ordinary 中没有任一 $D(h)$；第 10–11 行的互补 H 完成式恰好恢复这一平衡情形。该条件由集合大小和 row 生命周期推出，不是经验奇偶优化。

Adjoint 的真正收益来自状态职责替换：Base 显式持有 $D(h)$ 和高层前向 $A$，Enhanced 以补集转置复用较小 ordinary row，并从外侧递减完成。它并不改变 rooted DP 的数值语义，也不删除共同最高逻辑 ordinary 层 $D(q)$。

### 3.7 正确性主定理

证明应按“上界真实—下界可采纳—稀疏 row 精确—两种完成格完备”四层组织，而不是逐函数罗列。

**引理 1（真实上界）。** 所有写入 $U$ 的值都由原图真实路径、真实 rooted 子树或它们在 witness/support 上的精确 DP 组成，因此 $U$ 始终不小于最优值。

**引理 2（可采纳证书）。** component-cover、farthest、tour、公共 A1 缺项和 DirectedCut 势分别不超过其声明的剩余代价；取最大仍可采纳。

**引理 3（距离—根合同）。** bounded 与 complete realization 都返回真实上界和同语义的组距离接口。bounded 中未保存的位置至少达到构造 cutoff，只能作为拒绝证书；split、完成式和 witness 对这些位置一律读为无穷。若最优推导低于 cutoff，其所需距离必已精确保留；若等于 cutoff，已有真实上界已经闭合。

**引理 4（稀疏 ordinary 格）。** 算法 2 的规范 split 聚合不漏等价类；在可采纳 future 下拒绝的状态不能导出严格优于 $U$ 的完整树；锥体内的图闭包与完整 rooted DP 数值相同。

**引理 5（公共 A1）。** continuation 可采纳且 1-Lipschitz，因此 cone 保留任何可改善目标的最短路径前缀，row 外 fallback 由同一拒绝式推出。若 $U$ 改变则整轮重启，最终发布 row 只有一个固定证明边界。

**引理 6（完整前向完备）。** 对任意规范完整树，平衡分解可选出大小至多 $q$ 的锚定侧，并把余下组放入至多两个大小至多 $h$ 的 ordinary 块。Base 的 ordinary 半格、前向增长与完成式枚举这条第一次跨界推导。

**引理 7（Adjoint 等价）。** 辅助 $H(h)$ 精确实现 $D(h)$；单块、双块、successor 三类覆盖任意普通规范 split。递减归纳得到 $H(S,v)=D([k]\setminus S,v)$，普通边界和互补半格边界恢复完整前向格的全部第一次跨界推导。转置 budget 与 prefix 只使用引理 2 的可采纳证书。

**定理。** 对无向非负边权图和 $g\le16$ 的合法查询，Base、DirectedCutOnly 与 Enhanced 在实数算术模型下都返回精确最优权值，或正确报告 infeasible。

证明：真实上界给出 $U\ge\mathrm{OPT}$；引理 2–5 保证距离截断与所有证书剪枝不删除任何低于当前 $U$ 的完整推导；Base 由引理 6、Enhanced 由引理 7 保留至少一条最优规范推导。搜索结束后 $U\le\mathrm{OPT}$，两边合并得到相等。DirectedCutOnly 只增加引理 2 类型的证书并保留完整前向格。bounded/complete 距离、两种 witness 和前向/Adjoint 的差异分别由引理 3、引理 1 与引理 7 覆盖，所以配置替换不引入新的证明例外。浮点实现使用原始 `double` 次序闭合而不使用 epsilon，但不是任意精度实数认证；该边界放在实验设置或 artifact 限制中说明。

### 3.8 复杂度

令 $F=\sum_i\lvert K_i\rvert$， $\rho$ 为 facility 支撑顶点数， $s$ 为 certificate support 顶点数。正文给出主搜索的标准最坏界：

```math
O\left(3^g n+2^g(m+n)\log(n+m)\right)
```

时间和：

```math
O\left(2^g n+gn+m\right)
```

空间。这只是主状态格；同一段必须注明 tour 另付 $O(2^g g^3)$ 时间与 $O(2^g g^2)$ 空间，共同真实路径上界有保守 $O(g^4(m+n))$ 工作，DirectedCut 最坏支付 $O(g(m+n)\log(n+m))$，certificate support 支付 $O(s^3+2^g s^2+3^g s)$。完整总界和逐阶段表放附录，并链接详细方法文档，避免正文既声称简写又隐去 Enhanced 证书成本。

正文随后用实际稀疏 payload 解释性能。若 $M$ 是实际同根合并/转置候选数， $R$ 是实际检查的邻接项数，则主搜索更接近：

```math
O\left(M+R\log(n+m)\right).
```

因此实验必须同时报告时间、RSS 和实际主状态项数；状态数只用于解释状态域，不声称是跨算法等价的基本操作。

## 4. 正文伪代码、图与引理取舍

### 4.1 必须放正文

- 算法 1：总流程。它是 Base/Enhanced “一个算法、两种预声明配置”的最直接证据。
- 算法 2：稀疏 row。它连接 exact DP、future 剪枝与实现复杂度。
- 算法 3：Adjoint。它承载最难理解、也最可能被审稿人质疑的完备性。
- 图 1：D/A/H 层格和共同/替换关系。
- 主定理及 6 个压缩引理；其中 Adjoint 三分类要在正文给证明，不可全部推附录。

### 4.2 正文只写定义/引理，不单列伪代码

- DirectedCut：用容量可行性和证书闭环说明；具体 changed-arc/cone 枚举放附录。
- witness rent-or-buy：在算法 1 和公式中说明，无需第四段伪代码。
- A1 分级缓存：正文一句“数值等价的层级物化”，购买式放附录。
- certificate-support 增量 DP：实现优化与消融中说明，不占方法主线。

### 4.3 放附录或 artifact

- `GroupRow` cutoff 的逐接口合同和非连通 fallback。
- top-two locator、ranked-tail byte 布局、精确 mask-rent 表。
- staged future 的 epoch/bit packing、拒绝前沿和 `nextafter` 物理实现。
- potential cone 的两种边枚举、非内联机器码边界。
- support DP 脏超集增量证明与全部复杂度分项。
- 零权父指针反例、所有历史错误值和完整 CTest 清单。

## 5. 与实验章节的接口

方法正文只定义三种可运行配置：Base、DirectedCutOnly 和 Enhanced。性能主表报告 Base 与 Enhanced；DirectedCutOnly 只用于消融。实验必须据此验证：

1. 公共 A1 continuation 的作用：把 endpoint-floor 弱化为 farthest-only 时必须同时运行 Base/Enhanced，且保留精确 A1 递推，不能把共同操作伪装成 Enhanced 消融。
2. DirectedCut 的增量价值：Base → DirectedCutOnly。
3. Adjoint 的替换价值：DirectedCutOnly → Enhanced。
4. 端到端解释：时间、RSS、实际状态数和 timeout 方向一起报告。

消融不得使用图名或 $g$ 阈值选择开关，也不得用逐查询最快配置拼成新曲线。具体最小面板在 [`docs/EXPERIMENT_PLAN.md`](../docs/EXPERIMENT_PLAN.md) 冻结。

## 6. 论文表述红线

- 不把 Base 和 Enhanced 称作两个方法；它们是同一算法的配置链。
- 不声称 Enhanced 逐指令执行 Base 的全部工作；允许的是安全新增或同职责替换。
- 不把 A1 写成 Base-only、Enhanced-only、DirectedCut 替代物或经验 $g$ 分支。
- 不把 $H(S)$ 与 $A(S)$ 逐值比较；二者相同的是完整推导中的边界职责。
- 不把辅助 $H(h)$ 省略；没有它就没有被替换 $D(h)$ 的图闭包。
- 不把 successor 的 ordinary 侧限制为 branch；branch 可能已经位于 successor 一侧。
- 不声称状态数必然下降，也不把状态数当作跨算法同成本操作。
- 不声称当前二进制输出最优树边集；当前输出是精确权值和 feasibility。
- 不把 `double` 实现描述成任意精度认证。
- 不把 PrunedDP++-Safe 写成 2016 原作者 bit-for-bit 代码。

## 7. 初稿仍需作者决定的问题

1. ABHSS 的英文全称与论文标题如何统一；仓库当前没有权威扩写。
2. 主文是否把低组共同精确闭包放在方法总览，还是移到实现优化段。
3. 问题定义最终是否要求返回树边集；若要求，artifact freeze 前需实现回溯。
4. DirectedCutOnly 是否只出现在消融图中，正文总览可保留一行但不画第三条主曲线。
5. 复杂度正文采用标准主搜索界，完整保守界放附录；不能只报前者而隐去 Enhanced 证书成本。

## 8. 三轮复核记录

### 8.1 第一轮：叙事主线与伪代码一致性

发现并修正四点：补定义非锚组全集 $[k]$；把初始 DirectedCut 势/primal 构造放回预处理，把算法中后置步骤限定为 residual closure 的条件式购买；用集合差替代伪代码中的 `xor`；明确“公共 A1”指同一规则、代码路径与 row 合同，而不是在不同合法 incumbent/距离 realization 下强求 payload 相同。复核后，算法 1 的阶段顺序与 `PrepareProblem -> A1 -> ordinary -> forward/adjoint` 一致。

### 8.2 第二轮：正确性闭环

发现压缩主定理遗漏 bounded/complete 距离 realization 的合同证明，补入“未保存位置只作 cutoff 证书、精确消费者读无穷”的独立引理，并把它接入主定理。进一步补强 $g\le3$ 的分叉点恒等式、Base 的平衡第一次跨界论证、Adjoint 转置 prefix 的可采纳性，以及算法 3 对已发布 ordinary/H 域的限制。复核后，算法 1 的每类删枝或替换均可落到引理 1–7 的明确证明责任。

### 8.3 第三轮：篇幅、伪代码与渲染

发现复杂度段把明确计划放附录的保守总界再次完整抄入正文，且 facility 记号在生成初稿时发生断行。现已修正记号，正文只保留主搜索时间/空间和输出敏感界，同时用一段话显式列出 tour、真实路径上界、DirectedCut 与 support 的附加阶数；完整总界留给附录。保留 3 段、共 38 行核心伪代码，不再为 DirectedCut、witness scheduler 或 A1 缓存增加算法框。最终正文草案含 3 个 text 伪代码块；显示公式数量由删除总界前的 16 个降为 13 个，仍可在英文排版时把定义相邻式合并以控制在约 5 页。Markdown 验证作为本轮独立验收。
