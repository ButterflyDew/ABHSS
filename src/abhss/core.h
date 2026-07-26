#ifndef ABHSS_CORE_H
#define ABHSS_CORE_H

#include <memory>

#include "internal.h"

namespace gst::methods::abhss::internal
{
/**
 * @brief 公共锚定 singleton 层在 ordinary 阶段充当 future 时的只读视图。
 *
 * 只要状态格中存在 A1，Base 和 Enhanced 都在 ordinary 之前生成该公共层，
 * 并通过此视图供全部非平凡 ordinary 层复用为 future；Enhanced 另在统一
 * 下界中并入 dual，但不取消 A1。
 * 是否存在 A1 完全由锚定状态格的最高逻辑层决定，不使用经验性的 g 阈值。
 * 两种模式的 cone 外位置都用构造时 cutoff 与连续 farthest bound 恢复同一
 * 安全下界；dual 不进入 A1 构造，只在 ordinary 的其他 future 中
 * 作为独立证书。每个顶点缓存最大的两个 singleton bit，并把两个 32-bit
 * payload locator 压入
 * 一个按需触页的 64-bit 项，避免跨 D row 重复二分稀疏 A1；cone 外 locator
 * 直接重算已证明安全的公式。ordinary 结束后立即释放这些只读查找缓存。
 */
struct AnchoredSingletonFuture
{
    double cutoff = fp::kInf;
    std::vector<Row> row;
    std::vector<unsigned char> first;
    std::vector<unsigned char> second;
    std::unique_ptr<std::uint64_t[]> cached_locator_pair;

    /** @brief 读取公共 A1；cone 外返回统一的 farthest-based fallback。 */
    double Value(const Problem& problem, int bit, int vertex) const;
    /** @brief 读取 A1 并同时返回精确 payload 下标或 cone 外标志。 */
    double ValueWithLocator(const Problem& problem, int bit, int vertex, std::uint32_t& locator) const;
    /** @brief 用已缓存下标 O(1) 读取精确值，或返回统一 cone 外 fallback。 */
    double LocatedValue(const Problem& problem, int bit, int vertex, std::uint32_t locator) const;
    /** @brief 返回未覆盖 singleton 的最大 anchor-aware future 并维护 top-two。 */
    double Future(const Problem& problem, int remaining, int vertex);

    /** @brief ordinary 结束后释放 top-two 查找缓存，仅保留待移交的标准 A1 row。 */
    void ReleaseLookupCache()
    {
        std::vector<unsigned char>().swap(first);
        std::vector<unsigned char>().swap(second);
        cached_locator_pair.reset();
    }
};

/**
 * @brief 生成公共 A1 逻辑层，并建立 ordinary 可复用的 future 视图。
 *
 * 调用者必须先由完成计划确认 A1 确实存在；本函数自身不按 g、图名或
 * 运行时统计分类。返回的仍是标准 `Row`，ordinary 结束后直接移交给
 * 公共前向内核，不是 Base 独有的第二套状态结构。两种模式的前向前缀在
 * 逻辑域非空时都包含 A1。若条件式 witness 购买收紧上界，未完成 pass 会
 * 被丢弃并以新 cutoff 整轮重启；最终仍只发布、移交和登记一份 row。该函数
 * 不读取模式开关，因而 Base 与 Enhanced 的 A1 操作在代码层无法分叉。
 */
void BuildReusableAnchoredSingletonLayer(Problem& problem, AnchoredSingletonFuture& singleton_future, class WitnessUpperScheduler& witness_scheduler);

/**
 * @brief 按统一公式估计一次 witness-tree subset DP 的 buy 工作量。
 *
 * `witness_vertices` 取当前模式构造的真实 witness 顶点数，`nonanchor_count`
 * 决定共同的 subset 空间；Base 与 Enhanced 只把各自树大小代入同一公式。
 */
long long EstimateWitnessTreeDpWork(size_t witness_vertices, int nonanchor_count);

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
    /** @brief 从当前模式的 witness 大小计算 buy，rent 从 0 开始。 */
    explicit WitnessUpperScheduler(Problem& problem);

    /**
     * @brief 累加一段实际工作，并在满足共同阈值时购买树 DP。
     * @param ordinary_changed 本段结束时是否有新的 ordinary DP row 可用；
     *        A1 传 false，D row 完成后传 true。
     * @return 本次购买是否严格收紧 incumbent；A1 据此安全重启整轮。
     */
    bool Account(long long row_work, bool ordinary_changed);

    /** @brief 距离下一次当前输入修订可购买还需支付的 rent；不可买时返回上限。 */
    long long RemainingRentUntilBuy() const;

private:
    /** @brief 调用共用树 DP，并用所得可行值收紧 incumbent。 */
    void Evaluate();

    Problem& problem_;
    long long rent_ = 0;
    long long buy_ = 0;
    int ordinary_revision_ = 0;
    int evaluated_revision_ = -1;
};

/** @brief 按 |S| 递增生成 D(S,v)，仅发布不可继续同根拆分的规范 branch。 */
void BuildOrdinaryRows(Problem& problem, AnchoredSingletonFuture* singleton_future, WitnessUpperScheduler& witness_scheduler);

/** @brief 返回在递增数组中二分一次的保守比较次数，用于选择交集算法。 */
inline long long BinarySearchCost(size_t size)
{
    long long cost = 1;
    for (size_t bound = 2; bound < size + 1; bound <<= 1)
        ++cost;
    return cost;
}

/**
 * @brief 枚举一张普通值 row 与另一张 branch row 的同顶点交集。
 *
 * 函数按可预测比较次数在“双指针”“枚举 branch 后二分”“枚举 value 后
 * 二分”之间选择；三条路径只改变常数，不改变 row 表示或状态语义。
 */
template <class Use> void ForEachRowBranchIntersection(const Row& values, const Row& branches, Use&& use)
{
    // 三种遍历方法只是同一有序 row 上的小常数选择，不是三种存储布局。
    const long long linear = static_cast<long long>(values.vertex.size() + branches.vertex.size());
    const long long scan_branches = static_cast<long long>(branches.branch_count) * BinarySearchCost(values.vertex.size());
    const long long scan_values = static_cast<long long>(values.vertex.size()) * BinarySearchCost(branches.vertex.size());

    if (scan_branches < linear && scan_branches <= scan_values)
    {
        for (size_t j = 0; j < branches.vertex.size(); ++j)
        {
            if (!branches.IsBranch(j))
                continue;
            const auto it = std::lower_bound(values.vertex.begin(), values.vertex.end(), branches.vertex[j]);
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
            const auto it = std::lower_bound(branches.vertex.begin(), branches.vertex.end(), values.vertex[i]);
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
template <class Use> void ForEachCommonValue(const Problem& p, int left, int right, Use&& use)
{
    if (!left || !right)
    {
        // 一侧为空 mask 时只枚举另一侧，并把空侧值视为 0。
        ForEachOrdinaryValue(p, left | right, [&](int vertex, double value) { use(vertex, left ? value : 0.0, right ? value : 0.0); });
        return;
    }

    const size_t left_size = p.popcount[left] == 1 ? p.group_distance[p.bit_to_group[FirstBit(left)]].ExactSize(p.graph.n) : p.ordinary[left].vertex.size();
    const size_t right_size = p.popcount[right] == 1 ? p.group_distance[p.bit_to_group[FirstBit(right)]].ExactSize(p.graph.n) : p.ordinary[right].vertex.size();
    if (left_size <= right_size)
    {
        // 左侧更小时由左侧驱动，对右侧做单次读取与精确性检查。
        ForEachOrdinaryValue(p, left,
                             [&](int vertex, double a)
                             {
                                 const double b = OrdinaryValue(p, right, vertex);
                                 if (b < fp::kInf && (p.popcount[right] != 1 || p.group_distance[p.bit_to_group[FirstBit(right)]].IsExact(vertex)))
                                     use(vertex, a, b);
                             });
    }
    else
    {
        // 右侧更小时对称地由右侧驱动交集。
        ForEachOrdinaryValue(p, right,
                             [&](int vertex, double b)
                             {
                                 const double a = OrdinaryValue(p, left, vertex);
                                 if (a < fp::kInf && (p.popcount[left] != 1 || p.group_distance[p.bit_to_group[FirstBit(left)]].IsExact(vertex)))
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
template <class Use> void ForEachPivotBranch(const Problem& p, int accumulator, int branch, Use&& use)
{
    if (p.popcount[branch] == 1)
    {
        const int group = p.bit_to_group[FirstBit(branch)];
        // branch 为 singleton 时直接检查对应组距离的精确 membership。
        ForEachOrdinaryValue(p, accumulator,
                             [&](int vertex, double value)
                             {
                                 if (p.group_distance[group].IsExact(vertex))
                                     use(vertex, value, p.group_distance[group][vertex]);
                             });
        return;
    }
    if (p.popcount[accumulator] == 1)
    {
        const int group = p.bit_to_group[FirstBit(accumulator)];
        // accumulator 为 singleton 时由多组 branch row 驱动交集。
        ForEachBranch(p.ordinary[branch],
                      [&](int vertex, double value)
                      {
                          if (p.group_distance[group].IsExact(vertex))
                              use(vertex, p.group_distance[group][vertex], value);
                      });
        return;
    }

    ForEachRowBranchIntersection(p.ordinary[accumulator], p.ordinary[branch], std::forward<Use>(use));
}

} // namespace gst::methods::abhss::internal

#endif
