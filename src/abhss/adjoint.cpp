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
 * @brief 按顶点转置 ordinary 状态，只生成精确辅助半层的 adjoint 终端。
 *
 * 较低 H 层由辅助 H 与 successor 递推完整生成，直接 terminal 被该递推严格支配。
 * 若补集大小的 ordinary 层已经 ready，单张 D 又逐值支配同目标的双块 split；
 * 否则按 64 个顶点为块枚举恰好覆盖补集的双块 seed。所有筛选只使用可采纳下界。
 */
void BuildTransposedTerminals(Problem& p,
                              int high_last,
                              std::vector<std::vector<int>>& terminal_vertex,
                              std::vector<std::vector<double>>& terminal_value)
{
    terminal_vertex.assign(p.subset_count, {});
    terminal_value.assign(p.subset_count, {});
    const int terminal_cover = p.nonanchor_count - high_last;
    bool direct_layer_available = true;
    for (int mask = 1; mask < p.subset_count; ++mask)
        if (p.popcount[mask] == terminal_cover)
            direct_layer_available = direct_layer_available && OrdinaryAvailable(p, mask);
    std::array<std::vector<TerminalEntry>, 64> values_by_offset;
    std::vector<size_t> cursor(p.subset_count);
    std::vector<double> subset_potential(p.subset_count);
    std::vector<int> subset_stamp(p.subset_count);
    std::vector<double> value_at_mask(direct_layer_available ? 0 : p.subset_count);
    std::vector<double> reduced_at_mask(direct_layer_available ? 0 : p.subset_count);
    std::vector<int> value_stamp(direct_layer_available ? 0 : p.subset_count);
    std::vector<double> terminal_best(p.subset_count, fp::kInf);
    std::vector<int> touched_targets;
    int potential_epoch = 0;
    int value_epoch = 0;
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
            const int size = p.popcount[mask];
            if (!OrdinaryAvailable(p, mask) || (direct_layer_available ? size != terminal_cover : size >= terminal_cover))
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
            if (!direct_layer_available)
                std::sort(values.begin(), values.end(), [](const auto& a, const auto& b)
                {
                    return a.reduced != b.reduced ? a.reduced < b.reduced : a.mask < b.mask;
                });

            touched_targets.clear();
            // lambda：登记辅助 H 目标在当前顶点的最小外侧代价。
            auto Update = [&](int target, double value)
            {
                if (p.popcount[target] != high_last)
                    return;
                if (terminal_best[target] >= fp::kInf)
                    touched_targets.push_back(target);
                terminal_best[target] = std::min(terminal_best[target], value);
            };
            if (direct_layer_available)
            {
                for (const auto& entry : values)
                    if (entry.reduced <= budget)
                        Update(p.full_mask ^ entry.mask, entry.value);
            }
            else
            {
                ++value_epoch;
                long long submask_work = 0;
                for (const auto& entry : values)
                {
                    value_stamp[entry.mask] = value_epoch;
                    value_at_mask[entry.mask] = entry.value;
                    reduced_at_mask[entry.mask] = entry.reduced;
                    submask_work += (static_cast<long long>(p.subset_count) >> p.popcount[entry.mask]) - 1;
                }
                long long pair_work = 0;
                size_t right_limit = values.size();
                for (size_t left = 0; left + 1 < values.size(); ++left)
                {
                    while (right_limit > left + 1 && values[left].reduced + values[right_limit - 1].reduced > budget)
                        --right_limit;
                    if (right_limit <= left + 1)
                        break;
                    pair_work += static_cast<long long>(right_limit - left - 1);
                }
                if (pair_work <= submask_work)
                {
                    for (size_t left = 0; left < values.size(); ++left)
                    {
                        if (left + 1 == values.size() || values[left].reduced + values[left + 1].reduced > budget)
                            break;
                        for (size_t right = left + 1; right < values.size(); ++right)
                        {
                            const double pair_reduced = values[left].reduced + values[right].reduced;
                            if (pair_reduced > budget)
                                break;
                            if (values[left].mask & values[right].mask)
                                continue;
                            const int pair_union = values[left].mask | values[right].mask;
                            if (p.popcount[pair_union] != terminal_cover)
                                continue;
                            Update(p.full_mask ^ pair_union, values[left].value + values[right].value);
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
                            if (right <= left.mask || value_stamp[right] != value_epoch || p.popcount[left.mask] + p.popcount[right] != terminal_cover)
                                continue;
                            const int pair_union = left.mask | right;
                            const double pair_reduced = left.reduced + reduced_at_mask[right];
                            if (pair_reduced > budget)
                                continue;
                            Update(p.full_mask ^ pair_union, left.value + value_at_mask[right]);
                        }
                    }
                }
            }

            for (int target : touched_targets)
            {
                const int included = p.anchor_bit | p.original_mask[target];
                const double farthest = FarthestRemaining(p, vertex, included);
                double prefix = std::max(farthest, p.dual.GroupAt(vertex, p.anchor_group) + Potential(Potential, target));
                if (prefix < p.tour.UpperEnvelope(included, farthest))
                    prefix = std::max(prefix, p.tour.At(vertex, included, p.group_distance));
                if (terminal_best[target] + prefix < p.best)
                {
                    terminal_vertex[target].push_back(vertex);
                    terminal_value[target].push_back(terminal_best[target]);
                }
                terminal_best[target] = fp::kInf;
            }
        }
    }
}

}  // namespace

void SolveHighAdjoint(Problem& p,
                       const std::vector<Row>& anchored,
                       int low_last,
                       int high_last,
                       const char* probe_method)
{
    std::vector<std::vector<int>> terminal_vertex;
    std::vector<std::vector<double>> terminal_value;
    ProbeTimer transpose_timer;
    BuildTransposedTerminals(p, high_last, terminal_vertex, terminal_value);
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
                double prefix = std::max(farthest, p.dual.At(vertex, included));
                if (prefix < p.tour.UpperEnvelope(included, farthest))
                    prefix = std::max(prefix, p.tour.At(vertex, included, p.group_distance));
                prefix_cache[vertex] = prefix;
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
                queue.push({distance[vertex] + prefix_cache[vertex],
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
                    queue.push({next + prefix_cache[edge.to], next, edge.to});
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
