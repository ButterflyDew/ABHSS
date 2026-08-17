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

/**
 * @brief 枚举 H successor 与新增 ordinary 块的全部精确同根和。
 *
 * 补集转置后，普通规范拆分的 branch 可能位于 successor 所代表的一侧；
 * 因此递推侧不能再次要求新增块为 branch。额外候选仍是两棵互斥 rooted
 * 子树的真实并，只会增加等价可行推导，不会得到虚假的较低值。
 */
template <class Use>
void ForEachBackwardValueSum(const Problem& p, int block, const Row& backward, Use&& use)
{
    if (p.popcount[block] == 1)
    {
        const auto& singleton = p.group_distance[p.bit_to_group[FirstBit(block)]];
        // lambda：把 singleton 精确距离加到同顶点 H 值。
        ForEachValue(backward, [&](int vertex, double value) { use(vertex, value + singleton[vertex]); });
        return;
    }
    // lambda：把普通 row 全值加到同顶点 H 值；branch 约束可能已由 successor 承担。
    ForEachRowValueIntersection(backward, p.ordinary[block], [&](int vertex, double h, double d) { use(vertex, h + d); });
}

/** @brief 转置阶段在一个顶点处保存的普通状态及其对偶约化值。 */
struct TerminalEntry
{
    int mask = 0;
    double value = fp::kInf;
    double reduced = fp::kInf;
};

/**
 * @brief 按顶点转置 ordinary 状态，为每个物化 H 层补齐直接 ordinary split。
 *
 * 对目标 H(S)=D(full\S)，若一个规范 split 的较大一侧超过 ordinary 边界，
 * 它由已完成的 successor H 加回较小 ordinary 块；若两侧都不超过边界，
 * 则必须在这里直接转置。辅助半层仍负责恢复被省略的首张 D，较低层的
 * 直接 terminal 负责补齐 successor 递推无法表示的平衡 split。所有候选
 * 都由互斥的真实 rooted D 值组成，筛选只使用可采纳下界。
 */
void BuildTransposedTerminals(Problem& p,
                              int low_last,
                              int high_last,
                              std::vector<std::vector<int>>& terminal_vertex,
                              std::vector<std::vector<double>>& terminal_value)
{
    terminal_vertex.assign(p.subset_count, {});
    terminal_value.assign(p.subset_count, {});
    const int minimum_terminal_cover = p.nonanchor_count - high_last;
    const int maximum_terminal_cover = p.nonanchor_count - low_last - 1;

    // 偶数组数的辅助层可直接转置已完成 D；此时同层双块 split 被精确 D 支配。
    const int representative = minimum_terminal_cover ? (1 << minimum_terminal_cover) - 1 : 0;
    const bool direct_auxiliary_layer = minimum_terminal_cover == 0 || OrdinaryAvailable(p, representative);

    // 所有已完成 ordinary mask 都可能成为某个 H 层的直接 split；只筛一次。
    std::vector<int> transposed_masks;
    for (int mask = 1; mask < p.subset_count; ++mask)
        if (OrdinaryAvailable(p, mask))
            transposed_masks.push_back(mask);

    std::array<std::vector<TerminalEntry>, 64> values_by_offset;
    std::vector<size_t> cursor(p.subset_count);
    std::vector<double> subset_potential(p.subset_count);
    std::vector<int> subset_stamp(p.subset_count);
    std::vector<double> value_at_mask(p.subset_count);
    std::vector<double> reduced_at_mask(p.subset_count);
    std::vector<int> value_stamp(p.subset_count);
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
        const int end_vertex = std::min(p.graph.n + 1, static_cast<int>((word + 1) << 6));
        for (int mask : transposed_masks)
        {
            const int size = p.popcount[mask];
            if (size == 1)
            {
                const auto& row = p.group_distance[p.bit_to_group[FirstBit(mask)]];
                for (int vertex = first_vertex; vertex < end_vertex; ++vertex)
                    values_by_offset[vertex & 63].push_back({mask, row[vertex], 0.0});
                continue;
            }
            const Row& row = p.ordinary[mask];
            size_t& index = cursor[mask];
            while (index < row.vertex.size() && row.vertex[index] < end_vertex)
            {
                values_by_offset[row.vertex[index] & 63].push_back({mask, row.value[index], 0.0});
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
                subset_potential[mask] = self(self, mask ^ bit) + p.dual.GroupAt(vertex, p.bit_to_group[FirstBit(bit)]);
                return subset_potential[mask];
            };
            const double full_potential = p.dual.GroupAt(vertex, p.anchor_group) + Potential(Potential, p.full_mask);
            const double budget = p.best - full_potential;
            if (budget < 0.0)
                continue;
            for (auto& entry : values)
                entry.reduced = entry.value - Potential(Potential, entry.mask);
            std::sort(values.begin(), values.end(), [](const auto& a, const auto& b)
            {
                return a.reduced != b.reduced ? a.reduced < b.reduced : a.mask < b.mask;
            });

            touched_targets.clear();
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

            // 已有完整 ordinary row 时直接转置；辅助层的该项还支配同层 pair。
            for (const auto& entry : values)
            {
                if (entry.reduced > budget)
                    break;
                const int cover = p.popcount[entry.mask];
                if (cover >= minimum_terminal_cover && cover <= maximum_terminal_cover)
                    Update(p.full_mask ^ entry.mask, entry.value);
            }

            ++value_epoch;
            long long submask_work = 0;
            for (const auto& entry : values)
            {
                value_stamp[entry.mask] = value_epoch;
                value_at_mask[entry.mask] = entry.value;
                reduced_at_mask[entry.mask] = entry.reduced;
                submask_work += (static_cast<long long>(p.subset_count) >> p.popcount[entry.mask]) - 1;
            }

            // 先估计排序 pair 与互补 submask 的真实循环项数，再枚举同一候选集。
            long long pair_work = 0;
            size_t right_limit = values.size();
            for (size_t left = 0; left + 1 < values.size(); ++left)
            {
                while (right_limit > left + 1 && values[left].reduced + values[right_limit - 1].reduced > budget)
                    --right_limit;
                if (right_limit <= left + 1)
                    break;
                pair_work += static_cast<long long>(right_limit - left - 1);
                if (pair_work > submask_work)
                    break;
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
                        const int cover = p.popcount[pair_union];
                        if (cover < minimum_terminal_cover || cover > maximum_terminal_cover)
                            continue;
                        if (direct_auxiliary_layer && cover == minimum_terminal_cover)
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
                        if (right <= left.mask || value_stamp[right] != value_epoch)
                            continue;
                        const int pair_union = left.mask | right;
                        const int cover = p.popcount[pair_union];
                        if (cover < minimum_terminal_cover || cover > maximum_terminal_cover)
                            continue;
                        if (direct_auxiliary_layer && cover == minimum_terminal_cover)
                            continue;
                        const double pair_reduced = left.reduced + reduced_at_mask[right];
                        if (pair_reduced <= budget)
                            Update(p.full_mask ^ pair_union, left.value + value_at_mask[right]);
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
    BuildTransposedTerminals(p, low_last, high_last, terminal_vertex, terminal_value);
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
        // 旧循环扫描整个子集格后用 `(mask & ~successor)` 拒绝；这里按相同
        // 数值升序只生成 successor 的子掩码，减空全部必定失败的迭代。
        for (int mask = 0;; mask = (mask - successor) & successor)
        {
            // 所有不高于 low_last 的 A row 已按前向层序发布；successor 更高，
            // 所以 block 必非空。只保留定义域条件，不重复检查生命周期。
            if (p.popcount[mask] <= low_last)
            {
                const int block = successor ^ mask;
                // lambda：读取边界锚定值并用同根和更新完整可行上界。
                ForEachBackwardBranchSum(p, block, backward[successor], [&](int vertex, double value)
                {
                    const double anchor = AnchoredValue(p, anchored, mask, vertex);
                    if (anchor < fp::kInf)
                        p.best = std::min(p.best, anchor + value);
                });
            }
            if (mask == successor)
                break;
        }
    };

    for (int size = high_last; size > low_last; --size)
    {
        ProbeTimer layer_timer;
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
                if (p.popcount[successor] > high_last)
                    continue;
                // 合法 successor 位于已完成的更高 H 层，生命周期检查恒真。
                // lambda：补集侧可能已含规范 branch；新增 ordinary 块读取全部精确值。
                ForEachBackwardValueSum(
                    p, block, backward[successor], [&](int vertex, double value)
                {
                    Set(vertex, value);
                });
            }

            // 所有初始 H 标签均已缓存 prefix；统一线性建堆保持确定性出堆次序。
            SearchQueue queue = BuildInitialQueue(touched, distance, prefix_cache);
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
            // 奇数 g 的辅助半格把非锚组恰好二等分。两侧 D(h) 都由 H(h)
            // realization 承担，因此互补 H row 完成后直接与锚组距离结算；
            // 这逐项对应完整 ordinary 半格的 anchor + D(h) + D(h)。
            const int complement = p.full_mask ^ mask;
            if (size == high_last && p.popcount[complement] == size && complement < mask && backward[complement].ready)
            {
                const Row& other = backward[complement];
                const double previous_best = p.best;
                // lambda：把互补辅助 H row 与锚组距离结算为真实平衡完成上界。
                ForEachRowValueIntersection(row, other, [&](int vertex, double left, double right)
                {
                    const double anchor = p.group_distance[p.anchor_group].ExactValueOrInf(vertex);
                    if (anchor < fp::kInf)
                        p.best = std::min(p.best, left + right + anchor);
                });
                if (p.best < previous_best)
                    EmitAbhssProbe(probe_method, "adjoint_complementary_half_upper", p, -1.0, nullptr, size);
            }
            EvaluateBoundary(mask);
            for (int vertex : touched)
                distance[vertex] = fp::kInf;
        }
        EmitAbhssProbe(
            probe_method, "adjoint_layer", p, layer_timer.Seconds(), &backward, size);
    }
}

}  // namespace gst::methods::abhss::internal
