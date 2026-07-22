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

// Thin, input-only adapter around the CPU PrunedDP++ header shipped with the
// GPU4GST SIGMOD 2025 artifact.  The upstream algorithm source is unmodified.
void PrepareGraph(const Graph& graph);
SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gpu4gst_pruneddp
}  // namespace methods
}  // namespace gst

#endif  // GST_GPU4GST_PRUNEDDP_ARTIFACT_H
