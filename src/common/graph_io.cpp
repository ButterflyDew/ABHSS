#include "graph_io.h"
#include "fast_numeric_reader.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace gst
{
namespace
{
/**
 * @brief 在已构造的无向邻接表上一次性建立稠密连通分量编号。
 *
 * 这一步属于共同图加载而不是查询求解。按顶点递增选择分量根、按邻接插入
 * 顺序遍历，可保证跨平台编号稳定；编号本身只用于可行性判定，不影响答案。
 */
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
}  // namespace

std::vector<std::string> ListGraphFolders(const std::string& data_root)
{
    std::vector<std::string> folders;
    if (!fs::exists(data_root))
    {
        return folders;
    }
    for (const auto& entry : fs::directory_iterator(data_root))
    {
        if (entry.is_directory())
        {
            folders.push_back(entry.path().filename().string());
        }
    }
    std::sort(folders.begin(), folders.end());
    return folders;
}

std::string ResolveGraphFolder(const std::string& data_root, const std::string& graph_selector)
{
    if (!graph_selector.empty())
    {
        const fs::path direct_graph_folder = fs::path(graph_selector);
        if (fs::exists(direct_graph_folder) && fs::is_directory(direct_graph_folder))
        {
            return direct_graph_folder.string();
        }
    }

    const auto folders = ListGraphFolders(data_root);
    if (folders.empty())
    {
        throw std::runtime_error("No graph folders found under data root: " + data_root);
    }

    // 未显式指定图时，优先选择 example，便于快速测试。
    if (graph_selector.empty())
    {
        for (const auto& name : folders)
        {
            if (name == "example")
            {
                return (fs::path(data_root) / name).string();
            }
        }
        return (fs::path(data_root) / folders.front()).string();
    }

    bool is_number = !graph_selector.empty() &&
                     std::all_of(graph_selector.begin(), graph_selector.end(), [](char c)
                     {
                         return c >= '0' && c <= '9';
                     });
    if (is_number)
    {
        int idx = std::stoi(graph_selector);
        if (idx < 1 || idx > static_cast<int>(folders.size()))
        {
            throw std::runtime_error("Graph index out of range: " + graph_selector);
        }
        return (fs::path(data_root) / folders[idx - 1]).string();
    }

    for (const auto& name : folders)
    {
        if (name == graph_selector)
        {
            return (fs::path(data_root) / name).string();
        }
    }
    throw std::runtime_error("Graph folder not found: " + graph_selector);
}

Graph LoadGraphFromFolder(const std::string& graph_folder)
{
    fs::path graph_path = fs::path(graph_folder) / "graph.txt";
    if (!fs::exists(graph_path))
    {
        fs::path graph_path_alt = fs::path(graph_folder) / "Graph.txt";
        if (fs::exists(graph_path_alt))
        {
            graph_path = graph_path_alt;
        }
        else
        {
            throw std::runtime_error("graph.txt / Graph.txt not found under: " + graph_folder);
        }
    }

    io::FastNumericReader input(graph_path);
    if (!input.IsOpen())
    {
        throw std::runtime_error("Failed to open graph file: " + graph_path.string());
    }

    Graph g;
    if (!input.ReadInt(g.n) || !input.ReadInt(g.m) || g.n <= 0 || g.m < 0)
    {
        throw std::runtime_error("Invalid graph header in file: " + graph_path.string());
    }

    // 第一阶段只保存原边并统计精确度数。这样每个邻接 vector 只分配一次，
    // 避免大图上逐边 push 导致的多轮扩容、复制与额外容量。
    g.edges.reserve(g.m);
    // size_t 使自环的两个邻接项也能安全计数；若使用
    // int，在极端合法 header 中同一顶点的 2m 度数可溢出。
    std::vector<size_t> degree(g.n + 1);
    for (int i = 0; i < g.m; ++i)
    {
        int u = 0;
        int v = 0;
        double w = 0.0;
        if (!input.ReadInt(u) || !input.ReadInt(v) || !input.ReadDouble(w))
        {
            throw std::runtime_error("Invalid edge line at index " + std::to_string(i) + " in: " + graph_path.string());
        }
        if (u < 1 || u > g.n || v < 1 || v > g.n || w < 0.0)
        {
            throw std::runtime_error("Invalid edge value at index " + std::to_string(i) + " in: " + graph_path.string());
        }
        UndirectedEdge edge;
        edge.id = i;
        edge.u = u;
        edge.v = v;
        edge.w = w;
        g.minimum_edge_weight = std::min(g.minimum_edge_weight, w);
        g.edges.push_back(edge);
        ++degree[u];
        ++degree[v];
    }
    if (!input.AtEnd())
    {
        throw std::runtime_error(
            "Unexpected trailing token after declared edges in: " +
            graph_path.string());
    }

    // 第二阶段按精确度数一次性分配，再从紧凑原边数组构造双向邻接表。
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

}  // namespace gst
