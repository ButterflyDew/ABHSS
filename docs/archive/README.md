# 历史材料索引

本目录只保存设计演进、实验门禁、负结果和 artifact 恢复信息。它们用于追溯，不覆盖 `docs/` 下三份当前人类文档。历史记录可能对应旧提交或诊断构建；引用数字前必须先核对该节的证据边界。

- [`METHOD_EVOLUTION_AND_CORRECTNESS.md`](METHOD_EVOLUTION_AND_CORRECTNESS.md)：配置关系、D/A/H 演进、正确性事故与修复。
- [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md)：已采纳优化、性能门禁和冻结证据。
- [`REJECTED_AND_DEFERRED_DIRECTIONS.md`](REJECTED_AND_DEFERRED_DIRECTIONS.md)：负结果、被支配方案和未启用候选。
- [`DATA_BASELINES_AND_ARTIFACTS.md`](DATA_BASELINES_AND_ARTIFACTS.md)：数据、baseline 和第三方依赖。

## 维护约束

1. 当前代码和论文口径只写入 `docs/CODE_GUIDE.md`、`docs/METHOD.md`、`docs/EXPERIMENT_PLAN.md`；历史卷不得反向定义当前事实。
2. 新的失败探针应并入“已否决与暂缓方向”，不要为每次尝试新增一份顶层文档。新的正式门禁并入“已采纳优化与门禁”。
3. GitHub 数学公式统一使用独占行的 fenced math block；不要使用双美元分隔符或 GitHub 已禁用的宏。Markdown 修改后必须运行仓库渲染检查。
4. 自动化助手阅读历史材料时，先读本索引和三份当前文档，再按稳定锚点读取相关历史节；不得把旧勘误前的结论写回当前方法。

## 原文件映射

以下映射保留合并前的文件名，便于从旧日志和提交定位：

| 原文件 | 归档位置 |
|---|---|
| `A1_COMPLETE_RANKING_AND_STAGED_CEILING_GATE_20260817.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-a1-complete-ranking-and-staged-ceiling-gate-20260817`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-a1-complete-ranking-and-staged-ceiling-gate-20260817) |
| `A1_PUBLICATION_BARRIER_REDUCTION_GATE_20260817.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-a1-publication-barrier-reduction-gate-20260817`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-a1-publication-barrier-reduction-gate-20260817) |
| `A1_SEED_SUPPORT_ANCHOR_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-a1-seed-support-anchor-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-a1-seed-support-anchor-negative-probe-20260810) |
| `ABHSS_CONFIGURATION_REFACTOR.md` | [`METHOD_EVOLUTION_AND_CORRECTNESS.md#history-abhss-configuration-refactor`](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-abhss-configuration-refactor) |
| `ADAPTIVE_A1_TOP_TWO_MATERIALIZATION_GATE_20260816.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-adaptive-a1-top-two-materialization-gate-20260816`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-adaptive-a1-top-two-materialization-gate-20260816) |
| `ADJOINT_LIFECYCLE_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-adjoint-lifecycle-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-adjoint-lifecycle-negative-probe-20260809) |
| `ADJOINT_SPLIT_COMPLETENESS_AUDIT_20260817.md` | [`METHOD_EVOLUTION_AND_CORRECTNESS.md#history-adjoint-split-completeness-audit-20260817`](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-adjoint-split-completeness-audit-20260817) |
| `ANCHOR_ROOTED_GLOBAL_LOWER_NEGATIVE_PROBE_20260811.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-anchor-rooted-global-lower-negative-probe-20260811`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-anchor-rooted-global-lower-negative-probe-20260811) |
| `AUXILIARY_HALF_ADJOINT_CORRECTNESS_AUDIT_20260815.md` | [`METHOD_EVOLUTION_AND_CORRECTNESS.md#history-auxiliary-half-adjoint-correctness-audit-20260815`](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-auxiliary-half-adjoint-correctness-audit-20260815) |
| `BASELINE.md` | [`DATA_BASELINES_AND_ARTIFACTS.md#history-baseline`](DATA_BASELINES_AND_ARTIFACTS.md#history-baseline) |
| `BRANCH_WORD_ENUMERATION_NEGATIVE_PROBE_20260811.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-branch-word-enumeration-negative-probe-20260811`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-branch-word-enumeration-negative-probe-20260811) |
| `BRIDGE_UNION_BOUND_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-bridge-union-bound-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-bridge-union-bound-negative-probe-20260809) |
| `CACHED_DUAL_AND_WITNESS_RELEVANCE_NEGATIVE_PROBE_20260816.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-cached-dual-and-witness-relevance-negative-probe-20260816`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-cached-dual-and-witness-relevance-negative-probe-20260816) |
| `CAUSAL_BLOCK_FUTURE_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-causal-block-future-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-causal-block-future-negative-probe-20260810) |
| `COMPLEMENT_PAIR_SCHEDULING_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-complement-pair-scheduling-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-complement-pair-scheduling-negative-probe-20260809) |
| `COMPONENT_COVER_FUTURE_DOMINATED_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-component-cover-future-dominated-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-component-cover-future-dominated-probe-20260810) |
| `CONSISTENT_REPRESENTATIVE_UPPER_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-consistent-representative-upper-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-consistent-representative-upper-negative-probe-20260809) |
| `COST_GUIDED_CANONICAL_PIVOT_NEUTRAL_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-cost-guided-canonical-pivot-neutral-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-cost-guided-canonical-pivot-neutral-probe-20260810) |
| `D2_SUPPORT_ANCHOR_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-d2-support-anchor-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-d2-support-anchor-negative-probe-20260810) |
| `DATA_PROVENANCE.md` | [`DATA_BASELINES_AND_ARTIFACTS.md#history-data-provenance`](DATA_BASELINES_AND_ARTIFACTS.md#history-data-provenance) |
| `DOMINATED_OPERATION_REDUCTION_AUDIT_20260815.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-dominated-operation-reduction-audit-20260815`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-dominated-operation-reduction-audit-20260815) |
| `DOUBLE_RADIX_QUEUE_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-double-radix-queue-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-double-radix-queue-negative-probe-20260810) |
| `DUAL_ORDER_NEGATIVE_PROBE_20260807.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-dual-order-negative-probe-20260807`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-dual-order-negative-probe-20260807) |
| `DUAL_PRIMAL_SUPPORT_UNION_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-dual-primal-support-union-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-dual-primal-support-union-negative-probe-20260810) |
| `DUAL_REDUCED_PARTITION_SCREEN_NEGATIVE_PROBE_20260811.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-dual-reduced-partition-screen-negative-probe-20260811`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-dual-reduced-partition-screen-negative-probe-20260811) |
| `EARLY_QUAD_AND_HALF_PAIR_NEGATIVE_PROBE_20260815.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-early-quad-and-half-pair-negative-probe-20260815`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-early-quad-and-half-pair-negative-probe-20260815) |
| `ENDPOINT_PAIR_TREE_BOUND_DOMINATED_NOTE_20260812.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-endpoint-pair-tree-bound-dominated-note-20260812`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-endpoint-pair-tree-bound-dominated-note-20260812) |
| `EPHEMERAL_COMPLEMENTARY_DUAL_NEGATIVE_PROBE_20260813.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-ephemeral-complementary-dual-negative-probe-20260813`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-ephemeral-complementary-dual-negative-probe-20260813) |
| `FULL_POTENTIAL_ROOT_SCREEN_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-full-potential-root-screen-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-full-potential-root-screen-negative-probe-20260810) |
| `FUSED_TOP_ORDINARY_STATIC_NEGATIVE_20260811.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-fused-top-ordinary-static-negative-20260811`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-fused-top-ordinary-static-negative-20260811) |
| `GPU4GST_DATA.md` | [`DATA_BASELINES_AND_ARTIFACTS.md#history-gpu4gst-data`](DATA_BASELINES_AND_ARTIFACTS.md#history-gpu4gst-data) |
| `GROUP_REINSERT_UPPER_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-group-reinsert-upper-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-group-reinsert-upper-negative-probe-20260810) |
| `HIGH_G_COMPONENT_AND_OVERLAP_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-high-g-component-and-overlap-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-high-g-component-and-overlap-negative-probe-20260809) |
| `HSTAR_NEGATIVE_PROBE_20260807.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-hstar-negative-probe-20260807`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-hstar-negative-probe-20260807) |
| `H_SCHEDULING_PROBE_20260807.md` | [`METHOD_EVOLUTION_AND_CORRECTNESS.md#history-h-scheduling-probe-20260807`](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-h-scheduling-probe-20260807) |
| `INCREMENTAL_CERTIFICATE_SUPPORT_DP_GATE_20260818.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-incremental-certificate-support-dp-gate-20260818`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-incremental-certificate-support-dp-gate-20260818) |
| `INCREMENTAL_CLOSURE_BUY_MIXED_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-incremental-closure-buy-mixed-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-incremental-closure-buy-mixed-probe-20260810) |
| `INCUMBENT_HALF_EDGE_GROWTH_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-incumbent-half-edge-growth-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-incumbent-half-edge-growth-negative-probe-20260810) |
| `INDEPENDENT_WITNESS_ENSEMBLE_NEGATIVE_PROBE_20260811.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-independent-witness-ensemble-negative-probe-20260811`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-independent-witness-ensemble-negative-probe-20260811) |
| `LAZY_EXACT_DUAL_PURCHASE_NEGATIVE_PROBE_20260817.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-lazy-exact-dual-purchase-negative-probe-20260817`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-lazy-exact-dual-purchase-negative-probe-20260817) |
| `LAZY_MAXIMUM_RESIDUAL_MARGINAL_NEGATIVE_PROBE_20260814.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-lazy-maximum-residual-marginal-negative-probe-20260814`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-lazy-maximum-residual-marginal-negative-probe-20260814) |
| `MANDATORY_A1_AND_SIZE1_SCAN_NEGATIVE_PROBE_20260816.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-mandatory-a1-and-size1-scan-negative-probe-20260816`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-mandatory-a1-and-size1-scan-negative-probe-20260816) |
| `METRIC_PERIPHERAL_ANCHOR_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-metric-peripheral-anchor-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-metric-peripheral-anchor-negative-probe-20260809) |
| `MINIMAL_FORWARD_A1_ADJOINT_GATE_20260818.md` | [`METHOD_EVOLUTION_AND_CORRECTNESS.md#history-minimal-forward-a1-adjoint-gate-20260818`](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-minimal-forward-a1-adjoint-gate-20260818) |
| `MONOTONE_CERTIFICATE_FRONTIER_PROBE_20260815.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-monotone-certificate-frontier-probe-20260815`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-monotone-certificate-frontier-probe-20260815) |
| `OMITTED_TWO_ORDINARY_LAYERS_NEGATIVE_PROBE_20260812.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-omitted-two-ordinary-layers-negative-probe-20260812`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-omitted-two-ordinary-layers-negative-probe-20260812) |
| `ORACLE_OPTIMAL_UPPER_P2_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-oracle-optimal-upper-p2-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-oracle-optimal-upper-p2-negative-probe-20260810) |
| `ORDERED_ROW_BITMAP_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-ordered-row-bitmap-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-ordered-row-bitmap-negative-probe-20260810) |
| `ORDINARY_PAIR_PARTITION_UPPER_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-ordinary-pair-partition-upper-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-ordinary-pair-partition-upper-negative-probe-20260810) |
| `ORDINARY_REFILTER_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-ordinary-refilter-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-ordinary-refilter-negative-probe-20260809) |
| `ORIGINAL_GRAPH_FACILITY_UPPER_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-original-graph-facility-upper-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-original-graph-facility-upper-negative-probe-20260810) |
| `ORKUT_G15_EXTRA_Q10_OPTIMIZATION_AUDIT_20260813.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-orkut-g15-extra-q10-optimization-audit-20260813`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-orkut-g15-extra-q10-optimization-audit-20260813) |
| `P1_G5_G7_HISTORY_REGRESSION_PROBE_20260818.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-p1-g5-g7-history-regression-probe-20260818`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-p1-g5-g7-history-regression-probe-20260818) |
| `PACKED_CERTIFICATE_ROW_STATE_PROBE_20260814.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-packed-certificate-row-state-probe-20260814`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-packed-certificate-row-state-probe-20260814) |
| `PAIR_SEEDED_PATH_GROWTH_PROBE_20260809.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-pair-seeded-path-growth-probe-20260809`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-pair-seeded-path-growth-probe-20260809) |
| `PATH_GROWTH_WITNESS_REUSE_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-path-growth-witness-reuse-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-path-growth-witness-reuse-negative-probe-20260810) |
| `PURCHASED_FARTHEST_ORDER_NEGATIVE_PROBE_20260811.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-purchased-farthest-order-negative-probe-20260811`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-purchased-farthest-order-negative-probe-20260811) |
| `PURCHASED_VERTEX_GROUP_DISTANCE_NEGATIVE_PROBE_20260811.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-purchased-vertex-group-distance-negative-probe-20260811`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-purchased-vertex-group-distance-negative-probe-20260811) |
| `QUEUE_POP_AND_EMPTY_READY_NEGATIVE_PROBE_20260816.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-queue-pop-and-empty-ready-negative-probe-20260816`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-queue-pop-and-empty-ready-negative-probe-20260816) |
| `REDUNDANT_LEAF_PRUNING_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-redundant-leaf-pruning-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-redundant-leaf-pruning-negative-probe-20260809) |
| `REMAINING_A2_RANKING_NEGATIVE_PROBE_20260808.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-remaining-a2-ranking-negative-probe-20260808`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-remaining-a2-ranking-negative-probe-20260808) |
| `RESIDUAL_ANCHOR_FIRST_ORDER_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-anchor-first-order-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-anchor-first-order-negative-probe-20260810) |
| `RESIDUAL_CLOSURE_SAME_ORDER_P2_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-closure-same-order-p2-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-closure-same-order-p2-probe-20260810) |
| `RESIDUAL_DUAL_ORDER_ENSEMBLE_COMPLEMENT_RETEST_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-dual-order-ensemble-complement-retest-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-dual-order-ensemble-complement-retest-negative-probe-20260810) |
| `RESIDUAL_DUAL_ORDER_ENSEMBLE_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-dual-order-ensemble-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-dual-order-ensemble-negative-probe-20260810) |
| `RESIDUAL_LEAST_PAID_ORDER_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-least-paid-order-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-least-paid-order-negative-probe-20260810) |
| `RESIDUAL_SINGLE_DEMAND_RADIUS_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-single-demand-radius-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-residual-single-demand-radius-negative-probe-20260810) |
| `REUSABLE_ORDINARY_HEAP_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-reusable-ordinary-heap-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-reusable-ordinary-heap-negative-probe-20260810) |
| `ROOTED_COMPONENT_COVER_FUTURE_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-rooted-component-cover-future-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-rooted-component-cover-future-negative-probe-20260810) |
| `ROOTED_ENDPOINT_FLOOR_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-rooted-endpoint-floor-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-rooted-endpoint-floor-negative-probe-20260809) |
| `ROW_LAYOUT_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-row-layout-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-row-layout-negative-probe-20260809) |
| `SCREENED_A1_TOUR_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-screened-a1-tour-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-screened-a1-tour-negative-probe-20260809) |
| `SHARED_ORDINARY_HALF_NEGATIVE_PROBE_20260815.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-shared-ordinary-half-negative-probe-20260815`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-shared-ordinary-half-negative-probe-20260815) |
| `STAGED_CACHE_PHYSICAL_REDUCTION_NEGATIVE_PROBE_20260816.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-staged-cache-physical-reduction-negative-probe-20260816`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-staged-cache-physical-reduction-negative-probe-20260816) |
| `STATE_EXPLOSION_NEGATIVE_PROBES_20260807.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-state-explosion-negative-probes-20260807`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-state-explosion-negative-probes-20260807) |
| `STRUCTURAL_OMITTED_HALF_ORKUT_G15_GATE_20260812.md` | [`METHOD_EVOLUTION_AND_CORRECTNESS.md#history-structural-omitted-half-orkut-g15-gate-20260812`](METHOD_EVOLUTION_AND_CORRECTNESS.md#history-structural-omitted-half-orkut-g15-gate-20260812) |
| `THIRD_PARTY.md` | [`DATA_BASELINES_AND_ARTIFACTS.md#history-third-party`](DATA_BASELINES_AND_ARTIFACTS.md#history-third-party) |
| `TOUR_ENDPOINT_SCREEN_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-tour-endpoint-screen-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-tour-endpoint-screen-negative-probe-20260810) |
| `TRIPLE_COMPLETION_SCHEDULING_NEGATIVE_PROBE_20260809.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-triple-completion-scheduling-negative-probe-20260809`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-triple-completion-scheduling-negative-probe-20260809) |
| `TRIPLE_PATH_SUPPORT_REUSE_NEGATIVE_PROBE_20260810.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-triple-path-support-reuse-negative-probe-20260810`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-triple-path-support-reuse-negative-probe-20260810) |
| `TRIPLE_SEEDED_P1_FIXED_COST_PROBE_20260810.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-triple-seeded-p1-fixed-cost-probe-20260810`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-triple-seeded-p1-fixed-cost-probe-20260810) |
| `TRIPLE_SEEDED_P1_FULL_GATE_20260811.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-triple-seeded-p1-full-gate-20260811`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-triple-seeded-p1-full-gate-20260811) |
| `TRIPLE_SEEDED_PATH_GROWTH_PROBE_20260809.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-triple-seeded-path-growth-probe-20260809`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-triple-seeded-path-growth-probe-20260809) |
| `communication/CROSS_GRAPH_STRUCTURAL_BOUND_SURVEY.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-communication-cross-graph-structural-bound-survey`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-communication-cross-graph-structural-bound-survey) |
| `communication/FIVE_HOUR_FAIR_YOUTUBE_OPTIMIZATION_PROBE.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-communication-five-hour-fair-youtube-optimization-probe`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-communication-five-hour-fair-youtube-optimization-probe) |
| `communication/TEN_HOUR_OURS_ONLY_YOUTUBE_PROBE.md` | [`ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-communication-ten-hour-ours-only-youtube-probe`](ACCEPTED_OPTIMIZATIONS_AND_GATES.md#history-communication-ten-hour-ours-only-youtube-probe) |
| `communication/YOUTUBE_STRUCTURAL_LOWER_BOUND_ANALYSIS.md` | [`REJECTED_AND_DEFERRED_DIRECTIONS.md#history-communication-youtube-structural-lower-bound-analysis`](REJECTED_AND_DEFERRED_DIRECTIONS.md#history-communication-youtube-structural-lower-bound-analysis) |

<a id="history-github-rendering-maintenance-20260820"></a>

## GitHub 数学渲染兼容性详记（迁移于 2026-08-20）

> 以下故障模式和人工网页核验步骤从旧版 `CODE_GUIDE.md` 迁入，供自动化维护者和 artifact 管理者追溯；代码阅读者只需遵守主文档中的简要规则。

### 原始维护规则

后续 LLM 或人工修改 Markdown 时必须遵守以下仓库级约定。它们不是 LaTeX 数学语义限制，而是 GitHub 当前 Markdown 渲染器的兼容性边界。

1. 块公式统一使用带 `math` info string 的 fenced block，不使用“首尾各一行 `$$`”的三行式写法。标准形态为：

   ````markdown
   ```math
   E = mc^2
   ```
   ````

2. 不得在行内或块公式中使用 `\operatorname` 或 `\operatorname*`。截至 2026-07-24，GitHub 会显示 “The following macros are not allowed: operatorname”，并把公式源文回退成灰色代码块。普通命名使用 `\mathrm{name}`；例如 `\mathrm{OPT}`、`\mathrm{dist}` 和 `\mathrm{clamp}`。当前渲染器也曾把语法完整的 `\begin{cases}...\end{cases}` 报成 “Missing `\end{cases}`”；本仓库因此把分段函数拆成多个独立 `math` block，不再使用 `cases` 环境。
3. 不要把含下划线的代码标识符塞进数学文本命令，例如不要写 `$S\subseteq\texttt{full\_mask}$`。GitHub 曾把其中的 `_` 送到文本模式并报 “`'_' allowed only in math mode`”。代码名应留在公式外，用 Markdown 行内代码表示；若确实需要数学记号，则改写为 `$M_{\mathrm{full}}$` 这一类结构。
4. 表格单元格中的行内公式不能直接写竖线定界，如 `$|S|$`；使用 `$\lvert S\rvert$`，否则 Markdown 会先把竖线解释为列分隔符。
5. 行内公式的开界 `$` 前必须有安全边界。GitHub 已实测会把紧跟中文标点或词内连字号的后续公式留成原文：不要写 `$D$、$A$、$H$` 或 `fixed-$U_0$`，而应写成 $D$、 $A$、 $H$ 以及“固定的 $U_0$”。注意“定界符数量配对”不能发现这类问题，必须同时检查边界和上传后的实际 MathML 数量。
6. 行内数学源码不得含裸星号 `*`。即使写成 `^{*}`，GitHub Markdown 仍可能把同一段中两个公式的星号跨定界符配成强调标签，导致两个公式都保留为源码；应写成 `^{\star}`。静态门禁会拒绝行内公式里的裸星号。
7. 每次提交前运行 `make validate-markdown` 或 `python3 tools/experiments/validate_markdown.py`。该门禁检查 UTF-8、围栏、行内定界符及安全左边界、表格公式、已确认的 GitHub 禁用宏，以及本地链接目标是否以精确大小写被 Git 跟踪；不能让一个只在 Windows 本地存在或仅靠大小写不敏感解析成功的路径通过。`make release` 已依赖该门禁。
8. 上传后不能只统计公式容器，因为失败公式同样会生成容器。必须在 GitHub 的实际渲染页面（或编辑器 **Preview**）检查所有含公式的文件，并确认每个 `.js-display-math` 和 `.js-inline-math` 都含实际 MathML `<math>` 子节点，任何 `math-renderer` 内均无可见 `.flash-error`、黄色错误框或灰色公式源码回退。不要把整页 `.flash-error` 数量当成判据：GitHub 页面可能自带隐藏的通用错误模板。错误文本既可能是 “The following macros are not allowed”，也可能是 “Missing ...” 或文本模式错误。若 GitHub 以后出现新失败模式，先改写公式，再把可静态识别的模式加入 `validate_markdown.py`。

语法依据见 [GitHub 数学表达式官方文档](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/writing-mathematical-expressions)；`\operatorname` 的实际限制见 [github/markup#1688](https://github.com/github/markup/issues/1688)。
