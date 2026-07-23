#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../src/abhss/abhss.h"

namespace
{
using HeapItem = std::pair<double, int>;

void AddEdge(gst::Graph& graph, int u, int v, double weight)
{
    const int id = static_cast<int>(graph.edges.size());
    graph.edges.push_back({id, u, v, weight});
    graph.adj[u].push_back({v, id, weight});
    graph.adj[v].push_back({u, id, weight});
    graph.minimum_edge_weight = std::min(graph.minimum_edge_weight, weight);
    graph.m = static_cast<int>(graph.edges.size());
}

double ExactSubsetDp(const gst::Graph& graph, const gst::Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    if (!g)
        return 0.0;
    const int subset_count = 1 << g;
    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<std::vector<double>> dp(
        subset_count, std::vector<double>(graph.n + 1, infinity));

    for (int mask = 1; mask < subset_count; ++mask)
    {
        if (!(mask & (mask - 1)))
        {
            int group = 0;
            while (!(mask & (1 << group)))
                ++group;
            for (int terminal : query.groups[group])
                dp[mask][terminal] = 0.0;
        }
        else
        {
            const int pivot = mask & -mask;
            for (int left = (mask - 1) & mask;
                 left;
                 left = (left - 1) & mask)
            {
                const int right = mask ^ left;
                if (!right || !(left & pivot))
                    continue;
                for (int vertex = 1; vertex <= graph.n; ++vertex)
                    dp[mask][vertex] = std::min(
                        dp[mask][vertex], dp[left][vertex] + dp[right][vertex]);
            }
        }

        std::priority_queue<HeapItem,
                            std::vector<HeapItem>,
                            std::greater<HeapItem>> heap;
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            if (std::isfinite(dp[mask][vertex]))
                heap.push({dp[mask][vertex], vertex});
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != dp[mask][vertex])
                continue;
            for (const gst::AdjEdge& edge : graph.adj[vertex])
            {
                const double next = value + edge.w;
                if (next < dp[mask][edge.to])
                {
                    dp[mask][edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }
    }
    return *std::min_element(
        dp.back().begin() + 1, dp.back().end());
}

void Check(const char* method,
           const gst::methods::abhss::SolveResult& answer,
           double expected,
           int instance)
{
    if (!answer.feasible || std::fabs(answer.best_weight - expected) > 1e-9)
        throw std::runtime_error(
            std::string(method) + " differs from exact subset DP on instance " +
            std::to_string(instance) + ": expected=" +
            std::to_string(expected) + ", actual=" +
            std::to_string(answer.best_weight));
}

gst::Graph RandomConnectedGraph(std::mt19937& random, int n)
{
    gst::Graph graph;
    graph.n = n;
    graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
    graph.adj.assign(n + 1, {});
    std::vector<std::vector<unsigned char>> present(
        n + 1, std::vector<unsigned char>(n + 1));
    auto Weight = [&]()
    {
        const int draw = static_cast<int>(random() % 10);
        return draw < 2 ? 0.0 : 0.25 * (1 + static_cast<int>(random() % 20));
    };
    for (int vertex = 2; vertex <= n; ++vertex)
    {
        const int parent = 1 + static_cast<int>(random() % (vertex - 1));
        AddEdge(graph, parent, vertex, Weight());
        present[parent][vertex] = present[vertex][parent] = 1;
    }
    for (int u = 1; u <= n; ++u)
        for (int v = u + 1; v <= n; ++v)
            if (!present[u][v] && random() % 100 < 32)
            {
                AddEdge(graph, u, v, Weight());
                present[u][v] = present[v][u] = 1;
            }
    return graph;
}

gst::Query RandomQuery(std::mt19937& random, int n, int g)
{
    gst::Query query;
    query.groups.resize(g);
    for (std::vector<int>& group : query.groups)
    {
        const int size = 1 + static_cast<int>(random() % std::min(4, n));
        while (static_cast<int>(group.size()) < size)
        {
            const int vertex = 1 + static_cast<int>(random() % n);
            if (std::find(group.begin(), group.end(), vertex) == group.end())
                group.push_back(vertex);
        }
        std::sort(group.begin(), group.end());
    }
    return query;
}
}  // namespace

int main()
{
    std::mt19937 random(0xAB455u);
    constexpr int kInstances = 120;
    for (int instance = 0; instance < kInstances; ++instance)
    {
        const int n = 4 + static_cast<int>(random() % 6);
        const int g = 2 + static_cast<int>(random() % std::min(5, n - 1));
        const gst::Graph graph = RandomConnectedGraph(random, n);
        const gst::Query query = RandomQuery(random, n, g);
        const double expected = ExactSubsetDp(graph, query);
        Check("ABHSS-Light",
              gst::methods::abhss::SolveLightOneQuery(graph, query),
              expected,
              instance);
        Check("ABHSS-Heavy",
              gst::methods::abhss::SolveHeavyOneQuery(graph, query),
              expected,
              instance);
        Check("ABHSS-Heavy-Forward",
              gst::methods::abhss::SolveHeavyForwardOneQuery(graph, query),
              expected,
              instance);
    }
    std::cout << "ABHSS variants matched exact subset DP on "
              << kInstances << " deterministic random instances\n";
    return 0;
}
