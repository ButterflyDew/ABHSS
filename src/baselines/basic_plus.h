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

/** @brief 调用 PVLDB 2021 作者 Basic+；lambda=1、ratio=1 为纯边权精确模式。 */
SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace basic_plus
}  // namespace methods
}  // namespace gst

#endif  // GST_BASIC_PLUS_H
