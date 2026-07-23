#include "basic_plus.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <list>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// 上游 header-only 库使用多个未加 std:: 的 STL 名称。兼容声明仅限本
// adapter translation unit，不修改任何上游源码。
using namespace std;

#include <graph_hash_of_mixed_weighted/graph_hash_of_mixed_weighted.h>
#include <graph_hash_of_mixed_weighted/graph_hash_of_mixed_weighted_minimum_spanning_tree.h>
#include <graph_hash_of_mixed_weighted/graph_hash_of_mixed_weighted_MST_postprocessing.h>
#include <graph_hash_of_mixed_weighted/graph_hash_of_mixed_weighted_Basic_and_BasicPlus_vertex_edge_weighted.h>
#include <graph_hash_of_mixed_weighted/graph_hash_of_mixed_weighted_sum_of_nw_ec.h>

namespace gst
{
namespace methods
{
namespace basic_plus
{

/** @brief 转换统一图/组接口并以作者精确参数调用 Basic+。 */
SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    const int group_count = static_cast<int>(query.groups.size());
    if (group_count <= 0)
        return {0.0, true};
    if (group_count >= 15)
    {
        throw std::runtime_error(
            "The unmodified upstream Basic+ implementation supports at most 14 groups.");
    }

    graph_hash_of_mixed_weighted input_graph;
    for (int vertex = 1; vertex <= graph.n; ++vertex)
        graph_hash_of_mixed_weighted_add_vertex(input_graph, vertex, 0.0);
    for (const UndirectedEdge& edge : graph.edges)
        graph_hash_of_mixed_weighted_add_edge(input_graph, edge.u, edge.v, edge.w);

    graph_hash_of_mixed_weighted group_graph;
    std::unordered_set<int> compulsory_group_vertices;
    for (int group = 0; group < group_count; ++group)
    {
        const int dummy_vertex = graph.n + group + 1;
        compulsory_group_vertices.insert(dummy_vertex);
        graph_hash_of_mixed_weighted_add_vertex(group_graph, dummy_vertex, 0.0);
        for (const int member : query.groups[group])
        {
            if (member < 1 || member > graph.n)
                throw std::runtime_error("Query contains a vertex outside the graph.");
            graph_hash_of_mixed_weighted_add_edge(
                group_graph, dummy_vertex, member, 0.0);
        }
    }

    // 作者 driver 的纯边权 GST 精确设置：lambda=1、
    // maximum_return_app_ratio=1。
    graph_hash_of_mixed_weighted solution =
        graph_hash_of_mixed_weighted_BasicProgressivePlus_vertex_edge_weighted_ProgressiveReturn(
            input_graph, group_graph, compulsory_group_vertices, 1.0, 1.0);
    const double cost = graph_hash_of_mixed_weighted_sum_of_ec(solution);
    return {cost, true};
}

}  // namespace basic_plus
}  // namespace methods
}  // namespace gst
