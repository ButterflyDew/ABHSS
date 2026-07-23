#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "../src/abhss/abhss.h"
#include "../src/pruneddp/pruneddp.h"

namespace
{
/** @brief 向手工测试图加入一条无向边，并同步维护所有 Graph 统计字段。 */
void AddEdge(gst::Graph& graph, int u, int v, double weight)
{
    const int id = static_cast<int>(graph.edges.size());
    graph.edges.push_back({id, u, v, weight});
    graph.adj[u].push_back({v, id, weight});
    graph.adj[v].push_back({u, id, weight});
    graph.minimum_edge_weight = std::min(graph.minimum_edge_weight, weight);
    graph.m = static_cast<int>(graph.edges.size());
}

/** @brief 构造七点单位权路径；四个终端组的唯一最优连通子图权重为 6。 */
gst::Graph BuildPathGraph()
{
    gst::Graph graph;
    graph.n = 7;
    graph.adj.assign(graph.n + 1, {});
    graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
    for (int vertex = 1; vertex < graph.n; ++vertex)
        AddEdge(graph, vertex, vertex + 1, 1.0);
    return graph;
}

/** @brief 同时检查答案和同配置重复运行的状态计数确定性。 */
template <class Result>
void CheckRepeated(const char* label,
                   const Result& first,
                   const Result& second,
                   double expected)
{
    if (!first.feasible || !second.feasible ||
        std::fabs(first.best_weight - expected) > 1e-12 ||
        std::fabs(second.best_weight - expected) > 1e-12)
        throw std::runtime_error(std::string(label) + " returned a wrong weight");
    if (first.mask_vertex_states != second.mask_vertex_states)
        throw std::runtime_error(
            std::string(label) + " produced an unstable state count");
}
}  // namespace

/**
 * @brief 回归两种正式方法的 `(mask,v)` 计数契约。
 *
 * 测试验证 ABHSS Base/Enhanced 重复运行稳定（强上界在该路径上可使计数
 * 合法为 0）；PrunedDP++ Hash/Dense 后端发现相同状态集合，从而证明统计
 * 的是 present 项而非 Dense 预分配容量；平凡查询必须返回 0。ABHSS 非零
 * 计数路径由 144 个随机精确性实例的累计断言覆盖。
 */
int main()
{
    const gst::Graph graph = BuildPathGraph();
    gst::Query query;
    query.groups = {{1}, {3}, {5}, {7}};

    using gst::methods::abhss::SolveOneQuery;
    const auto base_options = gst::methods::abhss::SolveOptions::Base();
    const auto enhanced_options = gst::methods::abhss::SolveOptions::Enhanced();
    const auto base_first = SolveOneQuery(graph, query, base_options);
    const auto base_second = SolveOneQuery(graph, query, base_options);
    CheckRepeated("ABHSS-Base", base_first, base_second, 6.0);
    const auto enhanced_first = SolveOneQuery(graph, query, enhanced_options);
    const auto enhanced_second = SolveOneQuery(graph, query, enhanced_options);
    CheckRepeated("ABHSS-Enhanced", enhanced_first, enhanced_second, 6.0);

    gst::methods::pruned_dp::PrunedDpOptions hash_options;
    const auto pruned_hash_first =
        gst::methods::pruned_dp::SolveOneQuery(graph, query, hash_options);
    const auto pruned_hash_second =
        gst::methods::pruned_dp::SolveOneQuery(graph, query, hash_options);
    CheckRepeated(
        "PrunedDP++-Hash", pruned_hash_first, pruned_hash_second, 6.0);
    if (!pruned_hash_first.mask_vertex_states)
        throw std::runtime_error("PrunedDP++ failed to register discovered states");

    auto dense_options = hash_options;
    dense_options.state_storage = gst::methods::pruned_dp::StateStorage::Dense;
    const auto pruned_dense =
        gst::methods::pruned_dp::SolveOneQuery(graph, query, dense_options);
    if (!pruned_dense.feasible ||
        std::fabs(pruned_dense.best_weight - 6.0) > 1e-12 ||
        pruned_dense.mask_vertex_states != pruned_hash_first.mask_vertex_states)
        throw std::runtime_error(
            "PrunedDP++ state count depends on storage capacity/backend");

    const gst::Query empty_query;
    if (SolveOneQuery(graph, empty_query, base_options).mask_vertex_states != 0 ||
        gst::methods::pruned_dp::SolveOneQuery(
            graph, empty_query, hash_options).mask_vertex_states != 0)
        throw std::runtime_error("trivial query reported nonzero main states");

    std::cout << "ABHSS and PrunedDP++ mask-vertex state accounting passed\n";
    return 0;
}
