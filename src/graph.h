#pragma once

#include <string>
#include <limits>
#include <vector>

namespace abhss {

struct UndirectedEdge {
    int id, u, v;
    double w;
};

struct AdjEdge {
    int to, edge_id;
    double w;
};

// 原边编号与输入次序一致；每条无向边对应两个邻接项。
struct Graph {
    int n = 0, m = 0;
    double minimum_edge_weight = std::numeric_limits<double>::infinity();
    std::vector<UndirectedEdge> edges;
    std::vector<std::vector<AdjEdge>> adj;
    std::vector<int> component_of;
    int component_count = 0;
};

struct Query {
    std::vector<std::vector<int>> groups;
};

// 读取目录内的 graph.txt，预分配邻接表并建立连通分量。
Graph LoadGraph(const std::string& folder);

// 读取整个查询文件，组内顶点编号与原文件保持一致。
std::vector<Query> LoadQueries(const std::string& file);

// 查询可行当且仅当某个连通分量与全部组相交。
bool IsQueryFeasible(const Graph& graph, const Query& query);

} // namespace abhss
