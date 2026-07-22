#ifndef ABHSS_DUAL_CUT_H
#define ABHSS_DUAL_CUT_H

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <queue>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "../common/float_compare.h"
#include "../common/graph_io.h"
#include "../common/query_io.h"

namespace gst::methods::dual_cut
{
// Heavy 版使用的有向割对偶势。类中只保留冻结算法实际调用的路径：
// changed-arc 构造、势查询、primal 见证读取和 residual 生命周期管理。
class DualCutPotential
{
public:
    // 依次为各组分配有向边容量。每轮从上一轮真正改写过的弧启动
    // residual 最短路修复；最后在零 residual 弧上恢复一棵 primal 树。
    void BuildKeepingResidualChangedArcsWithPrimalEdges(
        const Graph& graph,
        const Query& query,
        const std::vector<std::vector<double>>& group_distance,
        int root)
    {
        BuildChangedArcs(graph, query, group_distance, root);
        primal_edge_words_.assign((static_cast<size_t>(graph.m) + 63) / 64, 0);
        primal_upper_ = RecoverPrimal(
            graph, query, root, residual_, primal_edge_words_);
    }

    // 释放只在预处理阶段使用的 2m directed residual；势和 primal 边仍保留。
    void ReleaseResidual()
    {
        residual_.clear();
        residual_.shrink_to_fit();
    }

    const std::vector<double>& Residual() const
    {
        return residual_;
    }

    const std::vector<std::uint64_t>& PrimalEdgeWords() const
    {
        return primal_edge_words_;
    }

    // 返回 mask 中所有组势在 vertex 处的和，即 Heavy 的 directed-cut 下界。
    double At(int vertex, int mask) const
    {
        double value = 0.0;
        while (mask)
        {
            const int bit = mask & -mask;
            value += potential_[FirstBit(bit)][vertex];
            mask ^= bit;
        }
        return value;
    }

    double GroupAt(int vertex, int group) const
    {
        return potential_[group][vertex];
    }

    double PrimalUpper() const
    {
        return primal_upper_;
    }

private:
    // 固定按根到组的原始距离递减处理各组。原始组距离对未改写弧满足
    // 三角不等式，因此第 t 轮只需检查此前势函数改变过的弧，再正常传播。
    void BuildChangedArcs(
        const Graph& graph,
        const Query& query,
        const std::vector<std::vector<double>>& group_distance,
        int root)
    {
        using HeapItem = std::pair<double, int>;

        const int n = graph.n;
        const int g = static_cast<int>(query.groups.size());
        potential_.assign(g, std::vector<double>(n + 1));

        std::vector<int> order(g);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int left, int right)
        {
            if (group_distance[left][root] != group_distance[right][root])
                return group_distance[left][root] > group_distance[right][root];
            return left < right;
        });

        residual_.assign(static_cast<size_t>(2) * graph.m, 0.0);
        for (const UndirectedEdge& edge : graph.edges)
        {
            residual_[2 * edge.id] = edge.w;
            residual_[2 * edge.id + 1] = edge.w;
        }

        std::vector<double> distance(n + 1);
        std::vector<double> capped(n + 1);
        std::vector<std::uint64_t> changed_arc_words(
            (static_cast<size_t>(2) * graph.m + 63) / 64);

        auto MarkChangedArc = [&](int arc)
        {
            std::uint64_t& word = changed_arc_words[static_cast<size_t>(arc) >> 6];
            const std::uint64_t bit = std::uint64_t{1} << (arc & 63);
            if (!(word & bit))
                word |= bit;
        };

        for (int order_index = 0; order_index < g; ++order_index)
        {
            const int group = order[order_index];
            distance = group_distance[group];
            std::priority_queue<HeapItem,
                                std::vector<HeapItem>,
                                std::greater<HeapItem>> heap;

            if (order_index > 0)
            {
                // 对每条已改写弧检查一次 Bellman 松弛。这里的 residual 弧
                // 方向与“到组距离”的传播方向相反，所以 source/target 对调。
                for (size_t word_index = 0;
                     word_index < changed_arc_words.size();
                     ++word_index)
                {
                    std::uint64_t bits = changed_arc_words[word_index];
                    while (bits)
                    {
#if defined(_MSC_VER)
                        unsigned long offset = 0;
                        _BitScanForward64(&offset, bits);
#else
                        const int offset = __builtin_ctzll(bits);
#endif
                        bits &= bits - 1;
                        const int arc =
                            static_cast<int>(word_index * 64 + offset);
                        if (arc >= 2 * graph.m)
                            continue;

                        const UndirectedEdge& edge = graph.edges[arc / 2];
                        const bool forward =
                            ArcIndex(edge.id, edge.u, edge.v) == arc;
                        const int target = forward ? edge.u : edge.v;
                        const int source = forward ? edge.v : edge.u;
                        const double next = residual_[arc] + distance[source];
                        if (next < distance[target])
                        {
                            distance[target] = next;
                            heap.push({next, target});
                        }
                    }
                }
            }

            while (!heap.empty())
            {
                const auto [value, vertex] = heap.top();
                heap.pop();
                if (value != distance[vertex])
                    continue;
                for (const AdjEdge& edge : graph.adj[vertex])
                {
                    const int arc = ArcIndex(edge.edge_id, edge.to, vertex);
                    const double next = value + residual_[arc];
                    if (next < distance[edge.to])
                    {
                        distance[edge.to] = next;
                        heap.push({next, edge.to});
                    }
                }
            }

            const double root_distance = distance[root];
            for (int vertex = 1; vertex <= n; ++vertex)
            {
                capped[vertex] = std::min(distance[vertex], root_distance);
                potential_[group][vertex] = capped[vertex];
            }

            for (const UndirectedEdge& edge : graph.edges)
            {
                const double forward =
                    std::max(0.0, capped[edge.u] - capped[edge.v]);
                const double backward =
                    std::max(0.0, capped[edge.v] - capped[edge.u]);
                const int forward_arc = ArcIndex(edge.id, edge.u, edge.v);
                const int backward_arc = ArcIndex(edge.id, edge.v, edge.u);
                if (forward > 0.0)
                    MarkChangedArc(forward_arc);
                if (backward > 0.0)
                    MarkChangedArc(backward_arc);
                residual_[forward_arc] =
                    std::max(0.0, residual_[forward_arc] - forward);
                residual_[backward_arc] =
                    std::max(0.0, residual_[backward_arc] - backward);
            }
        }
    }

    // 在 residual 为零的有向弧上，从 root 逐次连接尚未覆盖的组。
    // Dijkstra 的距离仍按原边权计价，恢复出的 edge bitmap 是合法 primal 树。
    static double RecoverPrimal(
        const Graph& graph,
        const Query& query,
        int root,
        const std::vector<double>& residual,
        std::vector<std::uint64_t>& primal_edge_words)
    {
        using HeapItem = std::pair<double, int>;

        const int g = static_cast<int>(query.groups.size());
        const int full_mask = (1 << g) - 1;
        std::vector<int> color(graph.n + 1);
        for (int group = 0; group < g; ++group)
            for (int vertex : query.groups[group])
                color[vertex] |= 1 << group;

        std::vector<char> in_tree(graph.n + 1);
        std::vector<int> parent(graph.n + 1);
        std::vector<int> parent_edge(graph.n + 1, -1);
        std::vector<double> distance(graph.n + 1, fp::kInf);
        in_tree[root] = 1;
        int covered = color[root];
        double cost = 0.0;

        while (covered != full_mask)
        {
            std::priority_queue<HeapItem,
                                std::vector<HeapItem>,
                                std::greater<HeapItem>> heap;
            std::fill(distance.begin(), distance.end(), fp::kInf);
            for (int vertex = 1; vertex <= graph.n; ++vertex)
            {
                if (!in_tree[vertex])
                    continue;
                distance[vertex] = 0.0;
                parent[vertex] = 0;
                heap.push({0.0, vertex});
            }

            int found = 0;
            while (!heap.empty())
            {
                const auto [value, vertex] = heap.top();
                heap.pop();
                if (value != distance[vertex])
                    continue;
                if (color[vertex] & (full_mask ^ covered))
                {
                    found = vertex;
                    break;
                }
                for (const AdjEdge& edge : graph.adj[vertex])
                {
                    const int arc = ArcIndex(edge.edge_id, vertex, edge.to);
                    const double tolerance = 1e-10 * std::max(1.0, edge.w);
                    if (residual[arc] > tolerance)
                        continue;
                    const double next = value + edge.w;
                    if (next < distance[edge.to])
                    {
                        distance[edge.to] = next;
                        parent[edge.to] = vertex;
                        parent_edge[edge.to] = edge.edge_id;
                        heap.push({next, edge.to});
                    }
                }
            }
            if (!found)
                return fp::kInf;

            cost += distance[found];
            for (int vertex = found; vertex && !in_tree[vertex];
                 vertex = parent[vertex])
            {
                const int edge_id = parent_edge[vertex];
                primal_edge_words[static_cast<size_t>(edge_id) >> 6] |=
                    std::uint64_t{1} << (edge_id & 63);
                in_tree[vertex] = 1;
                covered |= color[vertex];
            }
        }
        return cost;
    }

    static int FirstBit(int mask)
    {
#if defined(_MSC_VER)
        unsigned long index = 0;
        _BitScanForward(&index, static_cast<unsigned long>(mask));
        return static_cast<int>(index);
#else
        return __builtin_ctz(static_cast<unsigned int>(mask));
#endif
    }

    static int ArcIndex(int edge_id, int from, int to)
    {
        return 2 * edge_id + (from < to ? 0 : 1);
    }

    std::vector<std::vector<double>> potential_;
    std::vector<double> residual_;
    std::vector<std::uint64_t> primal_edge_words_;
    double primal_upper_ = fp::kInf;
};
}  // namespace gst::methods::dual_cut

#endif  // ABHSS_DUAL_CUT_H
