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

// Light/Heavy 只在这里声明算法策略。公共 D/A row、闭包与完成逻辑不再
// 通过散落的 bool 分支区分两个发行入口。
enum class SolverVariant : unsigned char
{
    Light,
    Heavy,
};

// 返回 mask 最低位 1 的位编号；调用者保证 mask!=0。该操作位于所有
// subset 热循环中，统一使用编译器位扫描而不是跨翻译单元逐位循环。
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

// D/A/H 统一使用这一种物理格式。branch_bits 说明普通值是否可作为
// 不可继续同根拆分的分支；A/H 行不携带 branch。
struct Row
{
    std::vector<int> vertex;
    std::vector<double> value;
    std::vector<std::uint64_t> branch_bits;
    size_t branch_count = 0;
    bool ready = false;

    bool IsBranch(size_t index) const
    {
        return (branch_bits[index >> 6] >> (index & 63)) & std::uint64_t{1};
    }
};

// 在递增顶点列表中二分查找；缺失状态返回无穷大。
double RowValue(const Row& row, int vertex);

template <class Use>
void ForEachValue(const Row& row, Use&& use)
{
    for (size_t i = 0; i < row.vertex.size(); ++i)
        use(row.vertex[i], row.value[i]);
}

template <class Use>
void ForEachBranch(const Row& row, Use&& use)
{
    for (size_t i = 0; i < row.vertex.size(); ++i)
        if (row.IsBranch(i))
            use(row.vertex[i], row.value[i]);
}

// 组距离是预处理表，不是 D/A/H 状态行。Light 的有界表按实际字节数
// 在 dense 与“有序精确值 + rank 位图”之间选择；Heavy 使用完整 dense 表。
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

    double operator[](int v) const;
    bool IsExact(int v) const;
    size_t ExactSize(int n) const;

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

struct ComponentCover
{
    double lower = 0.0;
    int cover_number = 0;
    std::vector<int> roots;
};

struct RootPathUnion
{
    double upper = fp::kInf;
    int root = 0;
    std::vector<int> edge_ids;
};

struct WitnessTree
{
    std::vector<int> vertex;
    std::vector<int> parent;
    std::vector<double> parent_edge;
};

class TourLowerBound
{
public:
    // 在组间最短路度量上预计算所有固定端点 Hamilton 路径。
    void Build(const std::vector<std::vector<double>>& metric);
    // 返回从 vertex 完成 mask 中剩余组的固定端点 tour 下界。
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
    Problem(const Graph& input_graph,
            const Query& input_query,
            SolverVariant solver_variant)
        : graph(input_graph), query(input_query), variant(solver_variant)
    {
    }

    bool UsesBoundedGroupDistances() const
    {
        return variant == SolverVariant::Light;
    }

    bool UsesDirectedCut() const
    {
        return variant == SolverVariant::Heavy;
    }

    const Graph& graph;
    const Query& query;
    SolverVariant variant = SolverVariant::Light;
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

    bool operator>(const QueueNode& other) const
    {
        if (key != other.key)
            return key > other.key;
        if (distance != other.distance)
            return distance > other.distance;
        return vertex > other.vertex;
    }
};

// 用零权连通分量计算覆盖数下界，并返回可用的代表根。
ComponentCover ComputeComponentCover(const Graph& graph, const Query& query);
// 从每组的规范终端构造 SPT 可行解，返回最好边并集代价与根。
double BuildCanonicalSptUpper(const Graph& graph, const Query& query, int& root);
// 计算每个查询组到全图的多源最短路。bounded 时只保留严格小于 cutoff 的精确值。
GroupTable BuildGroupDistances(const Graph& graph,
                               const Query& query,
                               bool bounded,
                               double cutoff);
// 扫描共同根 sum_i d_i(v) 上界，同时更新最佳根。
double RootStarUpper(const GroupTable& distance, int n, int& root);
// 恢复若干候选根到各组的最短路并集，并按原图边 id 去重计价。
RootPathUnion BuildRootPathUnion(const Graph& graph,
                                 const Query& query,
                                 const GroupTable& distance,
                                 const std::vector<int>& roots);
// 将根路径并集整理为以锚终端为根的真实见证树。
WitnessTree BuildRootPathWitness(const Graph& graph,
                                 const Query& query,
                                 const RootPathUnion& paths,
                                 int anchor_group);
// 将 directed-cut dual 恢复的 primal 边整理为真实见证树。
WitnessTree BuildDualWitness(const Graph& graph,
                             const Query& query,
                             const std::vector<std::uint64_t>& edge_words,
                             int root,
                             int anchor_group);
// 在真实见证树上做 subset DP，将已完成的 ordinary rooted 子树组合成可行上界。
double EvaluateWitnessTree(const WitnessTree& tree,
                           const Problem& problem,
                           const std::vector<Row>& ordinary);
// 在 dual primal 设施点上构造有向可行支撑度量，再做小规模 subset DP 得到可行上界。
double BuildPrimalFacilityUpper(const Problem& problem,
                                const std::vector<double>& residual,
                                const std::vector<std::uint64_t>& edge_words);

// 返回 true 表示上下界已经闭合，可以直接返回 problem.best。
bool PrepareProblem(Problem& problem);
// 剩余组中的最远组距离下界；命中每顶点缓存时 O(1)。
double FarthestRemaining(const Problem& problem, int vertex, int original_mask);
// 统一 future 下界：max(farthest,tour)，Heavy 再取 directed-cut 势的最大值。
double FutureBound(const Problem& problem, int vertex, int original_mask);
// 调用者已完成便宜的 farthest 检查时复用该值，避免在热路径重复扫描组。
double FutureBound(const Problem& problem,
                   int vertex,
                   int original_mask,
                   double farthest);

template <class Use>
void ForEachGroupValue(const Problem& p, int mask, Use&& use)
{
    const int group = p.bit_to_group[FirstBit(mask)];
    p.group_distance[group].ForEachExact(p.graph.n, std::forward<Use>(use));
}

template <class Use>
void ForEachOrdinaryValue(const Problem& p, int mask, Use&& use)
{
    if (p.popcount[mask] == 1)
        ForEachGroupValue(p, mask, std::forward<Use>(use));
    else
        ForEachValue(p.ordinary[mask], std::forward<Use>(use));
}

template <class Use>
void ForEachOrdinaryBranch(const Problem& p, int mask, Use&& use)
{
    if (p.popcount[mask] == 1)
        ForEachGroupValue(p, mask, std::forward<Use>(use));
    else
        ForEachBranch(p.ordinary[mask], std::forward<Use>(use));
}

// 统一读取 singleton 组距离或多组 ordinary row。
double OrdinaryValue(const Problem& p, int mask, int vertex);
// 判断 ordinary mask 是否已可供后续层使用。
bool OrdinaryAvailable(const Problem& p, int mask);

}  // namespace gst::methods::abhss::internal

#endif
