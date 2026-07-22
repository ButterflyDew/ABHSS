#include "pruneddp.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../common/float_compare.h"
#include "../common/probe_diagnostics.h"
#include "../common/query_feasibility.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace gst::methods::pruned_dp
{
namespace
{
constexpr int kNoWitness = -1;

int CountBits(int mask)
{
#if defined(_MSC_VER)
    return static_cast<int>(__popcnt(static_cast<unsigned int>(mask)));
#else
    return __builtin_popcount(static_cast<unsigned int>(mask));
#endif
}

struct StateEntry
{
    bool present = false;
    bool open = false;
    bool closed = false;
    double best_cost = fp::kInf;
    double open_cost = fp::kInf;
    double open_priority = fp::kInf;
    double settled_cost = fp::kInf;
    int settled_witness = kNoWitness;
    std::uint64_t version = 0;
};

// 统一封装 (vertex,mask) 状态。Hash 只为发现状态分配项，Dense 预分配 2^g(n+1) 项。
// 热路径只有 Find/Get 两个内联友好操作，避免通用容器层次。
class StateStore
{
public:
    StateStore(StateStorage storage, int n, int subset_count)
        : storage_(storage), n_(n), subset_count_(subset_count)
    {
        if (storage_ == StateStorage::Dense)
        {
            const std::size_t rows = static_cast<std::size_t>(subset_count_);
            const std::size_t width = static_cast<std::size_t>(n_) + 1;
            if (rows > std::numeric_limits<std::size_t>::max() / width)
                throw std::runtime_error("PrunedDP dense state table is too large.");
            dense_.resize(rows * width);
        }
    }

    StateEntry* Find(int vertex, int mask)
    {
        if (storage_ == StateStorage::Dense)
        {
            StateEntry& entry = dense_[DenseIndex(vertex, mask)];
            return entry.present ? &entry : nullptr;
        }
        const auto it = sparse_.find(Key(vertex, mask));
        return it == sparse_.end() ? nullptr : &it->second;
    }

    StateEntry& Get(int vertex, int mask)
    {
        if (storage_ == StateStorage::Dense)
        {
            StateEntry& entry = dense_[DenseIndex(vertex, mask)];
            if (!entry.present)
            {
                entry.present = true;
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
                ++size_;
#endif
            }
            return entry;
        }
        auto [it, inserted] = sparse_.try_emplace(Key(vertex, mask));
        if (inserted)
        {
            it->second.present = true;
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
            ++size_;
#endif
        }
        return it->second;
    }

    std::size_t Size() const
    {
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
        return size_;
#else
        return 0;
#endif
    }

private:
    std::size_t DenseIndex(int vertex, int mask) const
    {
        return static_cast<std::size_t>(mask) * (static_cast<std::size_t>(n_) + 1) +
               static_cast<std::size_t>(vertex);
    }

    static std::uint64_t Key(int vertex, int mask)
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(mask)) << 32) |
               static_cast<std::uint32_t>(vertex);
    }

    StateStorage storage_;
    int n_ = 0;
    int subset_count_ = 0;
    std::vector<StateEntry> dense_;
    std::unordered_map<std::uint64_t, StateEntry> sparse_;
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
    std::size_t size_ = 0;
#endif
};

enum class WitnessKind
{
    Seed,
    Edge,
    Merge,
};

struct Witness
{
    WitnessKind kind = WitnessKind::Seed;
    int left = kNoWitness;
    int right = kNoWitness;
    int edge_id = -1;
};

struct WitnessSpec
{
    WitnessKind kind = WitnessKind::Seed;
    int left = kNoWitness;
    int right = kNoWitness;
    int edge_id = -1;
};

struct QueueNode
{
    int vertex = 0;
    int mask = 0;
    double cost = 0.0;
    double priority = 0.0;
    int witness = kNoWitness;
    std::uint64_t version = 0;

    bool operator<(const QueueNode& other) const
    {
        if (priority != other.priority)
            return priority > other.priority;
        return cost > other.cost;
    }
};

struct RouteNode
{
    int start = 0;
    int end = 0;
    int mask = 0;
    double cost = 0.0;

    bool operator<(const RouteNode& other) const { return cost > other.cost; }
};

// MST 可行上界恢复中使用的并查集。
class DisjointSet
{
public:
    explicit DisjointSet(int size) : parent_(size), rank_(size)
    {
        for (int i = 0; i < size; ++i)
            parent_[i] = i;
    }

    int Find(int x)
    {
        while (parent_[x] != x)
        {
            parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }

    bool Unite(int left, int right)
    {
        left = Find(left);
        right = Find(right);
        if (left == right)
            return false;
        if (rank_[left] < rank_[right])
            std::swap(left, right);
        parent_[right] = left;
        if (rank_[left] == rank_[right])
            ++rank_[left];
        return true;
    }

private:
    std::vector<int> parent_;
    std::vector<unsigned char> rank_;
};

struct SolverContext
{
    const Graph* graph = nullptr;
    const Query* query = nullptr;
    PrunedDpOptions options;
    int n = 0;
    int group_count = 0;
    int full_mask = 0;
    int subset_count = 0;

    std::vector<std::vector<double>> group_distance;
    std::vector<std::vector<int>> next_vertex;
    std::vector<std::vector<int>> next_edge;
    std::vector<std::vector<double>> group_metric;
    std::vector<double> route_pair;
    std::vector<double> route_single;

    std::unique_ptr<StateStore> states;
    std::priority_queue<QueueNode> queue;
    std::vector<Witness> witnesses;
    std::vector<int> witness_epoch;
    std::vector<int> edge_epoch;
    std::vector<int> mst_local_index;
    int current_witness_epoch = 0;
    int current_edge_epoch = 0;

    double best = fp::kInf;

    bool probe = false;
    std::uint64_t candidate_attempts = 0;
    std::uint64_t accepted_candidates = 0;
    std::uint64_t settled_states = 0;
    std::uint64_t reopened_states = 0;
    std::uint64_t mst_calls = 0;
    std::uint64_t edge_attempts = 0;
    std::uint64_t merge_attempts = 0;
    std::size_t next_state_report = 65536;
};

using ProbeClock = std::chrono::steady_clock;

double ProbeSecondsSince(const ProbeClock::time_point& start)
{
    return std::chrono::duration<double>(ProbeClock::now() - start).count();
}

void EmitPrunedProbe(const SolverContext& ctx,
                     const char* phase,
                     double seconds = -1.0)
{
    if (!ctx.probe)
        return;
    std::ostringstream out;
    out << "method=pruneddp phase=" << phase;
    if (seconds >= 0.0)
        out << " seconds=" << seconds;
    out << " states=" << (ctx.states ? ctx.states->Size() : 0)
        << " settled=" << ctx.settled_states
        << " accepted=" << ctx.accepted_candidates
        << " candidates=" << ctx.candidate_attempts
        << " reopened=" << ctx.reopened_states
        << " mst_calls=" << ctx.mst_calls
        << " edge_attempts=" << ctx.edge_attempts
        << " merge_attempts=" << ctx.merge_attempts
        << " witnesses=" << ctx.witnesses.size()
        << " best=" << ctx.best;
    gst::EmitProbeDiagnostic(out.str());
}

// 将 (start,end,mask) 展平成 route_pair 下标。
std::size_t RoutePairIndex(const SolverContext& ctx, int start, int end, int mask)
{
    return (static_cast<std::size_t>(start) * ctx.group_count + end) *
               ctx.subset_count +
           mask;
}

// 将 (start,mask) 展平成 route_single 下标。
std::size_t RouteSingleIndex(const SolverContext& ctx, int start, int mask)
{
    return static_cast<std::size_t>(start) * ctx.subset_count + mask;
}

// 读取以 start/end 为端点、覆盖 mask 的组度量路径值。
double RoutePair(const SolverContext& ctx, int start, int end, int mask)
{
    return ctx.route_pair[RoutePairIndex(ctx, start, end, mask)];
}

// 读取固定 start、自由另一端点的最小组度量路径值。
double RouteSingle(const SolverContext& ctx, int start, int mask)
{
    return ctx.route_single[RouteSingleIndex(ctx, start, mask)];
}

// 对每个组运行多源 Dijkstra，同时保存 MST witness 所需的后继顶点与边。
// 随后由终端点取最小值得到组间最短路度量。
void BuildGroupDistances(SolverContext& ctx)
{
    using Item = std::pair<double, int>;
    ctx.group_distance.assign(
        ctx.group_count, std::vector<double>(ctx.n + 1, fp::kInf));
    ctx.next_vertex.assign(ctx.group_count, std::vector<int>(ctx.n + 1));
    ctx.next_edge.assign(ctx.group_count, std::vector<int>(ctx.n + 1, -1));

    for (int group = 0; group < ctx.group_count; ++group)
    {
        auto& distance = ctx.group_distance[group];
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
        for (int terminal : ctx.query->groups[group])
        {
            if (distance[terminal] == 0.0)
                continue;
            distance[terminal] = 0.0;
            heap.push({0.0, terminal});
        }
        while (!heap.empty())
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != distance[vertex])
                continue;
            for (const AdjEdge& edge : ctx.graph->adj[vertex])
            {
                const double next = value + edge.w;
                if (next < distance[edge.to])
                {
                    distance[edge.to] = next;
                    ctx.next_vertex[group][edge.to] = vertex;
                    ctx.next_edge[group][edge.to] = edge.edge_id;
                    heap.push({next, edge.to});
                }
            }
        }
    }

    ctx.group_metric.assign(
        ctx.group_count, std::vector<double>(ctx.group_count, fp::kInf));
    for (int left = 0; left < ctx.group_count; ++left)
    {
        for (int right = 0; right < ctx.group_count; ++right)
        {
            for (int terminal : ctx.query->groups[right])
            {
                ctx.group_metric[left][right] =
                    std::min(ctx.group_metric[left][right],
                             ctx.group_distance[left][terminal]);
            }
        }
    }
}

// 在组度量上枚举所有 mask 的端点路径，为论文 lb_2 提供查表值。
void BuildAllPaths(SolverContext& ctx)
{
    const std::size_t pair_size = static_cast<std::size_t>(ctx.group_count) *
                                  ctx.group_count * ctx.subset_count;
    ctx.route_pair.assign(pair_size, fp::kInf);
    ctx.route_single.assign(
        static_cast<std::size_t>(ctx.group_count) * ctx.subset_count, fp::kInf);
    std::priority_queue<RouteNode> queue;

    for (int group = 0; group < ctx.group_count; ++group)
    {
        const int mask = 1 << group;
        ctx.route_pair[RoutePairIndex(ctx, group, group, mask)] = 0.0;
        queue.push({group, group, mask, 0.0});
    }

    while (!queue.empty())
    {
        const RouteNode current = queue.top();
        queue.pop();
        if (current.cost != RoutePair(
                                ctx, current.start, current.end, current.mask))
            continue;
        double& single =
            ctx.route_single[RouteSingleIndex(ctx, current.start, current.mask)];
        single = std::min(single, current.cost);

        int remaining = ctx.full_mask ^ current.mask;
        while (remaining)
        {
            const int bit = remaining & -remaining;
            remaining ^= bit;
            const int next_group = CountBits(bit - 1);
            const int next_mask = current.mask | bit;
            const double next_cost =
                current.cost + ctx.group_metric[current.end][next_group];
            double& stored = ctx.route_pair[RoutePairIndex(
                ctx, current.start, next_group, next_mask)];
            if (next_cost < stored)
            {
                stored = next_cost;
                queue.push({current.start, next_group, next_mask, next_cost});
            }
        }
    }
}

// 计算 Algorithm 4 中的原始优先级：已支付 cost 加最远组、两类组路径下界的最大值。
double RawPriority(const SolverContext& ctx, int vertex, int mask, double cost)
{
    const int remaining = ctx.full_mask ^ mask;
    if (!remaining)
        return cost;

    double one_label = 0.0;
    double nearest = fp::kInf;
    for (int bits = remaining; bits; bits &= bits - 1)
    {
        const int bit = bits & -bits;
        const int group = CountBits(bit - 1);
        one_label = std::max(one_label, ctx.group_distance[group][vertex]);
        nearest = std::min(nearest, ctx.group_distance[group][vertex]);
    }

    double tour_one = fp::kInf;
    double tour_two = 0.0;
    for (int left_bits = remaining; left_bits; left_bits &= left_bits - 1)
    {
        const int left_bit = left_bits & -left_bits;
        const int left = CountBits(left_bit - 1);
        for (int right_bits = remaining; right_bits; right_bits &= right_bits - 1)
        {
            const int right_bit = right_bits & -right_bits;
            const int right = CountBits(right_bit - 1);
            tour_one = std::min(
                tour_one,
                ctx.group_distance[left][vertex] +
                    RoutePair(ctx, left, right, remaining) +
                    ctx.group_distance[right][vertex]);
        }
        tour_two = std::max(
            tour_two,
            ctx.group_distance[left][vertex] +
                RouteSingle(ctx, left, remaining) + nearest);
    }
    const double heuristic =
        std::max(one_label, std::max(tour_one * 0.5, tour_two * 0.5));
    return cost + heuristic;
}

// 只在启用 MST 上界时物化持久见证结点；关闭时零成本返回空 id。
int MaterializeWitness(SolverContext& ctx, const WitnessSpec& spec)
{
    if (!ctx.options.use_mst_upper_bound)
        return kNoWitness;
    const int id = static_cast<int>(ctx.witnesses.size());
    ctx.witnesses.push_back({spec.kind, spec.left, spec.right, spec.edge_id});
    ctx.witness_epoch.push_back(0);
    return id;
}

// 通过 epoch 复用访问标记数组，避免每次 MST 恢复清空 O(n+m) 内存。
void ResetEpoch(std::vector<int>& values, int& epoch)
{
    if (epoch == std::numeric_limits<int>::max())
    {
        std::fill(values.begin(), values.end(), 0);
        epoch = 1;
    }
    else
    {
        ++epoch;
    }
}

// 追溯当前 rooted 状态的真实边，再拼接剩余组最短路并做 Kruskal。
// 返回值是真实可行树上界，但这一步不在论文声称的理论复杂度内。
double BuildMstUpper(SolverContext& ctx, const QueueNode& state)
{
    ResetEpoch(ctx.witness_epoch, ctx.current_witness_epoch);
    ResetEpoch(ctx.edge_epoch, ctx.current_edge_epoch);
    std::vector<int> selected_edges;
    std::vector<int> stack;
    if (state.witness != kNoWitness)
        stack.push_back(state.witness);

    auto AddEdge = [&](int edge_id)
    {
        if (edge_id < 0 || ctx.edge_epoch[edge_id] == ctx.current_edge_epoch)
            return;
        ctx.edge_epoch[edge_id] = ctx.current_edge_epoch;
        selected_edges.push_back(edge_id);
    };

    while (!stack.empty())
    {
        const int witness_id = stack.back();
        stack.pop_back();
        if (ctx.witness_epoch[witness_id] == ctx.current_witness_epoch)
            continue;
        ctx.witness_epoch[witness_id] = ctx.current_witness_epoch;
        const Witness& witness = ctx.witnesses[witness_id];
        if (witness.kind == WitnessKind::Edge)
        {
            AddEdge(witness.edge_id);
            stack.push_back(witness.left);
        }
        else if (witness.kind == WitnessKind::Merge)
        {
            stack.push_back(witness.left);
            stack.push_back(witness.right);
        }
    }

    int remaining = ctx.full_mask ^ state.mask;
    while (remaining)
    {
        const int bit = remaining & -remaining;
        remaining ^= bit;
        const int group = CountBits(bit - 1);
        int vertex = state.vertex;
        int steps = 0;
        while (ctx.next_edge[group][vertex] >= 0)
        {
            AddEdge(ctx.next_edge[group][vertex]);
            vertex = ctx.next_vertex[group][vertex];
            if (++steps > ctx.n)
                throw std::runtime_error("PrunedDP shortest-path witness contains a cycle.");
        }
    }

    std::sort(selected_edges.begin(), selected_edges.end(), [&](int left, int right)
    {
        const UndirectedEdge& lhs = ctx.graph->edges[left];
        const UndirectedEdge& rhs = ctx.graph->edges[right];
        if (lhs.w != rhs.w)
            return lhs.w < rhs.w;
        return left < right;
    });

    std::vector<int> vertices;
    vertices.reserve(selected_edges.size() * 2);
    for (int edge_id : selected_edges)
    {
        const UndirectedEdge& edge = ctx.graph->edges[edge_id];
        if (ctx.mst_local_index[edge.u] < 0)
        {
            ctx.mst_local_index[edge.u] = static_cast<int>(vertices.size());
            vertices.push_back(edge.u);
        }
        if (ctx.mst_local_index[edge.v] < 0)
        {
            ctx.mst_local_index[edge.v] = static_cast<int>(vertices.size());
            vertices.push_back(edge.v);
        }
    }

    DisjointSet dsu(static_cast<int>(vertices.size()));
    double mst_cost = 0.0;
    for (int edge_id : selected_edges)
    {
        const UndirectedEdge& edge = ctx.graph->edges[edge_id];
        if (dsu.Unite(ctx.mst_local_index[edge.u], ctx.mst_local_index[edge.v]))
            mst_cost += edge.w;
    }
    for (int vertex : vertices)
        ctx.mst_local_index[vertex] = -1;
    return mst_cost;
}

// 统一处理 grow/merge 产生的候选。它应用 lb_2、incumbent 和开/闭状态 dominance，
// 必要时重开状态，最后把新版本推入优先队列。
bool AcceptCandidate(SolverContext& ctx,
                     int vertex,
                     int mask,
                     double cost,
                     double parent_priority,
                     const WitnessSpec& witness_spec)
{
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
    ++ctx.candidate_attempts;
#endif
    double priority = RawPriority(ctx, vertex, mask, cost);
    if (ctx.options.enforce_lb2_pathmax)
        priority = std::max(priority, parent_priority);
    if (priority >= ctx.best)
    {
        return false;
    }
    if (mask == ctx.full_mask)
    {
        if (cost < ctx.best)
        {
            ctx.best = cost;
        }
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
        ++ctx.accepted_candidates;
#endif
        return true;
    }

    StateEntry* existing = ctx.states->Find(vertex, mask);
    if (ctx.options.enforce_lb2_pathmax)
    {
        if (existing != nullptr && existing->closed)
        {
            return false;
        }
        if (existing != nullptr && existing->open &&
            !(priority < existing->open_priority))
            return false;
    }
    else
    {
        if (existing != nullptr && !(cost < existing->best_cost))
            return false;
        if (existing != nullptr && existing->closed)
        {
            existing->closed = false;
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
            ++ctx.reopened_states;
#endif
        }
    }

    const bool inserted = existing == nullptr;
    StateEntry& entry = existing == nullptr ? ctx.states->Get(vertex, mask) : *existing;
    if (!ctx.options.enforce_lb2_pathmax)
        entry.best_cost = cost;
    entry.open = true;
    entry.open_cost = cost;
    entry.open_priority = priority;
    ++entry.version;
    const int witness = MaterializeWitness(ctx, witness_spec);
    ctx.queue.push({vertex, mask, cost, priority, witness, entry.version});
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
    ++ctx.accepted_candidates;
    if (inserted && ctx.states->Size() >= ctx.next_state_report)
    {
        EmitPrunedProbe(ctx, "search_progress");
        if (ctx.next_state_report <=
            std::numeric_limits<std::size_t>::max() / 4)
            ctx.next_state_report *= 4;
    }
#else
    (void)inserted;
#endif
    return true;
}

// 弹出下一个与 StateStore 当前版本一致的标签，惰性跳过过期队列项。
bool PopCurrent(SolverContext& ctx, QueueNode& current)
{
    while (!ctx.queue.empty())
    {
        current = ctx.queue.top();
        if (current.priority >= ctx.best)
            return false;
        ctx.queue.pop();
        StateEntry* entry = ctx.states->Find(current.vertex, current.mask);
        if (entry == nullptr || !entry->open || entry->version != current.version ||
            entry->open_cost != current.cost || entry->open_priority != current.priority)
        {
            continue;
        }
        entry->open = false;
        if (ctx.options.enforce_lb2_pathmax && entry->closed)
        {
            continue;
        }
        if (!ctx.options.enforce_lb2_pathmax && current.cost != entry->best_cost)
        {
            continue;
        }
        entry->closed = true;
        entry->settled_cost = current.cost;
        entry->settled_witness = current.witness;
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
        ++ctx.settled_states;
#endif
        return true;
    }
    return false;
}

// PrunedDP++ 主循环：从所有终端 singleton 出发，交替执行边 grow、同根 merge 和 MST 上界收紧。
double Search(SolverContext& ctx)
{
    for (int group = 0; group < ctx.group_count; ++group)
    {
        const int mask = 1 << group;
        for (int vertex : ctx.query->groups[group])
        {
            AcceptCandidate(ctx,
                            vertex,
                            mask,
                            0.0,
                            0.0,
                            {WitnessKind::Seed, kNoWitness, kNoWitness, -1});
        }
    }

    QueueNode current;
    while (!ctx.queue.empty() && ctx.queue.top().priority < ctx.best &&
           PopCurrent(ctx, current))
    {
        if (current.cost >= ctx.best)
        {
            continue;
        }

        if (ctx.options.use_mst_upper_bound)
        {
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
            ++ctx.mst_calls;
#endif
            const double upper = BuildMstUpper(ctx, current);
            if (upper < ctx.best)
            {
                ctx.best = upper;
            }
        }

        const int complement = ctx.full_mask ^ current.mask;
        if (StateEntry* other = ctx.states->Find(current.vertex, complement);
            other != nullptr && other->closed)
        {
            AcceptCandidate(ctx,
                            current.vertex,
                            ctx.full_mask,
                            current.cost + other->settled_cost,
                            current.priority,
                            {WitnessKind::Merge,
                             current.witness,
                             other->settled_witness,
                             -1});
        }

        if (current.cost <= ctx.best * 0.5 + fp::kEps)
        {
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
            ctx.edge_attempts += ctx.graph->adj[current.vertex].size();
#endif
            for (const AdjEdge& edge : ctx.graph->adj[current.vertex])
            {
                AcceptCandidate(ctx,
                                edge.to,
                                current.mask,
                                current.cost + edge.w,
                                current.priority,
                                {WitnessKind::Edge,
                                 current.witness,
                                 kNoWitness,
                                 edge.edge_id});
            }

            for (int subset = complement; subset; subset = (subset - 1) & complement)
            {
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
                ++ctx.merge_attempts;
#endif
                StateEntry* other = ctx.states->Find(current.vertex, subset);
                if (other == nullptr || !other->closed)
                    continue;
                const double merged_cost = current.cost + other->settled_cost;
                if (merged_cost <= (2.0 / 3.0) * ctx.best + fp::kEps)
                {
                    AcceptCandidate(ctx,
                                    current.vertex,
                                    current.mask | subset,
                                    merged_cost,
                                    current.priority,
                                    {WitnessKind::Merge,
                                     current.witness,
                                     other->settled_witness,
                                     -1});
                }
            }
        }
    }
    return ctx.best;
}

// 根据查询组数建立 mask 空间、状态容器与 witness 复用缓冲。
void InitializeContext(SolverContext& ctx,
                       const Graph& graph,
                       const Query& query,
                       const PrunedDpOptions& options)
{
    ctx.graph = &graph;
    ctx.query = &query;
    ctx.options = options;
    ctx.probe = gst::ProbeDiagnosticsEnabled();
    ctx.n = graph.n;
    ctx.group_count = static_cast<int>(query.groups.size());
    ctx.subset_count = 1 << ctx.group_count;
    ctx.full_mask = ctx.subset_count - 1;
    ctx.states = std::make_unique<StateStore>(options.state_storage,
                                              ctx.n,
                                              ctx.subset_count);
    ctx.edge_epoch.assign(graph.m, 0);
    ctx.mst_local_index.assign(graph.n + 1, -1);
}

// 严格按“组距离 -> 组路径表 -> Algorithm 4 搜索”的顺序求得最优权重。
double SolveWeight(const Graph& graph,
                   const Query& query,
                   const PrunedDpOptions& options)
{
    SolverContext ctx;
    InitializeContext(ctx, graph, query, options);

    const auto group_start = ProbeClock::now();
    EmitPrunedProbe(ctx, "group_sssp_start");
    BuildGroupDistances(ctx);
    EmitPrunedProbe(ctx, "group_sssp_end", ProbeSecondsSince(group_start));

    const auto route_start = ProbeClock::now();
    EmitPrunedProbe(ctx, "route_dp_start");
    BuildAllPaths(ctx);
    EmitPrunedProbe(ctx, "route_dp_end", ProbeSecondsSince(route_start));

    const auto search_start = ProbeClock::now();
    EmitPrunedProbe(ctx, "search_start");
    const double answer = Search(ctx);
    EmitPrunedProbe(ctx, "search_end", ProbeSecondsSince(search_start));
    return answer;
}
}  // namespace

const char* StateStorageName(StateStorage storage)
{
    return storage == StateStorage::Hash ? "hash" : "dense";
}

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
{
    return SolveOneQuery(graph, query, PrunedDpOptions{});
}

SolveResult SolveOneQuery(const Graph& graph,
                          const Query& query,
                          const PrunedDpOptions& options)
{
    SolveResult result;
    const int group_count = static_cast<int>(query.groups.size());
    if (!group_count)
        return {0.0, true};
    if (group_count >= 31)
        throw std::runtime_error("PrunedDP currently supports group count <= 30.");
    if (!IsQueryFeasible(graph, query))
        return result;

    const double answer = SolveWeight(graph, query, options);
    if (answer >= fp::kInf / 4)
        return result;
    result.best_weight = answer;
    result.feasible = true;
    return result;
}

}  // namespace gst::methods::pruned_dp
