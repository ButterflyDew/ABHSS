#pragma once

#include <memory>

#include "internal.h"

namespace abhss {
// 公共锚定 singleton 层在 ordinary 阶段充当 future 时的只读视图。
// A1 行在 D 之前生成，随后直接交给前向 A。缓存只存排名和原值下标；逐 bit 查找的累计工作达到结构成本时，依次物化 top-two 和完整 byte 排名。D 结束后释放查找缓存。
struct AnchoredSingletonFuture {
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

    // 读取公共 A1；cone 外返回由共同 continuation 定义的统一 fallback。
    double Value(const Problem& problem, int bit, int vertex) const;
    // 读取 A1 并同时返回精确 payload 下标或 cone 外标志。
    double ValueWithLocator(const Problem& problem, int bit, int vertex, std::uint32_t& locator) const;
    // 返回指定 singleton 在 A1 cone 外的严格相同非负 fallback。
    double FallbackValue(const Problem& problem, int bit, int vertex) const;
    // 用已缓存下标 O(1) 读取精确值，或返回统一 cone 外 fallback。
    double LocatedValue(const Problem& problem, int bit, int vertex, std::uint32_t locator) const;
    // 返回 remaining 中统一 A1 future 视图的精确最大值。
    double Future(const Problem& problem, int remaining, int vertex);
    // 在全部 singleton row 发布后，由其形状初始化两级无参数购买式。
    void InitializeLookupPlan(const Problem& problem);
    // 物化与 lazy 路径相同的 top-two，并精确因子化各 mask 的 tail rent。
    void MaterializeAllTopTwo(const Problem& problem);
    // 稳定物化 top-two 之后的完整 A1 排名，用一次精确读取替代逐 bit 二分。
    void MaterializeRankedTail(const Problem& problem);

    // ordinary 结束后释放 top-two、locator 和完整 ranked tail，仅保留待移交的标准 A1 row。
    void ReleaseLookupCache() {
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

// 生成公共 A1 逻辑层，并建立 ordinary 可复用的 future 视图。
// q>0 时两种模式都调用本函数。cone 和 fallback 不读取 dual；树 DP 收紧上界时整轮重启，最终只发布一份标准 A1 行。
void BuildReusableAnchoredSingletonLayer(Problem& problem, AnchoredSingletonFuture& singleton_future, class WitnessUpperScheduler& witness_scheduler);

// 按统一公式估计一次 witness-tree subset DP 的 buy 工作量。
// 两种模式只代入各自 witness 的顶点数，购买公式相同。
long long EstimateWitnessTreeDpWork(size_t witness_vertices, int nonanchor_count);

// 两种模式共用的 witness-tree DP rent-or-buy 调度器。
// 构造时 rent 严格为 0，buy 只由当前 `Problem` 的 witness 顶点数和非锚组数代入共同公式得到。公共 A1 与 ordinary D 都只上报真实 queue pop/edgerelax 工作；达到 buy 后才调用同一个 `EvaluateWitnessTree`。调度器记录ordinary row 修订号，避免在可用 DP 信息完全相同期间重复购买同一结果。
class WitnessUpperScheduler {
public:
    explicit WitnessUpperScheduler(Problem& problem);
    ~WitnessUpperScheduler();

    // 累加一段实际工作，并在满足共同阈值时购买树 DP。ordinary_changed：本段结束时是否发布了新的 ordinary row；A1 传 false。返回：本次购买是否严格收紧 incumbent；A1 据此安全重启整轮。
    bool Account(long long row_work, bool ordinary_changed);

    // Enhanced把新 ordinary mask 交给 support-DP；Base 不调用。
    void PublishOrdinaryMask(int mask) {
        if (support_dp_)
            support_dp_->PublishOrdinary(mask);
    }

    long long TotalWork() const;

    // 上界 evaluator 切换到 certificate support 后重置 buy 与 rent。
    void RefreshCertificate();

    // 距离下一次当前输入修订可购买还需支付的 rent；不可买时返回上限。
    long long RemainingRentUntilBuy() const;

private:
    void Evaluate();

    Problem& problem_;
    long long rent_ = 0;
    long long purchased_rent_ = 0;
    long long buy_ = 0;
    int ordinary_revision_ = 0;
    int evaluated_revision_ = -1;
    bool enabled_ = false;
    std::unique_ptr<CertificateSupportDpCache> support_dp_;
};

// DirectedCut 的 residual 全势闭包 rent-or-buy 调度器。
// 初始增强预处理只构造截断势并释放 residual；本调度器继承已经发生的公共A1 工作，随后累计 ordinary queue-pop/edge-relax 的真实工作。rent 达到 residual 重建、势补全和 primal恢复的静态结构成本，且累计普通 row payload 已覆盖至少一遍顶点域后，一次性重建 residual、增加未支付容量上的安全势，并在新零弧支撑上复用现有 primal/facility 真实上界。A1 内部没有 closure 特有分支；Base 的对象保持禁用，也不维护只供该增强操作使用的累计量。
class ResidualClosureScheduler {
public:
    explicit ResidualClosureScheduler(Problem& problem);
    bool Account(long long row_work, long long new_payload = 0);

private:
    bool Buy();

    Problem& problem_;
    long long rent_ = 0;
    long long buy_ = 0;
    long long payload_ = 0;
    bool enabled_ = false;
    bool purchased_ = false;
};

// 按 |S| 递增生成 D(S,v)，发布存活值，并标记不可继续同根拆分的规范 branch。
void BuildOrdinaryRows(Problem& problem, AnchoredSingletonFuture* singleton_future, WitnessUpperScheduler& witness_scheduler, ResidualClosureScheduler& closure_scheduler, int last_layer);

// 返回在递增数组中二分一次的保守比较次数，用于选择交集算法。
// 原循环结果对非空数组严格等于 size 的二进制位宽；空数组仍取 1，以免把“逐 branch 二分空表”误估为零工作。位扫描只替换重复计数，不改变选择式。
inline long long BinarySearchCost(size_t size) {
    if (!size) return 1;
    return static_cast<long long>(64 - __builtin_clzll(static_cast<unsigned long long>(size)));
}

// 枚举一张普通值 row 与另一张 branch row 的同顶点交集。
// 函数按可预测比较次数在“双指针”“枚举 branch 后二分”“枚举 value 后二分”之间选择；三条路径只改变常数，不改变 row 表示或状态语义。
template <class Use>
void ForEachRowBranchIntersection(const Row& values, const Row& branches, Use&& use) {
    // 三种遍历方法只是同一有序 row 上的小常数选择，不是三种存储布局。
    const long long linear = static_cast<long long>(values.vertex.size() + branches.vertex.size());
    const long long scan_branches = static_cast<long long>(branches.branch_count) * BinarySearchCost(values.vertex.size());
    const long long scan_values = static_cast<long long>(values.vertex.size()) * BinarySearchCost(branches.vertex.size());

    if (scan_branches < linear && scan_branches <= scan_values) {
        for (size_t j = 0; j < branches.vertex.size(); ++j) {
            if (!branches.IsBranch(j)) continue;
            const auto it = std::lower_bound(values.vertex.begin(), values.vertex.end(), branches.vertex[j]);
            if (it != values.vertex.end() && *it == branches.vertex[j]) {
                const size_t i = static_cast<size_t>(it - values.vertex.begin());
                use(*it, values.value[i], branches.value[j]);
            }
        }
        return;
    }
    if (scan_values < linear) {
        for (size_t i = 0; i < values.vertex.size(); ++i) {
            const auto it = std::lower_bound(branches.vertex.begin(), branches.vertex.end(), values.vertex[i]);
            if (it == branches.vertex.end() || *it != values.vertex[i]) continue;
            const size_t j = static_cast<size_t>(it - branches.vertex.begin());
            if (branches.IsBranch(j))
                use(values.vertex[i], values.value[i], branches.value[j]);
        }
        return;
    }

    size_t i = 0;
    size_t j = 0;
    while (i < values.vertex.size() && j < branches.vertex.size()) {
        if (values.vertex[i] < branches.vertex[j])
            ++i;
        else if (branches.vertex[j] < values.vertex[i])
            ++j;
        else {
            if (branches.IsBranch(j))
                use(values.vertex[i], values.value[i], branches.value[j]);
            ++i;
            ++j;
        }
    }
}

// 枚举两张有序稀疏 row 在同一顶点均有值的位置。
template <class Use>
void ForEachRowValueIntersection(const Row& left, const Row& right, Use&& use) {
    const long long linear = static_cast<long long>(left.vertex.size() + right.vertex.size());
    const long long scan_left = static_cast<long long>(left.vertex.size()) * BinarySearchCost(right.vertex.size());
    const long long scan_right = static_cast<long long>(right.vertex.size()) * BinarySearchCost(left.vertex.size());
    if (scan_left < linear && scan_left <= scan_right) {
        for (size_t i = 0; i < left.vertex.size(); ++i) {
            const auto it = std::lower_bound(right.vertex.begin(), right.vertex.end(), left.vertex[i]);
            if (it != right.vertex.end() && *it == left.vertex[i]) {
                const size_t j = static_cast<size_t>(it - right.vertex.begin());
                use(left.vertex[i], left.value[i], right.value[j]);
            }
        }
        return;
    }
    if (scan_right < linear) {
        for (size_t j = 0; j < right.vertex.size(); ++j) {
            const auto it = std::lower_bound(left.vertex.begin(), left.vertex.end(), right.vertex[j]);
            if (it != left.vertex.end() && *it == right.vertex[j]) {
                const size_t i = static_cast<size_t>(it - left.vertex.begin());
                use(right.vertex[j], left.value[i], right.value[j]);
            }
        }
        return;
    }
    size_t i = 0;
    size_t j = 0;
    while (i < left.vertex.size() && j < right.vertex.size()) {
        if (left.vertex[i] < right.vertex[j])
            ++i;
        else if (right.vertex[j] < left.vertex[i])
            ++j;
        else {
            use(left.vertex[i], left.value[i], right.value[j]);
            ++i;
            ++j;
        }
    }
}

// 枚举两个 ordinary 状态在同一顶点均有值的位置。
// singleton 通过 GroupRow 的精确 membership 检查，多组 row 通过较小一侧驱动二分；空侧表示零代价，不物化专门的 D(0) row。
template <class Use>
void ForEachCommonValue(const Problem& p, int left, int right, Use&& use) {
    if (!left || !right) {
        // lambda：空侧按零值解释，并把非空 ordinary 值映射为统一二元回调。
        ForEachOrdinaryValue(p, left | right, [&](int vertex, double value) {
            use(vertex, left ? value : 0.0, right ? value : 0.0);
        });
        return;
    }

    const size_t left_size = p.popcount[left] == 1 ? p.group_distance[p.bit_to_group[FirstBit(left)]].ExactSize(p.graph.n) : p.ordinary[left].vertex.size();
    const size_t right_size = p.popcount[right] == 1 ? p.group_distance[p.bit_to_group[FirstBit(right)]].ExactSize(p.graph.n) : p.ordinary[right].vertex.size();
    if (left_size <= right_size) {
        // lambda：由较小 left 集驱动，并在同顶点读取 right 值和精确 membership。
        ForEachOrdinaryValue(p, left, [&](int vertex, double a) {
            const double b = OrdinaryValue(p, right, vertex);
            if (b < fp::kInf)
                use(vertex, a, b);
        });
    } else {
        // lambda：由较小 right 集驱动，并在同顶点读取 left 值和精确 membership。
        ForEachOrdinaryValue(p, right, [&](int vertex, double b) {
            const double a = OrdinaryValue(p, left, vertex);
            if (a < fp::kInf)
                use(vertex, a, b);
        });
    }
}

// 枚举 accumulator 值与规范 branch 的同根合并候选。
// 对 singleton 特化为一次组距离 membership；其余情况调用统一 row 交集，保证 ordinary D 的 canonical split 不会在后续层重复计数。
template <class Use>
void ForEachPivotBranch(const Problem& p, int accumulator, int branch, Use&& use) {
    if (p.popcount[branch] == 1) {
        const int group = p.bit_to_group[FirstBit(branch)];
        // lambda：用 accumulator 驱动并读取 singleton branch 的精确组距离。
        ForEachOrdinaryValue(p, accumulator, [&](int vertex, double value) {
            const double singleton = p.group_distance[group].ExactValueOrInf(vertex);
            if (singleton < fp::kInf)
                use(vertex, value, singleton);
        });
        return;
    }
    if (p.popcount[accumulator] == 1) {
        const int group = p.bit_to_group[FirstBit(accumulator)];
        // lambda：由多组规范 branch 驱动并读取 singleton accumulator 的精确组距离。
        ForEachBranch(p.ordinary[branch], [&](int vertex, double value) {
            const double singleton = p.group_distance[group].ExactValueOrInf(vertex);
            if (singleton < fp::kInf)
                use(vertex, singleton, value);
        });
        return;
    }

    ForEachRowBranchIntersection(p.ordinary[accumulator], p.ordinary[branch], std::forward<Use>(use));
}

}  // namespace abhss
