#ifndef ABHSS_PUBLIC_H
#define ABHSS_PUBLIC_H

#include "../common/graph_io.h"
#include "../common/query_io.h"

namespace gst::methods::abhss
{
// 两个发行入口使用相同的最小返回值：有解时 best_weight 为精确最优值，
// 无解时 feasible=false 且 best_weight=-1。
struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
};

// 轻量入口：有界组距离、提前 A1、根路径见证树和完整前向 A。
SolveResult SolveLightOneQuery(const Graph& graph, const Query& query);
// 重量入口：eager directed-cut、dual primal 见证、低 A/高 H 和补集转置。
SolveResult SolveHeavyOneQuery(const Graph& graph, const Query& query);
// Heavy 消融：保留同一 directed-cut/ordinary 预处理，但用完整前向 A
// 代替低 A、高 H 和补集转置。
SolveResult SolveHeavyForwardOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::abhss

#endif
