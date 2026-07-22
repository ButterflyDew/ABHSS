# SIGMOD/VLDB 投稿前 15 小时全数据探针

## 1. 目的与边界

本探针只回答三个投稿前问题：

1. `g<=8` 时，ABHSS-Light 与 ABHSS-Heavy 是否大体不差于主 baseline `PrunedDP++-Safe`；
2. `g=9..16` 时，是否已经出现数量级加速或显著完成率优势；
3. 若 Safe 很快而 ABHSS 很慢，退化来自哪一算法阶段，并与哪些图/询问特征一致。

探针不是论文最终实验。单询问广度/深度截止分别为 180/600 秒，只跑一次；正式矩阵的每询问截止仍为 10,000 秒。探针不能替代正式重复、bootstrap 置信区间和完整 completion/PAR-2 报告。

## 2. 不可事后更换的抽样规则

- seed 固定为 `20260722`。
- 先按来源、`g`、目标 `f` 和历史风险选择重要 cell，再在每个 cell 内用 `SHA-256(seed|suite|case_id|query_index)` 对全部合格询问排序。
- 第 1/2/3 个哈希顺位分别用于广度、深度或补样；选择在任何求解器运行前写入 `probe_manifest.json`。
- 运行结果驱动的 follow-up 单独标为 `adaptive_diagnosis`，只用于原因分析，不进入主张门控。
- Light 与 Heavy 始终分开评价，不把逐实例 best-of-two 当作论文算法。

## 3. 广度波（60 个 cell/query）

| 来源 | 选择 | 作用 |
|---|---|---|
| E1 controlled | 三图的 `f=400,g={4,10,16}`，另平衡选择 6 个 `f={200,1600}` cell | 分离 `g` 与 `f`，检验小 g 守门和大 g 趋势 |
| E2 GPU4GST-related | 八图 `g={4,12}`；四张代表图 `g={9,16}` | 覆盖全部图，并直接探测 `g=9..16` motivation |
| E3 Practical-original | 三图 `(g=5,f=400)` 与 `(g=8,f=1600)` | 检验原生成协议及大组大小 |
| E3 natural keyword | LinkedMDB/DBpedia 的 `g={4,10}` | 半真实/自然组，不强行控制平均 `f` |
| E4 author panel | 每图交替选择一个 `g=5/7` cell | 核对来源作者询问；同时跑作者 CPU artifact |
| E5 ablation | 三图各一个 `g=10,f=400` cell | 快速核验主要组件方向 |

五类来源采用 round-robin 排程，避免某一来源的超难 cell 吞掉全部预算。

## 4. 深度波与剩余预算

广度完成后，预注册深度波使用第二个哈希顺位和 600 秒截止，优先覆盖：

- E2 全八图 `g=16`；四张代表图 `g={9,14}`；
- E1 三图 `f=400,g={14,16}`；
- E3 natural `g=10` 与 Practical-original `(g=8,f=1600)`。

若广度发现 Safe 完成、ABHSS timeout，或 Safe 比最佳 ABHSS 至少快 2 倍，则最多选 8 个最强异常 cell，用第 2/3 哈希顺位做诊断复核。这些 follow-up 在报告中与预注册证据严格分开。仍有预算时，才按固定哈希顺序从 E1/E2/E3 全矩阵补样。

## 5. 探针专用诊断信息

探针构建三个独立 twin：`abhss_light_probe`、`abhss_heavy_probe`、`pruneddp_probe`。正式二进制没有启用诊断宏。

- PrunedDP++：组多源最短路、组路线 DP、主搜索时间；发现/settled 状态、reopen、MST 次数、grow/merge 尝试和 witness 数。
- ABHSS-Light：prepare、early anchor、ordinary、anchored 时间；每层 ready rows 与物化标量。
- ABHSS-Heavy：prepare（含 eager directed-cut）、ordinary、low anchor、adjoint 时间；每层 rows/标量。
- timeout 也保留截止前最后一个 phase/progress 事件，从而可以定位卡住的阶段。
- 探针 supervisor 每 0.2 秒从进程外采样 RSS，因而 timeout 记录也保留绝对峰值与相对 `[Ready]` 的查询峰值；正式 runner 不启用该轮询路径。

## 6. 原因分析口径

`analyze_probe_results.py` 对每个已运行询问计算：

- 图：`n,m`、平均度、密度、文件大小，以及固定随机字节偏移的边权样本统计；
- 询问：实际组大小均值/范围/CV、组并集、重复 membership、相交组对与 Jaccard；
- 方法：两边阶段时间、PrunedDP++ 稀疏状态数和 ABHSS 物化标量数。

异常解释分两层输出：

1. 方法层以实测主导 phase、状态/标量比例和 timeout 最后阶段为证据；
2. 数据层判断图稠密度、终端密度、组重叠和组大小不均衡是否与该机制一致。

自动报告使用“与证据一致”而非无实验支持的因果断言。若计数不能唯一归因，会明确要求针对该 cell 增加层级计数。

## 7. 投稿门控

- 小 g 探针守门：`T_ABHSS <= 1.2*T_Safe`，或两者绝对差不超过 0.05 秒；后一条件只用于避免单次亚秒计时噪声，原始比值仍完整报告。
- 大 g 强信号：双方完成且 Safe/ABHSS `>=10x`，或 Safe 在短截止 timeout 而 ABHSS 完成。
- 同时报告反向完成损失：Safe 完成而 ABHSS timeout。
- `error` 或 `[Ready]` 前 `graph_load_timeout` 单列为非搜索失败并触发执行可靠性门；全局边界的 `budget_exhausted` 只作顺序删失。
- 所有权重先做精确一致性检查；有任何不一致就暂停性能结论。

## 8. 运行与产物

前台启动命令：

```powershell
python tools/experiments/run_probe_15h.py --hours 15
```

控制器每个 cell 后原子更新 `probe_status.json`，全局 15 小时截止会主动回收当前子进程，不留下孤儿求解器；任务可由已有记录续跑。

每次运行目录包含：

- `probe_manifest.json`：完整抽样、顺位、理由、方法和最终状态；
- `probe_status.json`：当前进度、剩余预算、结果状态计数；
- `records/`、`logs/`：逐方法原始证据和诊断事件；
- `PROBE_15H_REPORT.md`：投稿门控与逐异常双层解释；
- `probe_summary.json`、`probe_coverage.csv`、`probe_pairs.csv`、`probe_ablations.csv`、`probe_author_artifact_audit.csv`、`probe_query_features.csv`、`probe_anomalies.json/csv`；
- `probe_reproducibility.json`：实际二进制、源码/配置以及 canonical records/logs 集合哈希；
- `probe_audit.json`：结束后由 `audit_probe_run.py` 独立重算排程并验收预算、哈希、记录键和精确权重。

机器关机、睡眠或用户强杀会中断墙钟任务；这不是算法 timeout。正式跑数前应关闭睡眠并记录机器负载。
