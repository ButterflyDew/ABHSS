#ifndef ABHSS_INTERNAL_H
#define ABHSS_INTERNAL_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "../common/float_compare.h"
#include "../common/graph_io.h"
#include "../common/query_io.h"
#include "dual_cut.h"
#include "abhss.h"

namespace gst::methods::abhss::internal
{
using HeapItem = std::pair<double, int>;
using Heap = std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>>;

/**
 * @brief 返回非零子集掩码最低位 1 的编号。
 * @param mask 调用者保证不为 0 的非负位掩码。
 *
 * 该函数处于所有 subset 热循环，统一映射到编译器位扫描指令，避免不同
 * 阶段各自实现逐位循环并产生不一致的常数开销。
 */
inline int FirstBit(int mask)
{
#if defined(_MSC_VER)
    unsigned long index = 0;
    _BitScanForward(&index, static_cast<unsigned long>(mask));
    return static_cast<int>(index);
#else
    return __builtin_ctz(static_cast<unsigned int>(mask));
#endif
}

/**
 * @brief D/A/H 共用的唯一稀疏 row 物理格式。
 *
 * `vertex` 严格递增并与 `value` 对齐；`branch_bits` 仅对 ordinary D 有效，
 * 标记该值是否可作为不可继续同根拆分的规范分支。`ready` 区分“已生成但
 * 为空”和“尚未生成”，避免用 payload 大小猜测状态生命周期。
 */
struct Row
{
    std::vector<int> vertex;
    std::vector<double> value;
    std::vector<std::uint64_t> branch_bits;
    size_t branch_count = 0;
    bool ready = false;

    /** @brief O(1) 读取给定 row 下标的 branch 位；调用者保证下标有效。 */
    bool IsBranch(size_t index) const
    {
        return (branch_bits[index >> 6] >> (index & 63)) & std::uint64_t{1};
    }
};

/** @brief 在递增顶点列表中二分读取状态值；缺失顶点返回正无穷。 */
double RowValue(const Row& row, int vertex);

/** @brief 按顶点递增顺序枚举 row 的全部 `(vertex,value)`。 */
template <class Use>
void ForEachValue(const Row& row, Use&& use)
{
    for (size_t i = 0; i < row.vertex.size(); ++i)
        use(row.vertex[i], row.value[i]);
}

/** @brief 按顶点递增顺序只枚举 ordinary row 中被标记的规范 branch。 */
template <class Use>
void ForEachBranch(const Row& row, Use&& use)
{
    for (size_t i = 0; i < row.vertex.size(); ++i)
        if (row.IsBranch(i))
            use(row.vertex[i], row.value[i]);
}

/**
 * @brief 一个查询组到全图的多源最短路表，不属于 D/A/H 状态 row。
 *
 * 基础配置只保留严格小于安全 cutoff 的精确距离，并按实际字节数在 dense
 * 与“有序值+membership/rank 位图”间选择；DirectedCut 增强需要完整势，
 * 因而使用非 bounded dense 表。两种布局具有完全相同的读取语义。
 */
struct GroupRow
{
    double cutoff = fp::kInf;
    bool bounded = false;
    bool dense = false;
    size_t exact_count = 0;
    std::vector<int> vertex;
    std::vector<double> value;
    std::vector<std::uint64_t> bits;
    std::vector<std::uint32_t> rank;

    /** @brief 读取顶点距离；有界表的未保存位置返回 cutoff 证书值。 */
    double operator[](int v) const;
    /** @brief 判断该顶点是否保存了严格小于 cutoff 的精确距离。 */
    bool IsExact(int v) const;
    /** @brief 返回可被精确枚举的顶点数，用于选择最小交集驱动方。 */
    size_t ExactSize(int n) const;

    /** @brief 按顶点递增顺序枚举全部精确距离，屏蔽 dense/sparse 差异。 */
    template <class Use>
    void ForEachExact(int n, Use&& use) const
    {
        if (!bounded)
        {
            for (int v = 1; v <= n; ++v)
                use(v, value[v]);
            return;
        }
        if (dense)
        {
            for (int v = 1; v <= n; ++v)
                if (value[v] < cutoff)
                    use(v, value[v]);
            return;
        }
        for (size_t i = 0; i < vertex.size(); ++i)
            use(vertex[i], value[i]);
    }
};

using GroupTable = std::vector<GroupRow>;

/** @brief 零权连通分量覆盖得到的全局下界、覆盖数和候选代表根。 */
struct ComponentCover
{
    double lower = 0.0;
    int cover_number = 0;
    std::vector<int> roots;
};

/** @brief 从共同根到各组的最短路边并集及其真实去重边权。 */
struct RootPathUnion
{
    double upper = fp::kInf;
    int root = 0;
    std::vector<int> edge_ids;
};

/** @brief 以局部下标存储的真实可行树，用于独立 subset DP 上界求值。 */
struct WitnessTree
{
    std::vector<int> vertex;
    std::vector<int> parent;
    std::vector<double> parent_edge;
};

class TourLowerBound
{
public:
    /**
     * @brief 在组间最短路度量上预计算所有固定端点 Hamilton 路径。
     * @param metric 组到组的对称/非对称最短连接代价矩阵。
     */
    void Build(const std::vector<std::vector<double>>& metric);
    /**
     * @brief 返回从给定顶点完成 mask 中剩余组的固定端点 tour 下界。
     * @return 不超过任何可行剩余树代价的 admissible lower bound。
     */
    double At(int vertex, int mask, const GroupTable& distance) const;

private:
    struct Endpoint
    {
        int left = 0;
        int right = 0;
        double path = 0.0;
    };
    int group_count_ = 0;
    std::vector<std::vector<Endpoint>> endpoints_;
};

struct Problem
{
    /**
     * @brief 绑定只读图、查询和本次求解的冻结增强配置。
     *
     * 构造函数不做预处理；所有随查询增长的数组都由 `PrepareProblem` 在
     * 通过可行性前置检查后分配，从而避免平凡/不可行查询支付指数空间。
     */
    Problem(const Graph& input_graph,
            const Query& input_query,
            SolveOptions solve_options)
        : graph(input_graph), query(input_query), options(solve_options)
    {
    }

    /** @brief 返回是否启用给定增强，是内部所有能力判断的唯一入口。 */
    bool HasEnhancement(Enhancement enhancement) const
    {
        return options.Enabled(enhancement);
    }

    /**
     * @brief 返回是否使用基础配置的有界组距离。
     *
     * directed-cut 需要每个顶点的完整组距离势；因此一旦开启该增强就改用
     * dense 完整距离，否则保留基础配置的 cutoff 安全截断。
     */
    bool UsesBoundedGroupDistances() const
    {
        return !HasEnhancement(Enhancement::DirectedCut);
    }

    /** @brief 返回是否构造并使用 directed-cut 对偶势及其 primal 见证。 */
    bool UsesDirectedCut() const
    {
        return HasEnhancement(Enhancement::DirectedCut);
    }

    /** @brief 返回是否用低层前向 A 加高层 adjoint H 完成锚定状态。 */
    bool UsesAdjointCompletion() const
    {
        return HasEnhancement(Enhancement::AdjointCompletion);
    }

    const Graph& graph;
    const Query& query;
    SolveOptions options;
    int g = 0;
    int half = 0;
    int anchor_group = 0;
    int root = 1;
    int nonanchor_count = 0;
    int subset_count = 0;
    int full_mask = 0;
    int original_full_mask = 0;
    int anchor_bit = 0;
    int nonanchor_original_mask = 0;
    double best = fp::kInf;

    GroupTable group_distance;
    TourLowerBound tour;
    ComponentCover component_cover;
    RootPathUnion root_path_union;
    WitnessTree witness_tree;
    dual_cut::DualCutPotential dual;

    std::vector<int> bit_to_group;
    std::vector<int> popcount;
    std::vector<int> original_mask;
    std::vector<unsigned char> farthest_group;
    std::vector<Row> ordinary;
    std::vector<double> ordinary_minimum;
};

struct QueueNode
{
    double key = 0.0;
    double distance = 0.0;
    int vertex = 0;

    /**
     * @brief 为 `std::greater` 提供确定性最小堆顺序。
     *
     * 依次比较 A* key、真实 distance 和 vertex，确保等权图跨平台访问顺序稳定。
     */
    bool operator>(const QueueNode& other) const
    {
        if (key != other.key)
            return key > other.key;
        if (distance != other.distance)
            return distance > other.distance;
        return vertex > other.vertex;
    }
};

/** @brief 用零权连通分量计算覆盖数下界，并返回可用的代表根。 */
ComponentCover ComputeComponentCover(const Graph& graph, const Query& query);
/** @brief 从每组规范终端构造 SPT 可行解，返回最好边并集代价与根。 */
double BuildCanonicalSptUpper(const Graph& graph, const Query& query, int& root);
/**
 * @brief 计算每组到全图的多源最短路。
 * @param bounded true 时只保存严格小于 cutoff 的精确值，其余位置以 cutoff
 *        作为安全证书；false 时保存完整 dense 距离。
 */
GroupTable BuildGroupDistances(const Graph& graph,
                               const Query& query,
                               bool bounded,
                               double cutoff);
/** @brief 扫描共同根 `sum_i d_i(v)` 可行上界，并通过引用更新最佳根。 */
double RootStarUpper(const GroupTable& distance, int n, int& root);
/** @brief 恢复候选根到各组的最短路并集，并按原图 edge id 去重计价。 */
RootPathUnion BuildRootPathUnion(const Graph& graph,
                                 const Query& query,
                                 const GroupTable& distance,
                                 const std::vector<int>& roots);
/** @brief 将根路径并集整理为以锚终端为根、无父指针环的真实见证树。 */
WitnessTree BuildRootPathWitness(const Graph& graph,
                                 const Query& query,
                                 const RootPathUnion& paths,
                                 int anchor_group);
/** @brief 将 directed-cut 恢复的 primal edge bitmap 整理为真实见证树。 */
WitnessTree BuildDualWitness(const Graph& graph,
                             const Query& query,
                             const std::vector<std::uint64_t>& edge_words,
                             int root,
                             int anchor_group);
/** @brief 在真实见证树上做 subset DP，将 ordinary rooted 子树组合成上界。 */
double EvaluateWitnessTree(const WitnessTree& tree,
                           const Problem& problem,
                           const std::vector<Row>& ordinary);
/** @brief 在 dual primal 设施点上构造支撑度量并做小规模 subset DP 上界。 */
double BuildPrimalFacilityUpper(const Problem& problem,
                                const std::vector<double>& residual,
                                const std::vector<std::uint64_t>& edge_words);

/** @brief 执行配置驱动的公共预处理；返回 true 表示上下界已经闭合。 */
bool PrepareProblem(Problem& problem);
/** @brief 返回剩余组中的最远组距离下界；命中顶点缓存时为 O(1)。 */
double FarthestRemaining(const Problem& problem, int vertex, int original_mask);
/** @brief 计算统一 future 下界；开启 DirectedCut 时再并入对偶势。 */
double FutureBound(const Problem& problem, int vertex, int original_mask);
/** @brief 复用调用者已计算的 farthest，避免热路径重复扫描剩余组。 */
double FutureBound(const Problem& problem,
                   int vertex,
                   int original_mask,
                   double farthest);

/** @brief 枚举 singleton mask 对应组的全部精确距离值。 */
template <class Use>
void ForEachGroupValue(const Problem& p, int mask, Use&& use)
{
    const int group = p.bit_to_group[FirstBit(mask)];
    p.group_distance[group].ForEachExact(p.graph.n, std::forward<Use>(use));
}

/** @brief 统一枚举 singleton 组距离或多组 ordinary row 的全部值。 */
template <class Use>
void ForEachOrdinaryValue(const Problem& p, int mask, Use&& use)
{
    if (p.popcount[mask] == 1)
        ForEachGroupValue(p, mask, std::forward<Use>(use));
    else
        ForEachValue(p.ordinary[mask], std::forward<Use>(use));
}

/** @brief 统一枚举 singleton 全体值或多组 ordinary row 的规范 branch。 */
template <class Use>
void ForEachOrdinaryBranch(const Problem& p, int mask, Use&& use)
{
    if (p.popcount[mask] == 1)
        ForEachGroupValue(p, mask, std::forward<Use>(use));
    else
        ForEachBranch(p.ordinary[mask], std::forward<Use>(use));
}

/** @brief 统一读取空 mask、singleton 组距离或多组 ordinary row。 */
double OrdinaryValue(const Problem& p, int mask, int vertex);
/** @brief 判断非空 ordinary mask 是否已生成、可供后续层使用。 */
bool OrdinaryAvailable(const Problem& p, int mask);

}  // namespace gst::methods::abhss::internal

#endif
