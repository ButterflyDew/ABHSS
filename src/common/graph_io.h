#ifndef GST_GRAPH_IO_H
#define GST_GRAPH_IO_H

#include <string>
#include <vector>

namespace gst
{

struct UndirectedEdge
{
    int id = -1;
    int u = 0;
    int v = 0;
    double w = 0.0;
};

struct AdjEdge
{
    int to = 0;
    int edge_id = -1;
    double w = 0.0;
};

struct Graph
{
    int n = 0;
    int m = 0;
    std::vector<UndirectedEdge> edges;
    std::vector<std::vector<AdjEdge>> adj;

    // 图加载阶段一次性建立的无向连通分量编号，查询可行性判断直接复用。
    std::vector<int> component_of;
    int component_count = 0;
};

/** @brief 从目录中的 graph.txt 快速读图，并一次构造原边、邻接表和连通分量。 */
Graph LoadGraph(const std::string& graph_folder);

} // namespace gst

#endif // GST_GRAPH_IO_H
