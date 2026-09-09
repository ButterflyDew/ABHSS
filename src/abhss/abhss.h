#pragma once

#include <cstdint>
#include "../graph.h"

namespace abhss {

// 可行时返回最优权值；不可行时权值为 -1。states 统计首次接纳的 D/A/H 状态。
struct SolveResult {
    double best_weight = -1.0;
    bool feasible = false;
    std::uint64_t mask_vertex_states = 0;
};

// 同一求解器的两个固定配置：false 为 Base，true 为 Enhanced。
SolveResult SolveOneQuery(const Graph& graph, const Query& query, bool enhanced = false);

} // namespace abhss
