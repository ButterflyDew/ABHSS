#ifndef GST_GPU4GST_PRUNEDDP_ARTIFACT_H
#define GST_GPU4GST_PRUNEDDP_ARTIFACT_H

#include "../common/graph_io.h"
#include "../common/query_io.h"

namespace gst
{
namespace methods
{
namespace gpu4gst_pruneddp
{

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
};

/** @brief 把统一 Graph 一次转换为 GPU4GST artifact 的 CPU 邻接表示。 */
void PrepareGraph(const Graph& graph);
/**
 * @brief 调用 GPU4GST SIGMOD 2025 artifact 随附的 CPU PrunedDP++ header。
 *
 * adapter 只做输入/输出转换，不修改上游算法源；其整数权重和 g 上限由入口
 * 显式检查，因此只参加适用子集校准与 correctness audit。
 */
SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gpu4gst_pruneddp
}  // namespace methods
}  // namespace gst

#endif  // GST_GPU4GST_PRUNEDDP_ARTIFACT_H
