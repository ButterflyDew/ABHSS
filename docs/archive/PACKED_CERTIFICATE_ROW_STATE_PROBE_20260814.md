# ordinary 证书行状态合并探针（2026-08-14）

## 1. 动机与候选

Enhanced 的 ordinary 热路径按 `(mask, vertex)` 复用 staged future。原实现为每个顶点并行维护 32-bit row stamp 与 8-bit stage；诊断构建还另有 32-bit exact-dual stamp。Orkut g=15 q10 的 1,200 秒诊断窗口中，前 163 张共同 ordinary row 已发生 2,855,058,390 次证书入口调用，其中 2,600,519,242 次直接被缓存证书拒绝。这里的主要物理成本不是重新计算数学下界，而是反复随机读取多个并行元数据数组。

候选把 DirectedCut 配置的 row epoch、stage 与 exact-dual 诊断标志合并到一个 32-bit 无符号字：高位保存 row epoch，低 3 bit 保存 stage，下一 bit 保存 exact 标志。`double` 下界值仍保存在原来的 `bound_cache` 中；证书值、求和顺序、比较式、队列 key 与状态定义均不改变。Base 不使用 staged dual，继续只分配原有的单一 32-bit stamp，不进入打包状态机。

## 2. 正确性不变量

每张 ordinary row 开始时递增 epoch。仓库限制 `g <= 16`，固定锚组后 ordinary mask 数严格小于 `2^15`，因此 epoch 左移 4 bit 后远未达到 32-bit 上限。低 4 bit 可安全留给元数据，不存在一条查询内的回绕。

stage 的语义保持为“已经完成到哪个候选无关证书阶段”，不是“上一标签是否被拒绝”。若 closure interval 的安全下端拒绝当前较大标签，但没有计算 exact fallback，代码只缓存可采纳下端并保持 stage 0；以后更小标签若不能被该下端拒绝，仍会继续 dual 判定。只有以下两种情况可以结束 dual 阶段：

- 已得到逐组 `double` 精确和；
- interval 上端证明当前标签通过，而后续到达同一顶点的标签只会更小，因此也必然通过 dual。

这与合并前状态机逐分支一致。候选不压缩浮点精度，不改变逐组求和顺序，也不把 interval 下端冒充 exact 值。

## 3. 空间账

设图有 `n` 个顶点。正式构建中：

- Base：合并前后均为一个 32-bit stamp，即约 `4(n+1)` bytes；
- DirectedCut 配置：合并前为 32-bit stamp 加 8-bit stage，即约 `5(n+1)` bytes；合并后为一个 32-bit state，即约 `4(n+1)` bytes；
- 诊断构建：原来的额外 32-bit exact stamp 被 state 中的一 bit 取代。

因此候选不会增加 Base 空间，并为 DirectedCut 配置确定性减少约一 byte/vertex 的常驻 ordinary 证书元数据。收益来自更少的随机元数据流，而不是少算状态或改变算法精度。

## 4. 正确性门

正式源码重建后，五项 CTest 全部通过。配置精确回归覆盖确定性随机实例、正权唯一终端压力实例与省略半格转置实例；零权 witness 与 `(mask, vertex)` 状态计数回归也通过。候选与合并前构建在所有共同诊断 row 上的 `best`、状态数、primitive work 及各证书调用计数逐项相同。

## 5. Orkut q10 固定窗口 A/B

两轮试验均使用 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt` 的逻辑 q10，固定总墙钟 1,200 秒，并把进程与内存都绑定到 NUMA node 1。每轮同时运行候选与冻结对照，第二轮交换 CPU5/CPU7；共同预处理、A1、residual closure、上界 59 与 closure 后的 1,169,801 个 retained ordinary 标量一致。

| 轮次 | 候选 CPU | 对照 CPU | 候选完成 ordinary rows | 对照完成 ordinary rows | 候选提升 |
|---|---:|---:|---:|---:|---:|
| 1 | 7 | 5 | 179 | 163 | 9.82% |
| 2 | 5 | 7 | 180 | 165 | 9.09% |

两轮方向一致，且共同 row 的算法诊断逐项相同，因此候选通过进入长测的固定窗口门。该结果只证明相同轨迹的物理吞吐提升，不能代替完整询问结果。

## 6. 严格门结果

正确的严格门使用追加 P2 面板 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt`，而不是图目录下含 300 条查询的 `query_g15.txt`。逻辑 q10 的统计为 `g=15, mean_f=309.8, min_f=56, max_f=793`。packed 候选曾在 CPU5、NUMA node 1 本地内存上运行；外层 10,070 秒只补偿约 30 秒图加载，验收要求结果行中的单询问 `query_seconds < 10000`。该进程最终达到外层门限，结果文件 `results/probes/orkut_g15_packed_strict_cross_q10_cpu5/.../weights.txt` 只有 run header，没有查询结果行。因此 packed state 是通过 P1 门的一般局部性优化，但它没有单独解决 q10，不能把固定窗口吞吐提升写成完整询问结果。

## 7. P1 不退化门

为避免与 Orkut q10 争用内存带宽，q10 固定在 CPU5/NUMA node 1，Youtube P1 固定在 CPU4/NUMA node 0。前 60 条先在同一 CPU 和内存节点上顺序运行 packed 候选与合并前冻结对照：

| 配置 | packed 候选时间 | 冻结对照时间 | 时间变化 | 候选峰值 | 对照峰值 | 状态一致性 |
|---|---:|---:|---:|---:|---:|---|
| Enhanced | 420.904 s | 427.824 s | -1.62% | 213.688 MiB | 217.719 MiB | 15,389,926，逐条一致 |
| Base | 380.925 s | 386.378 s | -1.41% | 122.008 MiB | 119.918 MiB | 16,793,059，逐条一致 |

Base 不进入 packed state 路径；约 2.09 MiB 的单次采样峰值反向波动没有对应常驻数组或状态变化。配对时间没有退化，不能把该采样差异解释为算法空间增加。

随后只补跑第 61--300 条，与前缀合并为最终候选全量结果。Enhanced 共 300 条、2,115.979 秒、峰值 218.590 MiB、67,570,613 个状态；历史同轨迹全量为 2,119.583 秒、峰值 217.859 MiB、状态相同。Base 共 300 条、1,876.588 秒、峰值 128.586 MiB、74,359,677 个状态；历史同阶段为 1,873.190 秒、峰值 128.035 MiB、状态相同，时间差约 +0.18%。两种配置的 300 条权值与状态均逐条完全一致。

因此 P1 的答案、状态和运行时间不退化门通过；不足 1 MiB 的全量峰值差异保守记为 1 ms RSS 采样与分配器瞬时波动，不声称空间收益。DirectedCut 配置确定性减少一 byte/vertex 的 ordinary 元数据仍由第 3 节的布局账证明。


## 8. 后续状态位演化

本文件记录的 packed 候选使用低 4 bit：3 bit stage 和 1 bit exact-dual。后续 ordinary 拒绝前沿又在同一个 32-bit 字的低位加入 `rejected-seen` 与 `rejected-frontier` 两个布尔标志，row epoch 因而由左移 4 bit 改为左移 6 bit；没有恢复并行元数据数组，常驻空间账仍为每顶点 4 bytes。新增标志及最终 stage-0 分析前沿的证明和门禁见 `MONOTONE_CERTIFICATE_FRONTIER_PROBE_20260815.md`。
