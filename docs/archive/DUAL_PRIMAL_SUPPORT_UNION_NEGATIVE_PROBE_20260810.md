# Dual primal 支撑并集负面探针（2026-08-10）

## 候选

延迟 residual closure 会恢复一棵新的真实 primal 树，而初始 directed-cut 已经恢复过另一棵树。候选不覆盖旧 bitmap，而是把两棵树的原图边取并集，再让现有 BuildPrimalFacilityUpper 在联合设施点和允许边上执行完全相同的 subset DP。候选空间严格包含单支撑版本；所有路径仍按原图边权计价，因此只可能安全收紧上界。

该操作只放在已经购买 closure 的 DirectedCut 配置中，不读取图名、组数区间、边权类型或运行秒数。为避免等待完整长查询，诊断构建在公共 A1 后立即购买一次，只比较购买结果。

## Orkut g15 q5 结果

输入为 experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt 第 5 条，Enhanced。

| 指标 | 单支撑 closure | 初始树与 closure 树并集 |
|---|---:|---:|
| 购买后上界 | 35 | 35 |
| 购买耗时 | 177.071 s | 177.558 s |
| closure 后首张 ordinary row 标量 | 394,232 | 394,232 |

两版的首张 row 逐项状态规模相同。联合支撑没有产生新的有效 facility 组合，也没有改变 lower certificate；继续运行不能凭该操作获得状态收益。

## 结论与清理

候选数学安全但在目标最困难询问上零上界、零状态收益，因此不进入正式实现。临时 eager 环境变量、支撑并集代码、对应测试断言和原始结果目录均已删除；仓库只保留本结论，防止以后重复试验。
