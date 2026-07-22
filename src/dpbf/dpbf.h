#ifndef GST_DPBF_H
#define GST_DPBF_H

#include "../common/graph_io.h"
#include "../common/query_io.h"

namespace gst::methods::dpbf
{

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
};

// Canonical Dreyfus-Wagner/DPBF recurrence for edge-weighted GST.  This
// implementation is deliberately a transparent correctness baseline, not a
// large-graph competitor.
SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::dpbf

#endif  // GST_DPBF_H
