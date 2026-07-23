#include "gpu4gst_pruneddp.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <climits>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// artifact header 使用未限定的 STL 名称；只在本 adapter translation unit
// 内兼容它的原始约定。
using namespace std;

#include <graph_v_of_v_idealID/graph_v_of_v_idealID.h>
#include <graph_hash_of_mixed_weighted/graph_hash_of_mixed_weighted.h>
#include <graph_hash_of_mixed_weighted_sum_of_nw_ec.h>
#include <CPUNONHOP.h>

namespace gst
{
namespace methods
{
namespace gpu4gst_pruneddp
{
namespace
{
/** @brief 无损检查并转换 artifact 只接受的非负 int 边权。 */
int ExactIntegerWeight(double value)
{
    const double rounded = std::round(value);
    if (value < 0.0 || rounded > static_cast<double>(INT_MAX) ||
        std::abs(value - rounded) > 1e-9)
    {
        throw std::runtime_error(
            "GPU4GST artifact PrunedDP++ accepts nonnegative integer edge weights only.");
    }
    return static_cast<int>(rounded);
}

const Graph* cached_source = nullptr;
graph_v_of_v_idealID cached_input_graph;
}  // namespace

/** @brief 缓存当前 Graph 的一次性 artifact 表示，避免每条查询重复转换。 */
void PrepareGraph(const Graph& graph)
{
    if (cached_source == &graph)
        return;
    graph_v_of_v_idealID converted(static_cast<size_t>(graph.n));
    for (const UndirectedEdge& edge : graph.edges)
    {
        graph_v_of_v_idealID_add_edge(
            converted, edge.u - 1, edge.v - 1, ExactIntegerWeight(edge.w));
    }
    cached_input_graph.swap(converted);
    cached_source = &graph;
}

/** @brief 转换组成员并调用未修改的 GPU4GST CPU PrunedDP++。 */
SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    const int group_count = static_cast<int>(query.groups.size());
    if (group_count <= 0)
        return {0.0, true};
    if (group_count >= 15)
    {
        throw std::runtime_error(
            "The unmodified GPU4GST artifact PrunedDP++ supports at most 14 groups.");
    }

    PrepareGraph(graph);

    // 虽然类型名包含 graph，作者读取器实际构造的是单向组成员列表，
    // 不添加反向边；adapter 必须保持这一输入语义。
    graph_v_of_v_idealID group_graph(static_cast<size_t>(group_count));
    std::unordered_set<int> compulsory_groups;
    for (int group = 0; group < group_count; ++group)
    {
        compulsory_groups.insert(group);
        for (const int member : query.groups[group])
        {
            if (member < 1 || member > graph.n)
                throw std::runtime_error("Query contains a vertex outside the graph.");
            group_graph[group].push_back({member - 1, 1});
        }
        std::sort(group_graph[group].begin(), group_graph[group].end());
    }

    int artifact_memory = 0;
    double artifact_seconds = 0.0;
    records artifact_records{};
    graph_hash_of_mixed_weighted solution =
        graph_v_of_v_idealID_PrunedDPPlusPlus(
            cached_input_graph,
            group_graph,
            compulsory_groups,
            1,
            artifact_memory,
            artifact_seconds,
            artifact_records);
    if (solution.hash_of_vectors.empty())
        return {};
    return {
        static_cast<double>(graph_hash_of_mixed_weighted_sum_of_ec(solution)),
        true};
}

}  // namespace gpu4gst_pruneddp
}  // namespace methods
}  // namespace gst
