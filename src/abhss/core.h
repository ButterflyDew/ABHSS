#ifndef ABHSS_CORE_H
#define ABHSS_CORE_H

#include <memory>

#include "internal.h"

namespace gst::methods::abhss::internal
{
/**
 * @brief 公共锚定 singleton 层在 ordinary 阶段充当 future 时的只读视图。
 *
 * 只要完成计划中存在 A1，全部合法配置都在 ordinary 之前生成该公共层，
 * 并通过此视图供全部非平凡 ordinary 层复用为 future；DirectedCut/Enhanced
 * 另在统一下界中并入 dual，不取消 A1。
 * 是否存在 A1 完全由锚定状态格的最高逻辑层决定，不使用经验性的 g 阈值。
 * 所有配置的 cone 外位置都用构造时 cutoff 与同一个连续 continuation 恢复
 * 安全下界；DirectedCut 不进入 A1 构造，只在 ordinary 的其他 future 中
 * 作为独立证书。每个顶点先缓存最大的两个 singleton bit，并把两个 32-bit
 * payload locator 压入一个按需触页的 64-bit 项；top-two 购买时还用精确子集递推
 * 因子化各 remaining mask 的 row 形状租金。若 tail 二分工作达到第二级结构购买点，
 * 再用 byte 保存 top-two 之后的完整 bit 次序。排名不保存 double，cone 外 locator
 * 仍重算同一安全公式。ordinary 结束后释放全部只读查找缓存。
 */
struct AnchoredSingletonFuture
{
    double cutoff = fp::kInf;
    std::vector<Row> row;
    std::vector<unsigned char> first;
    std::vector<unsigned char> second;
    std::vector<unsigned char> ranked_tail;
    std::vector<int> ranked_rent_by_mask;
    std::unique_ptr<std::uint64_t[]> cached_locator_pair;
    long long lookup_buy_work = 0;
    long long lookup_touch_remaining = 0;
    long long ranked_buy_work = 0;
    long long ranked_rent_work = 0;
    bool lookup_materialized = false;

    /** @brief 读取公共 A1；cone 外返回由共同 continuation 定义的统一 fallback。 */
    double Value(const Problem& problem, int bit, int vertex) const;
    /** @brief 读取 A1 并同时返回精确 payload 下标或 cone 外标志。 */
    double ValueWithLocator(const Problem& problem,
                            int bit,
                            int vertex,
                            std::uint32_t& locator) const;
    /** @brief 返回指定 singleton 在 A1 cone 外的严格相同正 fallback。 */
    double FallbackValue(const Problem& problem, int bit, int vertex) const;
    /** @brief 用已缓存下标 O(1) 读取精确值，或返回统一 cone 外 fallback。 */
    double LocatedValue(const Problem& problem,
                        int bit,
                        int vertex,
                        std::uint32_t locator) const;
    /** @brief 返回 remaining 中最大的精确 A1 singleton future。 */
    double Future(const Problem& problem, int remaining, int vertex);
    /** @brief 在全部 singleton row 发布后，由其形状初始化两级无参数购买式。 */
    void InitializeLookupPlan(const Problem& problem);
    /** @brief 物化与 lazy 路径相同的 top-two，并精确因子化各 mask 的 tail rent。 */
    void MaterializeAllTopTwo(const Problem& problem);
    /** @brief 稳定物化 top-two 之后的完整 A1 排名，用一次精确读取替代逐 bit 二分。 */
    void MaterializeRankedTail(const Problem& problem);

    /** @brief ordinary 结束后释放 top-two、locator 和完整 ranked tail，仅保留待移交的标准 A1 row。 */
    void ReleaseLookupCache()
    {
        std::vector<unsigned char>().swap(first);
        std::vector<unsigned char>().swap(second);
        std::vector<unsigned char>().swap(ranked_tail);
        std::vector<int>().swap(ranked_rent_by_mask);
        cached_locator_pair.reset();
        lookup_buy_work = 0;
        lookup_touch_remaining = 0;
        ranked_buy_work = 0;
        ranked_rent_work = 0;
        lookup_materialized = false;
    }
};

/**
 * @brief 生成公共 A1 逻辑层，并建立 ordinary 可复用的 future 视图。
 *
 * 调用者必须先由完成计划确认 A1 确实存在；本函数自身不按 g、图名或
 * 运行时统计分类。返回的仍是标准 `Row`，ordinary 结束后直接移交给
 * 公共前向内核，不是 Base 独有的第二套状态结构。全部配置的前向前缀在
 * 逻辑域非空时都包含 A1。若条件式 witness 购买收紧上界，未完成 pass 会
 * 被丢弃并以新 cutoff 整轮重启；最终仍只发布、移交和登记一份 row。该函数
 * 不读取 enhancement profile，因而 Base、DirectedCutOnly 与 Enhanced 的
 * A1 操作在代码层也无法分叉。
 */
void BuildReusableAnchoredSingletonLayer(
    Problem& problem,
    AnchoredSingletonFuture& singleton_future,
    class WitnessUpperScheduler& witness_scheduler);

/**
 * @brief 按统一公式估计一次 witness-tree subset DP 的 buy 工作量。
 *
 * `witness_vertices` 只允许取当前配置已经构造的真实 witness 顶点数；
 * `nonanchor_count` 决定共同的 subset 空间。函数不读取配置位，因此 Base、
 * DirectedCutOnly 与 Enhanced 只能把各自树大小代入同一公式。
 */
long long EstimateWitnessTreeDpWork(size_t witness_vertices,
                                    int nonanchor_count);

/**
 * @brief Base/Enhanced 共用的 witness-tree DP rent-or-buy 调度器。
 *
 * 构造时 rent 严格为 0，buy 只由当前 `Problem` 的 witness 顶点数和非锚组
 * 数代入共同公式得到。公共 A1 与 ordinary D 都只上报真实 queue pop/edge
 * relax 工作；达到 buy 后才调用同一个 `EvaluateWitnessTree`。调度器记录
 * ordinary row 修订号，避免在可用 DP 信息完全相同期间重复购买同一结果。
 */
class WitnessUpperScheduler
{
public:
    explicit WitnessUpperScheduler(Problem& problem);

    /**
     * @brief 累加一段实际工作，并在满足共同阈值时购买树 DP。
     * @param ordinary_changed 本段结束时是否有新的 ordinary DP row 可用；
     *        A1 传 false，D row 完成后传 true。
     * @return 本次购买是否严格收紧 incumbent；A1 据此安全重启整轮。
     */
    bool Account(long long row_work, bool ordinary_changed);

    long long BuyWork() const { return buy_; }
    long long RentWork() const { return rent_; }
    long long TotalWork() const;
    int EvaluationCount() const { return evaluation_count_; }

    /** @brief 上界 evaluator 切换到 certificate support 后重置 buy 与 rent。 */
    void RefreshCertificate();

    /** @brief 距离下一次当前输入修订可购买还需支付的 rent；不可买时返回上限。 */
    long long RemainingRentUntilBuy() const;

private:
    void Evaluate();

    Problem& problem_;
    long long rent_ = 0;
    long long purchased_rent_ = 0;
    long long buy_ = 0;
    int ordinary_revision_ = 0;
    int evaluated_revision_ = -1;
    int evaluation_count_ = 0;
    bool enabled_ = false;
};

/**
 * @brief DirectedCut 的 residual 全势闭包 rent-or-buy 调度器。
 *
 * 初始增强预处理只构造截断势并释放 residual；本调度器继承已经发生的公共
 * A1 工作，随后累计 ordinary queue-pop/edge-relax 的真实工作。rent 达到 residual 重建、势补全和 primal
 * 恢复的静态结构成本，且累计普通 row payload 已覆盖至少一遍顶点域后，一次性重建 residual、增加
 * 未支付容量上的安全势，并在新零弧支撑上复用现有 primal/facility 真实上界。
 * A1 内部没有 closure 特有分支；Base 的对象保持禁用，也不维护只供该增强操作使用的累计量。
 */
class ResidualClosureScheduler
{
public:
    explicit ResidualClosureScheduler(Problem& problem);
    bool Account(long long row_work, long long new_payload = 0);

    long long BuyWork() const { return buy_; }
    long long RentWork() const { return rent_; }
    bool Purchased() const { return purchased_; }

private:
    bool Buy();

    Problem& problem_;
    long long rent_ = 0;
    long long buy_ = 0;
    long long payload_ = 0;
    bool enabled_ = false;
    bool purchased_ = false;
};

/** @brief 按 |S| 递增生成 D(S,v)，仅发布不可继续同根拆分的规范 branch。 */
void BuildOrdinaryRows(Problem& problem,
                       AnchoredSingletonFuture* singleton_future,
                       WitnessUpperScheduler& witness_scheduler,
                       ResidualClosureScheduler& closure_scheduler,
                       int last_layer);

/**
 * @brief 返回在递增数组中二分一次的保守比较次数，用于选择交集算法。
 *
 * 原循环结果对非空数组严格等于 size 的二进制位宽；空数组仍取 1，以免
 * 把“逐 branch 二分空表”误估为零工作。位扫描只替换重复计数，不改变选择式。
 */
inline long long BinarySearchCost(size_t size)
{
    if (!size)
        return 1;
#if defined(_MSC_VER)
    unsigned long highest = 0;
#if defined(_WIN64)
    _BitScanReverse64(&highest, static_cast<unsigned __int64>(size));
#else
    _BitScanReverse(&highest, static_cast<unsigned long>(size));
#endif
    return static_cast<long long>(highest) + 1;
#else
    return static_cast<long long>(64 - __builtin_clzll(static_cast<unsigned long long>(size)));
#endif
}

/**
 * @brief 枚举一张普通值 row 与另一张 branch row 的同顶点交集。
 *
 * 函数按可预测比较次数在“双指针”“枚举 branch 后二分”“枚举 value 后
 * 二分”之间选择；三条路径只改变常数，不改变 row 表示或状态语义。
 */
template <class Use>
void ForEachRowBranchIntersection(const Row& values,
                                  const Row& branches,
                                  Use&& use)
{
    // 三种遍历方法只是同一有序 row 上的小常数选择，不是三种存储布局。
    const long long linear = static_cast<long long>(
        values.vertex.size() + branches.vertex.size());
    const long long scan_branches = static_cast<long long>(branches.branch_count) * BinarySearchCost(values.vertex.size());
    const long long scan_values =
        static_cast<long long>(values.vertex.size()) *
        BinarySearchCost(branches.vertex.size());

    if (scan_branches < linear && scan_branches <= scan_values)
    {
        for (size_t j = 0; j < branches.vertex.size(); ++j)
        {
            if (!branches.IsBranch(j))
                continue;
            const auto it = std::lower_bound(
                values.vertex.begin(), values.vertex.end(), branches.vertex[j]);
            if (it != values.vertex.end() && *it == branches.vertex[j])
            {
                const size_t i = static_cast<size_t>(it - values.vertex.begin());
                use(*it, values.value[i], branches.value[j]);
            }
        }
        return;
    }
    if (scan_values < linear)
    {
        for (size_t i = 0; i < values.vertex.size(); ++i)
        {
            const auto it = std::lower_bound(
                branches.vertex.begin(), branches.vertex.end(), values.vertex[i]);
            if (it == branches.vertex.end() || *it != values.vertex[i])
                continue;
            const size_t j = static_cast<size_t>(it - branches.vertex.begin());
            if (branches.IsBranch(j))
                use(values.vertex[i], values.value[i], branches.value[j]);
        }
        return;
    }

    size_t i = 0;
    size_t j = 0;
    while (i < values.vertex.size() && j < branches.vertex.size())
    {
        if (values.vertex[i] < branches.vertex[j])
            ++i;
        else if (branches.vertex[j] < values.vertex[i])
            ++j;
        else
        {
            if (branches.IsBranch(j))
                use(values.vertex[i], values.value[i], branches.value[j]);
            ++i;
            ++j;
        }
    }
}

/**
 * @brief 枚举两个 ordinary 状态在同一顶点均有值的位置。
 *
 * singleton 通过 GroupRow 的精确 membership 检查，多组 row 通过较小一侧
 * 驱动二分；空侧表示零代价，不物化专门的 D(0) row。
 */
template <class Use>
void ForEachCommonValue(const Problem& p, int left, int right, Use&& use)
{
    if (!left || !right)
    {
        // lambda：空侧按零值解释，并把非空 ordinary 值映射为统一二元回调。
        ForEachOrdinaryValue(p, left | right, [&](int vertex, double value)
        {
            use(vertex, left ? value : 0.0, right ? value : 0.0);
        });
        return;
    }

    const size_t left_size = p.popcount[left] == 1
                                 ? p.group_distance[p.bit_to_group[FirstBit(left)]].ExactSize(
                                       p.graph.n)
                                 : p.ordinary[left].vertex.size();
    const size_t right_size = p.popcount[right] == 1
                                  ? p.group_distance[p.bit_to_group[FirstBit(right)]].ExactSize(
                                        p.graph.n)
                                  : p.ordinary[right].vertex.size();
    if (left_size <= right_size)
    {
        // lambda：由较小 left 集驱动，并在同顶点读取 right 值和精确 membership。
        ForEachOrdinaryValue(p, left, [&](int vertex, double a)
        {
            const double b = OrdinaryValue(p, right, vertex);
            if (b < fp::kInf)
                use(vertex, a, b);
        });
    }
    else
    {
        // lambda：由较小 right 集驱动，并在同顶点读取 left 值和精确 membership。
        ForEachOrdinaryValue(p, right, [&](int vertex, double b)
        {
            const double a = OrdinaryValue(p, left, vertex);
            if (a < fp::kInf)
                use(vertex, a, b);
        });
    }
}

/**
 * @brief 枚举 accumulator 值与规范 branch 的同根合并候选。
 *
 * 对 singleton 特化为一次组距离 membership；其余情况调用统一 row 交集，
 * 保证 ordinary D 的 canonical split 不会在后续层重复计数。
 */
template <class Use>
void ForEachPivotBranch(const Problem& p, int accumulator, int branch, Use&& use)
{
    if (p.popcount[branch] == 1)
    {
        const int group = p.bit_to_group[FirstBit(branch)];
        // lambda：用 accumulator 驱动并读取 singleton branch 的精确组距离。
        ForEachOrdinaryValue(p, accumulator, [&](int vertex, double value)
        {
            const double singleton = p.group_distance[group].ExactValueOrInf(vertex);
            if (singleton < fp::kInf)
                use(vertex, value, singleton);
        });
        return;
    }
    if (p.popcount[accumulator] == 1)
    {
        const int group = p.bit_to_group[FirstBit(accumulator)];
        // lambda：由多组规范 branch 驱动并读取 singleton accumulator 的精确组距离。
        ForEachBranch(p.ordinary[branch], [&](int vertex, double value)
        {
            const double singleton = p.group_distance[group].ExactValueOrInf(vertex);
            if (singleton < fp::kInf)
                use(vertex, singleton, value);
        });
        return;
    }

    ForEachRowBranchIntersection(p.ordinary[accumulator], p.ordinary[branch], std::forward<Use>(use));
}

}  // namespace gst::methods::abhss::internal

#endif
