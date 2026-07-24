#include "core.h"

#include <numeric>

#include "diagnostics.h"

namespace gst::methods::abhss::internal
{
namespace
{
/** 高位表示该 top-two 值来自 cone 外公式，而非 row.value 下标。 */
constexpr std::uint32_t kA1FallbackLocator = std::uint32_t{1} << 31;
}

double AnchoredSingletonFuture::Value(const Problem& p,
                                      int bit,
                                      int vertex) const
{
    std::uint32_t locator = 0;
    return ValueWithLocator(p, bit, vertex, locator);
}

double AnchoredSingletonFuture::ValueWithLocator(
    const Problem& p,
    int bit,
    int vertex,
    std::uint32_t& locator) const
{
    const Row& values = row[bit];
    const auto it = std::lower_bound(
        values.vertex.begin(), values.vertex.end(), vertex);
    if (it != values.vertex.end() && *it == vertex)
    {
        locator = static_cast<std::uint32_t>(
            it - values.vertex.begin());
        return values.value[locator];
    }
    locator = kA1FallbackLocator;
    const int continuation = p.nonanchor_original_mask ^ p.original_mask[bit];
    return std::max(0.0, cutoff - FarthestRemaining(p, vertex, continuation));
}

double AnchoredSingletonFuture::LocatedValue(
    const Problem& p,
    int bit,
    int vertex,
    std::uint32_t locator) const
{
    if (!(locator & kA1FallbackLocator))
        return row[bit].value[locator];
    const int continuation = p.nonanchor_original_mask ^ p.original_mask[bit];
    return std::max(0.0, cutoff - FarthestRemaining(p, vertex, continuation));
}

double AnchoredSingletonFuture::Future(const Problem& p,
                                       int remaining,
                                       int vertex)
{
    if (first.empty() || !remaining)
        return 0.0;
    if (first[vertex] == 255)
    {
        double first_value = -1.0;
        double second_value = -1.0;
        int first_bit = -1;
        int second_bit = -1;
        std::uint32_t first_locator = 0;
        std::uint32_t second_locator = 0;
        for (int bit = 1; bit < p.subset_count; bit <<= 1)
        {
            if (!row[bit].ready)
                continue;
            std::uint32_t locator = 0;
            const double value =
                ValueWithLocator(p, bit, vertex, locator);
            if (value > first_value)
            {
                second_value = first_value;
                second_bit = first_bit;
                second_locator = first_locator;
                first_value = value;
                first_bit = FirstBit(bit);
                first_locator = locator;
            }
            else if (value > second_value)
            {
                second_value = value;
                second_bit = FirstBit(bit);
                second_locator = locator;
            }
        }
        if (first_bit >= 0)
        {
            first[vertex] = static_cast<unsigned char>(first_bit);
            if (second_bit >= 0)
                second[vertex] = static_cast<unsigned char>(second_bit);
            cached_locator_pair[vertex] =
                static_cast<std::uint64_t>(first_locator) |
                (static_cast<std::uint64_t>(second_locator) << 32);
        }
    }

    const int a = first[vertex];
    if (a != 255 && (remaining & (1 << a)))
        return LocatedValue(
            p,
            1 << a,
            vertex,
            static_cast<std::uint32_t>(cached_locator_pair[vertex]));
    const int b = second[vertex];
    if (b != 255 && (remaining & (1 << b)))
        return LocatedValue(
            p,
            1 << b,
            vertex,
            static_cast<std::uint32_t>(cached_locator_pair[vertex] >> 32));

    double value = 0.0;
    for (int bits = remaining; bits; bits &= bits - 1)
    {
        const int bit = bits & -bits;
        if (row[bit].ready)
            value = std::max(value, Value(p, bit, vertex));
    }
    return value;
}

void BuildReusableAnchoredSingletonLayer(
    Problem& p,
    AnchoredSingletonFuture& singleton_future)
{
    singleton_future.cutoff = p.best;
    singleton_future.row.assign(p.subset_count, {});

    std::vector<double> distance(p.graph.n + 1, fp::kInf);
    std::vector<double> continuation_cache(p.graph.n + 1);
    std::vector<int> continuation_stamp(p.graph.n + 1);
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;
    for (int mask = 1; mask < p.subset_count; mask <<= 1)
    {
        const int group = p.bit_to_group[FirstBit(mask)];
        const int continuation =
            p.nonanchor_original_mask ^ p.original_mask[mask];
        ++stamp;
        auto Continuation = [&](int vertex)
        {
            if (continuation_stamp[vertex] != stamp)
            {
                continuation_stamp[vertex] = stamp;
                continuation_cache[vertex] =
                    FarthestRemaining(p, vertex, continuation);
            }
            return continuation_cache[vertex];
        };
        touched.clear();
        settled.clear();
        std::priority_queue<QueueNode,
                            std::vector<QueueNode>,
                            std::greater<QueueNode>> queue;
        p.group_distance[group].ForEachExact(
            p.graph.n, [&](int vertex, double value)
        {
            const auto& anchor = p.group_distance[p.anchor_group];
            if (!anchor.IsExact(vertex))
                return;
            const double candidate = value + anchor[vertex];
            if (!(candidate < singleton_future.cutoff) ||
                candidate >= distance[vertex])
                return;
            const double key = candidate + Continuation(vertex);
            if (!(key < p.best))
                return;
            if (distance[vertex] >= fp::kInf)
                touched.push_back(vertex);
            distance[vertex] = candidate;
            queue.push({key, candidate, vertex});
        });

        while (!queue.empty())
        {
            const QueueNode node = queue.top();
            queue.pop();
            if (node.distance != distance[node.vertex] ||
                !(node.distance < singleton_future.cutoff) ||
                !(node.key < p.best))
                continue;
            settled.push_back(node.vertex);
            for (const auto& edge : p.graph.adj[node.vertex])
            {
                const double next = node.distance + edge.w;
                if (!(next < singleton_future.cutoff) ||
                    next >= distance[edge.to])
                    continue;
                const double key = next + Continuation(edge.to);
                if (!(key < p.best))
                    continue;
                if (distance[edge.to] >= fp::kInf)
                    touched.push_back(edge.to);
                distance[edge.to] = next;
                queue.push({key, next, edge.to});
            }
        }

        std::sort(settled.begin(), settled.end());
        settled.erase(
            std::unique(settled.begin(), settled.end()), settled.end());
        Row& row = singleton_future.row[mask];
        row.vertex = settled;
        row.value.reserve(settled.size());
        for (int vertex : settled)
            row.value.push_back(distance[vertex]);
        row.ready = true;
        // 该 A1 在这里首次物化并计数；移交给前向内核时只重滤和结算，
        // 不会再次登记同一批 `(mask,v)`。
        p.AccountMaskVertexStates(touched.size());
        for (int vertex : touched)
            distance[vertex] = fp::kInf;
    }
    singleton_future.first.assign(p.graph.n + 1, 255);
    singleton_future.second.assign(p.graph.n + 1, 255);
    // 每个实际查询过 future 的顶点只写一次 packed locator；未访问顶点的
    // 虚拟页面不触发物理 RSS，也避免两个独立数组的两次随机首次写入。
    singleton_future.cached_locator_pair.reset(
        new std::uint64_t[static_cast<size_t>(p.graph.n) + 1]);
}

namespace
{
/**
 * @brief 以 rent-or-buy 规则调度见证树 subset DP 上界求值。
 *
 * buy 是见证树节点数乘以完整 subset 合并工作；ordinary 每张 row 新增的
 * queue pop/edge relax 作为 rent。累计 rent 达到 buy 才重新求值，避免在
 * 上界尚无足够潜在收益时反复扫描见证树。每种配置的初始上界职责都在
 * 状态层开始前完成：Base 求 root-path witness，DirectedCut 配置以 primal
 * upper/witness 替换并额外求 facility 上界；本调度器只负责后续按实际
 * 工作量重新购买公共树 DP。
 */
class WitnessUpperScheduler
{
public:
    /**
     * @brief 绑定查询与 ordinary 表，并按见证大小计算一次 buy 成本。
     */
    WitnessUpperScheduler(Problem& problem,
                          const std::vector<Row>& ordinary)
        : problem_(problem), ordinary_(ordinary)
    {
        if (problem_.witness_tree.vertex.empty())
            return;
        long long merge_operations = 1;
        for (int bit = 0; bit < problem_.nonanchor_count; ++bit)
            merge_operations *= 3;
        buy_ = static_cast<long long>(problem_.witness_tree.vertex.size()) *
               ((merge_operations - 1) / 2 + merge_operations);
        enabled_ = buy_ > 0;
    }

    /** @brief 累加当前 row 的实际工作量，达到 buy 阈值时触发一次求值。 */
    void Account(long long row_work)
    {
        if (!enabled_)
            return;
        rent_ += row_work;
        if (rent_ >= buy_)
        {
            Evaluate();
            rent_ = 0;
        }
    }

private:
    /** @brief 在当前已 ready 的 ordinary row 上求见证树可行上界并收紧 incumbent。 */
    void Evaluate()
    {
        problem_.best = std::min(
            problem_.best,
            EvaluateWitnessTree(problem_.witness_tree, problem_, ordinary_));
    }

    Problem& problem_;
    const std::vector<Row>& ordinary_;
    long long rent_ = 0;
    long long buy_ = 0;
    bool enabled_ = false;
};

/**
 * @brief 枚举三个 ordinary 分块与永久锚组在共同根相遇的可行解。
 *
 * 从实际可枚举值最少的一侧驱动，缺失状态立即拒绝。该完成式在三分块
 * 大小达到平衡点时收紧上界，但不替代后续精确状态枚举。
 */
template <class Use>
void ForEachTriple(const Problem& p, int first, int second, int third, Use&& use)
{
    const int masks[3] = {first, second, third};
    int driver = 0;
    size_t driver_size = static_cast<size_t>(p.graph.n);
    for (int mask : masks)
    {
        if (!mask)
            continue;
        const size_t size = p.popcount[mask] == 1
                                ? p.group_distance[p.bit_to_group[FirstBit(mask)]].ExactSize(
                                      p.graph.n)
                                : p.ordinary[mask].vertex.size();
        if (size < driver_size)
        {
            driver = mask;
            driver_size = size;
        }
    }
    auto Visit = [&](int vertex)
    {
        double total = p.group_distance[p.anchor_group][vertex];
        if (!p.group_distance[p.anchor_group].IsExact(vertex))
            return;
        for (int mask : masks)
        {
            if (!mask)
                continue;
            if (p.popcount[mask] == 1)
            {
                const int group = p.bit_to_group[FirstBit(mask)];
                if (!p.group_distance[group].IsExact(vertex))
                    return;
                total += p.group_distance[group][vertex];
            }
            else
            {
                const double value = RowValue(p.ordinary[mask], vertex);
                if (value >= fp::kInf)
                    return;
                total += value;
            }
        }
        use(vertex, total);
    };
    if (driver)
        ForEachOrdinaryValue(p, driver, [&](int vertex, double) { Visit(vertex); });
    else
        for (int vertex = 1; vertex <= p.graph.n; ++vertex)
            Visit(vertex);
}
}  // namespace

void BuildOrdinaryRows(Problem& p,
                       AnchoredSingletonFuture* singleton_future)
{
    std::vector<double> distance(p.graph.n + 1, fp::kInf);
    std::vector<double> split(p.graph.n + 1, fp::kInf);
    std::vector<double> bound_cache(p.graph.n + 1);
    std::vector<int> bound_stamp(p.graph.n + 1);
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;
    std::vector<int> seeds;

    const int three_block_limit = (p.nonanchor_count + 2) / 3;
    // queue pop/relax 为下一次见证树 subset DP 支付 rent。初次求值已在
    // PrepareProblem 末尾完成，保证 A1 与 D 都看到同一个收紧后 incumbent。
    WitnessUpperScheduler witness(p, p.ordinary);

    for (int size = 1; size <= p.half; ++size)
    {
        long long layer_work = 0;
        for (int mask = 1; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] != size || size == 1)
                continue;
            long long row_work = 0;
            touched.clear();
            settled.clear();
            const int remaining_original = p.original_full_mask ^ p.original_mask[mask];
            const int remaining_nonanchor = p.full_mask ^ mask;
            ++stamp;
            auto Bound = [&](int vertex)
            {
                if (bound_stamp[vertex] != stamp)
                {
                    bound_stamp[vertex] = stamp;
                    bound_cache[vertex] = FutureBound(p, vertex, remaining_original);
                    if (singleton_future)
                        bound_cache[vertex] = std::max(
                            bound_cache[vertex],
                            singleton_future->Future(
                                p, remaining_nonanchor, vertex));
                }
                return bound_cache[vertex];
            };

            auto CanImprove = [&](int vertex, double value)
            {
                if (bound_stamp[vertex] == stamp)
                    return value + bound_cache[vertex] < p.best;

                double lower = 0.0;
                if (p.UsesDirectedCut())
                {
                    lower = p.dual.At(vertex, remaining_original);
                    if (!(value + lower < p.best))
                        return false;
                }
                lower = std::max(
                    lower, FarthestRemaining(p, vertex, remaining_original));
                if (!(value + lower < p.best))
                    return false;
                if (singleton_future)
                {
                    lower = std::max(
                        lower,
                        singleton_future->Future(
                            p, remaining_nonanchor, vertex));
                    if (!(value + lower < p.best))
                        return false;
                }
                lower = std::max(
                    lower,
                    p.tour.At(vertex, remaining_original, p.group_distance));
                bound_stamp[vertex] = stamp;
                bound_cache[vertex] = lower;
                return value + lower < p.best;
            };
            auto Set = [&](int vertex, double value)
            {
                if (value >= distance[vertex] || !CanImprove(vertex, value))
                    return;
                if (distance[vertex] >= fp::kInf)
                    touched.push_back(vertex);
                distance[vertex] = value;
            };

            if (size == 2)
            {
                const int first = mask & -mask;
                const int second = mask ^ first;
                ForEachCommonValue(p, first, second, [&](int vertex, double a, double b)
                {
                    Set(vertex, a + b);
                });
            }
            else
            {
                const int domain = mask ^ (mask & -mask);
                for (int branch = domain; branch; branch = (branch - 1) & domain)
                {
                    const int accumulator = mask ^ branch;
                    if (!OrdinaryAvailable(p, accumulator) ||
                        !OrdinaryAvailable(p, branch))
                        continue;
                    ForEachPivotBranch(p,
                                       accumulator,
                                       branch,
                                       [&](int vertex, double a, double b)
                    {
                        Set(vertex, a + b);
                    });
                }
            }

            seeds = touched;
            for (int vertex : seeds)
                split[vertex] = distance[vertex];
            std::priority_queue<QueueNode,
                                std::vector<QueueNode>,
                                std::greater<QueueNode>> queue;
            for (int vertex : touched)
                queue.push({distance[vertex] + Bound(vertex),
                            distance[vertex], vertex});
            while (!queue.empty())
            {
                const QueueNode node = queue.top();
                queue.pop();
                ++row_work;
                if (node.distance != distance[node.vertex] || !(node.key < p.best))
                    continue;
                settled.push_back(node.vertex);
                for (const auto& edge : p.graph.adj[node.vertex])
                {
                    ++row_work;
                    const double next = node.distance + edge.w;
                    if (next >= distance[edge.to] || !CanImprove(edge.to, next))
                        continue;
                    if (distance[edge.to] >= fp::kInf)
                        touched.push_back(edge.to);
                    distance[edge.to] = next;
                    queue.push({next + Bound(edge.to), next, edge.to});
                }
            }

            std::sort(settled.begin(), settled.end());
            settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
            Row& row = p.ordinary[mask];
            row.vertex = settled;
            row.value.reserve(settled.size());
            row.branch_bits.assign((settled.size() + 63) / 64, 0);
            double minimum = fp::kInf;
            for (size_t index = 0; index < settled.size(); ++index)
            {
                const int vertex = settled[index];
                row.value.push_back(distance[vertex]);
                if (distance[vertex] < split[vertex])
                {
                    row.branch_bits[index >> 6] |=
                        std::uint64_t{1} << (index & 63);
                    ++row.branch_count;
                }
                minimum = std::min(minimum, distance[vertex]);
            }
            row.ready = true;
            p.ordinary_minimum[mask] = minimum;
            // 一张 D(mask) 只生成一次。使用 touched 而非 queue pop 数，既
            // 包含已接纳但因 incumbent 收紧而未 settle 的状态，又不重复
            // 统计同一顶点的多次改进或过期队列项。
            p.AccountMaskVertexStates(touched.size());

            layer_work += row_work;
            witness.Account(row_work);

            if (size == p.half)
            {
                const int complement = p.full_mask ^ mask;
                if (OrdinaryAvailable(p, complement) &&
                    (p.popcount[complement] < size ||
                     (p.popcount[complement] == size && complement < mask)))
                    ForEachCommonValue(p, mask, complement, [&](int vertex, double a, double b)
                    {
                        if (p.group_distance[p.anchor_group].IsExact(vertex))
                            p.best = std::min(
                                p.best,
                                a + b + p.group_distance[p.anchor_group][vertex]);
                    });
            }

            if (size == three_block_limit)
            {
                const int remaining = p.full_mask ^ mask;
                for (int second = remaining;; second = (second - 1) & remaining)
                {
                    const int third = remaining ^ second;
                    if (second <= third && p.popcount[second] <= size &&
                        p.popcount[third] <= size &&
                        (!second || OrdinaryAvailable(p, second)) &&
                        (!third || OrdinaryAvailable(p, third)))
                        ForEachTriple(p, mask, second, third, [&](int, double value)
                        {
                            p.best = std::min(p.best, value);
                        });
                    if (!second)
                        break;
                }
            }

            for (int vertex : seeds)
                split[vertex] = fp::kInf;
            for (int vertex : touched)
                distance[vertex] = fp::kInf;
        }

        EmitAbhssProbe(ProbeFamilyMethod(p),
                       "ordinary_layer",
                       p,
                       -1.0,
                       &p.ordinary,
                       size,
                       layer_work);
    }
}

}  // namespace gst::methods::abhss::internal
