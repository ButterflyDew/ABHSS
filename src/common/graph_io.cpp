#include "graph_io.h"
#include "fast_numeric_reader.h"

#include <stdexcept>
#include <vector>

namespace gst
{
namespace
{
/** @brief 在已构造的无向邻接表上建立稠密连通分量编号。 */
void BuildConnectedComponentIndex(Graph& graph)
{
    graph.component_of.assign(graph.n + 1, -1);
    graph.component_count = 0;
    std::vector<int> frontier;
    for (int root = 1; root <= graph.n; ++root)
    {
        if (graph.component_of[root] >= 0)
            continue;
        frontier.clear();
        frontier.push_back(root);
        graph.component_of[root] = graph.component_count;
        for (size_t index = 0; index < frontier.size(); ++index)
        {
            const int vertex = frontier[index];
            for (const AdjEdge& edge : graph.adj[vertex])
            {
                if (graph.component_of[edge.to] >= 0)
                    continue;
                graph.component_of[edge.to] = graph.component_count;
                frontier.push_back(edge.to);
            }
        }
        ++graph.component_count;
    }
}
} // namespace

/** @brief 用 8 MiB 数字缓冲读取 graph.txt，并按精确度数一次分配邻接表。 */
Graph LoadGraph(const std::string& graph_folder)
{
    const std::string graph_path = graph_folder + "/graph.txt";
    io::FastNumericReader input(graph_path);

    Graph g;
    if (!input.IsOpen() || !input.ReadInt(g.n) || !input.ReadInt(g.m))
        throw std::runtime_error("Cannot read " + graph_path);

    // 第一遍保存紧凑原边并统计度数，使每个邻接数组只分配一次。
    g.edges.resize(g.m);
    std::vector<size_t> degree(g.n + 1);
    for (int i = 0; i < g.m; ++i)
    {
        int u = 0;
        int v = 0;
        double w = 0.0;
        input.ReadInt(u);
        input.ReadInt(v);
        input.ReadDouble(w);
        UndirectedEdge& edge = g.edges[i];
        edge = {i, u, v, w};
        ++degree[u];
        ++degree[v];
    }

    // 第二遍按精确容量构造双向邻接表，避免大图上的反复扩容与复制。
    g.adj.resize(g.n + 1);
    for (int vertex = 1; vertex <= g.n; ++vertex)
        g.adj[vertex].reserve(degree[vertex]);
    for (const UndirectedEdge& edge : g.edges)
    {
        g.adj[edge.u].push_back({edge.v, edge.id, edge.w});
        g.adj[edge.v].push_back({edge.u, edge.id, edge.w});
    }
    BuildConnectedComponentIndex(g);
    return g;
}

} // namespace gst
