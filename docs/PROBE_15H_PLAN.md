# SIGMOD/VLDB 投稿前 15 小时全数据探针

## 1. 目的和边界

探针只判断正式 A/B/C 结论是否大概率成立，并尽早发现 “PrunedDP++-Safe 很好而 ABHSS 较差” 的区域。探针使用短 timeout、单次固定抽样和诊断计数，不产生论文最终性能数字，也不允许根据探针结果更换正式查询。

三个中心方法是 ABHSS-Light、ABHSS-Heavy 和 PrunedDP++-Safe。探针专用二进制打开诊断输出，但算法语义与正式版本一致。正式 timeout 仍为 10,000 秒；探针 breadth/depth 默认分别为 180/600 秒，整项 wall budget 为 15 小时。

## 2. 冻结抽样规则

- 在任何 solver 运行前固定 `seed=20260722`。
- 每个预选单元按 `SHA-256(seed|suite|case_id|query_index)` 排序，先取第 1 顺位；depth 取第 2 顺位。
- Breadth、fixed-depth 与 outcome-adaptive follow-up 分开标记。
- adaptive 只用于原因诊断，不进入无偏胜率或速度统计。
- 旧作者数据、合成 MovieLens 控制、GPU/异构和近似算法不进入探针。

## 3. Breadth wave

| 家族 | 固定单元 | 数量 | 目的 |
|---|---|---:|---|
| A DBLP/IMDb | `f=400,g={4,8,12,16}`；`g=10,f={100,3200}` | 12 | 小/中/大 `g` 与扩展 `f` 两端 |
| B 八图 | 全八图 `g={4,12}` | 16 | 跨来源低/中高 `g` |
| B 代表图 | Wikipedia、Orkut、MovieLens、Toronto 的 `g=16` | 4 | 高 `g` 完成率风险 |
| C DBpedia | `g={1,4,8,10,11,12}` | 6 | 自然低/中/高 `g` |
| C LinkedMDB | `g={1,4,8,9,12}` | 5 | 第二自然来源与稀有高 `g` |
| 固定消融 | IMDb A、Wikipedia B、LinkedMDB C 各一格 | 3 | 快速检查机制方向 |

各单元 breadth 只运行哈希顺位第 1 条。四个 bucket 轮转执行，避免预算先被单一大图耗尽。

## 4. Fixed depth 和剩余预算

Breadth 完成后，固定 depth 顺序覆盖：

- A 的 DBLP/IMDb `g=16,f=400`；
- B 的八张图全部 `g=16`；
- C 的 DBpedia `g={10,12}` 和 LinkedMDB `g={9,12}`。

按当前冻结查询，前 13 个单元能够取得哈希顺位第 2 条，因此固定 depth 实际为 13 条。LinkedMDB 的自然 `g=12` 只有 1 条源查询映射，控制器会保留其 breadth 记录并跳过不存在的第 2 条，而不会复制同一查询伪造重复样本。

仍有预算时，按固定哈希顺序遍历所有 A/B/C 正式单元并补第 1/2/3 顺位。只有在 Safe 完成而 ABHSS timeout，或 Safe 至少比最快 ABHSS 快 2 倍时，才为该单元增加最多两条 adaptive 诊断查询。

## 5. 诊断输出

每次探针记录 wall/solver time、峰值 RSS、目标值、完成状态、扩展状态数、queue 操作、lower/upper-bound 命中、witness/anchor 命中、预处理分项和图/查询特征。短时运行也必须先通过目标一致性检查；错误答案不进入性能判断。

## 6. 当 baseline 更好时的双层分析

方法层检查：共同预处理是否重复；Light/Heavy 特有步骤是否在该格只有成本没有剪枝；下界是否松；上界是否晚；状态表或 priority queue 是否膨胀；零权边是否降低 anchor/witness 有效性；Heavy 的额外内存是否导致 cache 或分页损失。

数据层检查：`n/m`、平均度和连通分量；组大小、重叠和共同候选；组间最短距离；边权零值/重复/离散程度；标签频率与 hub 集中；来源查询映射后的 `g/f` 相关性。A 可隔离 `g/f`，B 检查跨图结构，C 检查自然语言映射偏差。

分析必须保留不利格。允许据此修复实现缺陷或补充预先可解释的算法机制，但修复后正式矩阵和所有受影响方法统一重跑；不得删图、换询问或只重跑较慢方法。

## 7. 投稿 gate

探针支持进入全量实验的最低条件：所有共同完成实例目标一致；小 `g` 的主要单元没有系统性大幅退化；至少一个真实大图家族在较大 `g` 显示明确完成率或数量级信号；不利单元能够由可复现的算法计数和数据特征解释。若这些条件不满足，应先改方法或收缩论文 claim，而不是启动昂贵的全矩阵计时。

## 8. 运行

```powershell
python tools/experiments/run_probe_15h.py --hours 15
```

静态计划在 `experiments/probe_15h_plan.json`，实际哈希选中的查询、执行顺序和 adaptive 标记写入探针目录的 `probe_manifest.json`。
