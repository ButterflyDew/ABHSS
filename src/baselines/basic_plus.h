#ifndef GST_BASIC_PLUS_H
#define GST_BASIC_PLUS_H

#include "../common/graph_io.h"
#include "../common/query_io.h"

namespace gst
{
namespace methods
{
namespace basic_plus
{

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
};

// Adapter around the authors' PVLDB 2021 Basic+ implementation.  Setting
// lambda=1 and the requested ratio to 1 gives the edge-only exact mode.
SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace basic_plus
}  // namespace methods
}  // namespace gst

#endif  // GST_BASIC_PLUS_H
