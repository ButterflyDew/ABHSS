# Rooted endpoint-floor 负结果（2026-08-09）

## 候选

现有 A1 endpoint-floor 固定一个 remaining-group 起点 $a$，使用从 $a$ 出发、终点自由且访问全部 remaining groups 的最短组度量路径下界 $P_a(R)$。候选再加入根到 remaining groups 的最近距离：

```math
L_{\mathrm{rooted}}(v,R)=
\frac{d_a(v)+P_a(R)+\min_{i\in R}d_i(v)}{2}.
```

这是 PrunedDP++ 第二类 tour 下界的常数时间松弛。树倍增与 shortcut 证明给出可采纳性；两个顶点距离项各乘二分之一，因此该函数仍为 1-Lipschitz，可同时用于 A1 cone 和 cone 外 fallback。方案不依赖整数权、图名、组数阈值或经验参数。

隔离实现比较了两种物理 realization：朴素扫描 remaining groups 求最近距离；以及为每个顶点保存前三近组的 12-bit 排名。A1 continuation 恰好只排除永久锚组和当前 singleton 组，故前三近组中必有一个仍在 remaining，top-three 与朴素扫描逐状态严格等值。两者均通过 5/5 CTest、随机配置精确性和零权边回归。

## Orkut 高组数 gate

数据为 Orkut `g=16,q4` Enhanced，主版本与候选结果如下：

| 版本 | 时间（秒） | 查询峰值（MiB） | states | 最优值 |
|---|---:|---:|---:|---:|
| 主版本 | 1,194.776 | 2,722.359 | 26,517,767 | 20 |
| rooted endpoint-floor | 1,189.288 | 2,722.559 | 26,517,056 | 20 |

候选只减少 711 个状态，约为 0.0027%；约 0.5% 的时间差属于单次并发运行波动，空间没有改善。top-three 与朴素版状态严格相同，因此在朴素 gate 判负后停止未完成的 top-three 运行。

## ordinary 扩展为何不运行完整 gate

完整 ordinary future 已计算 `TourLowerBound::At`。对固定起点 $a$：

```math
\min_b\{P_{ab}+d_b(v)\}
\ge
\min_b P_{ab}+\min_i d_i(v).
```

因此 rooted endpoint-floor 始终不超过现有完整 tour，下放到 ordinary/A/H 不可能增加任何剪枝，只会多做最近组查询。该扩展由严格支配关系直接淘汰，不需要用长实验重复证明零状态收益。

## 结论与清理

该证书数学安全、实现轻量，但在目标高组数实例上几乎完全被现有 farthest/tour/dual 组合覆盖，不能作为论文贡献，也不能解决状态爆炸。候选源码、构建和原始输出删除，只保留本结论。
