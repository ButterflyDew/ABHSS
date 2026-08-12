# Branch bitmap 置位枚举：负向探针

## 动机

ordinary row 已保存规范 branch bitmap。旧的“枚举 branch 后二分”路径用 `branch_count × log(row size)` 估计工作，却逐项扫描整张 branch row 才找到置位项。候选直接枚举 64-bit word 的置位项；自适应版按 `bitmap words + branch_count` 与完整 row 长度的结构工作量选择两种枚举，不读取图名、组数、时间或边权类型，也不改变状态和答案。

## 正确性

候选五个 CTest 全部通过。所有 P1 样本与 Orkut 探针的答案、状态数和逐 row primitive work 均与对照一致；候选只改变同一 branch 候选集合的物理遍历。

## P1 固定门

YouTube 作者 `g=7` 前 20 条、Base、同一 CPU 11 严格串行：

| 版本 | 20 条求解时间和 | 相对对照 |
|---|---:|---:|
| fresh control | 126.561655 秒 | 1.0000× |
| 强制 bitmap 置位枚举 | 128.263211 秒 | 1.0134× |
| 结构工作量自适应 | 127.787061 秒 | 1.0097× |

自适应选择消除了一部分退化，但仍没有形成 P1 非退化证据，因此未继续运行无必要的 Enhanced P1 小门。

## Orkut 高组门

在 `GPU4GST_Orkut`、`g=15`、第 5 条询问上，certificate-support 正向版本和自适应候选均固定 CPU 11、时限 1200 秒。对照完成约 368 个 `ordinary_row` 诊断事件，候选完成 366 个；best 均为 32，状态与 primitive work 轨迹一致。候选没有提高高组吞吐。

## 结论

branch row 全扫并非目标 q5 的主导成本，bitmap 置位枚举无法解决状态爆炸，并带来约 1% 的 P1 风险。该方向淘汰；源码宏、隔离 build 和原始输出删除，只保留本文结论。
