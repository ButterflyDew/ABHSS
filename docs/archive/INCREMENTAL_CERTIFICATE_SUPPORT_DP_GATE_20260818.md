# Certificate-support 增量 subset-DP 门禁（2026-08-18）

## 1. 目的与边界

本轮只优化 Enhanced/DirectedCut 在 residual closure 之后反复购买的 certificate-support evaluator。它不改 ordinary、A/H、directed-cut potential、rent-or-buy 公式、购买位置、refilter 条件、浮点精度或最终精确性口径。Base 不产生 certificate support，也不执行每张 ordinary row 的 support 通知。

对照源码为提交 `6593b0de3c15f4deb11b6c9e5597c8ce49f3dd08`。由该源码独立重建的正式二进制 SHA-256 为 `96a4add04c2a731f76c1c61bd7762760ee2e8eb11f0bfab7c2e3abfcf25554c3`，与保存的历史正式二进制逐字节相同；q10 实际使用的历史诊断二进制 SHA-256 为 `5c81392125b8b003ec681e679599f9caf202befb0f6f1caf53e2f65b885992c9`。q10 增量诊断候选 SHA-256 为 `7be8af050899a81c56498d00cc1450c02c8249b54a648cac7e91a20836b75a30`。随后只把 support-mask 通知从共同 `Account` 接口移到 `BuildOrdinaryRowsImpl<true>` 的编译期 DirectedCut 分支，最终正式候选 SHA-256 为 `793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced`。该移动不改变 Enhanced 的通知时点或求值语义，只保证 Base 热循环不承担新增判断。

为未来超长询问给 `ordinary_row/layer` 增加累计秒数后，最终诊断二进制 SHA-256 为 `e4dd07df4bca170d505cad10ad5fe8931929b8de1bd120b3fef599fee916122e`；正式二进制哈希仍逐字节保持 `793d4e27dfdcf52252602e4b2b8e11c3d9e06caab0a2b5f142edc2a45dc89ced`，证明诊断计时路径在正式构建中被完全消除。

## 2. 等价改写与正确性

旧 evaluator 每次购买都重新构造 support 顶点、Floyd metric 和全部 mask 的 subset DP。新 `CertificateSupportDpCache` 第一次仍执行完全相同的全量递推，之后保留固定 support 的 metric 和 DP。两次购买间若新发布的 ordinary mask 集为 $P$，脏域为：

```math
\mathcal U(P)=\{X\mid \exists M\in P,\ M\subseteq X\}.
```

新 $D(M)$ 只改变 $M$ 的 direct seed。对任意 $X\notin\mathcal U(P)$，其 direct seed不变；任一 split 子 mask 若受影响，就会包含某个 $M$，进而推出 $X$ 也包含 $M$，矛盾。按 mask 基数递增归纳，非脏状态的 split、merge 和固定 metric closure 均不变；只按原顺序重算脏超集即可得到与全量 evaluator 逐项相同的表。

以下生命周期事件会主动放弃增量假设：

- incumbent 收紧后的 destructive ordinary refilter 调用 `Reset`，下一次购买全表重建；
- residual closure 刷新 support 时销毁旧缓存并构造新缓存；
- 第一次 support 购买全表重建。

调度器继续使用完整 $B_{\mathrm{sup}}$，所以 rent、buy、购买序号与 refilter 时点不变。独立回归测试在合成 support 上分批发布多组 ordinary row，每批后把持久缓存结果与新建 stateless 全量 evaluator 比较；再删除已有 row 值并 `Reset` 后复比。全套 5/5 CTest 还包含 5,000 个确定性随机精确 oracle 实例和既有高组数反例。

## 3. Orkut `g=15` q10 自然完成结果

输入为 `experiment_data/p2_cross_g/GPU4GST_Orkut/cross_g15.txt` 的 q10，固定 CPU 5，单进程运行。历史与候选分别保存在 `results/paper_runs/final_6593b0d_orkut_g15_q10_diag_cpu5` 和 `results/paper_runs/incremental_support_q10_full_diag_cpu5`。两边都开启同一层级的稀疏诊断；历史 timeout 为 30,000 秒，候选为 86,400 秒，均自然完成而未触发截断。候选完成时间为 9250.911428 秒。

| 指标 | 历史对照 | 增量候选 | 差值或比值 |
|---|---:|---:|---:|
| solver 秒 | 9395.877740 | 9250.911428 | -144.966312 |
| 端到端 old/new | 1.000000 | 0.984571 | 1.015670 倍加速 |
| 最优权值 | 54 | 54 | 相同 |
| `(mask,v)` 状态 | 1,459,398,194 | 1,459,398,194 | 相同 |
| 查询峰值 MiB | 18,305.211 | 18,318.242 | +13.031 |
| watchdog RSS MiB | 24,696.816 | 24,709.785 | +12.969 |
| 距 10,000 秒余量 | 604.122 | 749.089 | +144.967 |

阶段时间如下。相邻长跑存在正常系统波动，adjoint 可作为未修改阶段的环境对照。

| 阶段 | 历史秒 | 候选秒 | 说明 |
|---|---:|---:|---|
| prepare | 188.074 | 185.288 | 未修改 |
| A1 | 84.797 | 84.500 | 未修改 |
| ordinary | 6665.830 | 6529.610 | 包含 support 购买与 refilter |
| low anchor | 9.134 | 8.877 | 未修改 |
| adjoint transpose | 160.814 | 159.378 | 未修改环境对照 |
| adjoint 总计 | 2447.990 | 2442.580 | 未修改环境对照 |

两版 ordinary 均发布 6461 行、1,091,601,442 个标量；refilter 都在第 218 次购买把 `best` 从 59 收紧到 58，删除 180,686,498 个标量，留下 1428 行和 458,071,592 个标量。refilter 时间为 144.827 与 144.776 秒，说明搜索轨迹与主要固定成本一致。

## 4. Support 购买细分

两版均购买 witness/support 496 次，最初两次树 witness 购买相同；其后 494 次为 support evaluator。候选的 494 次中，2 次全量、492 次增量。

| 指标 | 数值 |
|---|---:|
| 旧版全量等价 mask 总数 | 8,093,202 |
| 候选实际重算 mask 总数 | 1,374,856 |
| 实际/全量 | 16.9878% |
| 增量最小重算 mask | 480 |
| 第一四分位 | 1,404 |
| 中位数 | 2,240 |
| 第三四分位 | 3,597 |
| 最大值 | 10,496 |
| 平均值 | 2,727.825 |
| 发布 ordinary mask | 6,454 |
| 接纳为 direct seed 的 mask | 6,445 |

9 个发布与接纳差来自两次全量重建前待处理的 row；全量路径直接读取全部当前 ordinary，因此没有遗漏。全部 support buy（含 refilter）从 400.633 秒降到 205.545 秒；扣除约 144.8 秒相同 refilter 后，evaluator 本体约从 255.806 秒降到 60.769 秒，即约 4.21 倍。端到端只快 1.57%，因为 90% 以上时间仍在 ordinary 主搜索与 adjoint，而不是该 evaluator。

## 5. P1 非退化门

| 面板 | 历史秒 | 候选秒 | 结论 |
|---|---:|---:|---|
| Enhanced Musae `g=7`, 全 300 条 | 22.195592 | 22.021389 | 候选快 0.79%，答案与状态逐条相同 |
| Enhanced Orkut `g=7`, q175，两轮合计 | 392.950553 | 391.618166 | 候选快 0.34%，答案与状态相同 |
| Enhanced Orkut `g=7`, q175，最终二进制 | 197.194998 | 197.392606 | 候选慢 0.10%，中性波动；内存少 0.387 MiB |
| Base Musae `g=7`, 全 300 条，最终热路径 | 46.614704 | 46.083246 | 候选快 1.15%，状态相同 |
| Base Orkut `g=7`, q175，最终热路径 | 165.347131 | 166.651139 | 候选慢 0.79%，状态与内存相同 |

Base 两个方向正负翻转，且最终模板实例在编译期没有 `PublishOrdinaryMask`，不能把该波动归因于算法新增工作。P1 门只支持“无可识别退化”，不支持宣称 Base 加速。

## 6. 长询问必须保留的中间信息

诊断构建在运行中逐事件写日志，而不是等查询结束后才汇总。下一次允许自然超过 timeout 的探索长跑必须保存：

- `prepare_start/end`、`singleton_anchor_start/end`、`ordinary_start/end`、逐 `ordinary_layer`；
- 每张 `ordinary_row` 和每层 `ordinary_layer` 从 ordinary 开始的累计秒数及工作量；
- 每次 `witness_buy` 的序号、当前 `best`、秒数、已发布 rows/scalars 与 paid rent；
- 每次 `support_dp_full/incremental` 的 `evaluation`、`published_masks`、`activated_masks`、`recomputed_masks`；
- 每次 `witness_refilter` 的触发序号、前后 `best`、删除标量数与耗时；
- `adjoint_transpose`、逐 `adjoint_layer` 与 `adjoint_end`；
- 最终 solver time、weight、query peak RSS、watchdog RSS 与 `(mask,v)` 状态数；
- 输入路径和 SHA-256、源码提交、正式/诊断二进制 SHA-256、CPU 绑核与同时运行任务。

这些量足以区分“support evaluator 优化无效”“ordinary 状态爆炸”“refilter 固定成本”“adjoint 后缀昂贵”和“机器并发波动”。正式论文计时仍使用无诊断二进制；诊断日志只服务机制分析。

## 7. 被否决的复杂化

- 没有为 direct ordinary seed 再保存第二张 mask-vertex 表：ordinary row 已是唯一真值，复制只增加内存和同步风险。
- 没有让 refilter 返回精细 changed-mask 集：q10 只在一次 refilter 后多做约 0.4 秒全量 support 重建，不值得扩张 destructive 接口。
- 没有按 dirty 比率动态调整 buy：这会改变购买位置并引入经验参数，破坏干净对照。
- 没有按图名、 $g$、时间或内存选择路径，也没有降低浮点精度。
- 没有修改 ordinary 深度或 adjoint 递推；它们是 q10 剩余时间的主要来源，需要独立证明和独立门禁。
