# Purchased vertex-major group-distance layout：负向探针

## 动机

Enhanced 在购买完整 directed-cut closure 后已经持有全部组到全部顶点的精确距离。该探针额外复制一份按顶点连续的 `vertex × group` 布局，希望减少 `FarthestRemaining`、tour 下界和 adjoint 热路径中的跨数组访问。

## 对照设置

- 数据：`GPU4GST_Orkut`，`g=15`，第 5 条询问。
- 配置：当前 certificate-support 正向版本的 Enhanced。
- 机器约束：候选与对照严格串行，均固定 CPU 11，单次时限 1200 秒。
- 指标：相同时限内完成的 ordinary row 数；不将诊断版本绝对时间当作正式性能。

## 结果

| 版本 | 1200 秒内完成的 ordinary row | 观察到的额外内存 |
|---|---:|---:|
| 原布局对照 | 368 | 0 |
| 顶点连续复制 | 362 | 约 0.35 GiB |

候选少完成 6 行，约退化 1.6%，同时必须复制约 `n × g` 个 `double`。收益不足以补偿复制成本和更大的工作集。

## 结论

该方向失败，不进入正式实现。源代码宏、候选 build 和原始探针输出均删除；只保留本结论，避免后续重复尝试同一布局复制。
