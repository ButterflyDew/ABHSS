# A1 发布屏障与恒真检查减空门禁（2026-08-17）

## 1. 结论

本轮只研究公共 A1 已经完成发布之后的重复合法性检查，不增加下界、上界、状态、配置或数据相关开关。

当前源码接受两项减空：

1. A1 构建屏障之后，删除只读 future 视图中对 singleton `row[bit].ready` 的重复检查；
2. 第二级 ranked-tail 购买点删除恒真的 `ranked_buy_work > 0`，只比较已付 rent 是否达到 buy。

两项都作用于 Base、DirectedCutOnly 与 Enhanced 共用的 `AnchoredSingletonFuture`，不读取图名、查询编号、组数经验阈值、墙钟、状态数或 enhancement 位。数值、浮点读取顺序、购买公式、row payload 和 `(mask,v)` 状态数不变。

当前 combined production SHA-256 为 `57d2afa0f7098cec9baff15fd8088695c006edf72adccdb1c5668f4ba2a4b505`；diagnostics SHA-256 为 `37204d7f4c80d654a5ae58082ca907675ef001038467680d7458b416dd142095`。本记录写入时，combined 版本的 Orkut g15 q10 10000 秒硬门尚未运行，不得提前写成通过。

## 2. singleton row 发布屏障

`BuildReusableAnchoredSingletonLayer` 的外层循环只可能有两种结果：

- witness-tree DP 收紧 incumbent：当前工作 row 被清空，已构造的部分 singleton 集合整体丢弃，从第一个 bit 重新开始；
- 没有重启：内层按全部 singleton bit 完整结束，每张 row 都在 payload 写入后设置 `ready=true`，然后才初始化 future 查找计划并返回。

payload 可以为空，但空 payload 与“未发布”不同；前者同样已经设置 `ready=true`，并通过统一 cone 外 fallback 返回安全值。因此一旦 `InitializeLookupPlan` 可见，全部 singleton row 必然已经发布。只读阶段反复执行 `if (!row[bit].ready) continue` 不可能改变：

- buy/rent；
- row 与 fallback 的选择；
- top-two 或完整排名；
- 返回下界；
- owner 移交与状态计数。

当前源码只删除 A1 发布屏障之后的这些检查。ordinary、H、forward owner 交接及其他存在“未生成/已发布空”区别的生命周期仍保留各自 `ready` 语义。

## 3. ranked buy 严格为正

A1 只在逻辑正层域包含 A1 时构造。由

```math
q=\left\lfloor\frac{g}{2}\right\rfloor-1
```

以及 `q>=1` 可得 `g>=4`，故非锚 bit 数 `k=g-1>=3`。可行输入又有 `n>=1`。第一层购买工作对每个 singleton bit 至少加入 `2n`，所以：

```math
B_{\mathrm{scan}}>0,\qquad
B_{\mathrm{rank}}=
B_{\mathrm{scan}}+
n\left(\frac{(k-2)(k-3)}{2}+k-2\right)>0.
```

因此进入第二级 tail rent 路径时，`ranked_buy_work > 0` 恒真。删除它不改变购买点；实际条件仍是 `ranked_rent_work >= ranked_buy_work`。

## 4. 正确性与构建门

production 与 diagnostics 构建均通过 5/5 CTest：

- 快速图读入结构；
- 查询读入校验；
- 零权 witness；
- 配置精确性；
- `(mask,v)` 状态计数。

A1 直接回归遍历 64 个顶点与全部 15 个非空 remaining mask，对照独立逐 bit 最大值，并强制经过 lazy、top-two、ranked-tail 与精确 mask-rent 表。两项减空不改变该测试的值、购买路径或状态向量。

## 5. 接受项性能门

### 5.1 删除发布后 ready 检查

Musae g7 Base 全 300 条，两轮交换 CPU4/CPU5：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 候选 / 对照 |
|---|---:|---:|---:|
| 1 | 46.128891 | 45.566126 | 1.012351 |
| 2 | 45.825665 | 46.347212 | 0.988747 |

几何比为 1.000479，属于中性；两轮各 300 条权值、状态逐项相同，状态总数均为 6,872,062。

Orkut g15 q10 diagnostics 900 秒交换轮：

| 轮次 | 候选 ordinary row | 对照 ordinary row | 候选 / 对照总事件 |
|---|---:|---:|---:|
| 1 | 216 | 214 | 279 / 277 |
| 2 | 219 | 218 | 283 / 281 |

忽略观测耗时字段后，对照事件序列在两轮都是候选事件序列的严格前缀；峰值 RSS 同为约 10,061 MiB。

### 5.2 删除 ranked buy 恒正检查

Musae g7 Base 全 300 条交换轮：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 候选 / 对照 |
|---|---:|---:|---:|
| 1 | 46.746360 | 46.412513 | 1.007193 |
| 2 | 46.118568 | 46.579688 | 0.990100 |

几何比为 0.998610；全部权值、状态逐项一致。

Orkut g15 q10 diagnostics 900 秒交换轮：

| 轮次 | 候选 ordinary row / witness buy | 对照 ordinary row / witness buy | 候选 / 对照总事件 |
|---|---:|---:|---:|
| 1 | 220 / 48 | 216 / 47 | 284 / 279 |
| 2 | 221 / 48 | 214 / 47 | 285 / 277 |

两轮对照事件序列同样是候选严格前缀；峰值 RSS 无增长。

## 6. 回退项

### 6.1 top-two 哨兵守卫

A1 域内 `k>=3` 且 future 值非负，因此从 -1 初始化的 first/second 在扫描前两个 bit 后必有效。删除写入处与命中处的 `>=0` / `!=255` 守卫在语义上成立，但 Musae 两轮分别为：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 比值 |
|---|---:|---:|---:|
| 1 | 46.306288 | 45.871324 | 1.009482 |
| 2 | 46.110555 | 45.718554 | 1.008574 |

几何回归 0.9028%，交换核后方向不变，故完整回退。不能用源代码分支更少代替物理门禁。

### 6.2 Future 入口域检查

生产调用确实只来自已初始化 A1，并且 ordinary mask 是非锚 full mask 的真子集；但删除 `first.empty() || !remaining` 后，Musae 两轮为：

| 轮次 | 候选 / 秒 | 对照 / 秒 | 比值 |
|---|---:|---:|---:|
| 1 | 46.504723 | 45.614214 | 1.019523 |
| 2 | 46.359855 | 45.766229 | 1.012971 |

几何回归 1.6241%，故恢复该平凡边界检查。没有加入 NOP、强制对齐或按编译器分派等难以形成论文主线的机器特调。

## 7. 10000 秒硬门状态

在上述两项严格减空之前，“完整排名 + 精确租金表 + 冷物化边界”的 production 二进制 `d3aee9cf453327762a974ba332701f8c3ea388bf6328d6d7562bb8f1508c8ee0` 对 Orkut g15 q10 运行到 watchdog 10001.768655 秒后 timeout，没有写出权值；终止前人工 RSS 采样约 26.2 GiB。

该超时只否定 pre-reduction 二进制，不能证明当前 combined 版本失败。下一步必须对当前 SHA 单核运行同一 q10：

- timeout 为 10000 秒；
- 只有写出精确权值 54；
- 且 solver 时间严格小于 10000 秒；

才算通过。之后仍需 q1--q10 全门和 13 图 P1。
