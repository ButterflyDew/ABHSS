# 上包络筛选 A1 tour 负结果（2026-08-09）

## 动机与候选

此前无条件在每个 A1 候选上计算 all-endpoint floor 或完整 matched tour，会让 Musae/Twitch Base 明显退化。本轮先用常数时间上包络判断强证书是否可能达到当前 cutoff；只有可能时才支付强证书。筛选不是经验阈值，也不读取图名、运行时间或状态规模。

对 all-endpoint floor，令 $F(v,R)$ 为 remaining groups 的最远距离，令 $P_{\max}(R)$ 为终点自由路径 floor 的最大值，则：

```math
L_{\mathrm{all}}(v,R)
\le
\frac{F(v,R)+P_{\max}(R)}{2}.
```

对完整 matched tour，固定任一起点并选择路径长度最小的终点，可得：

```math
L_{\mathrm{tour}}(v,R)
\le
F(v,R)+\frac{P_{\max}(R)}{2}.
```

若 `partial + upper < incumbent`，真实强证书必然也不能拒绝当前候选，因此只用原 endpoint-floor；否则计算强证书。A1 cone 外 fallback 始终读取完整强证书，所以没有削弱其下界合同。两个候选均通过 5/5 CTest、随机配置精确性和零权边回归。

## Orkut `g=16,q4` Enhanced gate

| 版本 | 时间（秒） | 查询峰值（MiB） | states | 最优值 |
|---|---:|---:|---:|---:|
| 主版本 | 1,194.776 | 2,722.359 | 26,517,767 | 20 |
| screened all-endpoint | 1,191.908 | 2,730.559 | 26,517,767 | 20 |
| screened matched-tour | 1,183.157 | 2,722.559 | 26,466,994 | 20 |

all-endpoint 没有减少任何状态，并为平坦 endpoint 表增加约 8 MiB。matched-tour 减少 50,773 个状态，仅约 0.19%；单次并发运行的约 1% 时间差不能视为稳定加速，也远不足以改变 g15/g16 的数量级爆炸。

## 结论与清理

严格上包络确实避免了无条件强证书的灾难性固定成本，但目标实例上的新增剪枝仍过弱。继续做负对照面板只会证明一个不具备论文效应量的机制没有严重退化，不能让它成为有效贡献。因此两个候选均不合入主线；源码、构建和原始输出删除，只保留本结论。
