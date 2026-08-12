# 三元路径真实树支撑复用负面探针（2026-08-10）

## 候选

共同的三元一步前瞻路径生长上界已经构造并计价一棵真实可行树，但正式接口只返回权值。候选临时把最佳树的稀疏 edge ID 一并返回；若 Enhanced 后续购买 residual closure，则把初始 dual primal、三元真实树和 closure primal 的边并集交给现有 facility subset DP。

候选没有生成新路径，也没有读取图名、组数区间、边权类型或运行时间。所有允许边和设施连接仍按原图权值计价，因而所得值是安全上界。探针在公共 A1 后立即购买，只判断联合支撑是否改善目标 q5，不等待完整查询。

## Orkut g15 q5 结果

输入为 experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt 第 5 条，Enhanced。

| 指标 | 单 closure primal 支撑 | 再并入三元真实树 |
|---|---:|---:|
| 购买后上界 | 35 | 35 |
| 购买耗时 | 177.558 s | 179.909 s |
| closure 后首张 ordinary row 标量 | 394,232 | 394,232 |

三元树增加了 facility 工作，却没有增加有效组合；首 row 的 lower certificate 本来就相同，因此状态也逐项不变。

## 结论与清理

候选数学安全但零上界、零状态收益，不进入正式实现。临时返回结构、支撑注入函数、eager 环境变量和原始输出目录均已删除，只保留本文结论。
