# 辅助半层 Adjoint 正确性事故与修复审计（2026-08-15）

> 本文记录一次已经定位并修复的精确性事故。提交 `589894be3774ff6658e1120f9a9e899a53377ab1` 之后、采用旧 H 起点得到的 P1 与 Orkut `g=15` 长测只可作为历史性能探针，不能作为当前精确结果或最终门禁。当前方法口径以 [方法说明](../METHOD.md) 为准；修复版 q10 已以 11,178.235 秒得到精确值 54，但超过正式 10,000 秒 TL，完整 P1 与清理后 q1--q10 仍须重跑。

> **二次勘误（2026-08-17）。** 本文加入辅助 $H(h)$ 的第一轮修复仍错误地删除了全部较低直接 terminal，并让 successor 只读规范 branch；Musae q162/q295 证明该实现仍会高报。现行代码补齐必要较低双块终端、全值 successor 与互补 H 完成。本文第 6 节的相反陈述已经失效，详见 [Adjoint split 完备性审计](ADJOINT_SPLIT_COMPLETENESS_AUDIT_20260817.md)。

> **最终版状态（2026-08-18）。** 在 split 完备性修复之后，冻结求解器源码提交 `12d6adb` 采用最小共同 A1 与 certificate-support 增量求值，以自身二进制在 9,250.911 秒完成 Orkut g15 q10，精确权值仍为 54。q10 单条门已通过；冻结的 q1--q10 十条全门与 13 图 P1 仍待重跑。

## 1. 事故表现与定位

旧实现完整生成 ordinary 到最高逻辑层

```math
q=\left\lfloor\frac{g}{2}\right\rfloor-1,
```

却让反向 H 直接从 `q` 启动，并试图用一至三块同根 ordinary terminal 补偿未物化的半层

```math
h=\left\lfloor\frac{g}{2}\right\rfloor.
```

这只枚举了若干同根 split 标量，没有执行被省略 `D(h)` 的完整图最短路闭包。它因此不是 `D(h)` 的等价 realization，也不能作为后续 H 递推的精确基例。

全量 P1 的逐查询权值审计首先发现 LiveJournal 与 Orkut 上的高报。独立 Base、DirectedCutOnly 与 Enhanced 对照把问题隔离到 AdjointCompletion：例如 Orkut 作者查询 g7 q7 的 Base 和 DirectedCutOnly 都返回 29，旧 Enhanced 返回 30。进一步比较提交边界确认旧前向版本仍正确，而引入从逻辑层直接启动 H 的版本开始出现差异。

已知生产反例共九条：

| 图与查询 | 旧 Enhanced | 精确值 / 当前 Enhanced |
| --- | ---: | ---: |
| LiveJournal g7 q44 | 23 | 22 |
| LiveJournal g7 q46 | 61 | 60 |
| Orkut g7 q7 | 30 | 29 |
| Orkut g7 q16 | 32 | 31 |
| Orkut g7 q18 | 25 | 23 |
| Orkut g7 q22 | 30 | 29 |
| Orkut g7 q23 | 37 | 36 |
| Orkut g7 q27 | 31 | 30 |
| Orkut g7 q28 | 32 | 31 |

修复版对九条查询的直接结果保存在 `results/probes/final_auxiliary_known_mismatches`。该目录只作服务器本地审计证据，不作为仓库内分发数据。

已经启动但只完成一部分的错误 P1 campaign 已从 `results/paper_runs` 移到 `results/probes/invalid_exactness_p1_2fc72775176b_20260815`，避免正式汇总器把其中的 Enhanced 记录混入当前结果。

## 2. 最小固定反例

回归测试加入一个 12 点、7 组的确定性图。独立全子集 DP 的精确值为 5.75；旧 H 起点返回 6.25。该反例含零权边与重叠组，但错误不来自浮点容差或零权路径恢复，而是缺失 `D(h)` 的图闭包。当前三个合法配置都必须返回 5.75。

这个 fixture 比随机对拍更适合长期门禁：它固定触发被省略半层，能够在相关代码发生任何重构时直接锁住同一证明责任。

## 3. 当前修复

令锚组之外有 `k=g-1` 个组。Enhanced 仍与 Base 调用同一个 ordinary 构造器，完整生成所有 `|S|<=q` 的 `D(S)`。有非空 H 后缀时，物理 H 区间扩展为

```math
H(h),H(h-1),\ldots,H(\ell+1),
```

其中 `H(h)` 是辅助层，不是新的前向 A 职责。

- 若 `g=2h`，则 `k=2h-1`，辅助目标 `S` 的补集大小为 `h-1=q`。代码直接转置已经 ready 的 $D([k]\setminus S)$。
- 若 `g=2h+1`，则 `k=2h`，补集大小为 `h`。代码枚举 ordinary 递推中覆盖该补集的全部双块 split seed，再执行与 ordinary 相同的图闭包。

由此在当前可改善锥体内得到

```math
H(S,v)=D([k]\setminus S,v),
\qquad |S|=h.
```

随后从大到小构造 H。若 successor 已满足上述等式，则

```math
H(S\cup B,v)+D(B,v)
```

正好对应 ordinary 在补集侧把规范 branch `B` 加回去；执行同一图闭包后得到 $D([k]\setminus S)$。按层归纳，该等式对所有 $\ell<|S|\le h$ 成立。边界的

```math
A(L,v)+D(S\setminus L,v)+H(S,v)
```

因此覆盖锚组、低层组、边界块与全部外侧组，且每个有限项都可展开为原图真实子树。

## 4. 本轮严格支配减空

修复后只在辅助 `H(h)` 播种直接 terminal。

1. 较低 `H(S)` 的直接 terminal 已由精确辅助层、successor 转移和图闭包生成同一 $D([k]\setminus S)$；重复播种不会产生更优精确值，因而删除。
2. 偶数 `g` 时，直接 `D(Q,v)` 已 ready。ordinary 的精确递推保证它不大于任意同根 `D(B1,v)+D(B2,v)`，所以完全跳过 pair 枚举。
3. 奇数 `g` 缺少直接 `D(h)` 时才枚举双块。排序 pair 与互补 submask 只是同一候选集合的两种枚举顺序；代码用可预估的循环项数选择常数较小者，不读取图名、查询编号、时间、状态量或经验参数。

还测试过只枚举某一规范 branch 的更窄实现。它通过精确性门且在 Orkut g15 q3 仅少十余个状态，但两轮交换绑核后总时间约持平或略慢，并显著增加证明与 review 负担，故没有接入。这里遵循“逻辑可删还不够，热路径必须物理不退化”的既定准则。

## 5. 已通过与仍待完成的证据

当前修复已经具备：

- 12 点固定反例返回 5.75；
- 5,000 个确定性随机连通实例、500 个正权互异单终端实例及 160 个 g7--g16 omitted-half 实例逐例匹配独立全子集 DP；
- 上表九条生产反例恢复精确权值；
- 五项 CTest 全部通过。

下列旧证据全部降级为历史性能证据：旧二进制 SHA-256 `a957bdcecafc486575cb7d78779dcbeda687eb0ea7283a0fc1104089dce83b86`、旧 Enhanced 状态总数 28,064、旧 Orkut g15 q1--q10 状态与时间、以及旧 P1 Enhanced 聚合。它们验证不了当前修复版。

当前版本仍须直接完成两项硬门：

1. P1 的 13 图全量按图聚合，最快 ABHSS 配置不劣于 PrunedDP++，并逐查询核对精确权值；
2. Orkut g15 q1--q10 每条当前结果都严格小于 10,000 秒。旧 q10 的 9,934.603 秒只有历史风险估计意义，不能替代复跑。

在两项门禁完成前，任何文档都不得把旧 q10 或旧 P1 写成“当前最终版本已通过”。
