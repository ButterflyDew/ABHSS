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
    // 重复的逻辑组合法且不会改变最优树；使用四组可确保新的 g<=3 数学闭包
    // 不会绕过本测试要覆盖的 witness 重根代码。
    query.groups = {{2}, {4}, {2}, {4}};

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

    // 按 (v,v-1) 顺序扫描会形成 1->2->... 的并查集父链。迭代 Find 必须在
    // 不依赖线程栈深度的前提下压缩该链，并由共同零权 cover 返回 0。
    constexpr int kChainVertices = 50000;
    gst::Graph chain;
    chain.n = kChainVertices;
    chain.minimum_edge_weight = std::numeric_limits<double>::infinity();
    chain.adj.assign(chain.n + 1, {});
    for (int vertex = 2; vertex <= chain.n; ++vertex)
        AddEdge(chain, vertex, vertex - 1, 0.0);
    gst::Query chain_query;
    chain_query.groups = {{1}, {chain.n}, {1}, {chain.n}};
    CheckAnswer("ABHSS-Base deep zero chain",
                gst::methods::abhss::SolveOneQuery(
                    chain,
                    chain_query,
                    gst::methods::abhss::SolveOptions::Base()),
                0.0);
    CheckAnswer("ABHSS-Enhanced deep zero chain",
                gst::methods::abhss::SolveOneQuery(
                    chain,
                    chain_query,
                    gst::methods::abhss::SolveOptions::Enhanced()),
                0.0);
    std::cout << "zero-weight witness regression passed\n";
    return 0;
}
