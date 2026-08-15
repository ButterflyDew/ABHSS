#include "forward.h"

#include "diagnostics.h"

namespace gst::methods::abhss::internal
{
namespace
{
/** @brief 判断锚定 mask 是否为隐式 A(0) 或已经完成物化。 */
bool AnchoredAvailable(const std::vector<Row>& anchored, int mask)
{
    return !mask || anchored[mask].ready;
}

/**
 * @brief 枚举 A(anchor_side,v)+D(ordinary_side,v) 的合法同根种子。
 *
 * A(0) 和 singleton D 分别直接读取组距离；有界距离必须额外检查 exact
 * membership。多组 ordinary 只使用规范 branch，避免把可继续同根拆分的
 * 中间值重复注入锚定递推。
 */
template <class Use>
void ForEachAnchoredSum(const Problem& p,
                        const std::vector<Row>& anchored,
                        int anchor_side,
                        int ordinary_side,
                        Use&& use)
{
    const bool exact_membership_required = p.UsesBoundedGroupDistances();
    if (!anchor_side)
    {
        const GroupRow& anchor = p.group_distance[p.anchor_group];
        if (!exact_membership_required)
        {
            // lambda：完整距离模式下直接把隐式 A(0) 加到 ordinary branch。
            ForEachOrdinaryBranch(p, ordinary_side, [&](int vertex, double value)
            {
                use(vertex, value + anchor[vertex]);
            });
        }
        else
        {
            // lambda：bounded 模式下仅在锚组距离精确时提交隐式 A(0) 合并。
            ForEachOrdinaryBranch(p, ordinary_side, [&](int vertex, double value)
            {
                const double anchor_distance = anchor.ExactValueOrInf(vertex);
                if (anchor_distance < fp::kInf)
                    use(vertex, value + anchor_distance);
            });
        }
        return;
    }
    if (p.popcount[ordinary_side] == 1)
    {
        const GroupRow& singleton =
            p.group_distance[p.bit_to_group[FirstBit(ordinary_side)]];
        if (!exact_membership_required)
        {
            // lambda：完整距离模式下把 singleton ordinary 距离加到锚定值。
            ForEachValue(anchored[anchor_side], [&](int vertex, double value)
            {
                use(vertex, value + singleton[vertex]);
            });
        }
        else
        {
            // lambda：bounded 模式下仅在 singleton 距离精确时提交锚定合并。
            ForEachValue(anchored[anchor_side], [&](int vertex, double value)
            {
                const double singleton_distance = singleton.ExactValueOrInf(vertex);
                if (singleton_distance < fp::kInf)
                    use(vertex, value + singleton_distance);
            });
        }
        return;
    }

    // lambda：把锚定值与多组 ordinary 规范 branch 的同根和交给调用者。
    ForEachRowBranchIntersection(
        anchored[anchor_side], p.ordinary[ordinary_side],
        [&](int vertex, double a, double d) { use(vertex, a + d); });
}

/**
 * @brief 用当前 A(mask,·) 与至多两张 ordinary row 结算完整 GST 上界。
 *
 * 先用各 row 最小值做廉价整体拒绝，再从实际候选最少的一侧驱动同根检查。
 * `roots` 与 `anchored_distance` 只在本 row 生命周期内有效；函数不保存引用。
 */
void CompleteAnchoredRow(Problem& p,
                         int mask,
                         const std::vector<int>& roots,
                         const std::vector<double>& anchored_distance,
                         double anchored_minimum)
{
    const int remaining = p.full_mask ^ mask;
    for (int left = remaining;; left = (left - 1) & remaining)
    {
        const int right = remaining ^ left;
        if (left <= right && p.popcount[left] <= p.half &&
            p.popcount[right] <= p.half &&
            (!left || OrdinaryAvailable(p, left)) &&
            (!right || OrdinaryAvailable(p, right)))
        {
            const double left_minimum =
                !left || p.popcount[left] == 1 ? 0.0 : p.ordinary_minimum[left];
            const double right_minimum =
                !right || p.popcount[right] == 1 ? 0.0 : p.ordinary_minimum[right];
            if (anchored_minimum + left_minimum + right_minimum < p.best)
            {
                int driver = 0;
                size_t driver_size = roots.size();
                for (int side : {left, right})
                {
                    if (!side)
                        continue;
                    const size_t size = p.popcount[side] == 1
                                            ? p.group_distance[
                                                  p.bit_to_group[FirstBit(side)]].ExactSize(
                                                  p.graph.n)
                                            : p.ordinary[side].vertex.size();
                    if (size < driver_size)
                    {
                        driver = side;
                        driver_size = size;
                    }
                }

                // lambda：在候选根处结算锚定侧与两个 ordinary 分块的同根和。
                auto Visit = [&](int vertex, bool require_root_membership)
                {
                    if (require_root_membership &&
                        !std::binary_search(roots.begin(), roots.end(), vertex))
                        return;
                    const double a = left ? OrdinaryValue(p, left, vertex) : 0.0;
                    const double b = right ? OrdinaryValue(p, right, vertex) : 0.0;
                    if (a >= fp::kInf || b >= fp::kInf)
                        return;
                    p.best = std::min(p.best, anchored_distance[vertex] + a + b);
                };
                if (driver)
                {
                    // lambda：由最小 ordinary 候选集驱动并要求根属于 roots。
                    ForEachOrdinaryValue(p, driver, [&](int vertex, double)
                    {
                        Visit(vertex, true);
                    });
                }
                else
                    for (int vertex : roots)
                        Visit(vertex, false);
            }
        }
        if (!left)
            break;
    }
}
}  // namespace

std::vector<Row> BuildForwardAnchoredRows(
    Problem& p,
    const ForwardAnchoredPlan& plan,
    std::vector<Row> anchored)
{
    if (anchored.empty())
        anchored.resize(p.subset_count);
    else if (anchored.size() != static_cast<size_t>(p.subset_count))
        anchored.resize(p.subset_count);

    std::vector<double> distance(p.graph.n + 1, fp::kInf);
    std::vector<double> bound_cache(p.graph.n + 1);
    std::vector<int> bound_stamp(p.graph.n + 1);
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;

    // 完整锚定格没有任何正层时，A(0) 才直接参加最终完成式。不能仅凭
    // `last_size==0` 判断，因为 Enhanced 的低层前缀可以为空、而高层 H
    // 仍负责非空逻辑后缀。A(0) 始终是隐式组距离，不写入一张全图 row。
    if (plan.complete_implicit_anchor)
    {
        std::vector<int> roots;
        // lambda：把隐式 A(0) 的全部精确锚组距离装入一次性完成工作区。
        p.group_distance[p.anchor_group].ForEachExact(
            p.graph.n, [&](int vertex, double value)
        {
            roots.push_back(vertex);
            distance[vertex] = value;
        });
        CompleteAnchoredRow(p, 0, roots, distance, 0.0);
        for (int vertex : roots)
            distance[vertex] = fp::kInf;
    }

    for (int size = 1; size <= plan.last_size; ++size)
    {
        for (int mask = 1; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] != size)
                continue;
            touched.clear();
            settled.clear();
            double minimum = fp::kInf;
            const int remaining_nonanchor = p.full_mask ^ mask;
            const int remaining_original =
                p.nonanchor_original_mask ^ p.original_mask[mask];
            ++stamp;
            // lambda：先用低成本 farthest 筛选，再缓存当前 A row 的完整 future。
            auto CanImprove = [&](int vertex, double value)
            {
                if (bound_stamp[vertex] != stamp)
                {
                    const double farthest =
                        FarthestRemaining(p, vertex, remaining_original);
                    if (!(value + farthest < p.best))
                        return false;
                    bound_stamp[vertex] = stamp;
                    bound_cache[vertex] =
                        FutureBound(p, vertex, remaining_original, farthest);
                }
                return value + bound_cache[vertex] < p.best;
            };
            // 提前调度的公共 A1 在证书 cone 内已经完成精确图闭包。这里只按
            // 更新后的 incumbent 重滤，并调用相同的完成结算；不是第二套 A1。
            const bool reuses_singleton_row = anchored[mask].ready;
            if (reuses_singleton_row)
            {
                Row precomputed = std::move(anchored[mask]);
                // lambda：按新 incumbent 重滤提前生成的 A1，并同步尝试 root-star 上界。
                ForEachValue(precomputed, [&](int vertex, double value)
                {
                    if (!CanImprove(vertex, value))
                        return;
                    distance[vertex] = value;
                    touched.push_back(vertex);
                    settled.push_back(vertex);
                    minimum = std::min(minimum, value);
                    double star = value;
                    for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                        star += p.group_distance[
                            p.bit_to_group[FirstBit(bits & -bits)]][vertex];
                    p.best = std::min(p.best, star);
                });
                CompleteAnchoredRow(p, mask, settled, distance, minimum);
            }
            else
            {
                // lambda：登记通过可采纳 future 严格筛选的 A 距离标签。
                auto Set = [&](int vertex, double value)
                {
                    if (value >= distance[vertex] || !CanImprove(vertex, value))
                        return;
                    if (distance[vertex] >= fp::kInf)
                        touched.push_back(vertex);
                    distance[vertex] = value;
                };
                for (int ordinary_side = mask;
                     ordinary_side;
                     ordinary_side = (ordinary_side - 1) & mask)
                {
                    const int anchor_side = mask ^ ordinary_side;
                    if (!OrdinaryAvailable(p, ordinary_side) ||
                        !AnchoredAvailable(anchored, anchor_side))
                        continue;
                    ForEachAnchoredSum(
                        p, anchored, anchor_side, ordinary_side, Set);
                }
                if (touched.empty())
                    continue;

                std::priority_queue<QueueNode,
                                    std::vector<QueueNode>,
                                    std::greater<QueueNode>> queue;
                for (int vertex : touched)
                    queue.push({distance[vertex] + bound_cache[vertex],
                                distance[vertex],
                                vertex});
                while (!queue.empty())
                {
                    const QueueNode node = queue.top();
                    queue.pop();
                    if (node.distance != distance[node.vertex] ||
                        !(node.key < p.best))
                        continue;
                    settled.push_back(node.vertex);
                    minimum = std::min(minimum, node.distance);
                    double star = node.distance;
                    for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                        star += p.group_distance[
                            p.bit_to_group[FirstBit(bits & -bits)]][node.vertex];
                    p.best = std::min(p.best, star);
                    for (const AdjEdge& edge : p.graph.adj[node.vertex])
                    {
                        const double next = node.distance + edge.w;
                        if (next >= distance[edge.to] || !CanImprove(edge.to, next))
                            continue;
                        if (distance[edge.to] >= fp::kInf)
                            touched.push_back(edge.to);
                        distance[edge.to] = next;
                        queue.push({next + bound_cache[edge.to], next, edge.to});
                    }
                }
                std::sort(settled.begin(), settled.end());
                settled.erase(
                    std::unique(settled.begin(), settled.end()), settled.end());
                CompleteAnchoredRow(p, mask, settled, distance, minimum);
            }

            // 新生成的 A(mask) 以 touched 中的不同顶点作为实际发现状态；
            // 提前生成的 A1 已登记；这里仅转移所有权，不重复计算状态数。
            if (!reuses_singleton_row)
                p.AccountMaskVertexStates(touched.size());

            // lambda：发布 row 前删除已被最新 incumbent 与 future 严格淘汰的顶点。
            settled.erase(
                std::remove_if(settled.begin(), settled.end(), [&](int vertex)
                {
                    return !(distance[vertex] + bound_cache[vertex] < p.best);
                }),
                settled.end());
            if (size < plan.last_size || plan.retain_last_layer)
            {
                Row& row = anchored[mask];
                row.vertex = settled;
                row.value.reserve(settled.size());
                for (int vertex : settled)
                    row.value.push_back(distance[vertex]);
                row.ready = true;
            }
            for (int vertex : touched)
                distance[vertex] = fp::kInf;
        }
        if (plan.probe_method != nullptr && plan.probe_phase != nullptr)
            EmitAbhssProbe(
                plan.probe_method, plan.probe_phase, p, -1.0, &anchored, size);
    }
    return anchored;
}

std::vector<Row> RunForwardAnchoredStage(
    Problem& p,
    const ForwardAnchoredPlan& plan,
    const char* start_phase,
    const char* end_phase,
    std::vector<Row> anchored)
{
    ProbeTimer timer;
    EmitAbhssProbe(plan.probe_method, start_phase, p);
    anchored = BuildForwardAnchoredRows(p, plan, std::move(anchored));
    EmitAbhssProbe(
        plan.probe_method, end_phase, p, timer.Seconds(), &anchored);
    return anchored;
}

}  // namespace gst::methods::abhss::internal
