#include "dpbf.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace gst::methods::dpbf
{
namespace
{
constexpr std::size_t kMaximumDpCells = 80'000'000;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

using QueueEntry = std::pair<double, int>;

void ShortestPathClosure(const Graph& graph, std::vector<double>& distance)
{
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
    for (int vertex = 1; vertex <= graph.n; ++vertex)
    {
        if (std::isfinite(distance[vertex]))
            queue.emplace(distance[vertex], vertex);
    }

    while (!queue.empty())
    {
        const auto [current_distance, vertex] = queue.top();
        queue.pop();
        if (current_distance != distance[vertex])
            continue;

        for (const AdjEdge& edge : graph.adj[vertex])
        {
            const double candidate = current_distance + edge.w;
            if (candidate < distance[edge.to])
            {
                distance[edge.to] = candidate;
                queue.emplace(candidate, edge.to);
            }
        }
    }
}
}  // namespace

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    const int group_count = static_cast<int>(query.groups.size());
    if (group_count <= 0)
        return {0.0, true};
    if (group_count > 16)
        throw std::runtime_error("DPBF correctness baseline supports at most 16 groups.");

    const int state_count = 1 << group_count;
    const std::size_t cells = static_cast<std::size_t>(state_count) *
                              static_cast<std::size_t>(graph.n + 1);
    if (cells > kMaximumDpCells)
    {
        throw std::runtime_error(
            "DPBF dense table would exceed its 80M-cell safety limit; "
            "this baseline is restricted to the correctness/SteinLib panels.");
    }

    std::vector<std::vector<double>> dp(
        state_count, std::vector<double>(static_cast<std::size_t>(graph.n + 1), kInfinity));

    for (int group = 0; group < group_count; ++group)
    {
        for (const int vertex : query.groups[group])
        {
            if (vertex < 1 || vertex > graph.n)
                throw std::runtime_error("Query contains a vertex outside the graph.");
            dp[1 << group][vertex] = 0.0;
        }
    }

    const int full_mask = state_count - 1;
    for (int mask = 1; mask <= full_mask; ++mask)
    {
        // Join two rooted partial trees.  Processing only one of each
        // unordered submask pair halves the work without changing the DP.
        for (int left = (mask - 1) & mask; left != 0; left = (left - 1) & mask)
        {
            const int right = mask ^ left;
            if (right == 0 || left > right)
                continue;
            for (int vertex = 1; vertex <= graph.n; ++vertex)
            {
                const double candidate = dp[left][vertex] + dp[right][vertex];
                if (candidate < dp[mask][vertex])
                    dp[mask][vertex] = candidate;
            }
        }

        // Growing a rooted partial tree along shortest paths completes the
        // standard DPBF recurrence for nonnegative edge weights.
        ShortestPathClosure(graph, dp[mask]);
    }

    const double best = *std::min_element(dp[full_mask].begin() + 1,
                                          dp[full_mask].end());
    return std::isfinite(best) ? SolveResult{best, true}
                               : SolveResult{-1.0, false};
}

}  // namespace gst::methods::dpbf
