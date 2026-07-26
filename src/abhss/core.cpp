#include "core.h"

#include <numeric>

namespace gst::methods::abhss::internal
{
namespace
{
/** 高位表示该 top-two 值来自 cone 外公式，而非 row.value 下标。 */
constexpr std::uint32_t kA1FallbackLocator = std::uint32_t{1} << 31;
} // namespace

/** @brief 计算 witness-tree subset DP 的理论 buy 工作量。 */
long long EstimateWitnessTreeDpWork(size_t witness_vertices, int nonanchor_count)
{
    // 对 k 个非锚组，固定最低 bit 消除左右对称后的非空局部拆分总数为
    // (3^k-1)/2；一条父子关系的全 mask/submask 卷积总数为 3^k。
    // WitnessTree 对每个真实顶点恰有一次局部处理和一条通向父亲（含虚拟
    // 超根）的关系，所以两项都乘真实 witness 顶点数。仓库限制 g<=16，
    // 因而 3^(g-1) 及其与输入顶点数的乘积处于 signed 64-bit 范围内。
    long long ternary_subsets = 1;
    for (int bit = 0; bit < nonanchor_count; ++bit)
        ternary_subsets *= 3;
    const long long work_per_vertex = (ternary_subsets - 1) / 2 + ternary_subsets;
    return static_cast<long long>(witness_vertices) * work_per_vertex;
}

/** @brief 绑定当前查询，并由 witness 大小计算共同 buy 阈值。 */
WitnessUpperScheduler::WitnessUpperScheduler(Problem& problem) : problem_(problem)
{
    buy_ = EstimateWitnessTreeDpWork(problem_.witness_tree.vertex.size(), problem_.nonanchor_count);
}

/** @brief 累计实际工作，在 rent 达到 buy 且 DP 输入更新后购买树 DP。 */
bool WitnessUpperScheduler::Account(long long row_work, bool ordinary_changed)
{
    if (ordinary_changed)
        ++ordinary_revision_;
    rent_ += row_work;

    // 第一次购买可只使用隐式 singleton；以后只有 ordinary 修订号变化才有
    // 新信息可供树 DP 消费。若 A1 的后续 row 又积满 buy，则保留 rent，等
    // 第一张 D row ready 后再购买，避免对同一输入无条件重复求值。
    if (rent_ < buy_ || evaluated_revision_ == ordinary_revision_)
        return false;

    const double old_best = problem_.best;
    Evaluate();
    rent_ = 0;
    evaluated_revision_ = ordinary_revision_;
    return problem_.best < old_best;
}

/** @brief 返回距离下一次可购买树 DP 还需累计的工作量。 */
long long WitnessUpperScheduler::RemainingRentUntilBuy() const
{
    if (evaluated_revision_ == ordinary_revision_)
        return std::numeric_limits<long long>::max();
    return rent_ >= buy_ ? 0 : buy_ - rent_;
}

/** @brief 调用两种模式共用的 witness-tree DP 收紧可行上界。 */
void WitnessUpperScheduler::Evaluate()
{
    problem_.best = std::min(problem_.best, EvaluateWitnessTree(problem_.witness_tree, problem_, problem_.ordinary));
}

/** @brief 读取一个 A1 值，不需要下标缓存的调用点使用本入口。 */
double AnchoredSingletonFuture::Value(const Problem& p, int bit, int vertex) const
{
    std::uint32_t locator = 0;
    return ValueWithLocator(p, bit, vertex, locator);
}

/** @brief 读取 A1 值，并返回精确 payload 下标或 cone 外标记。 */
double AnchoredSingletonFuture::ValueWithLocator(const Problem& p, int bit, int vertex, std::uint32_t& locator) const
{
    const Row& values = row[bit];
    const auto it = std::lower_bound(values.vertex.begin(), values.vertex.end(), vertex);
    if (it != values.vertex.end() && *it == vertex)
    {
        locator = static_cast<std::uint32_t>(it - values.vertex.begin());
        return values.value[locator];
    }
    locator = kA1FallbackLocator;
    const int continuation = p.nonanchor_original_mask ^ p.original_mask[bit];
    return std::max(0.0, cutoff - FarthestRemaining(p, vertex, continuation));
}

/** @brief 用先前缓存的 locator 直接读取 A1 值或重算 cone 外公式。 */
double AnchoredSingletonFuture::LocatedValue(const Problem& p, int bit, int vertex, std::uint32_t locator) const
{
    if (!(locator & kA1FallbackLocator))
        return row[bit].value[locator];
    const int continuation = p.nonanchor_original_mask ^ p.original_mask[bit];
    return std::max(0.0, cutoff - FarthestRemaining(p, vertex, continuation));
}

/** @brief 返回剩余 singleton 中最大的 A1 future，并缓存每个顶点的前两名。 */
double AnchoredSingletonFuture::Future(const Problem& p, int remaining, int vertex)
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
            const double value = ValueWithLocator(p, bit, vertex, locator);
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
            cached_locator_pair[vertex] = static_cast<std::uint64_t>(first_locator) | (static_cast<std::uint64_t>(second_locator) << 32);
        }
    }

    const int a = first[vertex];
    if (a != 255 && (remaining & (1 << a)))
        return LocatedValue(p, 1 << a, vertex, static_cast<std::uint32_t>(cached_locator_pair[vertex]));
    const int b = second[vertex];
    if (b != 255 && (remaining & (1 << b)))
        return LocatedValue(p, 1 << b, vertex, static_cast<std::uint32_t>(cached_locator_pair[vertex] >> 32));

    double value = 0.0;
    for (int bits = remaining; bits; bits &= bits - 1)
    {
        const int bit = bits & -bits;
        if (row[bit].ready)
            value = std::max(value, Value(p, bit, vertex));
    }
    return value;
}

/** @brief 在 ordinary D 之前构造 Base/Enhanced 完全共用的 A1 层。 */
void BuildReusableAnchoredSingletonLayer(Problem& p, AnchoredSingletonFuture& singleton_future, WitnessUpperScheduler& witness_scheduler)
{
    std::vector<double> distance(p.graph.n + 1, fp::kInf);
    std::vector<double> continuation_cache(p.graph.n + 1);
    std::vector<int> continuation_stamp(p.graph.n + 1);
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;

    // A1 的 cone/fallback 证明要求最终接纳的一整轮 singleton row 共享同一
    // 个 U0。购买可发生在 queue-pop 安全检查点或 row 末：若没有收紧
    // incumbent，本轮继续；若确实收紧，则丢弃全部部分 row，以新 U0 从
    // 第一张 row 重建。调度器会记住 ordinary 修订号，所以同一输入至多
    // 购买一次，重启后的整轮 A1 不会再次改变 U0。
    for (;;)
    {
        singleton_future.cutoff = p.best;
        singleton_future.row.assign(p.subset_count, {});
        size_t accepted_states = 0;
        bool restart = false;

        for (int mask = 1; mask < p.subset_count; mask <<= 1)
        {
            long long row_work = 0;
            const int group = p.bit_to_group[FirstBit(mask)];
            ++stamp;
            const int continuation = p.nonanchor_original_mask ^ p.original_mask[mask];
            // 按顶点惰性缓存当前 singleton row 尚未覆盖组的最远距离下界。
            auto Continuation = [&](int vertex)
            {
                if (continuation_stamp[vertex] != stamp)
                {
                    continuation_stamp[vertex] = stamp;
                    continuation_cache[vertex] = FarthestRemaining(p, vertex, continuation);
                }
                return continuation_cache[vertex];
            };
            touched.clear();
            settled.clear();
            std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> queue;
            long long rent_until_buy = witness_scheduler.RemainingRentUntilBuy();
            // 用组距离的精确 support 初始化当前 A1 row 的多源最短路。
            p.group_distance[group].ForEachExact(p.graph.n,
                                                 [&](int vertex, double value)
                                                 {
                                                     const auto& anchor = p.group_distance[p.anchor_group];
                                                     if (!anchor.IsExact(vertex))
                                                         return;
                                                     const double candidate = value + anchor[vertex];
                                                     if (!(candidate < singleton_future.cutoff) || candidate >= distance[vertex])
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
                ++row_work;

                // 不必等整张 A1 row 才发现 rent 已越过 buy。以 queue-pop 为
                // 安全检查点可限制超额工作；若树 DP 收紧上界，下面统一清空
                // 当前工作区并重启整轮。未收紧时 incumbent 与固定 U0 均不变。
                if (row_work >= rent_until_buy)
                {
                    const bool improved = witness_scheduler.Account(row_work, false);
                    row_work = 0;
                    if (improved)
                    {
                        restart = true;
                        break;
                    }
                    rent_until_buy = witness_scheduler.RemainingRentUntilBuy();
                }
                if (node.distance != distance[node.vertex] || !(node.distance < singleton_future.cutoff) || !(node.key < p.best))
                    continue;
                settled.push_back(node.vertex);
                for (const auto& edge : p.graph.adj[node.vertex])
                {
                    ++row_work;
                    const double next = node.distance + edge.w;
                    if (!(next < singleton_future.cutoff) || next >= distance[edge.to])
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

            if (restart)
            {
                for (int vertex : touched)
                    distance[vertex] = fp::kInf;
                break;
            }

            std::sort(settled.begin(), settled.end());
            settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
            Row& row = singleton_future.row[mask];
            row.vertex = settled;
            row.value.reserve(settled.size());
            for (int vertex : settled)
                row.value.push_back(distance[vertex]);
            row.ready = true;
            accepted_states += touched.size();

            // 先清空复用工作区，再允许购买触发整轮重启。A1 不写 ordinary
            // row，但与 D 使用相同调度器，并按实际 queue-pop/edge-relax 付 rent。
            for (int vertex : touched)
                distance[vertex] = fp::kInf;
            if (witness_scheduler.Account(row_work, false))
            {
                restart = true;
                break;
            }
        }

        if (restart)
            continue;

        // 只登记最终被算法接纳并移交给前向内核的逻辑状态；因上界收紧而
        // 丢弃的部分尝试是调度 rent，而不是额外的 `(mask,v)` 状态。
        p.AccountMaskVertexStates(accepted_states);
        break;
    }
    singleton_future.first.assign(p.graph.n + 1, 255);
    singleton_future.second.assign(p.graph.n + 1, 255);
    // 每个实际查询过 future 的顶点只写一次 packed locator；未访问顶点的
    // 虚拟页面不触发物理 RSS，也避免两个独立数组的两次随机首次写入。
    singleton_future.cached_locator_pair.reset(new std::uint64_t[static_cast<size_t>(p.graph.n) + 1]);
}

namespace
{
/**
 * @brief 枚举三个 ordinary 分块与永久锚组在共同根相遇的可行解。
 *
 * 从实际可枚举值最少的一侧驱动，缺失状态立即拒绝。该完成式在三分块
 * 大小达到平衡点时收紧上界，但不替代后续精确状态枚举。
 */
template <class Use> void ForEachTriple(const Problem& p, int first, int second, int third, Use&& use)
{
    const int masks[3] = {first, second, third};
    int driver = 0;
    size_t driver_size = static_cast<size_t>(p.graph.n);
    for (int mask : masks)
    {
        if (!mask)
            continue;
        const size_t size = p.popcount[mask] == 1 ? p.group_distance[p.bit_to_group[FirstBit(mask)]].ExactSize(p.graph.n) : p.ordinary[mask].vertex.size();
        if (size < driver_size)
        {
            driver = mask;
            driver_size = size;
        }
    }
    // 在候选根上读取三个 ordinary 分块并提交完整完成式。
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
        // 非空时由最短 row 驱动候选根，避免扫描全部顶点。
        ForEachOrdinaryValue(p, driver, [&](int vertex, double) { Visit(vertex); });
    else
        for (int vertex = 1; vertex <= p.graph.n; ++vertex)
            Visit(vertex);
}
} // namespace

/** @brief 按子集大小递增构造 ordinary D，并持续执行共同上界调度。 */
void BuildOrdinaryRows(Problem& p, AnchoredSingletonFuture* singleton_future, WitnessUpperScheduler& witness_scheduler)
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

    for (int size = 1; size <= p.half; ++size)
    {
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
            // 按顶点缓存当前 row 的完整 future，只在队列键需要时求值。
            auto Bound = [&](int vertex)
            {
                if (bound_stamp[vertex] != stamp)
                {
                    bound_stamp[vertex] = stamp;
                    bound_cache[vertex] = FutureBound(p, vertex, remaining_original);
                    if (singleton_future)
                        bound_cache[vertex] = std::max(bound_cache[vertex], singleton_future->Future(p, remaining_nonanchor, vertex));
                }
                return bound_cache[vertex];
            };

            // 分阶段计算便宜到昂贵的下界，尽早拒绝不能改善上界的状态。
            auto CanImprove = [&](int vertex, double value)
            {
                if (bound_stamp[vertex] == stamp)
                    return value + bound_cache[vertex] < p.best;

                double lower = 0.0;
                if (p.enhanced)
                {
                    lower = p.dual.At(vertex, remaining_original);
                    if (!(value + lower < p.best))
                        return false;
                }
                lower = std::max(lower, FarthestRemaining(p, vertex, remaining_original));
                if (!(value + lower < p.best))
                    return false;
                if (singleton_future)
                {
                    lower = std::max(lower, singleton_future->Future(p, remaining_nonanchor, vertex));
                    if (!(value + lower < p.best))
                        return false;
                }
                lower = std::max(lower, p.tour.At(vertex, remaining_original, p.group_distance));
                bound_stamp[vertex] = stamp;
                bound_cache[vertex] = lower;
                return value + lower < p.best;
            };
            // 接纳一个更优且仍有希望完成的 seed，并登记首次触及的顶点。
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
                // 两个 singleton 在同一根相交，形成 size=2 的全部 seed。
                ForEachCommonValue(p, first, second, [&](int vertex, double a, double b) { Set(vertex, a + b); });
            }
            else
            {
                const int domain = mask ^ (mask & -mask);
                for (int branch = domain; branch; branch = (branch - 1) & domain)
                {
                    const int accumulator = mask ^ branch;
                    if (!OrdinaryAvailable(p, accumulator) || !OrdinaryAvailable(p, branch))
                        continue;
                    // 合并互补的规范 branch，生成当前 mask 的候选 seed。
                    ForEachPivotBranch(p, accumulator, branch, [&](int vertex, double a, double b) { Set(vertex, a + b); });
                }
            }

            seeds = touched;
            for (int vertex : seeds)
                split[vertex] = distance[vertex];
            std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> queue;
            for (int vertex : touched)
                queue.push({distance[vertex] + Bound(vertex), distance[vertex], vertex});
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
                    row.branch_bits[index >> 6] |= std::uint64_t{1} << (index & 63);
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

            witness_scheduler.Account(row_work, true);

            if (size == p.half)
            {
                const int complement = p.full_mask ^ mask;
                if (OrdinaryAvailable(p, complement) && (p.popcount[complement] < size || (p.popcount[complement] == size && complement < mask)))
                    // 两个半层 row 在同根相交，并补上永久锚组完成解。
                    ForEachCommonValue(p, mask, complement,
                                       [&](int vertex, double a, double b)
                                       {
                                           if (p.group_distance[p.anchor_group].IsExact(vertex))
                                               p.best = std::min(p.best, a + b + p.group_distance[p.anchor_group][vertex]);
                                       });
            }

            if (size == three_block_limit)
            {
                const int remaining = p.full_mask ^ mask;
                for (int second = remaining;; second = (second - 1) & remaining)
                {
                    const int third = remaining ^ second;
                    if (second <= third && p.popcount[second] <= size && p.popcount[third] <= size && (!second || OrdinaryAvailable(p, second)) &&
                        (!third || OrdinaryAvailable(p, third)))
                        // 三个平衡分块在同根相交，用完成式及时收紧上界。
                        ForEachTriple(p, mask, second, third, [&](int, double value) { p.best = std::min(p.best, value); });
                    if (!second)
                        break;
                }
            }

            for (int vertex : seeds)
                split[vertex] = fp::kInf;
            for (int vertex : touched)
                distance[vertex] = fp::kInf;
        }
    }
}

} // namespace gst::methods::abhss::internal
