#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>


#include "../common/float_compare.h"
#include "../graph.h"
#include "dual_cut.h"
#include "abhss.h"

namespace abhss {
using HeapItem = std::pair<double, int>;
using Heap = std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>>;

// 返回非零子集掩码最低位 1 的编号。mask：调用者保证不为 0 的非负位掩码。
// 该函数处于所有 subset 热循环，统一映射到编译器位扫描指令，避免不同阶段各自实现逐位循环并产生不一致的常数开销。
inline int FirstBit(int mask) {
    return __builtin_ctz(static_cast<unsigned int>(mask));
}

// D/A/H 共用的唯一稀疏 row 物理格式。
// `vertex` 严格递增并与 `value` 对齐；`branch_bits` 仅对 ordinary D 有效，标记该值是否可作为不可继续同根拆分的规范分支。对 ordinary、提前 A1所有权交接和 H 等会查询生命周期的 row，`ready` 区分“已发布但为空”与“尚未生成”。普通 forward A 内核另有一个严格局部例外：层序已处理且从未产生标签的空 row 可以不写 `ready`，后继只按空 payload 读取无穷。
struct Row {
    std::vector<int> vertex;
    std::vector<double> value;
    std::vector<std::uint64_t> branch_bits;
    size_t branch_count = 0;
    bool ready = false;

    // O(1) 读取给定 row 下标的 branch 位；调用者保证下标有效。
    bool IsBranch(size_t index) const {
        return (branch_bits[index >> 6] >> (index & 63)) & std::uint64_t{1};
    }
};

// 在递增顶点列表中二分读取状态值；缺失顶点返回正无穷。
double RowValue(const Row& row, int vertex);

// 按顶点递增顺序枚举 row 的全部 `(vertex,value)`。
template <class Use>
void ForEachValue(const Row& row, Use&& use) {
    for (size_t i = 0; i < row.vertex.size(); ++i)
        use(row.vertex[i], row.value[i]);
}

// 按顶点递增顺序只枚举 ordinary row 中被标记的规范 branch。
template <class Use>
void ForEachBranch(const Row& row, Use&& use) {
    for (size_t i = 0; i < row.vertex.size(); ++i)
        if (row.IsBranch(i))
            use(row.vertex[i], row.value[i]);
}

// 一个查询组到全图的多源最短路表，不属于 D/A/H 状态 row。
// 基础配置只保留严格小于安全 cutoff 的精确距离，并按实际字节数在 dense与“有序值+membership/rank 位图”间选择；DirectedCut 增强需要完整势，因而使用非 bounded dense 表。各布局共享两种读取职责：`operator[]` 可返回 cutoff 下界占位，`ExactValueOrInf` 只返回真实距离；占位不能冒充 DP seed。
struct GroupRow {
    double cutoff = fp::kInf;
    bool bounded = false;
    bool dense = false;
    size_t exact_count = 0;
    std::vector<int> vertex;
    std::vector<double> value;
    std::vector<std::uint64_t> bits;
    std::vector<std::uint32_t> rank;

    // 读取顶点距离；有界表的未保存位置返回 cutoff 证书值。
    double operator[](int v) const;
    // 单次布局查找：精确位置返回真实距离，cutoff 占位返回正无穷。
    double ExactValueOrInf(int v) const;
    // 返回可被精确枚举的顶点数，用于选择最小交集驱动方。
    size_t ExactSize(int n) const;

    // 按顶点递增顺序枚举全部精确距离，屏蔽 dense/sparse 差异。
    template <class Use>
    void ForEachExact(int n, Use&& use) const {
        if (!bounded) {
            for (int v = 1; v <= n; ++v)
                use(v, value[v]);
            return;
        }
        if (dense) {
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

// 两种距离初始化 realization 共同返回的完整输出合同。
// 在调用者已验证存在公共可行分量的前提下，`group_distance` 提供统一的值/精确性 oracle；`root` 是后续路径并集与锚组选择使用的候选根；`upper`必须是可由原图真实边展开的有限可行上界。bounded realization 可在内部先尝试构造 cutoff，complete realization 则由完整距离直接扫描共同根，但调用者不再调度配置专属的前置阶段。
struct DistanceRootInitialization {
    GroupTable group_distance;
    double upper = fp::kInf;
    int root = 1;
};

// 零权连通分量覆盖得到的全局下界、覆盖数和候选代表根。
struct ComponentCover {
    double lower = 0.0;
    int cover_number = 0;
    std::vector<int> roots;
};

// 从共同根到各组的最短路边并集及其真实去重边权。
struct RootPathUnion {
    double upper = fp::kInf;
    int root = 0;
    std::vector<int> edge_ids;
};

// 以局部下标存储的真实可行树，用于独立 subset DP 上界求值。
struct WitnessTree {
    std::vector<int> vertex;
    std::vector<int> parent;
    std::vector<double> parent_edge;
};

class TourLowerBound {
public:
    // 在组间最短路度量上预计算所有固定端点 Hamilton 路径。metric：无向输入图导出的对称组间最短连接代价矩阵。
    void Build(const std::vector<std::vector<double>>& metric);
    // 返回从给定顶点完成 mask 中剩余组的固定端点 tour 下界。返回：不超过任何可行剩余树代价的 admissible lower bound。
    double At(int vertex, int mask, const GroupTable& distance) const;
    // 返回完整 rooted tour 实现值的常数时间安全上包络。
    // `farthest` 必须是 vertex 到 mask 中最远组的距离。若已有可采纳下界不小于该值，则完整 `At` 不可能增大下界，可以跳过端点扫描。
    double UpperEnvelope(int mask, double farthest) const;
    // 返回固定规范起点组、终点自由的常数时间路径下界。
    // 多组 mask 选择终点自由 Hamilton 路径代价最大的起点组；查询时只读取该组到 vertex 的距离。单组直接返回组距离，空集返回 0。
    double EndpointFloorAt(int vertex, int mask, const GroupTable& distance) const;

private:
    struct Endpoint {
        int left = 0;
        int right = 0;
        double path = 0.0;
    };
    int group_count_ = 0;
    std::vector<std::vector<Endpoint>> endpoints_;
    std::vector<unsigned char> endpoint_floor_left_;
    std::vector<double> endpoint_floor_value_;
};

struct Problem {
    // 绑定只读图、查询和本次求解的运行模式。
    // 构造函数不做预处理；所有随查询增长的数组都由 `PrepareProblem` 在通过可行性前置检查后分配，从而避免平凡/不可行查询支付指数空间。
    Problem(const Graph& input_graph, const Query& input_query, bool use_enhanced) : graph(input_graph), query(input_query), enhanced(use_enhanced) {}

    const Graph& graph;
    const Query& query;
    bool enhanced;
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

    // 查询期间首次进入 D/A/H 行工作区的不同 `(mask,v)` 数量。
    // 每张逻辑 row 只构造一次，因此在该 row 中某顶点第一次由无穷变为有限值时即可一次性计数。D/A/H 状态族是键的一部分；同一数值 mask、vertex 出现在不同状态族时是不同项。所有配置提前调度的同一 A1 row转交给公共前向内核时不重复计数。组距离、tour、dual、转置候选和完整解结算不是主状态表，均不计入。
    std::uint64_t mask_vertex_states = 0;

    // 在一张 row 完成后批量登记其首次发现的顶点数，避免热循环逐项加法。
    void AccountMaskVertexStates(std::size_t discovered_vertices) {
        mask_vertex_states += static_cast<std::uint64_t>(discovered_vertices);
    }

    GroupTable group_distance;
    TourLowerBound tour;
    ComponentCover component_cover;
    RootPathUnion root_path_union;
    WitnessTree witness_tree;
    std::vector<int> certificate_support_edges;
    std::size_t certificate_support_vertex_count = 0;
    dual_cut::DualCutPotential dual;

    std::vector<int> bit_to_group;
    std::vector<int> popcount;
    std::vector<int> original_mask;
    std::vector<unsigned char> farthest_group;
    std::vector<Row> ordinary;
    std::vector<double> ordinary_minimum;
};

struct QueueNode {
    double key = 0.0;
    double distance = 0.0;
    int vertex = 0;

    // 为 `std::greater` 提供确定性最小堆顺序。
    // 依次比较 A* key、真实 distance 和 vertex，确保等权图跨平台访问顺序稳定。
    bool operator>(const QueueNode& other) const {
        if (key != other.key) return key > other.key;
        if (distance != other.distance) return distance > other.distance;
        return vertex > other.vertex;
    }
};

using SearchQueue = std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>>;

// 把一组互异顶点的已接纳初始标签一次性线性建成确定性最小堆。
// `touched` 中每个顶点只出现一次，且对应 lower 已由调用者缓存。与逐项`push` 相比，二者包含完全相同的 QueueNode；QueueNode 的 vertex 末级比较使顺序为全序，因此后续 pop 轨迹不变，只减去 O(t log t) 建堆工作。
inline SearchQueue BuildInitialQueue(const std::vector<int>& touched, const std::vector<double>& distance, const std::vector<double>& lower) {
    std::vector<QueueNode> storage;
    storage.reserve(touched.size());
    for (int vertex : touched)
        storage.push_back({distance[vertex] + lower[vertex], distance[vertex], vertex});
    return SearchQueue(std::greater<QueueNode>{}, std::move(storage));
}

// 用零权连通分量计算覆盖数下界，并返回可用的代表根。
ComponentCover ComputeComponentCover(const Graph& graph, const Query& query);
// 构造共同的“距离 oracle + 候选根 + 初始真实上界”预处理输出。
// 两种距离表示返回同一个结构，后续根路径并集与锚组选择直接消费它。
DistanceRootInitialization BuildDistanceRootInitialization(const Graph& graph, const Query& query, bool enhanced);
// 恢复候选根到各组的最短路并集，并按原图 edge id 去重计价。
RootPathUnion BuildRootPathUnion(const Graph& graph, const Query& query, const GroupTable& distance, const std::vector<int>& roots);
// 将根路径并集整理为以锚终端为根、无父指针环的真实见证树。
WitnessTree BuildRootPathWitness(const Graph& graph, const Query& query, const RootPathUnion& paths, int anchor_group);
// 将 directed-cut 恢复的 primal edge bitmap 整理为真实见证树。
WitnessTree BuildDualWitness(const Graph& graph, const Query& query, const std::vector<std::uint64_t>& edge_words, int root, int anchor_group);
// 在真实见证树上做 subset DP，将 ordinary rooted 子树组合成上界。
double EvaluateWitnessTree(const WitnessTree& tree, const Problem& problem, const std::vector<Row>& ordinary);
// 估计一次证书支持图 subset DP 的结构工作量。
long long EstimateCertificateSupportDpWork(std::size_t support_vertices, int nonanchor_count);
// 持久维护 certificate-support subset DP，并只重算新 ordinary 输入的超集。
// 新发布 D(M) 只可能改变含 M 的 mask。普通行重滤后重算全部 mask；固定支撑图的 Floyd 度量只建立一次。
class CertificateSupportDpCache {
public:
    // 绑定本查询；度量和 DP 数组留待首次购买时建立。
    explicit CertificateSupportDpCache(const Problem& problem) : problem_(problem) {}

    // 登记一张新发布的 ordinary row；实际投影延迟到下一次购买。
    void PublishOrdinary(int mask);
    // ordinary 被重滤后令下一次购买重建全表。
    void Reset();
    // 返回与当前全部已发布 ordinary 完全一致的 support 可行上界。
    double Evaluate();

private:
    void InitializeSupport();
    void MarkAllMasksDirty();
    void MarkSupersetsDirty(int mask);
    void RecomputeDirtyMasks();

    const Problem& problem_;
    std::vector<int> vertices_;
    std::vector<double> metric_;
    std::vector<double> dp_;
    std::vector<double> merged_;
    std::vector<double> closed_;
    std::vector<unsigned char> dirty_;
    std::vector<int> pending_masks_;
    bool initialized_ = false;
    bool full_rebuild_required_ = true;
};
// 在 dual primal 设施点上构造 residual-support 度量并做 subset DP 上界。
double BuildPrimalFacilityUpper(const Problem& problem, const std::vector<double>& residual, const std::vector<std::uint64_t>& edge_words);

// 执行配置驱动的公共预处理；返回 true 表示上下界已经闭合。
bool PrepareProblem(Problem& problem);
// 已购买增强证书刷新时，尝试登记四元路径与 primal 的真实支持图。
bool RefreshPurchasedPathGrowthCertificate(Problem& problem);
// 返回剩余组中的最远组距离下界；命中顶点最大组缓存时为 O(1)。
double FarthestRemaining(const Problem& problem, int vertex, int original_mask);
// 计算统一 future 下界；开启 DirectedCut 时再并入对偶势。
double FutureBound(const Problem& problem, int vertex, int original_mask);
// 复用调用者已计算的 farthest，避免热路径重复扫描剩余组。
double FutureBound(const Problem& problem, int vertex, int original_mask, double farthest);

// 枚举 singleton mask 对应组的全部精确距离值。
template <class Use>
void ForEachGroupValue(const Problem& p, int mask, Use&& use) {
    const int group = p.bit_to_group[FirstBit(mask)];
    p.group_distance[group].ForEachExact(p.graph.n, std::forward<Use>(use));
}

// 统一枚举 singleton 组距离或多组 ordinary row 的全部值。
template <class Use>
void ForEachOrdinaryValue(const Problem& p, int mask, Use&& use) {
    if (p.popcount[mask] == 1)
        ForEachGroupValue(p, mask, std::forward<Use>(use));
    else
        ForEachValue(p.ordinary[mask], std::forward<Use>(use));
}

// 统一枚举 singleton 全体值或多组 ordinary row 的规范 branch。
template <class Use>
void ForEachOrdinaryBranch(const Problem& p, int mask, Use&& use) {
    if (p.popcount[mask] == 1)
        ForEachGroupValue(p, mask, std::forward<Use>(use));
    else
        ForEachBranch(p.ordinary[mask], std::forward<Use>(use));
}

// 统一读取空 mask、精确 singleton 距离或多组 ordinary row。
inline double OrdinaryValue(const Problem& p, int mask, int vertex) {
    if (!mask) return 0.0;
    if (p.popcount[mask] == 1) return p.group_distance[p.bit_to_group[FirstBit(mask)]].ExactValueOrInf(vertex);
    return RowValue(p.ordinary[mask], vertex);
}

// 判断非空 ordinary mask 是否已生成、可供后续层使用。
bool OrdinaryAvailable(const Problem& p, int mask);

}  // namespace abhss
