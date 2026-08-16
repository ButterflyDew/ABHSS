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
- `ADAPTIVE_A1_TOP_TWO_MATERIALIZATION_GATE_20260816.md`：共同 A1 top-two 从 lazy 二分切换到原缓存顺序物化的精确性、P1 小门、Orkut 进度与尚未完成的 q10 硬门。
- `AUXILIARY_HALF_ADJOINT_CORRECTNESS_AUDIT_20260815.md`：从逻辑层直接启动 H 的精确性事故、12 点固定反例、九条生产差异、辅助 $H(h)$ 修复及当前待复跑硬门。
- `DOMINATED_OPERATION_REDUCTION_AUDIT_20260815.md`：当前源码的全链路严格支配审计，记录接受减空、物理回归候选和最终 P1/Orkut 硬门状态。
- `*_NEGATIVE_*.md` 及名称含 `NEGATIVE` 的文档：被拒绝候选的反例与清理依据；不得据此描述当前生产算法。
- `communication/`：算法候选筛选、跨图结构统计和 YouTube/P1/P2 探针记录；只作决策证据，不替代正式实验结果。

归档表示“辅助审计”，不表示内容作废。若归档材料与三个正式文档或机器清单冲突，以机器清单和正式文档为准，并应修正归档材料而不是同时保留两种现行口径。
