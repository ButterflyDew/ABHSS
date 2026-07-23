# ABHSS Light/Heavy 公共内核重构审计

更新时间：2026-07-22。

## 1. 目的与边界

本次重构服务于 SIGMOD/VLDB 论文行文：只把 Light 与 Heavy 中语义相同、却因历史演化形成两份实现的操作合并；不会把两个版本做成运行时 oracle，也不会删除它们各自针对的资源目标。

最终论文可以先给出一次公共 `D`/前向 `A` 算法，再分别说明：Light 如何用有界距离和 early-A1 控制固定成本，Heavy 如何支付 directed-cut 后把完整前向格改写为低层 `A` 与高层伴随 `H`。两个入口仍是两条独立曲线。

## 2. 差异审计与处理结论

| 操作 | 重构前 | 最终处理 | 论文含义 |
| --- | --- | --- | --- |
| 查询空集、`g<=16`、可行性检查 | 三个入口重复 | `ResolveQueryPrelude` 共用 | 与算法变体无关 |
| prepare/ordinary 计时和 probe 字段 | Light、Heavy 各一套 | 公共 pipeline 与零开销正式版 diagnostics | 两版采用同一计时边界 |
| 模式表达 | `Problem.light` 布尔值散落 | `SolverVariant` 只暴露“有界距离”“directed-cut”两项能力 | 避免把历史分支误写成算法贡献 |
| 掩码最低位 | 跨文件逐位循环 | 公共头文件内联 CPU 位扫描 | 纯公共原语优化 |
| ordinary future | 同一证书会在便宜预筛和 `Bound()` 中重复求值 | 每 row、每顶点首次通过时按固定顺序合成并缓存最大下界 | 证书集合不同，缓存与队列相同 |
| witness rent | Light 在每 row 重复累计“本层累计量”；Heavy 每层累计一次 | 两版均按当前 row 新增 pop/relax 计 rent，并在 row 边界检查 buy | 修正记账错误，同时保留层内及时收紧 |
| witness 初次求值 | 只有 Light 在 ordinary 前执行 | 保留 | Light 把根路径共享转为上界；Heavy 已有 primal/facility 上界，不能无 rent 增加固定成本 |
| 前向 `A` 的 seed、闭包、star 上界与 `A+D+D` 完成 | Light 完整格与 Heavy 低层格各约一份实现 | 唯一 `BuildForwardAnchoredRows` | 递推完全相同，只传计划参数 |
| `A(0)` 的 `g=2/3` 完成 | Light 在完整格内，Heavy 在入口外单独实现 | 公共前向内核统一处理 | 不再存在边界特例分叉 |
| 完成时的 driver 选择 | Light 可选有界 singleton；Heavy 只尝试普通 row | 同一函数按实际可枚举数量选择；dense singleton 自然不会胜过根列表 | 统一确定性操作数规则 |
| 前向阶段计时与逐层 probe | 三个入口手工包围同一内核 | `RunForwardAnchoredStage` 共用 | 起止边界和字段完全一致 |
| 有界组距离 membership | 仅 Light 必需 | 保留在公共读取器内的存储能力分支 | 属于 Light 的核心空间目标 |
| early-A1、缺失值证书 | 仅 Light | 保留 | Light 核心机制 |
| directed-cut、primal/facility、补集转置、高层 `H` | 仅 Heavy | 保留 | Heavy 核心机制 |

前向计划只有三个算法相关字段：生成到哪一层、最后一层是否保留、是否带入已完成的 early-A1 row。Light 使用 `last=h-1, retain=false`；Heavy 使用 `last=floor((h-1)/2), retain=true`。Heavy-Forward 消融使用 Heavy 的 prepare/ordinary，再把计划改为 Light 的完整前缀，不维护第四套递推。

## 3. 最终代码边界

- `src/abhss/pipeline.*`：公共前置检查和阶段编排。
- `src/abhss/diagnostics.h`：统一 probe 格式；未定义 `GST_ENABLE_PROBE_DIAGNOSTICS` 时编译为空路径。
- `src/abhss/core.*`：early-A1 证书、公共 ordinary `D`、下界缓存和 witness 调度。
- `src/abhss/forward.*`：唯一的前向 `A`、图闭包和完成结算。
- `src/abhss/light.cpp`：Light 与 Heavy-Forward 的薄入口。
- `src/abhss/heavy.cpp`：补集终端与高层伴随等 Heavy 专属逻辑。
- `src/abhss/preprocess.cpp`：公共预处理骨架；有界距离与 directed-cut 通过显式能力进入。

正式 Release 对 PrunedDP、DPBF、Light、Heavy 及其 probe/消融目标统一启用同一 IPO 规则。该规则不是 ABHSS 专属优化；正式实验 metadata 仍须记录编译器、CMake、build type 和实际 flags。

审计也包含“看起来可以共用、但热路径不应强并”的候选。Heavy 高层 `H` 的 prefix 计算需要同一次遍历里复用补集势和当前 mask；改为直接调用普通 `FutureBound` 的 WRP 11 例单遍工程门慢约 3%，因此保留为高层伴随内部的融合计算。这不是另一套 ordinary/forward 语义，而是 Heavy 核心高层布局的数据复用。

## 4. 正确性门

1. 保留零权 witness 回归，覆盖此前等距重挂形成父指针环的迁移错误。
2. 新增确定性随机回归：120 张 `n=4..9` 的连通小图，含零权边、重叠组和多终端；用独立的全子集 Dreyfus-Wagner DP 求真值，Light、Heavy、Heavy-Forward 必须逐例匹配。
3. 11 个 SteinLib WRP `g=11..16` 实例上，Light 与 Heavy 均逐例匹配已知最优值。
4. Toronto 与 Musae 的工程性能面板同时逐查询比较重构前后目标值；已完成的双方全部一致。

CTest 目标为 `abhss_zero_weight_witness` 与 `abhss_variant_exactness`。任一失败都应阻止正式实验。

## 5. 非退化工程门

下表只比较同一工作区、同一机器、Release、单查询线程下冻结的重构前二进制与最终二进制。主程序报告的 query time 不含图/查询加载。它用于拒绝代码重构退化，不是论文正式性能结果，也不能代替完整实验矩阵。

| 面板 | 重构前 | 最终 | 结论 |
| --- | ---: | ---: | --- |
| WRP 11 例，Light，一遍总 query time | 27.904 s | 17.682 s | 目标值全匹配，约快 37% |
| WRP 11 例，Heavy，两遍共 22 次 | 11.812 s | 9.962 s | 目标值全匹配，约快 16% |
| Musae `g=8` 前 5 条，Light，三遍平均 | 2.394 s | 2.267 s | 目标值全匹配，约快 5% |

Musae `g=8` 的 probe 还核验了状态序列：最终实现与重构前在查询 1 上均保留 91 张 ordinary row、3,622 个标量；统一调度器在第二层内及时买入 witness 上界，因此没有按层末检查造成的 12,192 标量膨胀。这个检查说明速度门不是由删除状态或放宽精确性获得。

## 6. 投稿时的表述限制

- 可以写“Light 与 Heavy 共用 ordinary DP 和 forward-anchor kernel，区别是证书/距离能力与高层求值策略”。
- 不应把位扫描、IPO、Dijkstra、row 交集或 rent-or-buy 单独宣称为贡献。
- 不应写 Heavy 是 Light 的严格超集；Heavy 不运行 early-A1，也不生成完整前向高层。
- 不应把两个入口按 query 取最快值组成 `ABHSS-best`。
- 本文档的重构计时只证明工程实现未退化，正式论文仍只使用实验矩阵、统一 deadline 和正式统计脚本产生的结果。
