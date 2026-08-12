#include "core.h"

#include <numeric>
#include "diagnostics.h"

namespace gst::methods::abhss::internal
{
namespace
{
/** 高位表示该 top-two 值来自 cone 外公式，而非 row.value 下标。 */
constexpr std::uint32_t kA1FallbackLocator = std::uint32_t{1} << 31;


/**
 * @brief 返回 A1 cone 与正 fallback 共同使用的剩余代价下界。
 *
 * farthest 与 endpoint-floor 都可采纳且沿边至多下降边权；取最大后仍保持
 * 该性质，因此既能安全决定 Dijkstra cone，也能用于 cone 外的 U-B fallback。
 */
double AnchoredSingletonContinuation(const Problem& p, int vertex, int continuation)
{
    return std::max(FarthestRemaining(p, vertex, continuation), p.tour.EndpointFloorAt(vertex, continuation, p.group_distance));
}

/** @brief 任一安全证书严格加强后删除已缓存 row 中不再可能改善 incumbent 的状态。 */
long long RefilterOrdinaryAfterCertificateUpgrade(Problem& p)
{
    long long removed = 0;
    for (int mask = 1; mask < p.subset_count; ++mask)
    {
        Row& row = p.ordinary[mask];
        if (!row.ready || row.vertex.empty())
            continue;
        const int remaining = p.original_full_mask ^ p.original_mask[mask];
        std::vector<std::uint64_t> branch_bits((row.vertex.size() + 63) / 64);
        size_t write = 0;
        double minimum = fp::kInf;
        int branch_count = 0;
        for (size_t read = 0; read < row.vertex.size(); ++read)
        {
            const int vertex = row.vertex[read];
            const double value = row.value[read];
            if (!(value + FutureBound(p, vertex, remaining) < p.best))
            {
                ++removed;
                continue;
            }
            row.vertex[write] = vertex;
            row.value[write] = value;
            if ((row.branch_bits[read >> 6] >> (read & 63)) & 1ULL)
            {
                branch_bits[write >> 6] |= std::uint64_t{1} << (write & 63);
                ++branch_count;
            }
            minimum = std::min(minimum, value);
            ++write;
        }
        row.vertex.resize(write);
        row.value.resize(write);
        branch_bits.resize((write + 63) / 64);
        row.branch_bits.swap(branch_bits);
        row.branch_count = branch_count;
        p.ordinary_minimum[mask] = minimum;
    }
    return removed;
}
}

long long EstimateWitnessTreeDpWork(size_t witness_vertices,
                                    int nonanchor_count)
{
    if (!witness_vertices || nonanchor_count < 0)
        return 0;

    // 对 k 个非锚组，固定最低 bit 消除左右对称后的非空局部拆分总数为
    // (3^k-1)/2；一条父子关系的全 mask/submask 卷积总数为 3^k。
    // WitnessTree 对每个真实顶点恰有一次局部处理和一条通向父亲（含虚拟
    // 超根）的关系，所以两项都乘真实 witness 顶点数。仓库限制 g<=16，
    // 因而 3^(g-1) 及其与输入顶点数的乘积处于 signed 64-bit 范围内。
    long long ternary_subsets = 1;
    for (int bit = 0; bit < nonanchor_count; ++bit)
        ternary_subsets *= 3;
    const long long work_per_vertex =
        (ternary_subsets - 1) / 2 + ternary_subsets;
    return static_cast<long long>(witness_vertices) * work_per_vertex;
}

WitnessUpperScheduler::WitnessUpperScheduler(Problem& problem)
    : problem_(problem)
{
    buy_ = problem_.certificate_support_edges.empty()
               ? EstimateWitnessTreeDpWork(problem_.witness_tree.vertex.size(), problem_.nonanchor_count)
               : EstimateCertificateSupportDpWork(problem_.certificate_support_vertex_count, problem_.nonanchor_count);
    enabled_ = buy_ > 0;
    // 诊断构建把共同零起点与 buy 写入事件；正式构建会在编译期消除。
    EmitAbhssProbe(
        ProbeFamilyMethod(problem_),
        "witness_rent_start",
        problem_,
        -1.0,
        nullptr,
        -1,
        buy_);
}

bool WitnessUpperScheduler::Account(long long row_work,
                                    bool ordinary_changed)
{
    if (!enabled_)
        return false;
    if (ordinary_changed)
        ++ordinary_revision_;
    rent_ += row_work;

    // 第一次购买可只使用隐式 singleton；以后只有 ordinary 修订号变化才有
    // 新信息可供树 DP 消费。若 A1 的后续 row 又积满 buy，则保留 rent，等
    // 第一张 D row ready 后再购买，避免对同一输入无条件重复求值。
    if (rent_ < buy_ || evaluated_revision_ == ordinary_revision_)
        return false;

    const long long paid_rent = rent_;
    const double old_best = problem_.best;
    ProbeTimer timer;
    Evaluate();
    if (problem_.best < old_best && ordinary_revision_ > 0)
    {
        const long long removed = RefilterOrdinaryAfterCertificateUpgrade(problem_);
        EmitAbhssProbe(ProbeFamilyMethod(problem_), "witness_refilter", problem_, -1.0, &problem_.ordinary, evaluation_count_ + 1, removed);
    }
    purchased_rent_ = paid_rent >= std::numeric_limits<long long>::max() - purchased_rent_
                           ? std::numeric_limits<long long>::max()
                           : purchased_rent_ + paid_rent;
    rent_ = 0;
    evaluated_revision_ = ordinary_revision_;
    ++evaluation_count_;
    EmitAbhssProbe(
        ProbeFamilyMethod(problem_),
        "witness_buy",
        problem_,
        timer.Seconds(),
        &problem_.ordinary,
        evaluation_count_,
        paid_rent);
    return problem_.best < old_best;
}

long long WitnessUpperScheduler::TotalWork() const
{
    if (!problem_.UsesDirectedCut())
        return 0;
    return rent_ >= std::numeric_limits<long long>::max() - purchased_rent_
               ? std::numeric_limits<long long>::max()
               : purchased_rent_ + rent_;
}

long long WitnessUpperScheduler::RemainingRentUntilBuy() const
{
    if (!enabled_ || evaluated_revision_ == ordinary_revision_)
        return std::numeric_limits<long long>::max();
    return rent_ >= buy_ ? 0 : buy_ - rent_;
}

void WitnessUpperScheduler::Evaluate()
{
    if (!problem_.certificate_support_edges.empty())
    {
        problem_.best = std::min(problem_.best, EvaluateCertificateSupport(problem_));
        return;
    }
    problem_.best = std::min(
        problem_.best,
        EvaluateWitnessTree(
            problem_.witness_tree, problem_, problem_.ordinary));
}

void WitnessUpperScheduler::RefreshCertificate()
{
    buy_ = problem_.certificate_support_edges.empty()
               ? EstimateWitnessTreeDpWork(problem_.witness_tree.vertex.size(), problem_.nonanchor_count)
               : EstimateCertificateSupportDpWork(problem_.certificate_support_vertex_count, problem_.nonanchor_count);
    rent_ = 0;
    evaluated_revision_ = ordinary_revision_;
    enabled_ = buy_ > 0;
    EmitAbhssProbe(ProbeFamilyMethod(problem_), "certificate_refresh", problem_, -1.0, &problem_.ordinary, evaluation_count_, buy_);
}

ResidualClosureScheduler::ResidualClosureScheduler(Problem& problem)
    : problem_(problem)
{
    if (problem_.UsesDirectedCut())
        buy_ = problem_.dual.ResidualClosureBuyWork(problem_.graph);
    enabled_ = buy_ > 0;
    EmitAbhssProbe(ProbeFamilyMethod(problem_), "dual_closure_rent_start", problem_, -1.0, nullptr, -1, buy_);
}

bool ResidualClosureScheduler::Account(long long row_work, long long new_payload)
{
    if (!enabled_ || purchased_)
        return false;
    if (new_payload >= std::numeric_limits<long long>::max() - payload_)
        payload_ = std::numeric_limits<long long>::max();
    else
        payload_ += new_payload;
    if (row_work >= std::numeric_limits<long long>::max() - rent_)
        rent_ = std::numeric_limits<long long>::max();
    else
        rent_ += row_work;
    if (rent_ < buy_)
        return false;
    if (payload_ < problem_.graph.n)
        return false;
    return Buy();
}

bool ResidualClosureScheduler::Buy()
{
    const long long paid_rent = rent_;
    ProbeTimer timer;
    std::vector<std::vector<double>> dense(problem_.g);
    for (int group = 0; group < problem_.g; ++group)
        dense[group].swap(problem_.group_distance[group].value);
    problem_.dual.CompleteResidualClosureKeepingResidualAndPrimalEdges(problem_.graph, problem_.query, dense, problem_.root);
    for (int group = 0; group < problem_.g; ++group)
        dense[group].swap(problem_.group_distance[group].value);

    const double primal_upper = problem_.dual.PrimalUpper();
    problem_.best = std::min(problem_.best, primal_upper);
    EmitAbhssProbe(ProbeFamilyMethod(problem_), "dual_closure_primal", problem_, timer.Seconds(), &problem_.ordinary, -1, paid_rent);
    ProbeTimer facility_timer;
    const double facility_upper = BuildPrimalFacilityUpper(problem_, problem_.dual.Residual(), problem_.dual.PrimalEdgeWords());
    problem_.best = std::min(problem_.best, facility_upper);
    EmitAbhssProbe(ProbeFamilyMethod(problem_), "dual_closure_facility", problem_, facility_timer.Seconds(), &problem_.ordinary, -1, paid_rent);
    problem_.dual.ReleaseResidual();
    const bool support_replaced = RefreshPurchasedPathGrowthCertificate(problem_);
    const long long removed = RefilterOrdinaryAfterCertificateUpgrade(problem_);
    EmitAbhssProbe(ProbeFamilyMethod(problem_), "dual_closure_refilter", problem_, -1.0, &problem_.ordinary, -1, removed);
    purchased_ = true;
    enabled_ = false;
    EmitAbhssProbe(ProbeFamilyMethod(problem_), "dual_closure_buy", problem_, timer.Seconds(), &problem_.ordinary, -1, paid_rent);
    return support_replaced;
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
    const int continuation =
        p.nonanchor_original_mask ^ p.original_mask[bit];
    return std::max(
        0.0,
        cutoff - AnchoredSingletonContinuation(p, vertex, continuation));
}

double AnchoredSingletonFuture::LocatedValue(
    const Problem& p,
    int bit,
    int vertex,
    std::uint32_t locator) const
{
    if (!(locator & kA1FallbackLocator))
        return row[bit].value[locator];
    const int continuation =
        p.nonanchor_original_mask ^ p.original_mask[bit];
    return std::max(
        0.0,
        cutoff - AnchoredSingletonContinuation(p, vertex, continuation));
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
    AnchoredSingletonFuture& singleton_future,
    WitnessUpperScheduler& witness_scheduler)
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
            const int continuation =
                p.nonanchor_original_mask ^ p.original_mask[mask];
            // lambda：按 row epoch 缓存公共 A1 对剩余组的 continuation 下界。
            auto Continuation = [&](int vertex)
            {
                if (continuation_stamp[vertex] != stamp)
                {
                    continuation_stamp[vertex] = stamp;
                    continuation_cache[vertex] =
                        AnchoredSingletonContinuation(p, vertex, continuation);
                }
                return continuation_cache[vertex];
            };
            touched.clear();
            settled.clear();
            std::priority_queue<QueueNode,
                                std::vector<QueueNode>,
                                std::greater<QueueNode>> queue;
            long long rent_until_buy =
                witness_scheduler.RemainingRentUntilBuy();
            // lambda：以目标组精确距离与锚组距离初始化当前公共 A1 row。
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
                ++row_work;

                // 不必等整张 A1 row 才发现 rent 已越过 buy。以 queue-pop 为
                // 安全检查点可限制超额工作；若树 DP 收紧上界，下面统一清空
                // 当前工作区并重启整轮。未收紧时 incumbent 与固定 U0 均不变。
                if (row_work >= rent_until_buy)
                {
                    const bool improved =
                        witness_scheduler.Account(row_work, false);
                    row_work = 0;
                    if (improved)
                    {
                        restart = true;
                        break;
                    }
                    rent_until_buy =
                        witness_scheduler.RemainingRentUntilBuy();
                }
                if (node.distance != distance[node.vertex] ||
                    !(node.distance < singleton_future.cutoff) ||
                    !(node.key < p.best))
                    continue;
                settled.push_back(node.vertex);
                for (const auto& edge : p.graph.adj[node.vertex])
                {
                    ++row_work;
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

            if (restart)
            {
                for (int vertex : touched)
                    distance[vertex] = fp::kInf;
                break;
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
    singleton_future.cached_locator_pair.reset(
        new std::uint64_t[static_cast<size_t>(p.graph.n) + 1]);
}

namespace
{
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
    // lambda：在同一顶点累加锚组和至多三个 ordinary 块并提交完整候选。
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
    {
        // lambda：由最小 ordinary 候选集驱动三块同根完整候选检查。
        ForEachOrdinaryValue(p, driver, [&](int vertex, double) { Visit(vertex); });
    }
    else
        for (int vertex = 1; vertex <= p.graph.n; ++vertex)
            Visit(vertex);
}

}  // namespace

void BuildOrdinaryRows(Problem& p,
                       AnchoredSingletonFuture* singleton_future,
                       WitnessUpperScheduler& witness_scheduler,
                       ResidualClosureScheduler& closure_scheduler,
                       int last_layer)
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

    for (int size = 1; size <= last_layer; ++size)
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
            // lambda：按需缓存 ordinary row 的公共 future 与可选 A1 future 最大值。
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

            // lambda：按由廉到贵的顺序求可采纳下界，失败即停止后续下界计算。
            auto CanImprove = [&](int vertex, double value)
            {
                if (bound_stamp[vertex] == stamp)
                    return value + bound_cache[vertex] < p.best;

                double lower = 0.0;
                if (p.UsesDirectedCut())
                {
                    if (!p.dual.CanImproveAllExcept(vertex, p.original_mask[mask], value, p.best, lower))
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
            // lambda：仅以更小距离更新仍可能严格改善 incumbent 的 ordinary 标签。
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
                // lambda：把两个 singleton 的同根和登记为 size-2 ordinary seed。
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
                    // lambda：把 accumulator 与规范 branch 的同根和登记为 ordinary seed。
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
            std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> queue;
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
            witness_scheduler.Account(row_work, true);
            if (closure_scheduler.Account(row_work, static_cast<long long>(row.vertex.size())))
                witness_scheduler.RefreshCertificate();
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_row", p, -1.0, nullptr, size, row_work);

            if (size == p.half)
            {
                const int complement = p.full_mask ^ mask;
                if (OrdinaryAvailable(p, complement) &&
                    (p.popcount[complement] < size ||
                     (p.popcount[complement] == size && complement < mask)))
                {
                    // lambda：在平衡半格把互补 ordinary 状态与锚组距离结算为完整上界。
                    ForEachCommonValue(p, mask, complement, [&](int vertex, double a, double b)
                    {
                        if (p.group_distance[p.anchor_group].IsExact(vertex))
                            p.best = std::min(
                                p.best,
                                a + b + p.group_distance[p.anchor_group][vertex]);
                    });
                }
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
                    {
                        // lambda：提交三个 ordinary 分块与锚组同根形成的完整候选。
                        ForEachTriple(p, mask, second, third, [&](int, double value)
                        {
                            p.best = std::min(p.best, value);
                        });
                    }
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
