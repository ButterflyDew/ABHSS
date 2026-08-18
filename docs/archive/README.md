# 历史审计材料

本目录保存仍可追溯、但不再作为当前论文口径唯一入口的细节材料。正式方法、代码边界和实验矩阵分别以 `../METHOD.md`、`../CODE_GUIDE.md` 与 `../EXPERIMENT_PLAN.md` 为准。

- `ABHSS_CONFIGURATION_REFACTOR.md`：旧双入口迁移到单入口配置链及当时的非退化证据。
- `BASELINE.md`：PrunedDP++-Safe 的复现歧义、反例与实现选择。
- `DATA_PROVENANCE.md`：实验方案冻结过程中的数据取得、归档与再分发说明。
- `GPU4GST_DATA.md`：GPU4GST 作者文件、转换器和跨 `g` 查询生成细节。
- `THIRD_PARTY.md`：Basic+、SCIP-Jack、Boost 等 correctness-only 依赖的恢复方法。
- `*_GATE_*.md`：候选在对应历史提交上的正确性、时间与空间门禁；其中 `STRUCTURAL_OMITTED_HALF_ORKUT_G15_GATE_20260812.md` 已被辅助半层反例取代，不再是当前精确证据。
- `SHARED_ORDINARY_HALF_NEGATIVE_PROBE_20260815.md`：ordinary 半层直接转交给 H 的已知真值反例；后续不得把补集域重闭包误删为重复工作。
- `EARLY_QUAD_AND_HALF_PAIR_NEGATIVE_PROBE_20260815.md`：提前 Quad 上界与辅助 H pair 分桶的状态汇合、热段回归及禁止重试边界。
- `CACHED_DUAL_AND_WITNESS_RELEVANCE_NEGATIVE_PROBE_20260816.md`：cached dual 区间复用与 witness 输入相关调度的等价性、零减空证据及禁止重试边界。
- `QUEUE_POP_AND_EMPTY_READY_NEGATIVE_PROBE_20260816.md`：D/H 出堆 cutoff 与 forward 空行 ready 补写的逻辑边界、交换绑核回归及当前恢复结论。
- `STAGED_CACHE_PHYSICAL_REDUCTION_NEGATIVE_PROBE_20260816.md`：staged cache 延迟写入的负结果，以及 ordinary 内核改写后对冻结配置模板分派的复查与恢复边界。
- `MANDATORY_A1_AND_SIZE1_SCAN_NEGATIVE_PROBE_20260816.md`：公共 A1 必然存在、nullable 分支与 ordinary size-1 扫描的严格证明、拆分门禁及物理回归。
- `ADAPTIVE_A1_TOP_TWO_MATERIALIZATION_GATE_20260816.md`：共同 A1 top-two 从 lazy 二分切换到原缓存顺序物化的精确性、P1 小门、Orkut 进度与尚未完成的 q10 硬门。
- `A1_COMPLETE_RANKING_AND_STAGED_CEILING_GATE_20260817.md`：完整 byte tail、精确 mask-rent 因子化与冷物化边界，以及无条件/staged ceiling、locator、farthest 和 API 变体的负向门禁。
- `A1_PUBLICATION_BARRIER_REDUCTION_GATE_20260817.md`：A1 完整发布后的 singleton ready 与 ranked-buy 恒正检查减空、交换绑核证据，以及 top-two/Future 入口守卫的物理回归。
- `MINIMAL_FORWARD_A1_ADJOINT_GATE_20260818.md`：Enhanced 只保留共同 A1、由 H 实现其余逻辑层的正确性边界、交换绑核证据和当前 Orkut g15 q10 完整门。
- `INCREMENTAL_CERTIFICATE_SUPPORT_DP_GATE_20260818.md`：固定 support 上 subset DP 的脏超集增量等价性、Orkut g15 q10 完整轨迹与直接 P1 非退化门。
- `P1_G5_G7_HISTORY_REGRESSION_PROBE_20260818.md`：当前版相对最近有效历史 P1 全量二进制的弱项定向抽样；保留 Orkut g7 q175 的时间与空间反向风险，不替代正式全量 P1。
- `LAZY_EXACT_DUAL_PURCHASE_NEGATIVE_PROBE_20260817.md`：non-exact interval 首次拒绝后延迟购买精确 directed-cut 势的正确性边界、交换绑核轻微回归与完整回退结论。
- `ADJOINT_SPLIT_COMPLETENESS_AUDIT_20260817.md`：较低 H 平衡 split、successor 全值职责与互补辅助半格的正确性事故、结构证明、Musae/SteinLib 门禁及 Orkut q10 完整长轨迹。
- `AUXILIARY_HALF_ADJOINT_CORRECTNESS_AUDIT_20260815.md`：从逻辑层直接启动 H 的精确性事故、12 点固定反例、九条生产差异、辅助 $H(h)$ 第一轮修复及其当时待复跑硬门。
- `DOMINATED_OPERATION_REDUCTION_AUDIT_20260815.md`：当前源码的全链路严格支配审计，记录接受减空、物理回归候选和最终 P1/Orkut 硬门状态。
- `*_NEGATIVE_*.md` 及名称含 `NEGATIVE` 的文档：被拒绝候选的反例与清理依据；不得据此描述当前生产算法。
- `communication/`：算法候选筛选、跨图结构统计和 YouTube/P1/P2 探针记录；只作决策证据，不替代正式实验结果。

归档表示“辅助审计”，不表示内容作废。若归档材料与三个正式文档或机器清单冲突，以机器清单和正式文档为准，并应修正归档材料而不是同时保留两种现行口径。
