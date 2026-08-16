#include "core.h"

#include <array>
#include <cmath>
#include <numeric>
#include "diagnostics.h"

// 两个物化函数在一条查询中都至多执行一次，属于 rent-or-buy 的冷购买路径。
// 显式阻止 IPO 把它们并入逐状态调用的 Future，避免未购买查询也扩大热循环；
// 这只约束机器码布局，不改变购买条件、执行次数或任何数学值。
#if defined(_MSC_VER)
#define ABHSS_A1_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define ABHSS_A1_NOINLINE __attribute__((noinline))
#else
#define ABHSS_A1_NOINLINE
#endif

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
        const int remaining_original = p.original_full_mask ^ p.original_mask[mask];
        const size_t old_size = row.vertex.size();
        const double old_minimum = p.ordinary_minimum[mask];
        size_t write = 0;
        int removed_branches = 0;
        bool removed_minimum = false;
        for (size_t read = 0; read < old_size; ++read)
        {
            const int vertex = row.vertex[read];
            const double value = row.value[read];
            if (!(value + FutureBound(p, vertex, remaining_original) < p.best))
            {
                ++removed;
                if ((row.branch_bits[read >> 6] >> (read & 63)) & 1ULL)
                    ++removed_branches;
                if (value == old_minimum)
                    removed_minimum = true;
                continue;
            }
            if (write != read)
            {
                row.vertex[write] = vertex;
                row.value[write] = value;
                const bool branch = (row.branch_bits[read >> 6] >> (read & 63)) & 1ULL;
                const std::uint64_t destination_bit = std::uint64_t{1} << (write & 63);
                if (branch)
                    row.branch_bits[write >> 6] |= destination_bit;
                else
                    row.branch_bits[write >> 6] &= ~destination_bit;
            }
            ++write;
        }
        if (write == old_size)
            continue;
        row.vertex.resize(write);
        row.value.resize(write);
        row.branch_bits.resize((write + 63) / 64);
        if (!row.branch_bits.empty() && (write & 63))
            row.branch_bits.back() &= (std::uint64_t{1} << (write & 63)) - 1;
        row.branch_count -= removed_branches;
        if (removed_minimum)
        {
            double minimum = fp::kInf;
            for (double row_value : row.value)
                minimum = std::min(minimum, row_value);
            p.ordinary_minimum[mask] = minimum;
        }
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

bool WitnessUpperScheduler::Account(long long row_work, bool ordinary_changed)
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
        ProbeTimer refilter_timer;
        const long long removed = RefilterOrdinaryAfterCertificateUpgrade(problem_);
        EmitAbhssProbe(ProbeFamilyMethod(problem_), "witness_refilter", problem_, refilter_timer.Seconds(), &problem_.ordinary, evaluation_count_ + 1, removed);
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
    ProbeTimer refilter_timer;
    const long long removed = RefilterOrdinaryAfterCertificateUpgrade(problem_);
    EmitAbhssProbe(ProbeFamilyMethod(problem_), "dual_closure_refilter", problem_, refilter_timer.Seconds(), &problem_.ordinary, -1, removed);
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
    return FallbackValue(p, bit, vertex);
}

double AnchoredSingletonFuture::FallbackValue(const Problem& p, int bit, int vertex) const
{
    const int continuation = p.nonanchor_original_mask ^ p.original_mask[bit];
    return std::max(0.0, cutoff - AnchoredSingletonContinuation(p, vertex, continuation));
}

double AnchoredSingletonFuture::LocatedValue(
    const Problem& p,
    int bit,
    int vertex,
    std::uint32_t locator) const
{
    if (!(locator & kA1FallbackLocator))
        return row[bit].value[locator];
    return FallbackValue(p, bit, vertex);
}

void AnchoredSingletonFuture::InitializeLookupPlan(const Problem& p)
{
    lookup_buy_work = 0;
    lookup_touch_remaining = 0;
    ranked_buy_work = 0;
    ranked_rent_work = 0;
    ranked_tail.clear();
    ranked_rent_by_mask.clear();
    lookup_materialized = false;
    const long long vertices = p.graph.n;
    long long rent_per_vertex = 0;
    // 第一层 buy 估计逐 row 顺扫全图与缺项 fallback；rent 是首次查询
    // 一个顶点时逐 row 二分的保守比较数。两级购买都不读取图名、g 阈值、
    // 计时或历史查询结果，只读取已发布 row 的静态形状与实际已付工作。
    for (int bit = 1; bit < p.subset_count; bit <<= 1)
    {
        const long long missing = vertices - static_cast<long long>(row[bit].vertex.size());
        lookup_buy_work += 2 * vertices + missing * (p.nonanchor_count + 2LL);
        rent_per_vertex += BinarySearchCost(row[bit].vertex.size()) + 1;
    }
    if (rent_per_vertex)
        lookup_touch_remaining = lookup_buy_work / rent_per_vertex + (lookup_buy_work % rent_per_vertex != 0);
    const long long tail_size = std::max(0, p.nonanchor_count - 2);
    // 第二层重新支付一次顺扫/fallback 的结构成本，并覆盖每顶点稳定插入
    // 排序的最坏比较数与全部 byte 写入；rent 只在第一层购买后累计。
    const long long ranking_work = tail_size * (tail_size - 1) / 2;
    const long long tail_write_work = tail_size;
    ranked_buy_work = lookup_buy_work + vertices * (ranking_work + tail_write_work);
}

ABHSS_A1_NOINLINE void AnchoredSingletonFuture::MaterializeAllTopTwo(const Problem& p)
{
    ProbeTimer timer;
    // 每张递增稀疏 row 只维持一个游标；顶点与 bit 均按 lazy 路径的稳定
    // 顺序扫描，因而并列值选择、精确 locator 与 fallback 标志逐项相同。
    std::vector<size_t> cursor(p.nonanchor_count);
    for (int vertex = 1; vertex <= p.graph.n; ++vertex)
    {
        double first_value = -1.0;
        double second_value = -1.0;
        int first_bit = -1;
        int second_bit = -1;
        std::uint32_t first_locator = 0;
        std::uint32_t second_locator = 0;
        int row_index = 0;
        for (int bit = 1; bit < p.subset_count; bit <<= 1)
        {
            const Row& values = row[bit];
            size_t& position = cursor[row_index++];
            std::uint32_t locator = kA1FallbackLocator;
            double value = 0.0;
            if (position < values.vertex.size() && values.vertex[position] == vertex)
            {
                locator = static_cast<std::uint32_t>(position);
                value = values.value[position++];
            }
            else
                value = FallbackValue(p, bit, vertex);
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
            first[vertex] = static_cast<unsigned char>(first_bit);
        if (second_bit >= 0)
            second[vertex] = static_cast<unsigned char>(second_bit);
        cached_locator_pair[vertex] = static_cast<std::uint64_t>(first_locator) | (static_cast<std::uint64_t>(second_locator) << 32);
    }
    // tail 查询对同一 remaining mask 反复累加完全相同的 row 形状成本。
    // top-two 购买时用一次子集递推精确因子化，后续查询只读一个整数。
    // g<=16 且 row 长度不超过 INT_MAX，单表项至多 15*(31+1)=480，int 无溢出。
    ranked_rent_by_mask.assign(p.subset_count, 0);
    for (int mask = 1; mask < p.subset_count; ++mask)
    {
        const int bit = mask & -mask;
        ranked_rent_by_mask[mask] = ranked_rent_by_mask[mask ^ bit];
        ranked_rent_by_mask[mask] += static_cast<int>(BinarySearchCost(row[bit].vertex.size()) + 1);
    }
    lookup_materialized = true;
    EmitAbhssProbe(ProbeFamilyMethod(p), "a1_top_two_materialize", p, timer.Seconds(), &row, -1, lookup_buy_work);
}

ABHSS_A1_NOINLINE void AnchoredSingletonFuture::MaterializeRankedTail(const Problem& p)
{
    ProbeTimer timer;
    // 第二级购买只在 top-two 已完整物化后发生；每个顶点恰有 k-2 个 tail bit。
    const int tail_size = p.nonanchor_count - 2;
    ranked_tail.resize((static_cast<size_t>(p.graph.n) + 1) * tail_size);
    // 与 top-two 顺扫相同，每张稀疏 row 保留一个递增游标，避免重新二分。
    std::vector<size_t> cursor(p.nonanchor_count);
    for (int vertex = 1; vertex <= p.graph.n; ++vertex)
    {
        const int first_bit = first[vertex];
        const int second_bit = second[vertex];
        // 只在当前顶点的栈数组中比较原 double；持久缓存最终只写 bit 次序。
        std::array<double, 16> ordered_value;
        std::array<unsigned char, 16> ordered_bit;
        int count = 0;
        int row_index = 0;
        for (int bit = 1; bit < p.subset_count; bit <<= 1)
        {
            const Row& values = row[bit];
            size_t& position = cursor[row_index++];
            const bool present = position < values.vertex.size() && values.vertex[position] == vertex;
            const int bit_index = FirstBit(bit);
            if (bit_index == first_bit || bit_index == second_bit)
            {
                position += present;
                continue;
            }
            const double value = present ? values.value[position++] : FallbackValue(p, bit, vertex);
            int insertion = count;
            while (insertion > 0 && value > ordered_value[insertion - 1])
            {
                ordered_value[insertion] = ordered_value[insertion - 1];
                ordered_bit[insertion] = ordered_bit[insertion - 1];
                --insertion;
            }
            ordered_value[insertion] = value;
            ordered_bit[insertion] = static_cast<unsigned char>(bit_index);
            ++count;
        }
        const size_t offset = static_cast<size_t>(vertex) * tail_size;
        // 稳定插入排序与 lazy 路径使用相同 bit 先后，并列值不改变返回值。
        for (int rank = 0; rank < count; ++rank)
            ranked_tail[offset + rank] = ordered_bit[rank];
    }
    EmitAbhssProbe(ProbeFamilyMethod(p), "a1_ranked_tail_materialize", p, timer.Seconds(), &row, -1, ranked_buy_work);
}

#undef ABHSS_A1_NOINLINE

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
            if (!lookup_materialized && --lookup_touch_remaining == 0)
                MaterializeAllTopTwo(p);
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

    if (!ranked_tail.empty())
    {
        const int tail_size = p.nonanchor_count - 2;
        const size_t offset = static_cast<size_t>(vertex) * tail_size;
        for (int rank = 0; rank < tail_size; ++rank)
        {
            const int bit_index = ranked_tail[offset + rank];
            if (remaining & (1 << bit_index))
                return Value(p, 1 << bit_index, vertex);
        }
        return 0.0;
    }

    // 完整排名是 top-two 之后的第二级表示；前一级尚未购买时累计
    // 后一级 rent 不可能触发购买，只会让停留在 lazy 阶段的查询付费。
    if (!lookup_materialized)
    {
        double value = 0.0;
        for (int bits = remaining; bits; bits &= bits - 1)
        {
            const int bit = bits & -bits;
            value = std::max(value, Value(p, bit, vertex));
        }
        return value;
    }

    double value = 0.0;
    for (int bits = remaining; bits; bits &= bits - 1)
    {
        const int bit = bits & -bits;
        value = std::max(value, Value(p, bit, vertex));
    }
    ranked_rent_work += ranked_rent_by_mask[remaining];
    // A1 域必有 n>=1、k>=3，构造式使 ranked_buy_work 严格为正。
    if (ranked_rent_work >= ranked_buy_work)
        MaterializeRankedTail(p);
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
                const double anchor_distance = anchor.ExactValueOrInf(vertex);
                if (anchor_distance >= fp::kInf)
                    return;
                const double candidate = value + anchor_distance;
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
                if (node.distance != distance[node.vertex] || !(node.distance < singleton_future.cutoff) || !(node.key < p.best))
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
    // 只有所有 singleton mask 都发布后才会退出上述重启循环；即使某张 row
    // 的 payload 为空，它也已标记 ready。以下只读视图因此直接遍历完整 bit 域，
    // 不在逐 future 查询的热路径重复检查这个已经由构建屏障保证的不变量。
    singleton_future.first.assign(p.graph.n + 1, 255);
    singleton_future.second.assign(p.graph.n + 1, 255);
    // lazy 阶段只写实际查询过 future 的 packed locator 页面；若结构性购买
    // 发生，顺序物化复用同一数组并触及全部页面，不另分配 dense double 表。
    singleton_future.cached_locator_pair.reset(
        new std::uint64_t[static_cast<size_t>(p.graph.n) + 1]);
    singleton_future.InitializeLookupPlan(p);
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
        double total = p.group_distance[p.anchor_group].ExactValueOrInf(vertex);
        if (total >= fp::kInf)
            return;
        for (int mask : masks)
        {
            if (!mask)
                continue;
            if (p.popcount[mask] == 1)
            {
                const int group = p.bit_to_group[FirstBit(mask)];
                const double singleton = p.group_distance[group].ExactValueOrInf(vertex);
                if (singleton >= fp::kInf)
                    return;
                total += singleton;
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
    // Base 继续使用单一 stamp；开启 DirectedCut 的配置将 row epoch、证书阶段和两个拒绝前沿位
    // 打包在一个 32-bit 字中。诊断构建复用保留位记录 exact dual，论文构建不会写该位。
    const bool staged_certificate_cache = p.UsesDirectedCut();
    std::vector<int> bound_stamp(staged_certificate_cache ? 0 : p.graph.n + 1);
    std::vector<std::uint32_t> bound_state(staged_certificate_cache ? p.graph.n + 1 : 0);
    constexpr std::uint32_t kBoundStageMask = 7;
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
    constexpr std::uint32_t kBoundExactDual = 8;
#endif
    constexpr std::uint32_t kBoundRejectedSeen = 16;
    constexpr std::uint32_t kBoundRejectedFrontier = 32;
    constexpr std::uint32_t kBoundMetadataMask = 63;
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;
    std::vector<int> rejected;
    const int three_block_limit = (p.nonanchor_count + 2) / 3;

    for (int size = 1; size <= last_layer; ++size)
    {
        long long layer_work = 0;
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
        long long layer_certificate_calls = 0;
        long long layer_certificate_cache_rejections = 0;
        long long layer_dual_evaluations = 0;
        long long layer_dual_rejections = 0;
        long long layer_dual_exact = 0;
        long long layer_farthest_evaluations = 0;
        long long layer_farthest_rejections = 0;
        long long layer_a1_evaluations = 0;
        long long layer_a1_rejections = 0;
        long long layer_tour_evaluations = 0;
        long long layer_tour_full_evaluations = 0;
        long long layer_tour_envelope_skips = 0;
        long long layer_tour_rejections = 0;
        long long layer_certificate_passes = 0;
#endif
        for (int mask = 1; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] != size || size == 1)
                continue;
            long long row_work = 0;
            touched.clear();
            settled.clear();
            rejected.clear();
            const int remaining_original = p.original_full_mask ^ p.original_mask[mask];
            const int remaining_nonanchor = p.full_mask ^ mask;
            bool rejection_frontier_admitted = false;
            ++stamp;
            const std::uint32_t row_epoch = static_cast<std::uint32_t>(stamp) << 6;
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
            long long certificate_cache_rejections = 0;
            long long exact_dual_cache_rejections = 0;
#endif
            // lambda：Base 一次缓存完整公共 future；DirectedCut 配置逐级缓存
            // 已算出的可采纳下界。每个候选仍用自己的 value 检查缓存证书，
            // 只有不足以拒绝时才继续计算下一阶段。
            auto CanImprove = [&](int vertex, double value)
            {
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                ++layer_certificate_calls;
#endif
                if (!staged_certificate_cache)
                {
                    if (bound_stamp[vertex] != stamp)
                    {
                        const double farthest = FarthestRemaining(p, vertex, remaining_original);
                        double lower = farthest;
                        if (!(value + lower < p.best))
                            return false;
                        if (singleton_future)
                        {
                            lower = std::max(lower, singleton_future->Future(p, remaining_nonanchor, vertex));
                            if (!(value + lower < p.best))
                                return false;
                        }
                        bound_stamp[vertex] = stamp;
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                        if (lower < p.tour.UpperEnvelope(remaining_original, farthest))
                            ++layer_tour_full_evaluations;
                        else
                            ++layer_tour_envelope_skips;
#endif
                        if (lower < p.tour.UpperEnvelope(remaining_original, farthest))
                            lower = std::max(lower, p.tour.At(vertex, remaining_original, p.group_distance));
                        bound_cache[vertex] = lower;
                    }
                    return value + bound_cache[vertex] < p.best;
                }

                std::uint32_t state = bound_state[vertex];
                if ((state & ~kBoundMetadataMask) != row_epoch)
                {
                    state = row_epoch;
                    bound_state[vertex] = state;
                    bound_cache[vertex] = 0.0;
                }
                else if (!(value + bound_cache[vertex] < p.best))
                {
                    // row 先用重复缓存拒绝确认前沿值得物化。stage 0 的
                    // directed-cut interval 尚未完成 exact/upper 判定，因此把
                    // U-L(v) 修正为按原 double 谓词确实拒绝的首个可表示值；
                    // 已完成 dual 的阶段仍只保存本次实际拒绝值。
                    if (state & kBoundRejectedFrontier)
                        distance[vertex] = value;
                    else if (distance[vertex] >= fp::kInf)
                    {
                        if (rejection_frontier_admitted || (state & kBoundRejectedSeen))
                        {
                            rejected.push_back(vertex);
                            if ((state & kBoundStageMask) == 0)
                            {
                                double rejection_cutoff = std::max(0.0, p.best - bound_cache[vertex]);
                                while (rejection_cutoff + bound_cache[vertex] < p.best)
                                    rejection_cutoff = std::nextafter(rejection_cutoff, fp::kInf);
                                distance[vertex] = rejection_cutoff;
                            }
                            else
                                distance[vertex] = value;
                            bound_state[vertex] = state | kBoundRejectedFrontier;
                            rejection_frontier_admitted = true;
                        }
                        else
                            bound_state[vertex] = state | kBoundRejectedSeen;
                    }
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                    ++certificate_cache_rejections;
                    ++layer_certificate_cache_rejections;
                    if (state & kBoundExactDual)
                        ++exact_dual_cache_rejections;
#endif
                    return false;
                }

                double lower = bound_cache[vertex];
                double farthest = -1.0;
                if ((state & kBoundStageMask) == 0)
                {
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                    ++layer_dual_evaluations;
#endif
                    bool exact_dual = false;
                    const bool dual_can_improve = p.dual.CanImproveAllExcept(vertex, p.original_mask[mask], value, p.best, lower, exact_dual);
                    bound_cache[vertex] = lower;
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                    if (exact_dual)
                    {
                        ++layer_dual_exact;
                        state |= kBoundExactDual;
                    }
#endif
                    if (!dual_can_improve)
                    {
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                        ++layer_dual_rejections;
#endif
                        // interval 下端只能拒绝当前标签；只有 exact fallback 已算出时，
                        // 后续更小标签才能复用同一 dual，严格保持旧状态机。
                        if (exact_dual)
                        {
                            state = (state & ~kBoundStageMask) | 1;
                            bound_state[vertex] = state;
                        }
                        return false;
                    }
                    state = (state & ~kBoundStageMask) | 1;
                    bound_state[vertex] = state;
                }
                if ((state & kBoundStageMask) == 1)
                {
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                    ++layer_farthest_evaluations;
#endif
                    farthest = FarthestRemaining(p, vertex, remaining_original);
                    lower = std::max(lower, farthest);
                    bound_cache[vertex] = lower;
                    state = (state & ~kBoundStageMask) | 2;
                    bound_state[vertex] = state;
                    if (!(value + lower < p.best))
                    {
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                        ++layer_farthest_rejections;
#endif
                        return false;
                    }
                }
                if ((state & kBoundStageMask) == 2)
                {
                    if (singleton_future)
                    {
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                        ++layer_a1_evaluations;
#endif
                        lower = std::max(lower, singleton_future->Future(p, remaining_nonanchor, vertex));
                    }
                    bound_cache[vertex] = lower;
                    state = (state & ~kBoundStageMask) | 3;
                    bound_state[vertex] = state;
                    if (!(value + lower < p.best))
                    {
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                        ++layer_a1_rejections;
#endif
                        return false;
                    }
                }
                if ((state & kBoundStageMask) == 3)
                {
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                    ++layer_tour_evaluations;
#endif
                    if (farthest < 0.0)
                        farthest = FarthestRemaining(p, vertex, remaining_original);
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                    if (lower < p.tour.UpperEnvelope(remaining_original, farthest))
                        ++layer_tour_full_evaluations;
                    else
                        ++layer_tour_envelope_skips;
#endif
                    if (lower < p.tour.UpperEnvelope(remaining_original, farthest))
                        lower = std::max(lower, p.tour.At(vertex, remaining_original, p.group_distance));
                    bound_cache[vertex] = lower;
                    state = (state & ~kBoundStageMask) | 4;
                    bound_state[vertex] = state;
                }
                const bool can_improve = value + lower < p.best;
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
                if (can_improve)
                    ++layer_certificate_passes;
                else if ((state & kBoundStageMask) == 4)
                    ++layer_tour_rejections;
#endif
                if (can_improve && (state & kBoundRejectedFrontier))
                {
                    touched.push_back(vertex);
                    bound_state[vertex] = state & ~kBoundRejectedFrontier;
                }
                return can_improve;
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
            // lambda：先只聚合同根拆分的逐顶点最小值；全部拆分结束后再对
            // 每个顶点运行一次统一证书链，避免被后续更优拆分覆盖的重复求值。
            auto Gather = [&](int vertex, double value)
            {
                if (value >= split[vertex])
                    return;
                if (split[vertex] >= fp::kInf)
                    touched.push_back(vertex);
                split[vertex] = value;
            };

            if (size == 2)
            {
                const int first = mask & -mask;
                const int second = mask ^ first;
                // lambda：size-2 只有唯一规范拆分，直接进入统一证书链；没有
                // 其他拆分会覆盖该值，不支付先暂存再二次遍历的固定成本。
                ForEachCommonValue(p, first, second, [&](int vertex, double a, double b)
                {
                    Set(vertex, a + b);
                });
                for (int vertex : touched)
                    split[vertex] = distance[vertex];
            }
            else
            {
                const int domain = mask ^ (mask & -mask);
                for (int branch = domain; branch; branch = (branch - 1) & domain)
                {
                    const int accumulator = mask ^ branch;
                    // accumulator 与 branch 都是真子集，已由较低层发布；空 row
                    // 同样是 ready 的精确空集，不需要在每个规范拆分重复检查。
                    ForEachPivotBranch(p, accumulator, branch, [&](int vertex, double a, double b)
                    {
                        Gather(vertex, a + b);
                    });
                }
            }

            if (size > 2)
            {
                size_t accepted = 0;
                for (int vertex : touched)
                {
                    const double value = split[vertex];
                    if (CanImprove(vertex, value))
                    {
                        distance[vertex] = value;
                        touched[accepted++] = vertex;
                    }
                    else
                        split[vertex] = fp::kInf;
                }
                touched.resize(accepted);
            }
            // Set/CanImprove 只有在完整 future 已缓存后才接纳标签；统一线性建堆。
            SearchQueue queue = BuildInitialQueue(touched, distance, bound_cache);
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
                    // CanImprove 成功时已完成并缓存全部 future 证书。
                    queue.push({next + bound_cache[edge.to], next, edge.to});
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
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_branch", p, -1.0, nullptr, size, static_cast<long long>(row.branch_count));
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_certificate_cache", p, -1.0, nullptr, size, certificate_cache_rejections);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_exact_dual_cache", p, -1.0, nullptr, size, exact_dual_cache_rejections);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_certificate_calls_row_prefix", p, -1.0, nullptr, size, layer_certificate_calls);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_certificate_cache_rejections_row_prefix", p, -1.0, nullptr, size, layer_certificate_cache_rejections);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_dual_evaluations_row_prefix", p, -1.0, nullptr, size, layer_dual_evaluations);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_dual_rejections_row_prefix", p, -1.0, nullptr, size, layer_dual_rejections);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_dual_exact_row_prefix", p, -1.0, nullptr, size, layer_dual_exact);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_farthest_evaluations_row_prefix", p, -1.0, nullptr, size, layer_farthest_evaluations);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_farthest_rejections_row_prefix", p, -1.0, nullptr, size, layer_farthest_rejections);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_a1_evaluations_row_prefix", p, -1.0, nullptr, size, layer_a1_evaluations);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_a1_rejections_row_prefix", p, -1.0, nullptr, size, layer_a1_rejections);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_tour_evaluations_row_prefix", p, -1.0, nullptr, size, layer_tour_evaluations);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_tour_full_evaluations_row_prefix", p, -1.0, nullptr, size, layer_tour_full_evaluations);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_tour_envelope_skips_row_prefix", p, -1.0, nullptr, size, layer_tour_envelope_skips);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_tour_rejections_row_prefix", p, -1.0, nullptr, size, layer_tour_rejections);
            EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_certificate_passes_row_prefix", p, -1.0, nullptr, size, layer_certificate_passes);
#endif
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
                // 补集若层更低已在前层发布；若同层则只在编号更小时结算，因而也已发布。
                if (p.popcount[complement] < size ||
                    (p.popcount[complement] == size && complement < mask))
                {
                    // lambda：在平衡半格把互补 ordinary 状态与锚组距离结算为完整上界。
                    ForEachCommonValue(p, mask, complement, [&](int vertex, double a, double b)
                    {
                        const double anchor_distance = p.group_distance[p.anchor_group].ExactValueOrInf(vertex);
                        if (anchor_distance < fp::kInf)
                            p.best = std::min(p.best, a + b + anchor_distance);
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

            for (int vertex : touched)
            {
                split[vertex] = fp::kInf;
                distance[vertex] = fp::kInf;
            }
            for (int vertex : rejected)
                distance[vertex] = fp::kInf;
        }

        EmitAbhssProbe(ProbeFamilyMethod(p),
                       "ordinary_layer",
                       p,
                       -1.0,
                       &p.ordinary,
                       size,
                       layer_work);
#if defined(GST_ENABLE_DETAILED_PROBE_DIAGNOSTICS)
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_certificate_calls_layer", p, -1.0, nullptr, size, layer_certificate_calls);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_certificate_cache_rejections_layer", p, -1.0, nullptr, size, layer_certificate_cache_rejections);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_dual_evaluations_layer", p, -1.0, nullptr, size, layer_dual_evaluations);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_dual_rejections_layer", p, -1.0, nullptr, size, layer_dual_rejections);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_dual_exact_layer", p, -1.0, nullptr, size, layer_dual_exact);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_farthest_evaluations_layer", p, -1.0, nullptr, size, layer_farthest_evaluations);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_farthest_rejections_layer", p, -1.0, nullptr, size, layer_farthest_rejections);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_a1_evaluations_layer", p, -1.0, nullptr, size, layer_a1_evaluations);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_a1_rejections_layer", p, -1.0, nullptr, size, layer_a1_rejections);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_tour_evaluations_layer", p, -1.0, nullptr, size, layer_tour_evaluations);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_tour_full_evaluations_layer", p, -1.0, nullptr, size, layer_tour_full_evaluations);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_tour_envelope_skips_layer", p, -1.0, nullptr, size, layer_tour_envelope_skips);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_tour_rejections_layer", p, -1.0, nullptr, size, layer_tour_rejections);
        EmitAbhssProbe(ProbeFamilyMethod(p), "ordinary_certificate_passes_layer", p, -1.0, nullptr, size, layer_certificate_passes);
#endif
    }
}

}  // namespace gst::methods::abhss::internal
