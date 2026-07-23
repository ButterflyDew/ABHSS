#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/common/graph_io.h"

namespace
{
/** @brief 测试期无向边真值，按 graph.txt 的原始行序保存。 */
struct ExpectedEdge
{
    int u = 0;
    int v = 0;
    double weight = 0.0;
};

/** @brief 以零容差检查可精确表示的小数，以紧容差检查长十进制权重。 */
void CheckWeight(double actual, double expected, const std::string& context)
{
    if (std::fabs(actual - expected) > 1e-12)
        throw std::runtime_error(context + " weight mismatch");
}

/**
 * @brief 检查一个顶点的邻接项顺序、edge id、对端和边权。
 *
 * 快速读取器必须保持旧加载器“按原边行序向两端插入”的确定性顺序；这对
 * 等权最短路的稳定访问次序很重要，不能只检查点边数量。
 */
void CheckAdjacency(const gst::Graph& graph,
                    int vertex,
                    const std::vector<gst::AdjEdge>& expected)
{
    const auto& actual = graph.adj[vertex];
    if (actual.size() != expected.size())
        throw std::runtime_error("adjacency size mismatch at vertex " +
                                 std::to_string(vertex));
    for (size_t index = 0; index < expected.size(); ++index)
    {
        if (actual[index].to != expected[index].to ||
            actual[index].edge_id != expected[index].edge_id)
            throw std::runtime_error("adjacency order mismatch at vertex " +
                                     std::to_string(vertex));
        CheckWeight(actual[index].w,
                    expected[index].w,
                    "adjacency at vertex " + std::to_string(vertex));
    }
}

/** @brief 断言 header/edge 之外的非法 token 不会被快速路径静默忽略。 */
void ExpectLoadRejected(const std::string& folder, const std::string& label)
{
    try
    {
        (void)gst::LoadGraphFromFolder(folder);
    }
    catch (const std::runtime_error&)
    {
        return;
    }
    throw std::runtime_error(label + " graph fixture was unexpectedly accepted");
}
}  // namespace

/**
 * @brief 读取含零、小数、科学计数和显式正号的夹具并核验完整图结构。
 *
 * 测试覆盖快速 token 跨类型解析、原边顺序、稳定 edge id、双向邻接顺序和
 * 最小边权；任何失败都表示快速路径不再是旧 graph.txt 接口的语义等价替换。
 */
int main()
{
    const std::string folder =
        std::string(GST_TEST_SOURCE_DIR) + "/tests/fixtures/fast_graph";
    const gst::Graph graph = gst::LoadGraphFromFolder(folder);
    if (graph.n != 5 || graph.m != 6 || graph.edges.size() != 6 ||
        graph.adj.size() != 6 || graph.component_count != 1 ||
        graph.component_of.size() != 6)
        throw std::runtime_error("fast graph reader header mismatch");
    CheckWeight(graph.minimum_edge_weight, 0.0, "minimum edge");

    const std::vector<ExpectedEdge> expected = {
        {1, 2, 0.0},
        {2, 3, 0.125},
        {3, 4, 100.0},
        {4, 5, 2.5},
        {5, 1, 3.1415926535},
        {2, 5, 7.0},
    };
    for (size_t index = 0; index < expected.size(); ++index)
    {
        const gst::UndirectedEdge& actual = graph.edges[index];
        if (actual.id != static_cast<int>(index) ||
            actual.u != expected[index].u || actual.v != expected[index].v)
            throw std::runtime_error("original edge order mismatch");
        CheckWeight(actual.w, expected[index].weight, "original edge");
    }

    CheckAdjacency(graph, 1, {{2, 0, 0.0}, {5, 4, 3.1415926535}});
    CheckAdjacency(graph,
                   2,
                   {{1, 0, 0.0}, {3, 1, 0.125}, {5, 5, 7.0}});
    CheckAdjacency(graph, 3, {{2, 1, 0.125}, {4, 2, 100.0}});
    CheckAdjacency(graph, 4, {{3, 2, 100.0}, {5, 3, 2.5}});
    CheckAdjacency(graph,
                   5,
                   {{4, 3, 2.5}, {1, 4, 3.1415926535}, {2, 5, 7.0}});

    const gst::Graph disconnected = gst::LoadGraphFromFolder(
        std::string(GST_TEST_SOURCE_DIR) +
        "/tests/fixtures/fast_graph_disconnected");
    const std::vector<int> expected_components = {-1, 0, 0, 1, 1, 2};
    if (disconnected.component_count != 3 ||
        disconnected.component_of != expected_components)
        throw std::runtime_error("connected-component cache mismatch");

    // 无向自环必须与旧读取器一样向同一邻接表插入两项，
    // 同时不得改变连通分量数。
    const gst::Graph self_loop = gst::LoadGraphFromFolder(
        std::string(GST_TEST_SOURCE_DIR) +
        "/tests/fixtures/fast_graph_self_loop");
    if (self_loop.n != 1 || self_loop.m != 1 ||
        self_loop.adj[1].size() != 2 || self_loop.component_count != 1)
        throw std::runtime_error("self-loop degree/adjacency semantics mismatch");
    CheckAdjacency(self_loop, 1, {{1, 0, 0.5}, {1, 0, 0.5}});

    ExpectLoadRejected(std::string(GST_TEST_SOURCE_DIR) +
                           "/tests/fixtures/fast_graph_trailing",
                       "trailing-token");
    ExpectLoadRejected(std::string(GST_TEST_SOURCE_DIR) +
                           "/tests/fixtures/fast_graph_bad_weight",
                       "bad-weight");

    std::cout << "fast graph reader preserved structure and cached components\n";
    return 0;
}
