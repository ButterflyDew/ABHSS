#include <algorithm>
#include <cmath>
#include <cstdint>
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
 * 独立精确 oracle；仅用于 n<=9 的工程回归，不进入论文性能实验。
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
}  // namespace

/** @brief 运行入口契约及 g=2..10 的 144 个确定性随机精确性实例。 */
int main()
{
    using gst::methods::abhss::AddedOperation;
    using gst::methods::abhss::Enhancement;
    using gst::methods::abhss::GroupDistanceRealization;
    using gst::methods::abhss::HighLayerRealization;
    using gst::methods::abhss::OrdinaryFutureRealization;
    using gst::methods::abhss::SolveOptions;
    using gst::methods::abhss::UpperWitnessRealization;

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

    // 进一步验证论文使用的“新增 + 同职责替换”关系。Base 不得拥有仅自己
    // 执行的逻辑阶段；距离、ordinary future、上界 witness 和高层完成均由
    // profile 显式选择 realization，真正新增的证书位只能单调增加。
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
        base_profile.group_distance != GroupDistanceRealization::BoundedCutoff ||
        directed_profile.group_distance !=
            GroupDistanceRealization::CompletePotential ||
        base_profile.ordinary_future !=
            OrdinaryFutureRealization::AnchoredSingletonCone ||
        directed_profile.ordinary_future !=
            OrdinaryFutureRealization::DirectedCutPotential ||
        base_profile.upper_witness != UpperWitnessRealization::RootPathTree ||
        directed_profile.upper_witness !=
            UpperWitnessRealization::DualPrimalTree ||
        base_profile.high_layer != HighLayerRealization::ForwardAnchoredA ||
        directed_profile.high_layer !=
            HighLayerRealization::ForwardAnchoredA ||
        enhanced_profile.high_layer != HighLayerRealization::AdjointH ||
        !enhanced_profile.Adds(AddedOperation::DirectedCutCertificate) ||
        !enhanced_profile.Adds(AddedOperation::FacilityUpperBound))
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
        if (base_schedule.highest_layer != expected_highest ||
            base_schedule.forward_last_layer != expected_highest ||
            base_schedule.uses_adjoint ||
            directed_schedule.highest_layer != expected_highest ||
            directed_schedule.forward_last_layer != expected_highest ||
            directed_schedule.uses_adjoint ||
            enhanced_schedule.highest_layer != expected_highest ||
            enhanced_schedule.forward_last_layer != expected_highest / 2 ||
            !enhanced_schedule.uses_adjoint)
            throw std::runtime_error(
                "ABHSS anchored layer boundary is not derived from the exact grid.");

        for (int layer = 1; layer <= expected_highest + 1; ++layer)
        {
            const bool required = layer <= expected_highest;
            if (base_schedule.ContainsLogicalLayer(layer) != required ||
                base_schedule.UsesForwardA(layer) != required ||
                base_schedule.UsesAdjointH(layer) ||
                directed_schedule.UsesForwardA(layer) != required ||
                directed_schedule.UsesAdjointH(layer) ||
                (enhanced_schedule.UsesForwardA(layer) &&
                 enhanced_schedule.UsesAdjointH(layer)) ||
                (enhanced_schedule.UsesForwardA(layer) ||
                 enhanced_schedule.UsesAdjointH(layer)) != required)
                throw std::runtime_error(
                    "ABHSS anchored layer has missing or duplicate realization.");
        }
    }

    CheckPreludeContracts(base, enhanced);
    CheckSubNanogapClosure(base);

    std::mt19937 random(0xAB455u);
    constexpr int kInstances = 144;
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
    std::cout << "ABHSS configurations matched exact subset DP on "
              << kInstances
              << " deterministic random instances with g=2..10; state totals="
              << base_state_total << '/' << directed_state_total << '/'
              << enhanced_state_total << '\n';
    return 0;
}
