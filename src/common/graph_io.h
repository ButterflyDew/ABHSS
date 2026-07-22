#ifndef GST_GRAPH_IO_H
#define GST_GRAPH_IO_H

#include <limits>
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
    double minimum_edge_weight = std::numeric_limits<double>::infinity();
    std::vector<UndirectedEdge> edges;
    std::vector<std::vector<AdjEdge>> adj;
};

// 按字典序列出 data_root 下的图目录，供数字选择器使用。
std::vector<std::string> ListGraphFolders(const std::string& data_root);
// 将目录路径、图名或 1-based 序号解析为真实图目录。
std::string ResolveGraphFolder(const std::string& data_root, const std::string& graph_selector);
// 读取 graph.txt，同时构造原边数组和双向邻接表。
Graph LoadGraphFromFolder(const std::string& graph_folder);

}  // namespace gst

#endif  // GST_GRAPH_IO_H
