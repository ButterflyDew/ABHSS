#ifndef ABHSS_CORE_H
#define ABHSS_CORE_H

#include "internal.h"

namespace gst::methods::abhss::internal
{
struct EarlyAnchor
{
    double cutoff = fp::kInf;
    std::vector<Row> row;
    std::vector<unsigned char> first;
    std::vector<unsigned char> second;

    // 读取提前 A1 精确值；cone 外位置返回由 continuation 导出的证书下界。
    double Value(const Problem& problem, int bit, int vertex) const;
    // 返回未覆盖 singleton 中最大的 anchor-aware future，并维护 top-two 精确组索引。
    double Future(const Problem& problem, int remaining, int vertex);
};

// Light 在 ordinary D2 之前生成所有有界 A1，后续正式 A 格直接复用。
void BuildEarlyAnchorRows(Problem& problem, EarlyAnchor& early);
// 按 |S| 递增生成 D(S,v)，并只发布不可继续同根拆分的 branch。
void BuildOrdinaryRows(Problem& problem, EarlyAnchor* early);

inline long long BinarySearchCost(size_t size)
{
    long long cost = 1;
    for (size_t bound = 2; bound < size + 1; bound <<= 1)
        ++cost;
    return cost;
}

template <class Use>
void ForEachRowBranchIntersection(const Row& values,
                                  const Row& branches,
                                  Use&& use)
{
    // 三种遍历方法只是同一有序 row 上的小常数选择，不是三种存储布局。
    const long long linear = static_cast<long long>(
        values.vertex.size() + branches.vertex.size());
    const long long scan_branches =
        static_cast<long long>(branches.branch_count) *
        BinarySearchCost(values.vertex.size());
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

template <class Use>
void ForEachCommonValue(const Problem& p, int left, int right, Use&& use)
{
    if (!left || !right)
    {
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
        ForEachOrdinaryValue(p, left, [&](int vertex, double a)
        {
            const double b = OrdinaryValue(p, right, vertex);
            if (b < fp::kInf &&
                (p.popcount[right] != 1 ||
                 p.group_distance[p.bit_to_group[FirstBit(right)]].IsExact(vertex)))
                use(vertex, a, b);
        });
    }
    else
    {
        ForEachOrdinaryValue(p, right, [&](int vertex, double b)
        {
            const double a = OrdinaryValue(p, left, vertex);
            if (a < fp::kInf &&
                (p.popcount[left] != 1 ||
                 p.group_distance[p.bit_to_group[FirstBit(left)]].IsExact(vertex)))
                use(vertex, a, b);
        });
    }
}

template <class Use>
void ForEachPivotBranch(const Problem& p, int accumulator, int branch, Use&& use)
{
    if (p.popcount[branch] == 1)
    {
        const int group = p.bit_to_group[FirstBit(branch)];
        ForEachOrdinaryValue(p, accumulator, [&](int vertex, double value)
        {
            if (p.group_distance[group].IsExact(vertex))
                use(vertex, value, p.group_distance[group][vertex]);
        });
        return;
    }
    if (p.popcount[accumulator] == 1)
    {
        const int group = p.bit_to_group[FirstBit(accumulator)];
        ForEachBranch(p.ordinary[branch], [&](int vertex, double value)
        {
            if (p.group_distance[group].IsExact(vertex))
                use(vertex, p.group_distance[group][vertex], value);
        });
        return;
    }

    ForEachRowBranchIntersection(
        p.ordinary[accumulator], p.ordinary[branch], std::forward<Use>(use));
}

}  // namespace gst::methods::abhss::internal

#endif
