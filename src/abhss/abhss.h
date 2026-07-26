#ifndef ABHSS_PUBLIC_H
#define ABHSS_PUBLIC_H

#include <cstdint>

#include "../common/graph_io.h"
#include "../common/query_io.h"

namespace gst::methods::abhss
{

// 单条查询的返回值：可行时 best_weight 是精确最优权值，状态数统计首次进入 D/A/H 行的项。
struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    std::uint64_t mask_vertex_states = 0;
};

// 求解一条精确 GST 查询。enhanced=false 运行 Base，true 运行全部增强。
SolveResult SolveOneQuery(const Graph& graph, const Query& query, bool enhanced = false);

} // namespace gst::methods::abhss

#endif // ABHSS_PUBLIC_H
