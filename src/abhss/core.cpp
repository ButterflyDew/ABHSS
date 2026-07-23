#include "core.h"

#include <numeric>

#include "diagnostics.h"

namespace gst::methods::abhss::internal
{
double EarlyAnchor::Value(const Problem& p, int bit, int vertex) const
{
    const double exact = RowValue(row[bit], vertex);
    if (exact < fp::kInf)
        return exact;
    const int continuation = p.nonanchor_original_mask ^ p.original_mask[bit];
    return std::max(0.0, cutoff - FarthestRemaining(p, vertex, continuation));
}

double EarlyAnchor::Future(const Problem& p, int remaining, int vertex)
{
    if (first.empty() || !remaining)
        return 0.0;
    if (first[vertex] == 255)
    {
        double first_value = -1.0;
        double second_value = -1.0;
        int first_bit = -1;
        int second_bit = -1;
        for (int bit = 1; bit < p.subset_count; bit <<= 1)
        {
            if (!row[bit].ready)
                continue;
            const double value = Value(p, bit, vertex);
            if (value > first_value)
            {
                second_value = first_value;
                second_bit = first_bit;
                first_value = value;
                first_bit = FirstBit(bit);
            }
            else if (value > second_value)
            {
                second_value = value;
                second_bit = FirstBit(bit);
            }
        }
        if (first_bit >= 0)
        {
            first[vertex] = static_cast<unsigned char>(first_bit);
            if (second_bit >= 0)
                second[vertex] = static_cast<unsigned char>(second_bit);
        }
    }

    const int a = first[vertex];
    if (a != 255 && (remaining & (1 << a)))
        return Value(p, 1 << a, vertex);
    const int b = second[vertex];
    if (b != 255 && (remaining & (1 << b)))
        return Value(p, 1 << b, vertex);

    double value = 0.0;
    for (int bits = remaining; bits; bits &= bits - 1)
    {
        const int bit = bits & -bits;
        if (row[bit].ready)
            value = std::max(value, Value(p, bit, vertex));
    }
    return value;
}

void BuildEarlyAnchorRows(Problem& p, EarlyAnchor& early)
{
    early.cutoff = p.best;
    early.row.assign(p.subset_count, {});
    if (p.half < 3)
        return;

    std::vector<double> distance(p.graph.n + 1, fp::kInf);
    std::vector<double> continuation_cache(p.graph.n + 1);
    std::vector<int> continuation_stamp(p.graph.n + 1);
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;
    for (int mask = 1; mask < p.subset_count; mask <<= 1)
    {
        const int group = p.bit_to_group[FirstBit(mask)];
        const int continuation = p.nonanchor_original_mask ^ p.original_mask[mask];
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
        p.group_distance[group].ForEachExact(p.graph.n, [&](int vertex, double value)
        {
            const auto& anchor = p.group_distance[p.anchor_group];
            if (!anchor.IsExact(vertex))
                return;
            const double candidate = value + anchor[vertex];
            if (!(candidate < early.cutoff) || candidate >= distance[vertex])
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
                !(node.distance < early.cutoff) || !(node.key < p.best))
                continue;
            settled.push_back(node.vertex);
            for (const auto& edge : p.graph.adj[node.vertex])
            {
                const double next = node.distance + edge.w;
                if (!(next < early.cutoff) || next >= distance[edge.to])
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
        settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
        Row& row = early.row[mask];
        row.vertex = settled;
        row.value.reserve(settled.size());
        for (int vertex : settled)
            row.value.push_back(distance[vertex]);
        row.ready = true;
        for (int vertex : touched)
            distance[vertex] = fp::kInf;
    }
    early.first.assign(p.graph.n + 1, 255);
    early.second.assign(p.graph.n + 1, 255);
}

namespace
{
/**
 * @brief 以 rent-or-buy 规则调度见证树 subset DP 上界求值。
 *
 * buy 是见证树节点数乘以完整 subset 合并工作；ordinary 每张 row 新增的
 * queue pop/edge relax 作为 rent。累计 rent 达到 buy 才重新求值，避免在
 * 上界尚无足够潜在收益时反复扫描见证树。配置差异只影响见证来源及是否
 * 做零 rent 初次求值，调度规则本身完全共用。
 */
class WitnessUpperScheduler
{
public:
    /**
     * @brief 绑定查询与 ordinary 表，并按见证大小计算一次 buy 成本。
     * @param evaluate_initial true 时在普通状态生成前立即求一次初始上界。
     */
    WitnessUpperScheduler(Problem& problem,
                          const std::vector<Row>& ordinary,
                          bool evaluate_initial)
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
        if (enabled_ && evaluate_initial)
            Evaluate();
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

void BuildOrdinaryRows(Problem& p, EarlyAnchor* early)
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
    // queue pop/relax 为一次见证树 subset DP 支付 rent。基础配置的根路径
    // 见证需要零 rent 初次求值；开启 directed-cut 后 primal/facility 已在
    // 预处理收紧上界，故只在后续 rent 达阈值时再次购买同一求值过程。
    WitnessUpperScheduler witness(
        p, p.ordinary, p.UsesBoundedGroupDistances());

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
                    if (early)
                        bound_cache[vertex] = std::max(
                            bound_cache[vertex],
                            early->Future(p, remaining_nonanchor, vertex));
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
                if (early)
                {
                    lower = std::max(
                        lower, early->Future(p, remaining_nonanchor, vertex));
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
