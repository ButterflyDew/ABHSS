# 原图设施度量上界负面探针（2026-08-10）

## 1. 候选

延迟 residual closure 恢复新的 primal 树后，已有设施 subset DP 原本只允许 residual 零弧支撑与 primal 树边。该候选保留完全相同的设施点、组到设施距离和 subset DP，只把设施间距离替换为原图最短路距离。每个候选仍可展开成原图真实路径的连通并集，因此是正确的可行上界。

该变化不读取图名、组数阈值、边权类型或运行时间，但需要从每个设施点在原图上执行一次截断 Dijkstra，代价明显高于 residual-support 度量。

## 2. Orkut $g=15$ q5

使用 additive buy 与逆序 residual completion：

- 初始预处理 173.468 秒，incumbent 为 35。
- closure 在 ordinary 23 行、26,515,090 个累计标量时购买，paid rent 为 6,245,118,154 primitive work。
- residual completion 与 primal 恢复耗时 151.516 秒，incumbent 仍为 35。
- 原图设施度量和 subset DP 额外耗时 38.696 秒，incumbent 仍为 35。
- 一次性 refilter 将当前 ordinary payload 从 26,515,090 降至 10,508,369，但这是新 dual 下界的作用，不是原图设施上界收益。
- 整次购买耗时 203.038 秒；设施上界没有带来任何状态剪枝。

## 3. 结论

该候选在目标劣势查询上产生确定的 38.7 秒额外成本，却没有把上界从 35 收紧，因此拒绝。最终实现恢复 residual-support 设施度量；原图域枚举、临时二进制和半截输出均不保留。

closure 后的 primal 设施集与初始 primal 设施集可能不同，因此又做了一次只到预处理结束的隔离探针：初始设施的 residual-support 主线预处理约为 173.5 秒，改为原图度量后为 210.932 秒，incumbent 仍为 35。第二棵设施树同样没有收益，并新增约 37.5 秒固定预处理。

两次独立设施集均判负，因此该方向彻底结束，无需再消耗 P1 门禁。后续优化应使用已经生成的 exact partial states 构造新的可行组合，而不是继续扩大同一设施度量域。
