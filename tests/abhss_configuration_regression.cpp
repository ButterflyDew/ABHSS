#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../src/abhss/abhss.h"
#include "../src/abhss/core.h"

namespace
{
using HeapItem = std::pair<double, int>;

/** @brief 向随机测试图加入一条无向边并维护邻接表与最小边权。 */
void AddEdge(gst::Graph& graph, int u, int v, double weight)
{
    const int id = static_cast<int>(graph.edges.size());
    graph.edges.push_back({id, u, v, weight});
    graph.adj[u].push_back({v, id, weight});
    graph.adj[v].push_back({u, id, weight});
    graph.minimum_edge_weight = std::min(graph.minimum_edge_weight, weight);
    graph.m = static_cast<int>(graph.edges.size());
}

/**
 * @brief 用独立的全子集 Dreyfus-Wagner DP 计算小图真值。
 *
 * 该实现不调用 ABHSS 的 row、锚定或下界代码，因此可作为所有增强配置的
 * 独立精确 oracle；仅用于顶点数随 g 线性增长的小图工程回归，不进入论文性能实验。
 */
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

/** @brief 检查一个配置的可行性与目标值是否逐例匹配独立真值。 */
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

/** @brief 检查前置处理应直接判定的可行/不可行结果。 */
void CheckPrelude(const char* label,
                  const gst::methods::abhss::SolveResult& answer,
                  bool feasible,
                  double expected = -1.0)
{
    if (answer.feasible != feasible ||
        (feasible && std::fabs(answer.best_weight - expected) > 1e-12) ||
        (!feasible && answer.best_weight != -1.0))
        throw std::runtime_error(std::string(label) +
                                 " failed a query-prelude regression");
}

/** @brief 生成含零权边的确定性随机连通图，覆盖 witness 等距重挂路径。 */
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

/** @brief 生成允许组重叠和每组多终端的确定性随机 GST 查询。 */
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

/**
 * @brief 覆盖空/单组、组重叠、跨分量无解、上限和非法开关等入口契约。
 *
 * 这些情况不依赖主 DP，但若处理错误会污染大批量完成率或触发掩码溢出，
 * 因而与随机可行图的目标值回归分开给出确定性断言。
 */
void CheckPreludeContracts(const gst::methods::abhss::SolveOptions& base,
                           const gst::methods::abhss::SolveOptions& enhanced)
{
    gst::Graph graph;
    graph.n = 4;
    graph.adj.assign(5, {});
    AddEdge(graph, 1, 2, 1.0);
    AddEdge(graph, 3, 4, 2.0);

    CheckPrelude("empty query",
                 gst::methods::abhss::SolveOneQuery(graph, {}, base),
                 true,
                 0.0);
    gst::Query singleton;
    singleton.groups = {{1, 3}};
    CheckPrelude("single group",
                 gst::methods::abhss::SolveOneQuery(graph, singleton, enhanced),
                 true,
                 0.0);
    gst::Query overlap;
    overlap.groups = {{1, 2}, {1, 4}, {1}};
    CheckPrelude("overlapping zero optimum",
                 gst::methods::abhss::SolveOneQuery(graph, overlap, base),
                 true,
                 0.0);
    gst::Query infeasible;
    infeasible.groups = {{1}, {4}};
    CheckPrelude("disconnected infeasible",
                 gst::methods::abhss::SolveOneQuery(graph, infeasible, enhanced),
                 false);

    gst::Query out_of_range;
    out_of_range.groups = {{1}, {5}};
    bool rejected_vertex = false;
    try
    {
        (void)gst::methods::abhss::SolveOneQuery(graph, out_of_range, base);
    }
    catch (const std::runtime_error&)
    {
        rejected_vertex = true;
    }
    if (!rejected_vertex)
        throw std::runtime_error("ABHSS accepted an out-of-range query vertex");

    gst::Query too_many;
    too_many.groups.assign(17, {1});
    bool rejected_group_count = false;
    try
    {
        (void)gst::methods::abhss::SolveOneQuery(graph, too_many, base);
    }
    catch (const std::runtime_error&)
    {
        rejected_group_count = true;
    }
    if (!rejected_group_count)
        throw std::runtime_error("ABHSS accepted more than 16 groups");

    const auto invalid = base.With(
        gst::methods::abhss::Enhancement::AdjointCompletion, true);
    if (gst::methods::abhss::IsValid(invalid))
        throw std::runtime_error("adjoint-only configuration was marked valid");
    bool rejected_invalid = false;
    try
    {
        (void)gst::methods::abhss::SolveOneQuery(graph, overlap, invalid);
    }
    catch (const std::invalid_argument&)
    {
        rejected_invalid = true;
    }
    if (!rejected_invalid)
        throw std::runtime_error("adjoint-only configuration reached solving");

    // 公开结构允许调用者直接写 bit mask，因此未知高位也必须在进入任何
    // 图/查询预处理前拒绝，不能被未来编译版本静默解释为某种配置。
    auto unknown = base;
    unknown.enhancements = std::uint32_t{1} << 31;
    if (gst::methods::abhss::IsValid(unknown) ||
        std::string(gst::methods::abhss::ConfigurationName(unknown)) != "invalid")
        throw std::runtime_error("unknown enhancement bit was marked valid");
    bool rejected_unknown = false;
    try
    {
        (void)gst::methods::abhss::SolveOneQuery(graph, overlap, unknown);
    }
    catch (const std::invalid_argument&)
    {
        rejected_unknown = true;
    }
    if (!rejected_unknown)
        throw std::runtime_error("unknown enhancement bit reached solving");
}

/**
 * @brief 防止用 1e-9 容差把严格为正的上下界 gap 当作已闭合。
 *
 * 低编号终端形成权重 3+5e-10 的规范路径，高编号终端形成真实最优权重 3
 * 的路径。旧代码在 Base 的 canonical SPT 后用 epsilon 比较 lower=3，曾会
 * 提前返回稍差路径；当前闭合条件必须使用原始 double 顺序。
 */
void CheckSubNanogapClosure(const gst::methods::abhss::SolveOptions& base)
{
    gst::Graph graph;
    graph.n = 8;
    graph.adj.assign(9, {});
    AddEdge(graph, 1, 2, 1.0);
    AddEdge(graph, 2, 3, 1.0);
    AddEdge(graph, 3, 4, 1.0 + 5e-10);
    AddEdge(graph, 5, 6, 1.0);
    AddEdge(graph, 6, 7, 1.0);
    AddEdge(graph, 7, 8, 1.0);
    AddEdge(graph, 4, 5, 100.0);

    gst::Query query;
    query.groups = {{1, 5}, {2, 6}, {3, 7}, {4, 8}};
    CheckPrelude("strict lower/upper closure",
                 gst::methods::abhss::SolveOneQuery(graph, query, base),
                 true,
                 3.0);
}

/** @brief 锁定两种距离—根 initialization realization 的共同输出合同。 */
void CheckDistanceRootInitializationContract()
{
    gst::Graph graph;
    graph.n = 7;
    graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
    graph.adj.assign(8, {});
    AddEdge(graph, 1, 2, 0.5);
    AddEdge(graph, 2, 3, 1.0);
    AddEdge(graph, 3, 4, 1.5);
    AddEdge(graph, 4, 5, 2.0);
    AddEdge(graph, 5, 6, 2.5);
    AddEdge(graph, 6, 7, 3.0);
    AddEdge(graph, 2, 6, 4.0);

    gst::Query query;
    query.groups = {{1}, {3}, {5}, {7}};
    using gst::methods::abhss::DistanceRootRealization;
    const auto bounded =
        gst::methods::abhss::internal::BuildDistanceRootInitialization(
            graph,
            query,
            DistanceRootRealization::BootstrappedBounded);
    const auto complete =
        gst::methods::abhss::internal::BuildDistanceRootInitialization(
            graph,
            query,
            DistanceRootRealization::CompletePotential);

    if (bounded.group_distance.size() != query.groups.size() ||
        complete.group_distance.size() != query.groups.size() ||
        bounded.root < 1 || bounded.root > graph.n ||
        complete.root < 1 || complete.root > graph.n ||
        !std::isfinite(bounded.upper) || !std::isfinite(complete.upper))
        throw std::runtime_error(
            "ABHSS distance-root initialization returned an invalid common contract.");

    for (size_t group = 0; group < query.groups.size(); ++group)
    {
        const auto& short_row = bounded.group_distance[group];
        const auto& full_row = complete.group_distance[group];
        if (!short_row.bounded || full_row.bounded ||
            full_row.ExactSize(graph.n) != static_cast<size_t>(graph.n))
            throw std::runtime_error(
                "ABHSS distance-root realization exposed the wrong oracle layout.");
        for (int vertex = 1; vertex <= graph.n; ++vertex)
        {
            const double short_exact = short_row.ExactValueOrInf(vertex);
            if (short_exact < gst::fp::kInf)
            {
                if (std::fabs(short_row[vertex] - full_row[vertex]) > 1e-12 || std::fabs(short_exact - full_row[vertex]) > 1e-12)
                    throw std::runtime_error(
                        "ABHSS bounded and complete exact distances disagree.");
            }
            else if (short_row[vertex] > full_row[vertex])
            {
                throw std::runtime_error(
                    "ABHSS bounded cutoff is not a safe lower placeholder or entered an exact consumer.");
            }
        }
    }

    // 非连通图中，每组的规范最小终端可能没有共同分量，导致私有 SPT
    // bootstrap 暂时返回无穷；但查询的其他候选仍可共享一个可行分量。
    // bounded realization 必须让多源搜索与共同 root-star 接管，而不能把
    // “规范终端失败”误判为查询无解或暴露无穷上界。
    gst::Graph disconnected;
    disconnected.n = 7;
    disconnected.minimum_edge_weight =
        std::numeric_limits<double>::infinity();
    disconnected.adj.assign(8, {});
    AddEdge(disconnected, 1, 2, 1.0);
    AddEdge(disconnected, 3, 4, 1.0);
    AddEdge(disconnected, 5, 6, 1.0);
    AddEdge(disconnected, 6, 7, 1.0);

    gst::Query fallback_query;
    fallback_query.groups = {{1, 5}, {3, 6}, {4, 7}};
    const auto fallback_bounded =
        gst::methods::abhss::internal::BuildDistanceRootInitialization(
            disconnected,
            fallback_query,
            DistanceRootRealization::BootstrappedBounded);
    const auto fallback_complete =
        gst::methods::abhss::internal::BuildDistanceRootInitialization(
            disconnected,
            fallback_query,
            DistanceRootRealization::CompletePotential);
    if (!std::isfinite(fallback_bounded.upper) ||
        std::fabs(fallback_bounded.upper - 2.0) > 1e-12 ||
        std::fabs(fallback_complete.upper - 2.0) > 1e-12 ||
        fallback_bounded.root < 5 || fallback_bounded.root > 7 ||
        fallback_complete.root < 5 || fallback_complete.root > 7)
        throw std::runtime_error(
            "ABHSS distance-root initialization failed its disconnected canonical-terminal fallback.");
}

/** @brief 锁定 g<=3 root-star 数学闭包在全部合法配置中的共同精确语义。 */
void CheckLowGroupExactClosure(const gst::methods::abhss::SolveOptions& base,
                               const gst::methods::abhss::SolveOptions& directed,
                               const gst::methods::abhss::SolveOptions& enhanced)
{
    gst::Graph graph;
    graph.n = 8;
    graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
    graph.adj.assign(9, {});
    AddEdge(graph, 1, 4, 2.0);
    AddEdge(graph, 2, 4, 3.0);
    AddEdge(graph, 3, 4, 5.0);
    AddEdge(graph, 4, 5, 1.0);
    AddEdge(graph, 5, 6, 7.0);
    AddEdge(graph, 6, 7, 2.0);
    AddEdge(graph, 7, 8, 4.0);

    const std::vector<gst::Query> queries{{{{1, 8}, {2, 7}}}, {{{1, 8}, {2, 7}, {3, 6}}}};
    const std::vector<gst::methods::abhss::SolveOptions> options{base, directed, enhanced};
    for (int query_index = 0; query_index < static_cast<int>(queries.size()); ++query_index)
    {
        const double expected = ExactSubsetDp(graph, queries[query_index]);
        for (int option_index = 0; option_index < static_cast<int>(options.size()); ++option_index)
        {
            const auto answer = gst::methods::abhss::SolveOneQuery(graph, queries[query_index], options[option_index]);
            Check("ABHSS low-group closure", answer, expected, 10 * query_index + option_index);
            if (answer.mask_vertex_states != 0)
                throw std::runtime_error("ABHSS g<=3 closure entered the exponential state tables.");
        }
    }
}

/** @brief 独立复算每条有向弧的势梯度，锁定 cone 遍历没有漏减 residual。 */
void CheckDirectedCutResidualAccounting()
{
    gst::Graph graph;
    graph.n = 7;
    graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
    graph.adj.assign(8, {});
    AddEdge(graph, 1, 2, 1.0);
    AddEdge(graph, 2, 3, 2.0);
    AddEdge(graph, 3, 4, 3.0);
    AddEdge(graph, 4, 5, 4.0);
    AddEdge(graph, 5, 6, 5.0);
    AddEdge(graph, 6, 7, 6.0);
    AddEdge(graph, 1, 7, 12.0);
    AddEdge(graph, 2, 6, 8.0);
    AddEdge(graph, 3, 5, 2.5);
    AddEdge(graph, 4, 4, 0.75);

    gst::Query query;
    query.groups = {{1}, {3}, {5}, {7}};
    const auto initialization = gst::methods::abhss::internal::BuildDistanceRootInitialization(
        graph, query, gst::methods::abhss::DistanceRootRealization::CompletePotential);
    std::vector<std::vector<double>> distance(query.groups.size());
    for (size_t group = 0; group < query.groups.size(); ++group)
        distance[group] = initialization.group_distance[group].value;

    gst::methods::dual_cut::DualCutPotential dual;
    dual.BuildKeepingResidualChangedArcsWithPrimalEdges(graph, query, distance, initialization.root);
    // 按实现约定把一条无向边的指定方向映射到 residual 数组中的弧编号。
    auto Arc = [](const gst::UndirectedEdge& edge, int from, int to)
    {
        return 2 * edge.id + (from < to ? 0 : 1);
    };
    auto CheckResidual = [&](const char* phase)
    {
        const auto& residual = dual.Residual();
        if (residual.size() != static_cast<size_t>(2 * graph.m))
            throw std::runtime_error(std::string(phase) + " residual has the wrong size.");
        for (const gst::UndirectedEdge& edge : graph.edges)
        {
            double forward = edge.w;
            double backward = edge.w;
            for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
            {
                forward = std::max(0.0, forward - std::max(0.0, dual.GroupAt(edge.u, group) - dual.GroupAt(edge.v, group)));
                backward = std::max(0.0, backward - std::max(0.0, dual.GroupAt(edge.v, group) - dual.GroupAt(edge.u, group)));
            }
            if (std::fabs(residual[Arc(edge, edge.u, edge.v)] - forward) > 1e-10 ||
                std::fabs(residual[Arc(edge, edge.v, edge.u)] - backward) > 1e-10)
                throw std::runtime_error(std::string(phase) + " skipped a nonzero arc gradient.");
        }
    };
    CheckResidual("ABHSS potential-cone traversal");

    std::vector<std::vector<double>> before(query.groups.size(), std::vector<double>(graph.n + 1));
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            before[group][vertex] = dual.GroupAt(vertex, group);

    const long long expected_buy = 2LL * graph.m + static_cast<long long>(query.groups.size()) * (2LL * graph.m + graph.n);
    if (dual.ResidualClosureBuyWork(graph) < expected_buy)
        throw std::runtime_error("ABHSS residual-closure buy work fell below its static floor.");

    dual.ReleaseResidual();
    dual.CompleteResidualClosureKeepingResidualAndPrimalEdges(graph, query, distance, initialization.root);
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            if (dual.GroupAt(vertex, group) + 1e-12 < before[group][vertex])
                throw std::runtime_error("ABHSS residual closure weakened an existing group potential.");
    CheckResidual("ABHSS residual closure");
    const double optimum = ExactSubsetDp(graph, query);
    if (!std::isfinite(dual.PrimalUpper()) || dual.PrimalUpper() + 1e-9 < optimum)
        throw std::runtime_error("ABHSS residual closure returned an invalid primal upper bound.");
    if (dual.ResidualClosureBuyWork(graph) != 0)
        throw std::runtime_error("ABHSS residual closure remained purchasable after completion.");
}
/** @brief 锁定辅助 H 半格对被省略 D 半格的精确转置职责。 */
void CheckAuxiliaryHalfAdjointRegression()
{
    gst::Graph graph;
    graph.n = 12;
    graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
    graph.adj.assign(13, {});
    AddEdge(graph, 1, 2, 2.5);
    AddEdge(graph, 2, 3, 3.75);
    AddEdge(graph, 3, 4, 4.25);
    AddEdge(graph, 2, 5, 0.5);
    AddEdge(graph, 2, 6, 4.25);
    AddEdge(graph, 3, 7, 0.0);
    AddEdge(graph, 2, 8, 5.0);
    AddEdge(graph, 5, 9, 0.75);
    AddEdge(graph, 8, 10, 4.0);
    AddEdge(graph, 3, 11, 2.25);
    AddEdge(graph, 2, 12, 0.0);
    AddEdge(graph, 1, 4, 0.0);
    AddEdge(graph, 1, 11, 3.5);
    AddEdge(graph, 1, 12, 3.25);
    AddEdge(graph, 2, 7, 1.5);
    AddEdge(graph, 2, 10, 3.0);
    AddEdge(graph, 3, 5, 4.25);
    AddEdge(graph, 4, 8, 1.5);
    AddEdge(graph, 4, 12, 2.0);
    AddEdge(graph, 5, 6, 2.5);
    AddEdge(graph, 5, 7, 3.25);
    AddEdge(graph, 5, 8, 1.25);
    AddEdge(graph, 5, 12, 1.5);
    AddEdge(graph, 6, 11, 3.5);
    AddEdge(graph, 9, 10, 1.5);
    AddEdge(graph, 9, 11, 2.5);
    AddEdge(graph, 9, 12, 0.25);
    AddEdge(graph, 10, 12, 0.5);

    gst::Query query;
    query.groups = {{3, 4, 6, 7}, {6}, {5}, {3, 5, 7, 8}, {2, 7}, {3, 11}, {4, 11}};
    const double expected = ExactSubsetDp(graph, query);
    if (std::fabs(expected - 5.75) > 1e-12)
        throw std::runtime_error("auxiliary-half fixture oracle changed");
    const auto answer = gst::methods::abhss::SolveOneQuery(graph, query, gst::methods::abhss::SolveOptions::Enhanced());
    Check("ABHSS auxiliary-half adjoint", answer, expected, 0);
}

/** @brief 逐项证明 A1 lazy、top-two 与完整排名购买返回同一精确视图。 */
void CheckAnchoredSingletonMaterializationEquivalence()
{
    constexpr int kVertices = 64;
    constexpr int kBits = 4;
    gst::Graph graph;
    graph.n = kVertices;
    graph.adj.assign(kVertices + 1, {});
    gst::Query query;
    query.groups.resize(kBits + 1);
    gst::methods::abhss::internal::Problem problem(graph, query, {});
    problem.g = kBits + 1;
    problem.nonanchor_count = kBits;
    problem.subset_count = 1 << kBits;

    gst::methods::abhss::internal::AnchoredSingletonFuture future;
    future.row.resize(problem.subset_count);
    for (int index = 0; index < kBits; ++index)
    {
        auto& row = future.row[1 << index];
        row.ready = true;
        for (int vertex = 1; vertex <= kVertices; ++vertex)
        {
            row.vertex.push_back(vertex);
            row.value.push_back(static_cast<double>((vertex * 7 + index * 3) % 11));
        }
    }
    future.first.assign(kVertices + 1, 255);
    future.second.assign(kVertices + 1, 255);
    future.cached_locator_pair.reset(new std::uint64_t[kVertices + 1]);
    future.InitializeLookupPlan(problem);

    for (int vertex = 1; vertex <= kVertices; ++vertex)
    for (int remaining = 1; remaining < problem.subset_count; ++remaining)
    {
        double expected = 0.0;
        for (int bits = remaining; bits; bits &= bits - 1)
        {
            const int bit = bits & -bits;
            expected = std::max(expected, future.row[bit].value[vertex - 1]);
        }
        const double actual = future.Future(problem, remaining, vertex);
        if (actual != expected)
            throw std::runtime_error("A1 lazy/materialized view changed an exact future value.");
    }
    if (!future.lookup_materialized)
        throw std::runtime_error("A1 materialization equivalence regression did not exercise the purchased path.");
    if (future.ranked_rent_by_mask.empty())
        throw std::runtime_error("A1 tail-rent subset factorization was not materialized with top-two.");
    for (int remaining = 0; remaining < problem.subset_count; ++remaining)
    {
        int expected_rent = 0;
        for (int bits = remaining; bits; bits &= bits - 1)
        {
            const int bit = bits & -bits;
            expected_rent += static_cast<int>(gst::methods::abhss::internal::BinarySearchCost(future.row[bit].vertex.size()) + 1);
        }
        if (future.ranked_rent_by_mask[remaining] != expected_rent)
            throw std::runtime_error("A1 tail-rent subset factorization changed the exact paid work.");
    }
    if (future.ranked_tail.empty())
        throw std::runtime_error("A1 materialization equivalence regression did not exercise the ranked-tail purchase.");
}

}  // namespace

/** @brief 运行入口契约、层计划不变量及 g=2..10 的确定性随机精确性实例。 */
int main()
{
    CheckAuxiliaryHalfAdjointRegression();
    CheckAnchoredSingletonMaterializationEquivalence();

    // rent-or-buy 的 buy 只能由 witness 大小与非锚组数决定。这里直接锁定
    // 共同公式，防止以后又在 Base/Enhanced 分支中各写一份近似估计。
    using gst::methods::abhss::internal::EstimateWitnessTreeDpWork;
    if (EstimateWitnessTreeDpWork(0, 5) != 0 ||
        EstimateWitnessTreeDpWork(7, 0) != 7 ||
        EstimateWitnessTreeDpWork(5, 1) != 20 ||
        EstimateWitnessTreeDpWork(5, 3) != 200)
        throw std::runtime_error(
            "ABHSS common witness buy formula is inconsistent.");

    // 调度器本身也必须从零 rent、零次求值启动；不能把 Base 的旧式预买
    // 偷藏进构造函数。这里只设置公式所需的树大小，不触发实际树 DP。
    gst::Graph scheduler_graph;
    gst::Query scheduler_query;
    gst::methods::abhss::internal::Problem scheduler_problem(
        scheduler_graph,
        scheduler_query,
        gst::methods::abhss::SolveOptions::Base());
    scheduler_problem.nonanchor_count = 3;
    scheduler_problem.witness_tree.vertex.resize(5);
    gst::methods::abhss::internal::WitnessUpperScheduler scheduler(
        scheduler_problem);
    if (scheduler.BuyWork() != 200 || scheduler.RentWork() != 0 ||
        scheduler.TotalWork() != 0 || scheduler.EvaluationCount() != 0)
        throw std::runtime_error(
            "ABHSS witness scheduler does not start from zero rent.");
    scheduler.Account(7, false);
    if (scheduler.TotalWork() != 0)
        throw std::runtime_error(
            "ABHSS Base paid residual-closure-only accounting work.");

    using gst::methods::abhss::AddedOperation;
    using gst::methods::abhss::Enhancement;
    using gst::methods::abhss::DistanceRootRealization;
    using gst::methods::abhss::HighLayerRealization;
    using gst::methods::abhss::OrdinaryFutureRealization;
    using gst::methods::abhss::SolveOptions;
    using gst::methods::abhss::UpperWitnessRealization;

    CheckDistanceRootInitializationContract();

    // 显式验证“完整增强逐项关开关即回到基础配置”的配置链，而不是仅依赖
    // 三个工厂函数碰巧返回相同掩码。
    const SolveOptions enhanced = SolveOptions::Enhanced();
    const SolveOptions directed_only =
        enhanced.With(Enhancement::AdjointCompletion, false);
    const SolveOptions base =
        directed_only.With(Enhancement::DirectedCut, false);
    if (directed_only.enhancements !=
            SolveOptions::DirectedCutOnly().enhancements ||
        base.enhancements != SolveOptions::Base().enhancements)
        throw std::runtime_error("ABHSS enhancement switch chain is inconsistent.");

    // 进一步验证论文使用的“公共操作 + 安全新增 + 同职责替换”关系。Base
    // 不得拥有仅自己执行的逻辑阶段；ordinary future 在全部配置中固定复用
    // A1，dual 只作为新增证书。其余替换由 profile 显式选择 realization。
    const auto base_profile =
        gst::methods::abhss::DescribeConfiguration(base);
    const auto directed_profile =
        gst::methods::abhss::DescribeConfiguration(directed_only);
    const auto enhanced_profile =
        gst::methods::abhss::DescribeConfiguration(enhanced);
    if (!base_profile.valid || !directed_profile.valid ||
        !enhanced_profile.valid || base_profile.added_operations != 0 ||
        (base_profile.added_operations & ~directed_profile.added_operations) ||
        (directed_profile.added_operations & ~enhanced_profile.added_operations) ||
        base_profile.distance_root !=
            DistanceRootRealization::BootstrappedBounded ||
        directed_profile.distance_root !=
            DistanceRootRealization::CompletePotential ||
        base_profile.ordinary_future !=
            OrdinaryFutureRealization::AnchoredSingletonCone ||
        directed_profile.ordinary_future !=
            OrdinaryFutureRealization::AnchoredSingletonCone ||
        enhanced_profile.ordinary_future !=
            OrdinaryFutureRealization::AnchoredSingletonCone ||
        base_profile.upper_witness != UpperWitnessRealization::RootPathTree ||
        directed_profile.upper_witness !=
            UpperWitnessRealization::DualPrimalTree ||
        base_profile.high_layer != HighLayerRealization::ForwardAnchoredA ||
        directed_profile.high_layer !=
            HighLayerRealization::ForwardAnchoredA ||
        enhanced_profile.high_layer != HighLayerRealization::AdjointH ||
        !directed_profile.Adds(AddedOperation::DirectedCutCertificate) ||
        !directed_profile.Adds(AddedOperation::FacilityUpperBound) ||
        !directed_profile.Adds(AddedOperation::ResidualCertificateRefresh) ||
        !enhanced_profile.Adds(AddedOperation::DirectedCutCertificate) ||
        !enhanced_profile.Adds(AddedOperation::FacilityUpperBound) ||
        !enhanced_profile.Adds(AddedOperation::ResidualCertificateRefresh))
        throw std::runtime_error(
            "ABHSS add-or-replace configuration contract is inconsistent.");

    // 层计划只能来自平衡递推域。逐个 g 核对每一正层由一种 realization
    // 恰好覆盖，防止以后重新加入“某个组数以上才提前 A1”之类经验分派。
    for (int group_count = 0; group_count <= 16; ++group_count)
    {
        const int half = group_count / 2;
        const int expected_highest = half > 0 ? half - 1 : 0;
        const auto base_schedule =
            gst::methods::abhss::MakeAnchoredCompletionSchedule(
                group_count, base_profile);
        const auto directed_schedule =
            gst::methods::abhss::MakeAnchoredCompletionSchedule(
                group_count, directed_profile);
        const auto enhanced_schedule =
            gst::methods::abhss::MakeAnchoredCompletionSchedule(
                group_count, enhanced_profile);
        const bool has_adjoint_suffix = enhanced_schedule.forward_last_layer < expected_highest;
        const int expected_enhanced_ordinary = has_adjoint_suffix ? expected_highest : half;
        const int expected_enhanced_adjoint = has_adjoint_suffix ? half : expected_highest;
        if (base_schedule.highest_layer != expected_highest ||
            base_schedule.forward_last_layer != expected_highest ||
            base_schedule.ordinary_last_layer != half ||
            base_schedule.adjoint_last_layer != expected_highest ||
            base_schedule.uses_adjoint ||
            directed_schedule.highest_layer != expected_highest ||
            directed_schedule.forward_last_layer != expected_highest ||
            directed_schedule.ordinary_last_layer != half ||
            directed_schedule.adjoint_last_layer != expected_highest ||
            directed_schedule.uses_adjoint ||
            enhanced_schedule.highest_layer != expected_highest ||
            enhanced_schedule.forward_last_layer !=
                (expected_highest > 0
                     ? std::max(1, expected_highest / 2)
                     : 0) ||
            enhanced_schedule.ordinary_last_layer != expected_enhanced_ordinary ||
            enhanced_schedule.adjoint_last_layer != expected_enhanced_adjoint ||
            !enhanced_schedule.uses_adjoint ||
            (expected_highest > 0 &&
             (!base_schedule.UsesForwardA(1) ||
              !directed_schedule.UsesForwardA(1) ||
              !enhanced_schedule.UsesForwardA(1))))
            throw std::runtime_error(
                "ABHSS anchored boundary or common A1 prefix is inconsistent.");

        for (int layer = 1; layer <= expected_highest + 1; ++layer)
        {
            const bool required = layer <= expected_highest;
            const bool expected_enhanced_forward = required && layer <= enhanced_schedule.forward_last_layer;
            const bool expected_enhanced_h = has_adjoint_suffix && layer > enhanced_schedule.forward_last_layer && layer <= half;
            if (base_schedule.ContainsLogicalLayer(layer) != required ||
                base_schedule.UsesForwardA(layer) != required ||
                base_schedule.UsesAdjointH(layer) ||
                directed_schedule.UsesForwardA(layer) != required ||
                directed_schedule.UsesAdjointH(layer) ||
                enhanced_schedule.ContainsLogicalLayer(layer) != required ||
                enhanced_schedule.UsesForwardA(layer) != expected_enhanced_forward ||
                enhanced_schedule.UsesAdjointH(layer) != expected_enhanced_h ||
                (required && expected_enhanced_forward == expected_enhanced_h))
                throw std::runtime_error(
                    "ABHSS anchored layer has missing or duplicate realization.");
        }
    }

    CheckPreludeContracts(base, enhanced);
    CheckSubNanogapClosure(base);
    CheckLowGroupExactClosure(base, directed_only, enhanced);
    CheckDirectedCutResidualAccounting();

    std::mt19937 random(0xAB455u);
    constexpr int kInstances = 5000;
    std::uint64_t base_state_total = 0;
    std::uint64_t directed_state_total = 0;
    std::uint64_t enhanced_state_total = 0;
    for (int instance = 0; instance < kInstances; ++instance)
    {
        const int n = 4 + static_cast<int>(random() % 6);
        const int g = 2 + instance % 9;
        const gst::Graph graph = RandomConnectedGraph(random, n);
        const gst::Query query = RandomQuery(random, n, g);
        const double expected = ExactSubsetDp(graph, query);
        const auto base_answer =
            gst::methods::abhss::SolveOneQuery(graph, query, base);
        Check("ABHSS-Base", base_answer, expected, instance);
        base_state_total += base_answer.mask_vertex_states;
        const auto directed_answer =
            gst::methods::abhss::SolveOneQuery(graph, query, directed_only);
        Check("ABHSS-DirectedCutOnly", directed_answer, expected, instance);
        directed_state_total += directed_answer.mask_vertex_states;
        const auto enhanced_answer =
            gst::methods::abhss::SolveOneQuery(graph, query, enhanced);
        Check("ABHSS-Enhanced", enhanced_answer, expected, instance);
        enhanced_state_total += enhanced_answer.mask_vertex_states;
    }
    if (!base_state_total || !directed_state_total || !enhanced_state_total)
        throw std::runtime_error(
            "an ABHSS configuration never registered a mask-vertex state");
    for (int instance = 0; instance < 500; ++instance)
    {
        const int g = 6 + instance % 5;
        gst::Graph graph = RandomConnectedGraph(random, g + 2);
        graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
        for (gst::UndirectedEdge& edge : graph.edges)
        {
            if (edge.w == 0.0)
                edge.w = 0.25;
            graph.minimum_edge_weight = std::min(graph.minimum_edge_weight, edge.w);
        }
        for (std::vector<gst::AdjEdge>& adjacency : graph.adj)
            for (gst::AdjEdge& edge : adjacency)
                edge.w = graph.edges[edge.edge_id].w;
        gst::Query query;
        query.groups.resize(g);
        for (int group = 0; group < g; ++group)
            query.groups[group] = {group + 1};
        const double expected = ExactSubsetDp(graph, query);
        const auto answer = gst::methods::abhss::SolveOneQuery(graph, query, enhanced);
        Check("ABHSS-Enhanced unique terminals", answer, expected, instance);
    }
    constexpr std::array<int, 10> kTransposeGroupCounts = {7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    constexpr int kTransposeInstancesPerGroup = 16;
    std::array<int, kTransposeGroupCounts.size()> transpose_exercised{};
    for (int group_index = 0; group_index < static_cast<int>(kTransposeGroupCounts.size()); ++group_index)
    for (int repetition = 0; repetition < kTransposeInstancesPerGroup; ++repetition)
    {
        const int g = kTransposeGroupCounts[group_index];
        gst::Graph graph = RandomConnectedGraph(random, g + 2);
        graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
        for (gst::UndirectedEdge& edge : graph.edges)
        {
            if (edge.w == 0.0)
                edge.w = 0.25;
            graph.minimum_edge_weight = std::min(graph.minimum_edge_weight, edge.w);
        }
        for (std::vector<gst::AdjEdge>& adjacency : graph.adj)
            for (gst::AdjEdge& edge : adjacency)
                edge.w = graph.edges[edge.edge_id].w;
        gst::Query query;
        query.groups.resize(g);
        for (int group = 0; group < g; ++group)
            query.groups[group] = {group + 1};
        const double expected = ExactSubsetDp(graph, query);
        const auto answer = gst::methods::abhss::SolveOneQuery(graph, query, enhanced);
        Check("ABHSS-Enhanced omitted-half transpose", answer, expected, group_index * kTransposeInstancesPerGroup + repetition);
        transpose_exercised[group_index] += answer.mask_vertex_states != 0;
    }
    for (int group_index = 0; group_index < static_cast<int>(kTransposeGroupCounts.size()); ++group_index)
        if (!transpose_exercised[group_index])
            throw std::runtime_error("ABHSS omitted-half transpose stress panel skipped a group-count state search.");
    std::cout << "ABHSS configurations matched exact subset DP on "
              << kInstances
              << " deterministic random instances with g=2..10, 500 positive unique-terminal stress instances with g=6..10, and "
              << kTransposeGroupCounts.size() * kTransposeInstancesPerGroup
              << " omitted-half transpose instances with g=7..16; state totals="
              << base_state_total << '/' << directed_state_total << '/'
              << enhanced_state_total << '\n';
    return 0;
}
