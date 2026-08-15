#include "core.h"

#include "diagnostics.h"
#include "forward.h"
#include "adjoint.h"

namespace gst::methods::abhss::internal
{
namespace
{
/**
 * @brief 读取低层锚定状态；空 mask 对应仅含永久锚组的组距离。
 *
 * 该统一读取器让高层 adjoint 的边界求值不需要为 A(0) 物化全图 row。
 */
double AnchoredValue(const Problem& p,
                     const std::vector<Row>& anchored,
                     int mask,
                     int vertex)
{
    return mask ? RowValue(anchored[mask], vertex)
                : p.group_distance[p.anchor_group][vertex];
}

/**
 * @brief 枚举一张高层 H row 与普通分块 D 的同根合法和。
 *
 * singleton 分块直接读取 dense 组距离；多组分块只允许使用 ordinary row
 * 中标记为不可继续同根拆分的 branch，保持与前向完成式相同的规范分解。
 */
template <class Use>
void ForEachBackwardBranchSum(const Problem& p,
                              int block,
                              const Row& backward,
                              Use&& use)
{
    if (p.popcount[block] == 1)
    {
        const auto& singleton =
            p.group_distance[p.bit_to_group[FirstBit(block)]];
        // lambda：把 singleton 组距离加到同顶点 H 值后交给调用者。
        ForEachValue(backward, [&](int vertex, double value)
        {
            use(vertex, value + singleton[vertex]);
        });
        return;
    }
    // lambda：把多组 ordinary 规范 branch 加到同顶点 H 值后交给调用者。
    ForEachRowBranchIntersection(
        backward, p.ordinary[block],
        [&](int vertex, double h, double d) { use(vertex, h + d); });
}

/** @brief 转置阶段在一个顶点处保存的普通状态及其对偶约化值。 */
struct TerminalEntry
{
    int mask = 0;
    double value = fp::kInf;
    double reduced = fp::kInf;
};

/**
 * @brief 按顶点转置 ordinary 状态，一次生成全部高层 adjoint 终端。
 *
 * 函数先按 64 个顶点为块聚合可用 D(S,v)，再根据补集关系、低/高层边界
 * 和 directed-cut 势筛选终端。输出按目标 mask 分组且顶点递增，后续 H
 * 闭包可直接线性装载。所有剪枝都只使用可采纳下界，不改变精确答案。
 */
void BuildTransposedTerminals(Problem& p,
                              int low_last,
                              int high_last,
                              bool build_three_block_terminals,
                              std::vector<std::vector<int>>& terminal_vertex,
                              std::vector<std::vector<double>>& terminal_value)
{
    terminal_vertex.assign(p.subset_count, {});
    terminal_value.assign(p.subset_count, {});
    std::array<std::vector<TerminalEntry>, 64> values_by_offset;
    std::vector<size_t> cursor(p.subset_count);
    std::vector<double> subset_potential(p.subset_count);
    std::vector<int> subset_stamp(p.subset_count);
    std::vector<double> value_at_mask(p.subset_count);
    std::vector<double> reduced_at_mask(p.subset_count);
    std::vector<int> value_stamp(p.subset_count);
    std::vector<double> pair_best(build_three_block_terminals ? p.subset_count : 0, fp::kInf);
    std::vector<int> touched_pair_unions;
    std::vector<double> terminal_best(p.subset_count, fp::kInf);
    std::vector<int> touched_targets;
    int potential_epoch = 0;
    int value_epoch = 0;
    std::array<std::array<int, 17>, 17> choose{};
    if (build_three_block_terminals)
        for (int n = 0; n <= 16; ++n)
        {
            choose[n][0] = choose[n][n] = 1;
            for (int k = 1; k < n; ++k)
                choose[n][k] = choose[n - 1][k - 1] + choose[n - 1][k];
        }
    const int minimum_terminal_cover = p.nonanchor_count - high_last;
    const int maximum_terminal_cover = p.nonanchor_count - low_last - 1;
    const size_t word_count = (static_cast<size_t>(p.graph.n + 1) + 63) / 64;

    for (size_t word = 0; word < word_count; ++word)
    {
        for (auto& values : values_by_offset)
            values.clear();
        const int first_vertex = std::max(1, static_cast<int>(word << 6));
        const int end_vertex =
            std::min(p.graph.n + 1, static_cast<int>((word + 1) << 6));
        for (int mask = 1; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] > p.half || !OrdinaryAvailable(p, mask))
                continue;
            if (p.popcount[mask] == 1)
            {
                const auto& row =
                    p.group_distance[p.bit_to_group[FirstBit(mask)]];
                for (int vertex = first_vertex; vertex < end_vertex; ++vertex)
                    values_by_offset[vertex & 63].push_back(
                        {mask, row[vertex], 0.0});
                continue;
            }
            const Row& row = p.ordinary[mask];
            size_t& index = cursor[mask];
            while (index < row.vertex.size() && row.vertex[index] < end_vertex)
            {
                values_by_offset[row.vertex[index] & 63].push_back(
                    {mask, row.value[index], 0.0});
                ++index;
            }
        }

        for (int vertex = first_vertex; vertex < end_vertex; ++vertex)
        {
            auto& values = values_by_offset[vertex & 63];
            if (values.empty())
                continue;
            ++potential_epoch;
            subset_stamp[0] = potential_epoch;
            subset_potential[0] = 0.0;
            // lambda：按当前顶点的 epoch 递归缓存任意 mask 的组势和。
            auto Potential = [&](auto&& self, int mask) -> double
            {
                if (subset_stamp[mask] == potential_epoch)
                    return subset_potential[mask];
                const int bit = mask & -mask;
                subset_stamp[mask] = potential_epoch;
                subset_potential[mask] =
                    self(self, mask ^ bit) +
                    p.dual.GroupAt(vertex, p.bit_to_group[FirstBit(bit)]);
                return subset_potential[mask];
            };
            const double full_potential =
                p.dual.GroupAt(vertex, p.anchor_group) +
                Potential(Potential, p.full_mask);
            const double budget = p.best - full_potential;
            if (budget < 0.0)
                continue;
            for (auto& entry : values)
                entry.reduced = entry.value - Potential(Potential, entry.mask);
            std::sort(values.begin(), values.end(), [](const auto& a, const auto& b)
            {
                return a.reduced != b.reduced ? a.reduced < b.reduced
                                              : a.mask < b.mask;
            });

            ++value_epoch;
            long long submask_work = 0;
            for (const auto& entry : values)
            {
                value_stamp[entry.mask] = value_epoch;
                value_at_mask[entry.mask] = entry.value;
                reduced_at_mask[entry.mask] = entry.reduced;
                submask_work +=
                    (static_cast<long long>(p.subset_count) >> p.popcount[entry.mask]) - 1;
            }
            int maximum_entry_size = 0;
            if (build_three_block_terminals)
                for (const auto& entry : values)
                    maximum_entry_size = std::max(maximum_entry_size, p.popcount[entry.mask]);
            const bool build_three = build_three_block_terminals && values.size() >= 3 && 3 * maximum_entry_size >= minimum_terminal_cover;
            const double triple_pair_limit = build_three ? budget - values.front().reduced : budget;
            const double pair_enumeration_limit = build_three ? std::max(budget, triple_pair_limit) : budget;

            long long pair_work = 0;
            size_t right_limit = values.size();
            for (size_t left = 0; left + 1 < values.size(); ++left)
            {
                while (right_limit > left + 1 && values[left].reduced + values[right_limit - 1].reduced > pair_enumeration_limit)
                    --right_limit;
                if (right_limit <= left + 1)
                    break;
                pair_work += static_cast<long long>(right_limit - left - 1);
            }

            touched_targets.clear();
            touched_pair_unions.clear();
            // lambda：登记某个 H 目标在当前顶点的最小外侧代价。
            auto Update = [&](int target, double value)
            {
                const int size = p.popcount[target];
                if (size <= low_last || size > high_last)
                    return;
                if (terminal_best[target] >= fp::kInf)
                    touched_targets.push_back(target);
                terminal_best[target] = std::min(terminal_best[target], value);
            };
            // lambda：按并集聚合两块最小同根和；固定并集后只需保留这一项与第三块结合。
            auto UpdatePair = [&](int pair_union, double value, double reduced)
            {
                if (!build_three || reduced > triple_pair_limit)
                    return;
                if (pair_best[pair_union] >= fp::kInf)
                    touched_pair_unions.push_back(pair_union);
                pair_best[pair_union] = std::min(pair_best[pair_union], value);
            };
            for (const auto& entry : values)
            {
                if (entry.reduced > budget)
                    break;
                Update(p.full_mask ^ entry.mask, entry.value);
            }
            if (pair_work <= submask_work)
            {
                for (size_t left = 0; left < values.size(); ++left)
                {
                    if (left + 1 == values.size() || values[left].reduced + values[left + 1].reduced > pair_enumeration_limit)
                        break;
                    for (size_t right = left + 1; right < values.size(); ++right)
                    {
                        const double pair_reduced = values[left].reduced + values[right].reduced;
                        if (pair_reduced > pair_enumeration_limit)
                            break;
                        if (values[left].mask & values[right].mask)
                            continue;
                        const int pair_union = values[left].mask | values[right].mask;
                        const double pair_value = values[left].value + values[right].value;
                        if (build_three)
                            UpdatePair(pair_union, pair_value, pair_reduced);
                        if (pair_reduced <= budget)
                            Update(p.full_mask ^ pair_union, pair_value);
                    }
                }
            }
            else
            {
                for (const auto& left : values)
                {
                    const int complement = p.full_mask ^ left.mask;
                    for (int right = complement; right; right = (right - 1) & complement)
                    {
                        if (right <= left.mask || value_stamp[right] != value_epoch)
                            continue;
                        const double pair_reduced = left.reduced + reduced_at_mask[right];
                        if (pair_reduced > pair_enumeration_limit)
                            continue;
                        const int pair_union = left.mask | right;
                        const double pair_value = left.value + value_at_mask[right];
                        if (build_three)
                            UpdatePair(pair_union, pair_value, pair_reduced);
                        if (pair_reduced <= budget)
                            Update(p.full_mask ^ pair_union, pair_value);
                    }
                }
            }

            // 三块的前两块先按并集做 min-plus 聚合；再与第三块结合即可保留
            // 每个三块并集的精确最小值，并把稠密组维度从四路指派降为两次三路指派。
            if (build_three)
            {
                for (int pair_union : touched_pair_unions)
                {
                    const int pair_size = p.popcount[pair_union];
                    const int minimum_third_size = std::max(1, minimum_terminal_cover - pair_size);
                    const int maximum_third_size = std::min(maximum_entry_size, maximum_terminal_cover - pair_size);
                    if (minimum_third_size > maximum_third_size)
                        continue;
                    const int complement = p.full_mask ^ pair_union;
                    const int complement_size = p.popcount[complement];
                    if (minimum_third_size > complement_size)
                        continue;
                    int submask_candidates = 0;
                    for (int third_size = minimum_third_size; third_size <= maximum_third_size && third_size <= complement_size; ++third_size)
                        submask_candidates += choose[complement_size][third_size];
                    const double pair_reduced = pair_best[pair_union] - Potential(Potential, pair_union);
                    // lambda：用同一 reduced budget 接纳与 pair-union 互斥的第三块。
                    auto TryThird = [&](int third)
                    {
                        if (value_stamp[third] != value_epoch)
                            return;
                        const int third_size = p.popcount[third];
                        if (third_size < minimum_third_size || third_size > maximum_third_size || pair_reduced + reduced_at_mask[third] > budget)
                            return;
                        Update(p.full_mask ^ (pair_union | third), pair_best[pair_union] + value_at_mask[third]);
                    };
                    if (submask_candidates <= static_cast<int>(values.size()))
                    {
                        std::array<int, 16> bit_value{};
                        int bit_count = 0;
                        for (int bits = complement; bits; bits &= bits - 1)
                            bit_value[bit_count++] = bits & -bits;
                        const unsigned limit = 1U << bit_count;
                        for (int third_size = minimum_third_size; third_size <= maximum_third_size && third_size <= bit_count; ++third_size)
                        {
                            unsigned combination = (1U << third_size) - 1;
                            while (combination < limit)
                            {
                                int third = 0;
                                for (unsigned bits = combination; bits; bits &= bits - 1)
                                    third |= bit_value[__builtin_ctz(bits)];
                                TryThird(third);
                                const unsigned low = combination & -combination;
                                const unsigned next = combination + low;
                                combination = (((next ^ combination) >> 2) / low) | next;
                            }
                        }
                    }
                    else
                    {
                        for (const auto& third_entry : values)
                        {
                            if (pair_reduced + third_entry.reduced > budget)
                                break;
                            if (!(third_entry.mask & pair_union))
                                TryThird(third_entry.mask);
                        }
                    }
                }
            }

            for (int target : touched_targets)
            {
                const int included = p.anchor_bit | p.original_mask[target];
                const double farthest = FarthestRemaining(p, vertex, included);
                const double prefix = std::max(
                    farthest,
                    std::max(p.tour.At(vertex, included, p.group_distance),
                             p.dual.GroupAt(vertex, p.anchor_group) +
                                 Potential(Potential, target)));
                if (terminal_best[target] + prefix < p.best)
                {
                    terminal_vertex[target].push_back(vertex);
                    terminal_value[target].push_back(terminal_best[target]);
                }
                terminal_best[target] = fp::kInf;
            }
            for (int pair_union : touched_pair_unions)
                pair_best[pair_union] = fp::kInf;
        }
    }
}

}  // namespace

void SolveHighAdjoint(Problem& p,
                       const std::vector<Row>& anchored,
                       int low_last,
                       int high_last,
                       bool build_three_block_terminals,
                       const char* probe_method)
{
    std::vector<std::vector<int>> terminal_vertex;
    std::vector<std::vector<double>> terminal_value;
    ProbeTimer transpose_timer;
    BuildTransposedTerminals(
        p, low_last, high_last, build_three_block_terminals, terminal_vertex, terminal_value);
    EmitAbhssProbe(probe_method, "adjoint_transpose", p, transpose_timer.Seconds());

    std::vector<Row> backward(p.subset_count);
    std::vector<double> distance(p.graph.n + 1, fp::kInf);
    std::vector<double> prefix_cache(p.graph.n + 1);
    std::vector<int> prefix_stamp(p.graph.n + 1);
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;

    // lambda：把一张 H successor 与全部合法低层 A/ordinary 边界同根结算。
    auto EvaluateBoundary = [&](int successor)
    {
        for (int mask = 0; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] > low_last || (mask & ~successor) ||
                (mask && !anchored[mask].ready))
                continue;
            const int block = successor ^ mask;
            if (!block)
                continue;
            // lambda：读取边界锚定值并用同根和更新完整可行上界。
            ForEachBackwardBranchSum(
                p, block, backward[successor], [&](int vertex, double value)
            {
                const double anchor = AnchoredValue(p, anchored, mask, vertex);
                if (anchor < fp::kInf)
                    p.best = std::min(p.best, anchor + value);
            });
        }
    };

    for (int size = high_last; size > low_last; --size)
    {
        for (int mask = 1; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] != size)
                continue;
            touched.clear();
            settled.clear();
            ++stamp;
            const int included = p.anchor_bit | p.original_mask[mask];
            // lambda：缓存当前 H 目标在顶点处的 farthest/tour/dual 公共前缀下界。
            auto Prefix = [&](int vertex)
            {
                if (prefix_stamp[vertex] == stamp)
                    return prefix_cache[vertex];
                prefix_stamp[vertex] = stamp;
                const double farthest = FarthestRemaining(p, vertex, included);
                prefix_cache[vertex] = std::max(
                    farthest,
                    std::max(p.tour.At(vertex, included, p.group_distance),
                             p.dual.At(vertex, included)));
                return prefix_cache[vertex];
            };
            // lambda：仅登记仍可能严格改善 incumbent 的 H 距离标签。
            auto Set = [&](int vertex, double value)
            {
                if (value >= distance[vertex] || !(value + Prefix(vertex) < p.best))
                    return;
                if (distance[vertex] >= fp::kInf)
                    touched.push_back(vertex);
                distance[vertex] = value;
            };
            for (size_t i = 0; i < terminal_vertex[mask].size(); ++i)
                Set(terminal_vertex[mask][i], terminal_value[mask][i]);
            const int outside = p.full_mask ^ mask;
            for (int block = outside; block; block = (block - 1) & outside)
            {
                const int successor = mask | block;
                if (p.popcount[successor] > high_last || !backward[successor].ready)
                    continue;
                // lambda：把 successor 与被移除 ordinary 块的同根和作为当前 H seed。
                ForEachBackwardBranchSum(
                    p, block, backward[successor], [&](int vertex, double value)
                {
                    Set(vertex, value);
                });
            }

            std::priority_queue<QueueNode,
                                std::vector<QueueNode>,
                                std::greater<QueueNode>> queue;
            for (int vertex : touched)
                queue.push({distance[vertex] + Prefix(vertex),
                            distance[vertex],
                            vertex});
            while (!queue.empty())
            {
                const QueueNode node = queue.top();
                queue.pop();
                if (node.distance != distance[node.vertex] || !(node.key < p.best))
                    continue;
                settled.push_back(node.vertex);
                for (const auto& edge : p.graph.adj[node.vertex])
                {
                    const double next = node.distance + edge.w;
                    if (next >= distance[edge.to] || !(next + Prefix(edge.to) < p.best))
                        continue;
                    if (distance[edge.to] >= fp::kInf)
                        touched.push_back(edge.to);
                    distance[edge.to] = next;
                    queue.push({next + Prefix(edge.to), next, edge.to});
                }
            }
            std::sort(settled.begin(), settled.end());
            settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
            Row& row = backward[mask];
            row.vertex = settled;
            row.value.reserve(settled.size());
            for (int vertex : settled)
                row.value.push_back(distance[vertex]);
            row.ready = true;
            // H(mask) 与 D/A 是不同的状态族；同一 mask 下的 touched 顶点
            // 已去重，因此这里批量计入实际发现的高层 adjoint 状态。
            p.AccountMaskVertexStates(touched.size());
            EvaluateBoundary(mask);
            for (int vertex : touched)
                distance[vertex] = fp::kInf;
        }
        EmitAbhssProbe(
            probe_method, "adjoint_layer", p, -1.0, &backward, size);
    }
}

}  // namespace gst::methods::abhss::internal
