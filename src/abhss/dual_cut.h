#ifndef ABHSS_DUAL_CUT_H
#define ABHSS_DUAL_CUT_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <queue>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "../common/float_compare.h"
#include "../common/graph_io.h"
#include "../common/query_io.h"


// dual 构造每条查询至多调用一次，却包含较大的 changed-arc/cone 冷路径。
// 明确禁止 IPO 把它并入 PrepareProblem，避免未开启增强的 Base 热布局受污染。
#if defined(_MSC_VER)
#define ABHSS_DUAL_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define ABHSS_DUAL_NOINLINE __attribute__((noinline))
#else
#define ABHSS_DUAL_NOINLINE
#endif

namespace gst::methods::dual_cut
{
/**
 * @brief `DirectedCut` 增强使用的有向割对偶势与 primal 见证容器。
 *
 * 类中只保留冻结算法实际调用的 changed-arc 构造、势查询、primal 恢复和
 * residual 生命周期管理。基础配置不构造该对象的数据；关闭增强即可沿同一
 * ABHSS 入口跳过全部成员操作。
 */
class DualCutPotential
{
public:
    /**
     * @brief 构造全部组势、保留 residual，并恢复一棵 primal 可行树。
     *
     * 各组按根距离递减分配有向边容量；每轮只从此前真正改写过的弧启动
     * residual 最短路修复，并只在截断势 cone 支撑上扣减容量。函数保持冷
     * 非内联边界；最后在固定容差内的数值零 residual 弧上恢复原图边
     * bitmap，候选路径始终按原边权计价。
     * 调用者可在生成 witness 后用 `ReleaseResidual` 回收 2m 临时数组。
     */
    ABHSS_DUAL_NOINLINE void BuildKeepingResidualChangedArcsWithPrimalEdges(
        const Graph& graph,
        const Query& query,
        const std::vector<std::vector<double>>& group_distance,
        int root)
    {
        BuildChangedArcs(graph, query, group_distance, root);
        primal_edge_words_.assign((static_cast<size_t>(graph.m) + 63) / 64, 0);
        primal_upper_ = RecoverPrimal(graph, query, root, residual_, primal_edge_words_);
    }

    /** @brief 释放只在预处理使用的 2m residual；保留势和 primal 边。 */
    void ReleaseResidual()
    {
        residual_.clear();
        residual_.shrink_to_fit();
    }

    /** @brief 返回仍在预处理生命周期内的有向 residual 数组只读引用。 */
    const std::vector<double>& Residual() const
    {
        return residual_;
    }

    /** @brief 返回恢复出的 primal 原图边 bitmap 只读引用。 */
    const std::vector<std::uint64_t>& PrimalEdgeWords() const
    {
        return primal_edge_words_;
    }

    /** @brief 返回 mask 内各完整证书组势和的最大值，即 directed-cut 下界。 */
    double At(int vertex, int mask) const
    {
        double value = 0.0;
        if (!vertex_potential_.empty())
        {
            const double* potential = vertex_potential_.data() + static_cast<size_t>(vertex) * potential_group_count_;
            for (int bits = mask; bits; bits &= bits - 1)
                value += potential[FirstBit(bits & -bits)];
            return value;
        }
        for (int bits = mask; bits; bits &= bits - 1)
            value += potential_[FirstBit(bits & -bits)][vertex];
        return value;
    }

    /**
     * @brief 返回除 excluded_mask 外全部组势之和。
     *
     * residual 购买并转置后，用缓存的全组势减去已覆盖组势；按非负浮点
     * 求和的标准 gamma 误差界向下修正，保证返回值不超过直接剩余组和。
     * 未转置时退化为原来的剩余 mask 直接求和。
     */
    bool CanImproveAllExcept(int vertex, int excluded_mask, double value, double incumbent, double& lower) const
    {
        if (!vertex_potential_.empty())
        {
            const int original_excluded_mask = excluded_mask;
            // lambda：用预计算全势和与标准浮点误差界返回安全的剩余势区间。
            auto CertifiedInterval = [&](const std::vector<double>& matrix, const std::vector<double>& full, double& low, double& high)
            {
                const double* potential = matrix.data() + static_cast<size_t>(vertex) * potential_group_count_;
                double excluded = 0.0;
                for (int bits = original_excluded_mask; bits; bits &= bits - 1)
                    excluded += potential[FirstBit(bits & -bits)];
                const double total = full[vertex];
                const double error = potential_sum_error_factor_ * (total + excluded);
                low = std::max(0.0, total - excluded - error);
                high = std::max(0.0, total - excluded + error);
            };
            double primary_low = 0.0;
            double primary_high = 0.0;
            CertifiedInterval(vertex_potential_, vertex_full_potential_, primary_low, primary_high);
            lower = primary_low;
            double upper = primary_high;
            if (!(value + lower < incumbent))
                return false;
            if (value + upper < incumbent)
                return true;
            lower = At(vertex, ((1 << potential_group_count_) - 1) ^ original_excluded_mask);
            return value + lower < incumbent;
        }
        const int group_count = static_cast<int>(potential_.size());
        lower = At(vertex, ((1 << group_count) - 1) ^ excluded_mask);
        return value + lower < incumbent;
    }

    /** @brief 读取一个原始查询组在指定顶点的单组对偶势。 */
    double GroupAt(int vertex, int group) const
    {
        if (!vertex_potential_.empty())
            return vertex_potential_[static_cast<size_t>(vertex) * potential_group_count_ + group];
        return potential_[group][vertex];
    }

    /** @brief 返回数值零 residual 弧上恢复的 primal 可行树真实边权。 */
    double PrimalUpper() const
    {
        return primal_upper_;
    }

    /**
     * @brief 返回 residual 全势闭包不可避免的图线性 primitive-work 底价。
     *
     * 2m+g(2m+n) 是只由图规模和组数给出的静态扫描底价；另一项逐段计入
     * residual 重放、势补全和 primal 恢复必需的 bitmap、changed-arc、原边与
     * 顶点扫描。购买价取两者最大值，不在初始 directed-cut 热循环维护一份
     * 以后不再消费的重复工作计数。
     */
    long long ResidualClosureBuyWork(const Graph& graph) const
    {
        if (potential_.empty() || residual_closure_complete_)
            return 0;
        const long long n = graph.n;
        const long long m = graph.m;
        const long long g = static_cast<long long>(potential_.size());
        const long long words = (2 * m + 63) / 64;
        const long long static_floor = 2 * m + g * (2 * m + n);
        const long long replay_floor = words + g * (words + changed_arc_count_);
        const long long completion_floor = g * (words + changed_arc_count_ + m + 3 * n);
        const long long closure_floor = 2 * m + replay_floor + completion_floor;
        return std::max(static_floor, closure_floor);
    }

    /**
     * @brief 从已保存的截断势重建 residual，完成全势闭包并恢复新的 primal 树。
     *
     * 初始预处理已经释放 O(m) residual。本函数只在 ordinary 的真实搜索工作
     * 支付上述 buy 后调用一次；重建严格重放各组势对两条有向容量的扣减，随后
     * 每组用剩余容量上的精确距离增加一个非负可行势。最终 primal 仍按原图边
     * 恢复和计价。调用者读取上界后应立即 ReleaseResidual。
     */
    ABHSS_DUAL_NOINLINE void CompleteResidualClosureKeepingResidualAndPrimalEdges(
        const Graph& graph,
        const Query& query,
        const std::vector<std::vector<double>>& group_distance,
        int root)
    {
        if (residual_closure_complete_)
            return;

        const std::vector<int> order = BuildOrder(group_distance, root);
        RestoreInitialResidual(graph, order);
        std::vector<std::uint64_t> primary_changed_arc_words = changed_arc_words_;
        std::vector<int> completion_order(order.rbegin(), order.rend());
        CompleteResidualPotentials(graph, group_distance, completion_order, primary_changed_arc_words);
        primal_edge_words_.assign((static_cast<size_t>(graph.m) + 63) / 64, 0);
        primal_upper_ = RecoverPrimal(graph, query, root, residual_, primal_edge_words_);
        TransposePotentialsByVertex(graph.n);
        changed_arc_words_.clear();
        changed_arc_words_.shrink_to_fit();
        residual_closure_complete_ = true;
    }

private:
    /** @brief closure 后把完整证书转为按顶点连续布局，并释放按组布局。 */
    void TransposePotentialsByVertex(int n)
    {
        potential_group_count_ = static_cast<int>(potential_.size());
        vertex_potential_.resize((static_cast<size_t>(n) + 1) * potential_group_count_);
        vertex_full_potential_.assign(n + 1, 0.0);
        const double unit_roundoff = std::numeric_limits<double>::epsilon() * 0.5;
        const double operations = 2.0 * potential_group_count_ + 4.0;
        const double gamma = operations * unit_roundoff / (1.0 - operations * unit_roundoff);
        potential_sum_error_factor_ = gamma / (1.0 - gamma);
        for (int vertex = 1; vertex <= n; ++vertex)
        {
            for (int group = 0; group < potential_group_count_; ++group)
            {
                const size_t index = static_cast<size_t>(vertex) * potential_group_count_ + group;
                vertex_potential_[index] = potential_[group][vertex];
                vertex_full_potential_[vertex] += potential_[group][vertex];
            }
        }
        potential_.clear();
        potential_.shrink_to_fit();
    }

    /** @brief 按根距离递减、组号递增并列规则返回统一的 directed-cut 组顺序。 */
    static std::vector<int> BuildOrder(
        const std::vector<std::vector<double>>& group_distance,
        int root)
    {
        std::vector<int> order(group_distance.size());
        std::iota(order.begin(), order.end(), 0);
        // lambda：锁定根距离递减、组号递增的确定性并列顺序。
        std::sort(order.begin(), order.end(), [&](int left, int right)
        {
            if (group_distance[left][root] != group_distance[right][root])
                return group_distance[left][root] > group_distance[right][root];
            return left < right;
        });
        return order;
    }

    /** @brief 重建原始容量并严格重放初始截断势，供两份闭包从同一点独立出发。 */
    void RestoreInitialResidual(const Graph& graph, const std::vector<int>& order)
    {
        residual_.assign(static_cast<size_t>(2) * graph.m, 0.0);
        for (const UndirectedEdge& edge : graph.edges)
        {
            residual_[2 * edge.id] = edge.w;
            residual_[2 * edge.id + 1] = edge.w;
        }
        for (int group : order)
            for (size_t word_index = 0; word_index < changed_arc_words_.size(); ++word_index)
            {
                std::uint64_t bits = changed_arc_words_[word_index];
                while (bits)
                {
#if defined(_MSC_VER)
                    unsigned long offset = 0;
                    _BitScanForward64(&offset, bits);
#else
                    const int offset = __builtin_ctzll(bits);
#endif
                    bits &= bits - 1;
                    const int arc = static_cast<int>(word_index * 64 + offset);
                    if (arc >= 2 * graph.m)
                        continue;
                    const UndirectedEdge& edge = graph.edges[arc / 2];
                    const bool forward = ArcIndex(edge.id, edge.u, edge.v) == arc;
                    const int from = forward ? edge.u : edge.v;
                    const int to = forward ? edge.v : edge.u;
                    const double gradient = std::max(0.0, potential_[group][from] - potential_[group][to]);
                    residual_[arc] = std::max(0.0, residual_[arc] - gradient);
                }
            }
    }

    /**
     * @brief 用 changed-arc 修复构造每个组的可行有向割势。
     *
     * 固定按根到组的原始距离递减处理各组。原始组距离对未改写弧满足三角
     * 不等式，因此第 t 轮只检查此前势函数改变过的弧，再从违反处正常传播；
     * 输出写入 `potential_` 与 `residual_`，不会改动输入距离表。
     */
    void BuildChangedArcs(
        const Graph& graph,
        const Query& query,
        const std::vector<std::vector<double>>& group_distance,
        int root)
    {
        using HeapItem = std::pair<double, int>;

        const int n = graph.n;
        const int g = static_cast<int>(query.groups.size());
        residual_closure_complete_ = false;
        vertex_potential_.clear();
        potential_group_count_ = 0;
        vertex_full_potential_.clear();
        potential_sum_error_factor_ = 0.0;
        changed_arc_words_.clear();
        changed_arc_count_ = 0;
        potential_.assign(g, std::vector<double>(n + 1));

        const std::vector<int> order = BuildOrder(group_distance, root);

        residual_.assign(static_cast<size_t>(2) * graph.m, 0.0);
        for (const UndirectedEdge& edge : graph.edges)
        {
            residual_[2 * edge.id] = edge.w;
            residual_[2 * edge.id + 1] = edge.w;
        }

        std::vector<double> distance(n + 1);
        std::vector<double> capped(n + 1);
        std::vector<unsigned char> in_cone(n + 1);
        std::vector<int> cone;
        std::vector<std::uint64_t> changed_arc_words(
            (static_cast<size_t>(2) * graph.m + 63) / 64);
        long long changed_arc_count = 0;

        // lambda：把首次发生 residual 改变的有向弧登记到稀疏位图并计数。
        auto MarkChangedArc = [&](int arc)
        {
            std::uint64_t& word = changed_arc_words[static_cast<size_t>(arc) >> 6];
            const std::uint64_t bit = std::uint64_t{1} << (arc & 63);
            if (!(word & bit))
            {
                word |= bit;
                ++changed_arc_count;
            }
        };

        for (int order_index = 0; order_index < g; ++order_index)
        {
            const int group = order[order_index];
            distance = group_distance[group];
            std::priority_queue<HeapItem,
                                std::vector<HeapItem>,
                                std::greater<HeapItem>> heap;

            if (order_index > 0)
            {
                // 对每条已改写弧检查一次 Bellman 松弛。这里的 residual 弧
                // 方向与“到组距离”的传播方向相反，所以 source/target 对调。
                for (size_t word_index = 0;
                     word_index < changed_arc_words.size();
                     ++word_index)
                {
                    std::uint64_t bits = changed_arc_words[word_index];
                    while (bits)
                    {
#if defined(_MSC_VER)
                        unsigned long offset = 0;
                        _BitScanForward64(&offset, bits);
#else
                        const int offset = __builtin_ctzll(bits);
#endif
                        bits &= bits - 1;
                        const int arc =
                            static_cast<int>(word_index * 64 + offset);
                        if (arc >= 2 * graph.m)
                            continue;

                        const UndirectedEdge& edge = graph.edges[arc / 2];
                        const bool forward =
                            ArcIndex(edge.id, edge.u, edge.v) == arc;
                        const int target = forward ? edge.u : edge.v;
                        const int source = forward ? edge.v : edge.u;
                        const double next = residual_[arc] + distance[source];
                        if (next < distance[target])
                        {
                            distance[target] = next;
                            heap.push({next, target});
                        }
                    }
                }
            }

            while (!heap.empty())
            {
                const auto [value, vertex] = heap.top();
                heap.pop();
                if (value != distance[vertex])
                    continue;
                for (const AdjEdge& edge : graph.adj[vertex])
                {
                    const int arc = ArcIndex(edge.edge_id, edge.to, vertex);
                    const double next = value + residual_[arc];
                    if (next < distance[edge.to])
                    {
                        distance[edge.to] = next;
                        heap.push({next, edge.to});
                    }
                }
            }

            const double root_distance = distance[root];
            cone.clear();
            size_t cone_degree = 0;
            for (int vertex = 1; vertex <= n; ++vertex)
            {
                capped[vertex] = std::min(distance[vertex], root_distance);
                potential_[group][vertex] = capped[vertex];
                // potential cone 只含严格低于根 cap 的顶点。cone 外势值全都等于
                // root_distance，因此两端都在 cone 外的边梯度严格为零。
                if (capped[vertex] < root_distance)
                {
                    in_cone[vertex] = 1;
                    cone.push_back(vertex);
                    cone_degree += graph.adj[vertex].size();
                }
            }

            // 无论采用稀疏 cone 邻接还是稠密原边扫描，都只枚举同一批可能有
            // 非零梯度的边并执行同一 residual 更新。这只是确定性的物理遍历
            // 选择，不读取图名、g、时间或配置，也不改变 directed-cut 证书。
            auto ApplyPotentialGradient = [&](const UndirectedEdge& edge)
            {
                const double forward =
                    std::max(0.0, capped[edge.u] - capped[edge.v]);
                const double backward =
                    std::max(0.0, capped[edge.v] - capped[edge.u]);
                const int forward_arc = ArcIndex(edge.id, edge.u, edge.v);
                const int backward_arc = ArcIndex(edge.id, edge.v, edge.u);
                if (forward > 0.0)
                    MarkChangedArc(forward_arc);
                if (backward > 0.0)
                    MarkChangedArc(backward_arc);
                residual_[forward_arc] =
                    std::max(0.0, residual_[forward_arc] - forward);
                residual_[backward_arc] =
                    std::max(0.0, residual_[backward_arc] - backward);
            };
            if (cone_degree < graph.edges.size())
            {
                // cone 较小时只扫其邻接。cone 内边由原边记录的 u 端处理一次；
                // 跨边只有一个 cone 端，自然也只处理一次。自环梯度恒为零。
                for (int vertex : cone)
                {
                    for (const AdjEdge& adjacent : graph.adj[vertex])
                    {
                        const UndirectedEdge& edge = graph.edges[adjacent.edge_id];
                        if (edge.u == edge.v)
                            continue;
                        if (in_cone[adjacent.to] && edge.u != vertex)
                            continue;
                        ApplyPotentialGradient(edge);
                    }
                }
            }
            else
            {
                for (const UndirectedEdge& edge : graph.edges)
                    if (in_cone[edge.u] || in_cone[edge.v])
                        ApplyPotentialGradient(edge);
            }
            for (int vertex : cone)
                in_cone[vertex] = 0;
        }
        changed_arc_count_ = changed_arc_count;
        changed_arc_words_ = std::move(changed_arc_words);
    }

    /**
     * @brief 用第一阶段未支付的 residual 容量完成一次全图距离势闭包。
     *
     * 每个增量势是当前 residual 上到该组的精确最短距离；从同向 residual
     * 弧扣除其正梯度后，新增势只使用尚未分配的容量。故各组增量可安全累加。
     * 固定组顺序只执行一遍：完成一组后每个可达顶点已有到该组的零 residual
     * 路径，不需要轮数、收敛阈值或数据相关停止条件。
     */
    void CompleteResidualPotentials(
        const Graph& graph,
        const std::vector<std::vector<double>>& group_distance,
        const std::vector<int>& order,
        std::vector<std::uint64_t>& changed_arc_words)
    {
        using HeapItem = std::pair<double, int>;

        const int n = graph.n;
        std::vector<double> distance(n + 1);
        std::vector<double> capped(n + 1);
        // lambda：把新增势首次触及的有向弧并入后续组的修复启动位图。
        auto MarkChangedArc = [&](int arc)
        {
            changed_arc_words[static_cast<size_t>(arc) >> 6] |= std::uint64_t{1} << (arc & 63);
        };

        for (int group : order)
        {
            distance = group_distance[group];
            std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;

            // residual 只相对原边权下降；从全部已改写弧启动即可修复精确距离。
            for (size_t word_index = 0; word_index < changed_arc_words.size(); ++word_index)
            {
                std::uint64_t bits = changed_arc_words[word_index];
                while (bits)
                {
#if defined(_MSC_VER)
                    unsigned long offset = 0;
                    _BitScanForward64(&offset, bits);
#else
                    const int offset = __builtin_ctzll(bits);
#endif
                    bits &= bits - 1;
                    const int arc = static_cast<int>(word_index * 64 + offset);
                    if (arc >= 2 * graph.m)
                        continue;
                    const UndirectedEdge& edge = graph.edges[arc / 2];
                    const bool forward = ArcIndex(edge.id, edge.u, edge.v) == arc;
                    const int target = forward ? edge.u : edge.v;
                    const int source = forward ? edge.v : edge.u;
                    const double next = residual_[arc] + distance[source];
                    if (next < distance[target])
                    {
                        distance[target] = next;
                        heap.push({next, target});
                    }
                }
            }
            while (!heap.empty())
            {
                const auto [value, vertex] = heap.top();
                heap.pop();
                if (value != distance[vertex])
                    continue;
                for (const AdjEdge& edge : graph.adj[vertex])
                {
                    const int arc = ArcIndex(edge.edge_id, edge.to, vertex);
                    const double next = value + residual_[arc];
                    if (next < distance[edge.to])
                    {
                        distance[edge.to] = next;
                        heap.push({next, edge.to});
                    }
                }
            }

            // 不可达分量统一取最大有限值；分量之间无边，因此不产生非法梯度。
            double cap = 0.0;
            for (int vertex = 1; vertex <= n; ++vertex)
                if (distance[vertex] < fp::kInf)
                    cap = std::max(cap, distance[vertex]);
            for (int vertex = 1; vertex <= n; ++vertex)
            {
                capped[vertex] = std::min(distance[vertex], cap);
                potential_[group][vertex] += capped[vertex];
            }

            // 从同向容量扣除本组新增势梯度，逐弧保持 residual 非负。
            for (const UndirectedEdge& edge : graph.edges)
            {
                if (capped[edge.u] == cap && capped[edge.v] == cap)
                    continue;
                const double forward = std::max(0.0, capped[edge.u] - capped[edge.v]);
                const double backward = std::max(0.0, capped[edge.v] - capped[edge.u]);
                const int forward_arc = ArcIndex(edge.id, edge.u, edge.v);
                const int backward_arc = ArcIndex(edge.id, edge.v, edge.u);
                if (forward > 0.0)
                    MarkChangedArc(forward_arc);
                if (backward > 0.0)
                    MarkChangedArc(backward_arc);
                residual_[forward_arc] = std::max(0.0, residual_[forward_arc] - forward);
                residual_[backward_arc] = std::max(0.0, residual_[backward_arc] - backward);
            }
        }
    }

    /**
     * @brief 在固定容差内的数值零 residual 有向弧上逐次连接未覆盖组。
     * @return 恢复出的原图边并集真实权重；无法覆盖全部组时返回正无穷。
     *
     * Dijkstra 仍按原边权计价，并把选中路径写入调用者持有的 edge bitmap，
     * 因此结果是可独立复核的 primal 见证，而不是只依赖对偶值的上界数字。
     */
    static double RecoverPrimal(
        const Graph& graph,
        const Query& query,
        int root,
        const std::vector<double>& residual,
        std::vector<std::uint64_t>& primal_edge_words,
        long long* work = nullptr)
    {
        using HeapItem = std::pair<double, int>;

        const int g = static_cast<int>(query.groups.size());
        const int full_mask = (1 << g) - 1;
        std::vector<int> color(graph.n + 1);
        for (int group = 0; group < g; ++group)
            for (int vertex : query.groups[group])
                color[vertex] |= 1 << group;

        std::vector<char> in_tree(graph.n + 1);
        std::vector<int> parent(graph.n + 1);
        std::vector<int> parent_edge(graph.n + 1, -1);
        std::vector<double> distance(graph.n + 1, fp::kInf);
        in_tree[root] = 1;
        int covered = color[root];
        double cost = 0.0;

        while (covered != full_mask)
        {
            if (work)
                *work += 2LL * graph.n;
            std::priority_queue<HeapItem,
                                std::vector<HeapItem>,
                                std::greater<HeapItem>> heap;
            std::fill(distance.begin(), distance.end(), fp::kInf);
            for (int vertex = 1; vertex <= graph.n; ++vertex)
            {
                if (!in_tree[vertex])
                    continue;
                distance[vertex] = 0.0;
                parent[vertex] = 0;
                heap.push({0.0, vertex});
            }

            int found = 0;
            while (!heap.empty())
            {
                const auto [value, vertex] = heap.top();
                heap.pop();
                if (work)
                    ++*work;
                if (value != distance[vertex])
                    continue;
                if (color[vertex] & (full_mask ^ covered))
                {
                    found = vertex;
                    break;
                }
                if (work)
                    *work += static_cast<long long>(graph.adj[vertex].size());
                for (const AdjEdge& edge : graph.adj[vertex])
                {
                    const int arc = ArcIndex(edge.edge_id, vertex, edge.to);
                    const double tolerance = 1e-10 * std::max(1.0, edge.w);
                    if (residual[arc] > tolerance)
                        continue;
                    const double next = value + edge.w;
                    if (next < distance[edge.to])
                    {
                        distance[edge.to] = next;
                        parent[edge.to] = vertex;
                        parent_edge[edge.to] = edge.edge_id;
                        heap.push({next, edge.to});
                    }
                }
            }
            if (!found)
                return fp::kInf;

            cost += distance[found];
            for (int vertex = found; vertex && !in_tree[vertex];
                 vertex = parent[vertex])
            {
                const int edge_id = parent_edge[vertex];
                primal_edge_words[static_cast<size_t>(edge_id) >> 6] |=
                    std::uint64_t{1} << (edge_id & 63);
                in_tree[vertex] = 1;
                covered |= color[vertex];
            }
        }
        return cost;
    }

    /** @brief 返回非零 mask 的最低位编号，用于组势位循环。 */
    static int FirstBit(int mask)
    {
#if defined(_MSC_VER)
        unsigned long index = 0;
        _BitScanForward(&index, static_cast<unsigned long>(mask));
        return static_cast<int>(index);
#else
        return __builtin_ctz(static_cast<unsigned int>(mask));
#endif
    }

    /** @brief 将无向 edge id 与方向映射为稳定的 `[0,2m)` 有向弧编号。 */
    static int ArcIndex(int edge_id, int from, int to)
    {
        return 2 * edge_id + (from < to ? 0 : 1);
    }

    std::vector<std::vector<double>> potential_;
    std::vector<double> vertex_potential_;
    int potential_group_count_ = 0;
    std::vector<double> vertex_full_potential_;
    double potential_sum_error_factor_ = 0.0;
    std::vector<double> residual_;
    std::vector<std::uint64_t> primal_edge_words_;
    std::vector<std::uint64_t> changed_arc_words_;
    long long changed_arc_count_ = 0;
    double primal_upper_ = fp::kInf;
    bool residual_closure_complete_ = false;
};
}  // namespace gst::methods::dual_cut

#undef ABHSS_DUAL_NOINLINE

#endif  // ABHSS_DUAL_CUT_H
