#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "../src/abhss/abhss.h"

namespace
{
/** @brief 向测试图加入一条双向邻接边，并同步维护图统计量。 */
void AddEdge(gst::Graph& graph, int u, int v, double weight)
{
    const int id = static_cast<int>(graph.edges.size());
    graph.edges.push_back({id, u, v, weight});
    graph.adj[u].push_back({v, id, weight});
    graph.adj[v].push_back({u, id, weight});
    graph.minimum_edge_weight = std::min(graph.minimum_edge_weight, weight);
    graph.m = static_cast<int>(graph.edges.size());
}

/** @brief 断言一个 ABHSS 配置返回指定精确权重，否则终止回归测试。 */
void CheckAnswer(const char* method,
                 const gst::methods::abhss::SolveResult& answer,
                 double expected)
{
    if (!answer.feasible || std::fabs(answer.best_weight - expected) > 1e-12)
        throw std::runtime_error(std::string(method) +
                                 " failed the zero-weight witness regression.");
}
}  // namespace

/** @brief 构造历史零权反例并检查三种合法配置的共同精确答案。 */
int main()
{
    // 有用见证为 4--(1)--3--(0)--2。旧迁移代码在锚 4 处重根时曾允许
    // 顶点 2 以相同距离重新挂接已经 settled 的顶点 3，形成 2<->3 环。
    // 4--1 叶只用于削弱分量下界，迫使全部配置实际构造并求值该见证。
    gst::Graph graph;
    graph.n = 4;
    graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
    graph.adj.assign(graph.n + 1, {});
    AddEdge(graph, 4, 3, 1.0);
    AddEdge(graph, 3, 2, 0.0);
    AddEdge(graph, 4, 1, 0.125);

    gst::Query query;
    query.groups = {{2}, {4}};

    CheckAnswer("ABHSS-Base",
                gst::methods::abhss::SolveOneQuery(
                    graph,
                    query,
                    gst::methods::abhss::SolveOptions::Base()),
                1.0);
    CheckAnswer("ABHSS-DirectedCutOnly",
                gst::methods::abhss::SolveOneQuery(
                    graph,
                    query,
                    gst::methods::abhss::SolveOptions::DirectedCutOnly()),
                1.0);
    CheckAnswer("ABHSS-Enhanced",
                gst::methods::abhss::SolveOneQuery(
                    graph,
                    query,
                    gst::methods::abhss::SolveOptions::Enhanced()),
                1.0);
    std::cout << "zero-weight witness regression passed\n";
    return 0;
}
