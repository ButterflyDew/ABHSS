#include "internal.h"
#include "path_growth_upper.h"

#include <deque>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace gst::methods::abhss::internal
{
namespace
{
using PairIndexTable = std::array<std::array<unsigned char, 256>, 17>;

/** @brief 预计算至多 16 个紧凑端点位置的对称上三角下标。 */
constexpr PairIndexTable BuildPairIndexTable()
{
    PairIndexTable table{};
    for (int count = 2; count <= 16; ++count)
        for (int left = 0; left < count; ++left)
            for (int right = left + 1; right < count; ++right)
            {
                const int index = left * (2 * count - left - 1) / 2 + right - left - 1;
                table[count][(left << 4) | right] = static_cast<unsigned char>(index);
                table[count][(right << 4) | left] = static_cast<unsigned char>(index);
            }
    return table;
}

constexpr PairIndexTable kPairIndex = BuildPairIndexTable();

/** @brief 构造每顶点最远组；已物化 rooted-entry 时同时构造最近组。 */
template <bool kBuildNearest>
void BuildExtremeGroupCache(Problem& p)
{
    p.farthest_group.resize(p.graph.n + 1);
    if constexpr (kBuildNearest)
        p.nearest_group.resize(p.graph.n + 1);
    for (int vertex = 1; vertex <= p.graph.n; ++vertex)
    {
        int farthest = 0;
        double farthest_value = p.group_distance.front()[vertex];
        int nearest = 0;
        double nearest_value = farthest_value;
        for (int group = 1; group < p.g; ++group)
        {
            const double value = p.group_distance[group][vertex];
            if (value > farthest_value)
            {
                farthest = group;
                farthest_value = value;
            }
            if constexpr (kBuildNearest)
            {
                if (value < nearest_value)
                {
                    nearest = group;
                    nearest_value = value;
                }
            }
        }
        p.farthest_group[vertex] = static_cast<unsigned char>(farthest);
        if constexpr (kBuildNearest)
            p.nearest_group[vertex] = static_cast<unsigned char>(nearest);
    }
}

/** @brief 统计 64 位 membership word 中的置位数，用于 sparse GroupRow rank。 */
int Popcount64(std::uint64_t bits)
{
    int count = 0;
    while (bits)
    {
        bits &= bits - 1;
        ++count;
    }
    return count;
}

/** @brief 将无向边及方向映射为稳定的有向 residual 弧编号。 */
int ArcIndex(int edge_id, int from, int to)
{
    return 2 * edge_id + (from < to ? 0 : 1);
}

/**
 * @brief 把一组原图边重根为无父指针环的真实见证树。
 * @param fallback_root 只用于保证空边集合仍至少含一个候选顶点。
 * @param anchor_group 根必须取自的永久锚组。
 *
 * 函数在选中边诱导子图上用严格改进 Dijkstra 建父指针。零权等距候选保留
 * 第一个合法父亲，防止已经 settled 的祖先被重新挂到后代而形成环；最后
 * 额外验证所有见证顶点均从锚根可达。
 */
WitnessTree BuildWitnessFromEdges(const Graph& graph,
                                  const Query& query,
                                  const std::vector<int>& edge_ids,
                                  int fallback_root,
                                  int anchor_group)
{
    std::vector<int> vertices{fallback_root};
    for (int edge_id : edge_ids)
    {
        vertices.push_back(graph.edges[edge_id].u);
        vertices.push_back(graph.edges[edge_id].v);
    }
    std::sort(vertices.begin(), vertices.end());
    vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());

    // lambda：把原图顶点映射到当前稀疏 witness 的连续局部编号。
    auto Index = [&](int vertex)
    {
        const auto it = std::lower_bound(vertices.begin(), vertices.end(), vertex);
        return it != vertices.end() && *it == vertex
                   ? static_cast<int>(it - vertices.begin())
                   : -1;
    };

    int root = -1;
    for (int terminal : query.groups[anchor_group])
    {
        root = Index(terminal);
        if (root >= 0)
            break;
    }
    if (root < 0)
        throw std::runtime_error("ABHSS witness misses the anchor group.");

    std::vector<std::vector<std::pair<int, double>>> adjacency(vertices.size());
    for (int edge_id : edge_ids)
    {
        const auto& edge = graph.edges[edge_id];
        const int u = Index(edge.u);
        const int v = Index(edge.v);
        adjacency[u].push_back({v, edge.w});
        adjacency[v].push_back({u, edge.w});
    }

    WitnessTree tree;
    tree.vertex = std::move(vertices);
    tree.parent.assign(tree.vertex.size(), -1);
    tree.parent_edge.assign(tree.vertex.size(), 0.0);
    std::vector<double> distance(tree.vertex.size(), fp::kInf);
    std::vector<unsigned char> settled(tree.vertex.size());
    Heap heap;
    distance[root] = 0.0;
    tree.parent[root] = static_cast<int>(tree.vertex.size());
    heap.push({0.0, root});
    int reached = 0;
    while (!heap.empty())
    {
        const auto [value, u] = heap.top();
        heap.pop();
        if (value != distance[u] || settled[u])
            continue;
        settled[u] = 1;
        ++reached;
        for (const auto& [v, weight] : adjacency[u])
        {
            const double next = value + weight;
            // 必须严格改进。零权边上的等距重挂可能把 settled 祖先指回后代，
            // 形成父指针环并重复弹出相同最终 key。堆和邻接顺序已经确定，
            // 因此等距候选保留第一个合法前驱即可。
            if (next < distance[v])
            {
                distance[v] = next;
                tree.parent[v] = u;
                tree.parent_edge[v] = weight;
                heap.push({next, v});
            }
        }
    }
    if (reached != static_cast<int>(tree.vertex.size()))
    {
        // 异常路径保留足够证据，用来区分选边确实不连通和等距 key 重复弹出。
        // 合法见证不会进入此分支，因此这些统计不影响正式计时。
        std::vector<unsigned char> seen(tree.vertex.size());
        std::vector<int> stack{root};
        seen[root] = 1;
        for (size_t i = 0; i < stack.size(); ++i)
            for (const auto& [v, unused_weight] : adjacency[stack[i]])
            {
                (void)unused_weight;
                if (!seen[v])
                {
                    seen[v] = 1;
                    stack.push_back(v);
                }
            }
        size_t zero_edges = 0;
        for (int edge_id : edge_ids)
            zero_edges += graph.edges[edge_id].w == 0.0;
        std::ostringstream message;
        message << "ABHSS witness traversal invariant failed: heap_pops=" << reached
                << ", vertices=" << tree.vertex.size()
                << ", graph_reachable=" << stack.size()
                << ", selected_edges=" << edge_ids.size()
                << ", zero_edges=" << zero_edges
                << ", root_parent=" << tree.parent[root] << '.';
        throw std::runtime_error(message.str());
    }
    return tree;
}
}  // namespace

/** @brief 对递增稀疏 row 做二分读取，缺失顶点统一返回 `fp::kInf`。 */
double RowValue(const Row& row, int vertex)
{
    const auto it = std::lower_bound(row.vertex.begin(), row.vertex.end(), vertex);
    if (it == row.vertex.end() || *it != vertex)
        return fp::kInf;
    return row.value[static_cast<size_t>(it - row.vertex.begin())];
}

/** @brief 屏蔽完整、有界 dense 和有界 ranked-bitmap 三种组距离布局。 */
double GroupRow::operator[](int v) const
{
    if (!bounded || dense)
        return value[v];
    const size_t word = static_cast<size_t>(v) >> 6;
    const int offset = v & 63;
    const std::uint64_t bit = std::uint64_t{1} << offset;
    if (word >= bits.size() || !(bits[word] & bit))
        return cutoff;
    const std::uint64_t lower = offset ? bits[word] & (bit - 1) : 0;
    const size_t index = rank[word] + Popcount64(lower);
    return value[index];
}

/** @brief 返回可作为真实 singleton 子树的距离；不让 cutoff 占位进入上界 DP。 */
double GroupRow::ExactValueOrInf(int v) const
{
    if (!bounded)
        return value[v];
    if (dense)
        return value[v] < cutoff ? value[v] : fp::kInf;
    const size_t word = static_cast<size_t>(v) >> 6;
    const int offset = v & 63;
    const std::uint64_t bit = std::uint64_t{1} << offset;
    if (word >= bits.size() || !(bits[word] & bit))
        return fp::kInf;
    const std::uint64_t lower = offset ? bits[word] & (bit - 1) : 0;
    return value[rank[word] + Popcount64(lower)];
}

/** @brief 返回可安全参加精确合并的顶点数，供交集驱动器比较工作量。 */
size_t GroupRow::ExactSize(int n) const
{
    return bounded ? exact_count : static_cast<size_t>(n);
}

/**
 * @brief 在零权连通分量覆盖超图上计算全局下界和可选 rooted 子集 future。
 *
 * 覆盖数为 1 时可直接证明零代价可行；否则结合最小正边权给出不超过任意
 * 可行树的连接代价。正权图使用加载期最小边权并只排序查询终端；含零权边
 * 时才扫描原边和建立并查集。代表根只用于构造上界；每顶点免费组掩码只在
 * 后续确有 rooted future 消费者时连同子集表一起物化，不影响全局下界。
 */
ComponentCover ComputeComponentCover(const Graph& graph, const Query& query, bool build_rooted_future)
{
    ComponentCover result;
    const int g = static_cast<int>(query.groups.size());
    const int full_mask = (1 << g) - 1;

    // 正权图是正式数据的主路径：加载器已经缓存全图最小边权，因此无需每条
    // 查询再次扫描 m 条边。只有确实含零权边时才建立并查集并寻找最小正权。
    const bool has_zero = graph.minimum_edge_weight == 0.0;
    double minimum_positive = graph.minimum_edge_weight > 0.0 ? graph.minimum_edge_weight : fp::kInf;
    std::vector<int> parent;
    if (has_zero)
    {
        parent.resize(graph.n + 1);
        std::iota(parent.begin(), parent.end(), 0);
    }
    // lambda：迭代寻找根并压缩整条访问路径，避免恶意零权边顺序形成深链后递归爆栈。
    auto Find = [&](int x)
    {
        int root = x;
        while (parent[root] != root)
            root = parent[root];
        while (parent[x] != x)
        {
            const int next = parent[x];
            parent[x] = root;
            x = next;
        }
        return root;
    };
    if (has_zero)
    {
        for (const auto& edge : graph.edges)
        {
            if (edge.w > 0.0)
            {
                minimum_positive = std::min(minimum_positive, edge.w);
                continue;
            }
            int u = Find(edge.u);
            int v = Find(edge.v);
            if (u != v)
                parent[v] = u;
        }
    }

    // 只为询问中实际出现的零权分量构造记录；正权图中每个顶点本身就是分量。
    // 临时命中记录占 O(F)，根所在分量的组掩码按顶点保存，额外占 O(n)。
    struct TerminalHit
    {
        int component = 0;
        int group = 0;
        int vertex = 0;
        int order = 0;
    };
    struct QueryComponent
    {
        int id = 0;
        int mask = 0;
        int representative = 0;
        int first_order = 0;
    };
    size_t terminal_count = 0;
    for (const auto& group : query.groups)
        terminal_count += group.size();
    std::vector<TerminalHit> hits;
    hits.reserve(terminal_count);
    int input_order = 0;
    for (int group = 0; group < g; ++group)
        for (int vertex : query.groups[group])
            hits.push_back({has_zero ? Find(vertex) : vertex, group, vertex, input_order++});
    // 先把相同物理分量排在一起；组内保留输入顺序以稳定选择代表顶点。
    std::sort(hits.begin(), hits.end(), [](const TerminalHit& left, const TerminalHit& right)
    {
        if (left.component != right.component)
            return left.component < right.component;
        return left.order < right.order;
    });

    std::vector<QueryComponent> components;
    components.reserve(terminal_count);
    for (size_t begin = 0; begin < hits.size();)
    {
        size_t end = begin;
        QueryComponent component{hits[begin].component, 0, hits[begin].vertex, hits[begin].order};
        while (end < hits.size() && hits[end].component == hits[begin].component)
        {
            component.mask |= 1 << hits[end].group;
            ++end;
        }
        components.push_back(component);
        begin = end;
    }
    // 恢复旧实现按查询首次触及分量的顺序，保持 set-cover 并列根完全稳定。
    std::sort(components.begin(), components.end(), [](const QueryComponent& left, const QueryComponent& right)
    {
        return left.first_order < right.first_order;
    });

    std::vector<int> superset(full_mask + 1, -1);
    for (int index = 0; index < static_cast<int>(components.size()); ++index)
        superset[components[index].mask] = index;
    for (int bit = 0; bit < g; ++bit)
        for (int mask = 0; mask <= full_mask; ++mask)
            if (!(mask & (1 << bit)) && superset[mask] < 0)
                superset[mask] = superset[mask | (1 << bit)];

    std::vector<int> cover(full_mask + 1, g + 1);
    std::vector<int> choice(full_mask + 1);
    cover[0] = 0;
    for (int mask = 1; mask <= full_mask; ++mask)
    {
        const int first = mask & -mask;
        for (int block = mask; block; block = (block - 1) & mask)
        {
            if (!(block & first) || superset[block] < 0)
                continue;
            if (1 + cover[mask ^ block] < cover[mask])
            {
                cover[mask] = 1 + cover[mask ^ block];
                choice[mask] = block;
            }
        }
    }

    result.cover_number = cover[full_mask];
    const bool materialize_rooted_future = build_rooted_future && result.cover_number > 1;
    if (materialize_rooted_future)
    {
        result.root_component_group_mask.assign(graph.n + 1, 0);
        for (const QueryComponent& component : components)
            result.root_component_group_mask[component.id] = static_cast<std::uint16_t>(component.mask);
        if (has_zero)
            for (int vertex = 1; vertex <= graph.n; ++vertex)
                result.root_component_group_mask[vertex] = result.root_component_group_mask[Find(vertex)];
    }
    if (materialize_rooted_future && graph.all_edges_unit_weight)
    {
        // 单位权图没有零边，每个 QueryComponent 对应一个真实候选顶点。先在
        // 查询候选诱导子图上求连通分量，再为每个可能根记录不经过额外
        // 非候选 Steiner 顶点能够到达的全部组。
        const int component_count = static_cast<int>(components.size());
        std::vector<int> induced_parent(component_count);
        std::iota(induced_parent.begin(), induced_parent.end(), 0);
        auto InducedFind = [&](int vertex)
        {
            int root = vertex;
            while (induced_parent[root] != root)
                root = induced_parent[root];
            while (induced_parent[vertex] != vertex)
            {
                const int next = induced_parent[vertex];
                induced_parent[vertex] = root;
                vertex = next;
            }
            return root;
        };
        std::unordered_map<int, int> induced_index;
        induced_index.reserve(static_cast<size_t>(component_count) * 2 + 1);
        for (int index = 0; index < component_count; ++index)
            induced_index.emplace(components[index].id, index);
        for (int index = 0; index < component_count; ++index)
            for (const AdjEdge& edge : graph.adj[components[index].id])
            {
                const auto other = induced_index.find(edge.to);
                if (other == induced_index.end())
                    continue;
                const int left = InducedFind(index);
                const int right = InducedFind(other->second);
                if (left != right)
                    induced_parent[right] = left;
            }

        std::vector<std::uint16_t> induced_group_mask(component_count);
        for (int index = 0; index < component_count; ++index)
            induced_group_mask[InducedFind(index)] |= static_cast<std::uint16_t>(components[index].mask);
        result.unit_terminal_induced_group_mask.assign(graph.n + 1, 0);
        for (int index = 0; index < component_count; ++index)
        {
            const int vertex = components[index].id;
            const std::uint16_t mask = induced_group_mask[InducedFind(index)];
            result.unit_terminal_induced_group_mask[vertex] |= mask;
            for (const AdjEdge& edge : graph.adj[vertex])
                if (!result.root_component_group_mask[edge.to])
                    result.unit_terminal_induced_group_mask[edge.to] |= mask;
        }

        // 每个非查询候选顶点贡献一个 block：只经过该顶点和候选诱导边即可
        // 接触的组。任意 rooted 完成树中，根免费 block 之外的每个终端候选
        // 分量都必须邻接树内某个非候选顶点；因此这些真实非候选顶点的 block
        // 覆盖全部剩余组。忽略 block 间连通性只会降低所需顶点数，不会高估。
        std::vector<unsigned char> block_superset(full_mask + 1);
        for (int vertex = 1; vertex <= graph.n; ++vertex)
            if (!result.root_component_group_mask[vertex])
                block_superset[result.unit_terminal_induced_group_mask[vertex]] = 1;
        for (int bit = 0; bit < g; ++bit)
            for (int mask = 0; mask <= full_mask; ++mask)
                if (!(mask & (1 << bit)))
                    block_superset[mask] |= block_superset[mask | (1 << bit)];

        result.unit_nonterminal_cover_number.assign(full_mask + 1, static_cast<unsigned char>(g + 1));
        result.unit_nonterminal_cover_number[0] = 0;
        for (int mask = 1; mask <= full_mask; ++mask)
        {
            const int first = mask & -mask;
            for (int block = mask; block; block = (block - 1) & mask)
            {
                if (!(block & first) || !block_superset[block])
                    continue;
                result.unit_nonterminal_cover_number[mask] = std::min(
                    result.unit_nonterminal_cover_number[mask],
                    static_cast<unsigned char>(1 + result.unit_nonterminal_cover_number[mask ^ block]));
            }
        }
    }
    for (int remaining = full_mask; remaining;)
    {
        const int block = choice[remaining];
        if (!block)
            break;
        result.roots.push_back(components[superset[block]].representative);
        remaining ^= block;
    }
    if (materialize_rooted_future)
    {
        result.subset_lower.assign(full_mask + 1, 0.0);
        result.rooted_uncovered_lower.assign(full_mask + 1, 0.0);
    }
    // fma 给出一次舍入的乘法残差：只在普通乘积确实向上舍入时退一 ulp，
    // 防止机器下界略高于实数值。单位权正式数据保持精确整数和原剪枝行为。
    auto SafePositiveMultiple = [&](int count)
    {
        if (count <= 0 || minimum_positive >= fp::kInf)
            return 0.0;
        const double product = count * minimum_positive;
        if (graph.all_edges_unit_weight || count == 1)
            return product;
        if (!std::isfinite(product))
            return std::nextafter(product, 0.0);
        const double error = std::fma(static_cast<double>(count), minimum_positive, -product);
        return error < 0.0 ? std::nextafter(product, 0.0) : product;
    };
    if (minimum_positive < fp::kInf)
    {
        result.lower = SafePositiveMultiple(std::max(0, result.cover_number - 1));
        if (materialize_rooted_future)
            for (int mask = 1; mask <= full_mask; ++mask)
            {
                result.subset_lower[mask] = SafePositiveMultiple(std::max(0, cover[mask] - 1));
                result.rooted_uncovered_lower[mask] = SafePositiveMultiple(cover[mask]);
            }
    }
    return result;
}

namespace
{
/**
 * @brief 为 bootstrapped-bounded realization 尝试构造 cutoff 与候选根。
 *
 * 这是该 realization 内部的数据表示启动器，不是调用者可见的 Base-only
 * 逻辑阶段。返回边并集由原图真实边组成，所以既可安全截断距离，也可作为
 * `DistanceRootInitialization::upper` 的初值。若非连通图中各组的规范最小
 * 终端没有落在同一个可行分量，本函数可返回无穷；随后以无穷 cutoff 执行
 * 的多源距离不会截断有限标签，共同 root-star 扫描会在调用者已验证存在的
 * 公共分量中取得有限上界。
 */
double BootstrapBoundedDistanceUpper(const Graph& graph,
                                     const Query& query,
                                     int& best_root)
{
    const int g = static_cast<int>(query.groups.size());
    const int full_mask = (1 << g) - 1;
    std::vector<int> group_mask(graph.n + 1);
    for (int group = 0; group < g; ++group)
        for (int vertex : query.groups[group])
            group_mask[vertex] |= 1 << group;

    std::vector<std::tuple<size_t, int, int>> order;
    for (int group = 0; group < g; ++group)
        order.push_back({query.groups[group].size(),
                         *std::min_element(query.groups[group].begin(),
                                           query.groups[group].end()),
                         group});
    std::sort(order.begin(), order.end());

    double best = fp::kInf;
    std::vector<int> roots;
    // 候选根共用同一批连续工作区。每轮顺序重置距离和边位图；parent_edge
    // 只会沿本轮已发现顶点读取，无需清零，从而避免反复申请大块内存。
    std::vector<double> distance(graph.n + 1, fp::kInf);
    std::vector<int> parent_edge(graph.n + 1, -1);
    std::vector<std::uint64_t> used((static_cast<size_t>(graph.m) + 63) / 64);
    for (const auto& [unused_size, root, unused_group] : order)
    {
        (void)unused_size;
        (void)unused_group;
        if (std::find(roots.begin(), roots.end(), root) != roots.end())
            continue;
        roots.push_back(root);

        std::fill(distance.begin(), distance.end(), fp::kInf);
        std::fill(used.begin(), used.end(), 0);
        Heap heap;
        distance[root] = 0.0;
        heap.push({0.0, root});
        int covered = 0;
        double cost = 0.0;
        while (!heap.empty() && covered != full_mask)
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != distance[vertex])
                continue;
            if (group_mask[vertex] & ~covered)
            {
                for (int v = vertex; v != root;)
                {
                    const int edge_id = parent_edge[v];
                    if (edge_id < 0)
                        throw std::runtime_error("ABHSS invalid SPT witness.");
                    const size_t word = static_cast<size_t>(edge_id) >> 6;
                    const std::uint64_t bit = std::uint64_t{1} << (edge_id & 63);
                    if (!(used[word] & bit))
                    {
                        used[word] |= bit;
                        cost += graph.edges[edge_id].w;
                    }
                    const auto& edge = graph.edges[edge_id];
                    v = edge.u == v ? edge.v : edge.u;
                }
            }
            covered |= group_mask[vertex];
            if (covered == full_mask || !(cost < best))
                break;
            for (const auto& edge : graph.adj[vertex])
            {
                const double next = value + edge.w;
                if (next < distance[edge.to])
                {
                    distance[edge.to] = next;
                    parent_edge[edge.to] = edge.edge_id;
                    heap.push({next, edge.to});
                }
            }
        }
        if (covered == full_mask && cost < best)
        {
            best = cost;
            best_root = root;
        }
        if (best == 0.0)
            break;
    }
    return best;
}

/**
 * @brief 为全部组执行多源 Dijkstra，并构造统一 GroupRow。
 *
 * bounded 模式只 settle 严格小于 cutoff 的顶点，随后比较 dense bounded 与
 * ranked-bitmap 的实际字节数；完整模式保存 n 个真实距离供 directed-cut。
 */
GroupTable BuildGroupDistances(const Graph& graph,
                               const Query& query,
                               bool bounded,
                               double cutoff)
{
    GroupTable table(query.groups.size());
    // 稀疏 bounded row 构造完成后只重置实际触及位置并复用 dense scratch；
    // 完整或 dense row 取得数组所有权后，下一组再申请必需的新数组。
    std::vector<double> distance(graph.n + 1, fp::kInf);
    std::vector<int> touched;
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
    {
        if (distance.empty())
            distance.assign(graph.n + 1, fp::kInf);
        touched.clear();
        if (graph.all_edges_unit_weight)
        {
            std::deque<int> queue;
            for (int terminal : query.groups[group])
            {
                if (distance[terminal] == 0.0)
                    continue;
                distance[terminal] = 0.0;
                touched.push_back(terminal);
                queue.push_back(terminal);
            }
            while (!queue.empty())
            {
                const int vertex = queue.front();
                queue.pop_front();
                const double next = distance[vertex] + 1.0;
                if (bounded && !(next < cutoff))
                    continue;
                for (const AdjEdge& edge : graph.adj[vertex])
                {
                    if (!(next < distance[edge.to]))
                        continue;
                    distance[edge.to] = next;
                    touched.push_back(edge.to);
                    queue.push_back(edge.to);
                }
            }
        }
        else
        {
            Heap heap;
            for (int terminal : query.groups[group])
            {
                if (distance[terminal] == 0.0)
                    continue;
                distance[terminal] = 0.0;
                touched.push_back(terminal);
                heap.push({0.0, terminal});
            }
            while (!heap.empty())
            {
                const auto [value, vertex] = heap.top();
                if (bounded && !(value < cutoff))
                    break;
                heap.pop();
                if (value != distance[vertex])
                    continue;
                for (const AdjEdge& edge : graph.adj[vertex])
                {
                    const double next = value + edge.w;
                    if ((bounded && !(next < cutoff)) || !(next < distance[edge.to]))
                        continue;
                    if (distance[edge.to] >= fp::kInf)
                        touched.push_back(edge.to);
                    distance[edge.to] = next;
                    heap.push({next, edge.to});
                }
            }
        }

        GroupRow& row = table[group];
        row.bounded = bounded;
        row.cutoff = cutoff;
        if (!bounded)
        {
            row.dense = true;
            row.exact_count = static_cast<size_t>(graph.n);
            row.value = std::move(distance);
            continue;
        }

        row.exact_count = touched.size();
        const size_t word_count = (static_cast<size_t>(graph.n + 1) + 63) / 64;
        const size_t dense_bytes = static_cast<size_t>(graph.n + 1) * sizeof(double);
        const size_t sparse_bytes = touched.size() * (sizeof(int) + sizeof(double)) +
                                    word_count * (sizeof(std::uint64_t) +
                                                  sizeof(std::uint32_t));
        if (dense_bytes <= sparse_bytes)
        {
            row.dense = true;
            for (double& value : distance)
                if (!(value < cutoff))
                    value = cutoff;
            row.value = std::move(distance);
            continue;
        }

        std::sort(touched.begin(), touched.end());
        row.vertex = std::move(touched);
        row.value.reserve(row.vertex.size());
        row.bits.assign(word_count, 0);
        for (int vertex : row.vertex)
        {
            row.value.push_back(distance[vertex]);
            row.bits[static_cast<size_t>(vertex) >> 6] |=
                std::uint64_t{1} << (vertex & 63);
        }
        row.rank.resize(word_count);
        std::uint32_t prefix = 0;
        for (size_t word = 0; word < word_count; ++word)
        {
            row.rank[word] = prefix;
            prefix += static_cast<std::uint32_t>(Popcount64(row.bits[word]));
        }
        for (int vertex : row.vertex)
            distance[vertex] = fp::kInf;
    }
    return table;
}

/** @brief 扫描所有共同根的组距离和，返回最小合法 star 上界并更新根。 */
double RootStarUpper(const GroupTable& distance, int n, int& root)
{
    if (distance.empty())
        return 0.0;
    double best = distance.front().bounded ? distance.front().cutoff : fp::kInf;
    const GroupRow* driver = &distance.front();
    for (const auto& row : distance)
        if (row.ExactSize(n) < driver->ExactSize(n))
            driver = &row;
    // lambda：由最小精确组表驱动，扫描同一顶点的全部组距离和。
    driver->ForEachExact(n, [&](int vertex, double)
    {
        double value = 0.0;
        for (const auto& row : distance)
            value += row[vertex];
        if (value < best)
        {
            best = value;
            root = vertex;
        }
    });
    return best;
}

}  // namespace

/**
 * @brief 用所选 realization 一次构造距离 oracle、候选根和初始真实上界。
 *
 * 外层控制流对所有配置相同；差异被限制在同一输出合同的内部 realization。
 * 这保持原有调用顺序：bounded 先尝试得到 cutoff 再构造组距离；若 bootstrap
 * 暂为无穷，该次多源搜索自然退化为不截断。complete 直接保存完整距离；
 * 两边最后都用共同根 star 扫描取得或改进根与有限上界。
 */
DistanceRootInitialization BuildDistanceRootInitialization(
    const Graph& graph,
    const Query& query,
    DistanceRootRealization realization)
{
    DistanceRootInitialization result;
    const bool bounded =
        realization == DistanceRootRealization::BootstrappedBounded;
    if (!bounded && realization != DistanceRootRealization::CompletePotential)
        throw std::logic_error("ABHSS unknown distance-root realization.");

    if (bounded)
        result.upper = BootstrapBoundedDistanceUpper(
            graph, query, result.root);
    result.group_distance = BuildGroupDistances(
        graph,
        query,
        bounded,
        bounded ? result.upper : fp::kInf);
    result.upper = std::min(
        result.upper,
        RootStarUpper(result.group_distance, graph.n, result.root));
    return result;
}

/** @brief 用 subset DP 构建每个组子集、每对固定端点的最短 Hamilton path。 */
void TourLowerBound::Build(const std::vector<std::vector<double>>& metric)
{
    group_count_ = static_cast<int>(metric.size());
    const int subset_count = 1 << group_count_;
    const int full_mask = subset_count - 1;
    std::vector<double> path(static_cast<size_t>(subset_count) * group_count_ * group_count_, fp::kInf);
    // lambda：把 (mask,start,last) 映射到连续 Hamilton-path DP 存储。
    auto Index = [&](int mask, int start, int last)
    {
        return (static_cast<size_t>(mask) * group_count_ + start) * group_count_ + last;
    };
    for (int start = 0; start < group_count_; ++start)
    {
        path[Index(1 << start, start, start)] = 0.0;
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (!(mask & (1 << start)))
                continue;
            for (int last = 0; last < group_count_; ++last)
            {
                const double current = path[Index(mask, start, last)];
                if (current >= fp::kInf)
                    continue;
                for (int bits = full_mask ^ mask; bits; bits &= bits - 1)
                {
                    const int next = FirstBit(bits & -bits);
                    double& target = path[Index(mask | (1 << next), start, next)];
                    target = std::min(target, current + metric[last][next]);
                }
            }
        }
    }

    mask_groups_.assign(static_cast<size_t>(subset_count) * group_count_, 255);
    mask_group_count_.resize(subset_count);
    endpoint_pair_offset_.resize(subset_count + 1);
    std::uint32_t pair_count = 0;
    for (int mask = 0; mask < subset_count; ++mask)
    {
        endpoint_pair_offset_[mask] = pair_count;
        const int count = __builtin_popcount(static_cast<unsigned>(mask));
        mask_group_count_[mask] = static_cast<unsigned char>(count);
        pair_count += static_cast<std::uint32_t>(count * (count - 1) / 2);
    }
    endpoint_pair_offset_[subset_count] = pair_count;
    endpoint_pair_path_.assign(pair_count, fp::kInf);
    endpoint_floor_by_position_.assign(static_cast<size_t>(subset_count) * group_count_, fp::kInf);
    endpoint_floor_left_.assign(subset_count, 255);
    endpoint_floor_value_.assign(subset_count, 0.0);
    for (int mask = 1; mask < subset_count; ++mask)
    {
        const size_t group_offset = static_cast<size_t>(mask) * group_count_;
        int count = 0;
        for (int bits = mask; bits; bits &= bits - 1)
            mask_groups_[group_offset + count++] = static_cast<unsigned char>(FirstBit(bits & -bits));
        if (!(mask & (mask - 1)))
            continue;
        size_t pair_index = endpoint_pair_offset_[mask];
        for (int left_position = 0; left_position < count; ++left_position)
        {
            const int left = mask_groups_[group_offset + left_position];
            for (int right_position = left_position + 1; right_position < count; ++right_position)
            {
                const int right = mask_groups_[group_offset + right_position];
                const double value = std::min(path[Index(mask, left, right)], path[Index(mask, right, left)]);
                endpoint_pair_path_[pair_index++] = value;
                endpoint_floor_by_position_[group_offset + left_position] = std::min(endpoint_floor_by_position_[group_offset + left_position], value);
                endpoint_floor_by_position_[group_offset + right_position] = std::min(endpoint_floor_by_position_[group_offset + right_position], value);
            }
        }
        int selected = -1;
        double selected_floor = -1.0;
        for (int position = 0; position < count; ++position)
        {
            const int group = mask_groups_[group_offset + position];
            const double floor = endpoint_floor_by_position_[group_offset + position];
            if (floor > selected_floor)
            {
                selected = group;
                selected_floor = floor;
            }
        }
        endpoint_floor_left_[mask] = static_cast<unsigned char>(selected);
        endpoint_floor_value_[mask] = selected_floor;
    }
}

/** @brief 把当前顶点接到预计算路径两端，返回剩余 mask 的 admissible tour 下界。 */
double TourLowerBound::At(int vertex, int mask, const GroupTable& distance) const
{
    if (!mask)
        return 0.0;
    if (!(mask & (mask - 1)))
    {
        const int group = FirstBit(mask);
        return unit_distance_ ? unit_distance_[static_cast<size_t>(vertex) * group_count_ + group] : distance[group][vertex];
    }

    const size_t group_offset = static_cast<size_t>(mask) * group_count_;
    const int count = mask_group_count_[mask];
    std::array<double, 16> rooted;
    std::array<double, 16> fixed;
    const std::uint16_t* compact = unit_distance_ ? unit_distance_ + static_cast<size_t>(vertex) * group_count_ : nullptr;
    const bool direct_distance = !distance.front().bounded;
    for (int position = 0; position < count; ++position)
    {
        const int group = mask_groups_[group_offset + position];
        rooted[position] = compact ? compact[group] : direct_distance ? distance[group].value[vertex] : distance[group][vertex];
        fixed[position] = fp::kInf;
    }
    size_t pair_index = endpoint_pair_offset_[mask];
    for (int left = 0; left < count; ++left)
    {
        for (int right = left + 1; right < count; ++right)
        {
            const double candidate = rooted[left] + endpoint_pair_path_[pair_index++] + rooted[right];
            fixed[left] = std::min(fixed[left], candidate);
            fixed[right] = std::min(fixed[right], candidate);
        }
    }
    double value = 0.0;
    for (int position = 0; position < count; ++position)
        value = std::max(value, fixed[position]);
    return value * 0.5;
}

/**
 * @brief 每个端点项完整后立即尝试拒绝；未拒绝时仍只扫描每个无序端点对一次。
 */
double TourLowerBound::AtUntilRejected(int vertex,
                                       int mask,
                                       const GroupTable& distance,
                                       double prefix,
                                       double incumbent,
                                       double lower,
                                       bool integral_completion,
                                       bool& exact) const
{
    // 单位权图的真实补全费用为整数，因此 ceil(lower) 才是实际可采纳下界。
    // prefix 与 incumbent 同样是精确整数，ceil(lower) < incumbent-prefix 等价于
    // lower <= incumbent-prefix-1；直接比较该半开阈值可避免热路径反复调用 ceil。
    const double integral_limit = incumbent - prefix - 1.0;
    auto CanStillImprove = [&](double candidate)
    {
        return integral_completion ? candidate <= integral_limit : prefix + candidate < incumbent;
    };

    if (!mask)
    {
        exact = true;
        return lower;
    }
    if (!(mask & (mask - 1)))
    {
        exact = true;
        const int group = FirstBit(mask);
        const double rooted = unit_distance_ ? unit_distance_[static_cast<size_t>(vertex) * group_count_ + group] : distance[group][vertex];
        return std::max(lower, rooted);
    }

    const size_t group_offset = static_cast<size_t>(mask) * group_count_;
    const int count = mask_group_count_[mask];
    std::array<double, 16> rooted;
    std::array<double, 16> fixed;
    const std::uint16_t* compact = unit_distance_ ? unit_distance_ + static_cast<size_t>(vertex) * group_count_ : nullptr;
    const bool direct_distance = !distance.front().bounded;
    for (int position = 0; position < count; ++position)
    {
        const int group = mask_groups_[group_offset + position];
        rooted[position] = compact ? compact[group] : direct_distance ? distance[group].value[vertex] : distance[group][vertex];
        fixed[position] = fp::kInf;
    }
    const size_t pair_offset = endpoint_pair_offset_[mask];

    int pivot = -1;
    double pivot_score = -1.0;
    for (int position = 0; position < count; ++position)
    {
        const double score = rooted[position] + endpoint_floor_by_position_[group_offset + position];
        if (score > pivot_score)
        {
            pivot = position;
            pivot_score = score;
        }
    }
    lower = std::max(lower, pivot_score * 0.5);
    if (!CanStillImprove(lower))
    {
        exact = false;
        return lower;
    }
    // 先完整求最强 pivot 的固定端点项；只触碰 count-1 个 pair，命中拒绝时
    // 不再为其余端点支付三角扫描。候选加法始终按较小位置在前，保持 At 的
    // 规范浮点求和顺序。
    for (int other = 0; other < count; ++other)
    {
        if (other == pivot)
            continue;
        const int left = std::min(pivot, other);
        const int right = std::max(pivot, other);
        const size_t pair_index = kPairIndex[count][(left << 4) | right];
        const double candidate = rooted[left] + endpoint_pair_path_[pair_offset + pair_index] + rooted[right];
        fixed[left] = std::min(fixed[left], candidate);
        fixed[right] = std::min(fixed[right], candidate);
    }
    double value = fixed[pivot];
    lower = std::max(lower, value * 0.5);
    if (!CanStillImprove(lower))
    {
        exact = count == 2;
        return lower;
    }

    // pivot 已覆盖的 pair 直接跳过；其余 pair 按与 At 相同的规范上三角顺序
    // 顺扫，因此未提前拒绝时返回逐候选一致的完整 tour 值。
    size_t pair_index = pair_offset;
    for (int left = 0; left < count; ++left)
    {
        for (int right = left + 1; right < count; ++right, ++pair_index)
        {
            if (left == pivot || right == pivot)
                continue;
            const double candidate = rooted[left] + endpoint_pair_path_[pair_index] + rooted[right];
            fixed[left] = std::min(fixed[left], candidate);
            fixed[right] = std::min(fixed[right], candidate);
        }
        if (left == pivot)
            continue;
        value = std::max(value, fixed[left]);
        lower = std::max(lower, value * 0.5);
        if (!CanStillImprove(lower))
        {
            const int higher_nonpivot = count - left - 1 - (pivot > left ? 1 : 0);
            exact = higher_nonpivot == 0;
            return lower;
        }
    }
    exact = true;
    return lower;
}

/** @brief 用最远根距离和最大终点自由路径值上包络完整 rooted tour。 */
double TourLowerBound::UpperEnvelope(int mask, double farthest) const
{
    if (!mask || !(mask & (mask - 1)))
        return farthest;
    double upper = farthest + endpoint_floor_value_[mask];
    upper += farthest;
    return upper * 0.5;
}

/** @brief 用预选起点组和终点自由路径值计算 A1 的常数时间下界。 */
double TourLowerBound::EndpointFloorAt(int vertex, int mask, const GroupTable& distance) const
{
    if (!mask)
        return 0.0;
    const int group = !(mask & (mask - 1)) ? FirstBit(mask) : endpoint_floor_left_[mask];
    const double rooted = unit_distance_ ? unit_distance_[static_cast<size_t>(vertex) * group_count_ + group] : distance[group][vertex];
    return !(mask & (mask - 1)) ? rooted : (rooted + endpoint_floor_value_[mask]) * 0.5;
}

/**
 * @brief 恢复候选根到各组的最短路，按 edge id 去重并选择最便宜边并集。
 *
 * 恢复只使用 GroupRow 中的精确 predecessor cone；最终边并集必须连通并覆盖
 * 所有组，故其去重边权是合法上界且可进一步重根为 witness。
 */
RootPathUnion BuildRootPathUnion(const Graph& graph,
                                 const Query& query,
                                 const GroupTable& distance,
                                 const std::vector<int>& roots)
{
    RootPathUnion best;
    std::vector<unsigned char> selected(graph.m);
    std::vector<unsigned char> terminal(graph.n + 1);
    std::vector<int> epoch(graph.n + 1);
    std::vector<int> next_edge(graph.n + 1);
    std::vector<int> path;
    std::vector<int> path_edges;
    std::vector<int> touched;
    int current_epoch = 0;

    for (int root : roots)
    {
        touched.clear();
        bool complete = true;
        for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
        {
            ++current_epoch;
            for (int v : query.groups[group])
                terminal[v] = 1;
            path.assign(1, root);
            path_edges.clear();
            epoch[root] = current_epoch;
            next_edge[root] = 0;
            while (!path.empty() && !terminal[path.back()])
            {
                const int vertex = path.back();
                const double current_distance = distance[group][vertex];
                bool advanced = false;
                while (next_edge[vertex] < static_cast<int>(graph.adj[vertex].size()))
                {
                    const auto& edge = graph.adj[vertex][next_edge[vertex]++];
                    if (epoch[edge.to] == current_epoch ||
                        !fp::Eq(edge.w + distance[group][edge.to], current_distance))
                        continue;
                    epoch[edge.to] = current_epoch;
                    next_edge[edge.to] = 0;
                    path.push_back(edge.to);
                    path_edges.push_back(edge.edge_id);
                    advanced = true;
                    break;
                }
                if (!advanced)
                {
                    path.pop_back();
                    if (!path_edges.empty())
                        path_edges.pop_back();
                }
            }
            for (int v : query.groups[group])
                terminal[v] = 0;
            if (path.empty())
            {
                complete = false;
                break;
            }
            for (int edge_id : path_edges)
                if (!selected[edge_id])
                {
                    selected[edge_id] = 1;
                    touched.push_back(edge_id);
                }
        }

        if (complete)
        {
            double upper = 0.0;
            for (int edge_id : touched)
                upper += graph.edges[edge_id].w;
            if (upper < best.upper)
            {
                best.upper = upper;
                best.root = root;
                best.edge_ids = touched;
            }
        }
        for (int edge_id : touched)
            selected[edge_id] = 0;
    }
    return best;
}

/** @brief 将 Base 的根路径边并集转为永久锚组根下的无环 witness。 */
WitnessTree BuildRootPathWitness(const Graph& graph,
                                 const Query& query,
                                 const RootPathUnion& paths,
                                 int anchor_group)
{
    return BuildWitnessFromEdges(
        graph, query, paths.edge_ids, paths.root, anchor_group);
}

/** @brief 将 directed-cut primal bitmap 展开为 edge id，再构造同格式 witness。 */
WitnessTree BuildDualWitness(const Graph& graph,
                             const Query& query,
                             const std::vector<std::uint64_t>& edge_words,
                             int root,
                             int anchor_group)
{
    std::vector<int> edges;
    for (const auto& edge : graph.edges)
        if ((edge_words[static_cast<size_t>(edge.id) >> 6] >> (edge.id & 63)) & 1ULL)
            edges.push_back(edge.id);
    return BuildWitnessFromEdges(graph, query, edges, root, anchor_group);
}

/**
 * @brief 在真实 witness 树上组合已 ready 的 ordinary rooted 子树并求可行上界。
 *
 * 树边代价只沿父子关系加入一次；缺失 ordinary 状态保持无穷，因此任何返回
 * 有限值都对应原图中的真实连通覆盖，而不会把 lower bound 当作解。
 */
double EvaluateWitnessTree(const WitnessTree& tree,
                           const Problem& p,
                           const std::vector<Row>& ordinary)
{
    if (tree.vertex.empty())
        return fp::kInf;
    struct Child
    {
        int id;
        double edge;
    };
    const int root = static_cast<int>(tree.vertex.size());
    std::vector<std::vector<Child>> children(root + 1);
    for (int node = 0; node < root; ++node)
        children[tree.parent[node]].push_back({node, tree.parent_edge[node]});
    std::vector<int> order{root};
    for (size_t i = 0; i < order.size(); ++i)
        for (const auto& child : children[order[i]])
            order.push_back(child.id);

    std::vector<std::vector<double>> dp(
        root + 1, std::vector<double>(p.subset_count, fp::kInf));
    std::vector<double> block(p.subset_count, fp::kInf);
    std::vector<double> local(p.subset_count, fp::kInf);
    std::vector<double> merged(p.subset_count, fp::kInf);
    for (auto it = order.rbegin(); it != order.rend(); ++it)
    {
        const int node = *it;
        dp[node][0] = 0.0;
        if (node != root)
        {
            const int vertex = tree.vertex[node];
            std::fill(block.begin(), block.end(), fp::kInf);
            for (int mask = 1; mask < p.subset_count; ++mask)
            {
                if (p.popcount[mask] == 1)
                    block[mask] = p.group_distance[p.bit_to_group[FirstBit(mask)]].ExactValueOrInf(vertex);
                else if (ordinary[mask].ready)
                    block[mask] = RowValue(ordinary[mask], vertex);
            }
            std::fill(local.begin(), local.end(), fp::kInf);
            local[0] = 0.0;
            for (int remaining = 1; remaining < p.subset_count; ++remaining)
            {
                const int first = remaining & -remaining;
                for (int part = remaining; part; part = (part - 1) & remaining)
                    if ((part & first) && block[part] < fp::kInf &&
                        local[remaining ^ part] < fp::kInf)
                        local[remaining] = std::min(
                            local[remaining],
                            local[remaining ^ part] + block[part]);
            }
            dp[node] = local;
        }
        for (const auto& child : children[node])
        {
            std::fill(merged.begin(), merged.end(), fp::kInf);
            for (int mask = 0; mask < p.subset_count; ++mask)
                for (int below = mask;; below = (below - 1) & mask)
                {
                    if (dp[node][mask ^ below] < fp::kInf &&
                        dp[child.id][below] < fp::kInf)
                        merged[mask] = std::min(
                            merged[mask],
                            dp[node][mask ^ below] + dp[child.id][below] +
                                (below ? child.edge : 0.0));
                    if (!below)
                        break;
                }
            dp[node].swap(merged);
        }
    }
    return dp[root][p.full_mask];
}

namespace
{
/** @brief signed 64-bit 饱和加法，避免理论工作量估计溢出。 */
long long SaturatingAdd(long long left, long long right)
{
    return right >= std::numeric_limits<long long>::max() - left ? std::numeric_limits<long long>::max() : left + right;
}

/** @brief signed 64-bit 饱和乘法，避免理论工作量估计溢出。 */
long long SaturatingMultiply(long long left, long long right)
{
    if (!left || !right)
        return 0;
    return left > std::numeric_limits<long long>::max() / right ? std::numeric_limits<long long>::max() : left * right;
}
}

long long EstimateCertificateSupportDpWork(size_t support_vertices, int nonanchor_count)
{
    if (!support_vertices || nonanchor_count < 0)
        return 0;
    long long binary_subsets = 1;
    long long ternary_subsets = 1;
    for (int bit = 0; bit < nonanchor_count; ++bit)
    {
        binary_subsets *= 2;
        ternary_subsets *= 3;
    }
    const long long vertices = static_cast<long long>(support_vertices);
    const long long partitions = (ternary_subsets - 2 * binary_subsets + 1) / 2;
    long long work = SaturatingMultiply(vertices, partitions);
    work = SaturatingAdd(work, SaturatingMultiply(binary_subsets - 1, SaturatingMultiply(vertices, vertices)));
    work = SaturatingAdd(work, SaturatingMultiply(vertices, SaturatingMultiply(vertices, vertices)));
    return SaturatingAdd(work, SaturatingMultiply(binary_subsets - 1, vertices));
}

void CertificateSupportDpCache::PublishOrdinary(int mask)
{
    if (mask > 0)
        pending_masks_.push_back(mask);
}

void CertificateSupportDpCache::Reset()
{
    full_rebuild_required_ = true;
    pending_masks_.clear();
}

void CertificateSupportDpCache::InitializeSupport()
{
    // 收集固定 certificate support 的去重顶点域；后续所有 DP 都使用这组局部编号。
    vertices_.reserve(2 * problem_.certificate_support_edges.size());
    for (int edge_id : problem_.certificate_support_edges)
    {
        vertices_.push_back(problem_.graph.edges[edge_id].u);
        vertices_.push_back(problem_.graph.edges[edge_id].v);
    }
    std::sort(vertices_.begin(), vertices_.end());
    vertices_.erase(std::unique(vertices_.begin(), vertices_.end()), vertices_.end());
    initialized_ = true;
    if (vertices_.empty())
        return;

    // lambda：把原图顶点映射到 certificate support 的连续局部编号。
    auto Index = [&](int vertex)
    {
        const auto it = std::lower_bound(vertices_.begin(), vertices_.end(), vertex);
        return it != vertices_.end() && *it == vertex ? static_cast<int>(it - vertices_.begin()) : -1;
    };

    // 在 support 原边上建立局部邻接矩阵，再用 Floyd 得到只经过真实 support
    // 路径的完备度量；证书不变时这张矩阵只构造一次。
    const int count = static_cast<int>(vertices_.size());
    metric_.assign(static_cast<size_t>(count) * count, fp::kInf);
    for (int i = 0; i < count; ++i)
        metric_[static_cast<size_t>(i) * count + i] = 0.0;
    for (int edge_id : problem_.certificate_support_edges)
    {
        const UndirectedEdge& edge = problem_.graph.edges[edge_id];
        const int u = Index(edge.u);
        const int v = Index(edge.v);
        metric_[static_cast<size_t>(u) * count + v] = std::min(metric_[static_cast<size_t>(u) * count + v], edge.w);
        metric_[static_cast<size_t>(v) * count + u] = std::min(metric_[static_cast<size_t>(v) * count + u], edge.w);
    }
    for (int middle = 0; middle < count; ++middle)
        for (int from = 0; from < count; ++from)
        {
            const double prefix = metric_[static_cast<size_t>(from) * count + middle];
            if (prefix >= fp::kInf)
                continue;
            for (int to = 0; to < count; ++to)
                metric_[static_cast<size_t>(from) * count + to] = std::min(metric_[static_cast<size_t>(from) * count + to], prefix + metric_[static_cast<size_t>(middle) * count + to]);
        }

    dp_.assign(static_cast<size_t>(problem_.subset_count) * count, fp::kInf);
    merged_.resize(count);
    closed_.resize(count);
    dirty_.assign(problem_.subset_count, 0);
}

void CertificateSupportDpCache::MarkAllMasksDirty()
{
    std::fill(dirty_.begin(), dirty_.end(), 0);
    for (int mask = 1; mask < problem_.subset_count; ++mask)
        dirty_[mask] = 1;
    pending_masks_.clear();
    full_rebuild_required_ = false;
}

bool CertificateSupportDpCache::IsPublishedOrdinaryMask(int mask) const
{
    return mask > 0 && mask < problem_.subset_count && problem_.popcount[mask] > 1 && problem_.ordinary[mask].ready;
}

void CertificateSupportDpCache::MarkSupersetsDirty(int mask)
{
    const int remaining = problem_.full_mask ^ mask;
    for (int extra = remaining;; extra = (extra - 1) & remaining)
    {
        dirty_[mask | extra] = 1;
        if (!extra)
            break;
    }
}

void CertificateSupportDpCache::RecomputeDirtyMasks()
{
    const int count = static_cast<int>(vertices_.size());
    // 按基数递增保证每个脏 mask 的真子集已经是当前版本；同基数继续保持旧
    // evaluator 的数值 mask 次序，使所有浮点加法与比较顺序不变。
    for (int size = 1; size <= problem_.nonanchor_count; ++size)
        for (int mask = 1; mask < problem_.subset_count; ++mask)
        {
            if (problem_.popcount[mask] != size || !dirty_[mask])
                continue;
            ++last_recomputed_mask_count_;
            const size_t offset = static_cast<size_t>(mask) * count;
            // singleton 读取精确组距离；其余 mask 直接读取 ordinary 唯一真值，
            // 未发布或被 refilter 为空的 row 由 RowValue 返回无穷。
            for (int i = 0; i < count; ++i)
            {
                if (size == 1)
                    merged_[i] = problem_.group_distance[problem_.bit_to_group[FirstBit(mask)]].value[vertices_[i]];
                else
                    merged_[i] = RowValue(problem_.ordinary[mask], vertices_[i]);
            }
            // 枚举与旧 evaluator 相同的规范同根拆分，固定最低 bit 消除左右对称。
            const int pivot = mask & -mask;
            for (int left = (mask - 1) & mask; left; left = (left - 1) & mask)
            {
                const int right = mask ^ left;
                if (!right || !(left & pivot))
                    continue;
                const size_t left_offset = static_cast<size_t>(left) * count;
                const size_t right_offset = static_cast<size_t>(right) * count;
                for (int i = 0; i < count; ++i)
                    merged_[i] = std::min(merged_[i], dp_[left_offset + i] + dp_[right_offset + i]);
            }
            // 把同根合并值沿固定 support metric 闭包到每个可选根。
            std::fill(closed_.begin(), closed_.end(), fp::kInf);
            for (int root = 0; root < count; ++root)
                for (int branch = 0; branch < count; ++branch)
                    closed_[root] = std::min(closed_[root], merged_[branch] + metric_[static_cast<size_t>(branch) * count + root]);
            std::copy(closed_.begin(), closed_.end(), dp_.begin() + offset);
            dirty_[mask] = 0;
        }
}

double CertificateSupportDpCache::Evaluate()
{
    // 第一次购买建立固定 support metric 与 DP 工作区；空 support 没有可行上界。
    if (!initialized_)
        InitializeSupport();
    if (vertices_.empty())
        return fp::kInf;
    last_evaluation_was_full_ = full_rebuild_required_;
    last_published_mask_count_ = pending_masks_.size();
    last_activated_mask_count_ = 0;
    last_recomputed_mask_count_ = 0;
    // 首次购买/refilter 后重算全部 mask；普通购买只失效新 direct seed 的超集。
    if (full_rebuild_required_)
        MarkAllMasksDirty();
    else
    {
        for (int mask : pending_masks_)
            if (IsPublishedOrdinaryMask(mask))
            {
                ++last_activated_mask_count_;
                MarkSupersetsDirty(mask);
            }
        pending_masks_.clear();
    }
    RecomputeDirtyMasks();

    // 只在 support 内真实锚组终端读取 full mask，保持旧 evaluator 的上界语义。
    double best = fp::kInf;
    const int count = static_cast<int>(vertices_.size());
    const size_t full_offset = static_cast<size_t>(problem_.full_mask) * count;
    for (int terminal : problem_.query.groups[problem_.anchor_group])
    {
        const auto it = std::lower_bound(vertices_.begin(), vertices_.end(), terminal);
        if (it != vertices_.end() && *it == terminal)
            best = std::min(best, dp_[full_offset + static_cast<size_t>(it - vertices_.begin())]);
    }
    return best;
}

double EvaluateCertificateSupport(const Problem& p)
{
    CertificateSupportDpCache cache(p);
    return cache.Evaluate();
}

/**
 * @brief 在 directed-cut primal 设施点上构造可行支撑度量并做小型 subset DP。
 *
 * residual 与 primal bitmap 只在增强预处理阶段有效；函数返回后调用者即可
 * 释放 residual，不让 O(m) 临时数组进入 ordinary 主阶段。
 */
double BuildPrimalFacilityUpper(const Problem& p,
                                const std::vector<double>& residual,
                                const std::vector<std::uint64_t>& edge_words)
{
    // lambda：读取 directed-cut primal 位图，判断原图边是否属于当前证书树。
    auto IsTreeEdge = [&](int edge_id)
    {
        return (edge_words[static_cast<size_t>(edge_id) >> 6] >> (edge_id & 63)) & 1ULL;
    };
    std::vector<int> facility{p.root};
    // primal bitmap 通常只有少量置位；直接枚举置位边，避免为一棵小树扫描全部 m 条原边。
    for (size_t word_index = 0; word_index < edge_words.size(); ++word_index)
    {
        std::uint64_t bits = edge_words[word_index];
        while (bits)
        {
            const int bit = __builtin_ctzll(bits);
            const size_t edge_id = (word_index << 6) + static_cast<size_t>(bit);
            if (edge_id >= p.graph.edges.size())
                break;
            const UndirectedEdge& edge = p.graph.edges[edge_id];
            facility.push_back(edge.u);
            facility.push_back(edge.v);
            bits &= bits - 1;
        }
    }
    std::sort(facility.begin(), facility.end());
    facility.erase(std::unique(facility.begin(), facility.end()), facility.end());
    const int count = static_cast<int>(facility.size());
    std::vector<int> facility_index(p.graph.n + 1, -1);
    for (int i = 0; i < count; ++i)
        facility_index[facility[i]] = i;

    std::vector<double> scratch(p.graph.n + 1, fp::kInf);
    std::vector<int> touched;
    // lambda：在 residual 度量中从一个设施点求到全部设施点的最短距离。
    auto DistancesFrom = [&](int source)
    {
        std::vector<double> answer(count, fp::kInf);
        Heap heap;
        scratch[source] = 0.0;
        touched.push_back(source);
        heap.push({0.0, source});
        int remaining = count;
        while (!heap.empty() && remaining)
        {
            const auto [value, vertex] = heap.top();
            heap.pop();
            if (value != scratch[vertex])
                continue;
            const int index = facility_index[vertex];
            if (index >= 0 && answer[index] >= fp::kInf)
            {
                answer[index] = value;
                --remaining;
            }
            for (const auto& edge : p.graph.adj[vertex])
            {
                const int arc = ArcIndex(edge.edge_id, vertex, edge.to);
                const double tolerance = 1e-10 * std::max(1.0, edge.w);
                if (residual[arc] > tolerance && !IsTreeEdge(edge.edge_id))
                    continue;
                const double next = value + edge.w;
                if (next >= scratch[edge.to])
                    continue;
                if (scratch[edge.to] >= fp::kInf)
                    touched.push_back(edge.to);
                scratch[edge.to] = next;
                heap.push({next, edge.to});
            }
        }
        for (int vertex : touched)
            scratch[vertex] = fp::kInf;
        touched.clear();
        return answer;
    };

    std::vector<std::vector<double>> metric(count);
    for (int i = 0; i < count; ++i)
        metric[i] = DistancesFrom(facility[i]);

    const int subset_count = 1 << p.g;
    const int full_mask = subset_count - 1;
    std::vector<int> popcount(subset_count);
    std::vector<std::vector<double>> row(subset_count);
    for (int mask = 1; mask < subset_count; ++mask)
        popcount[mask] = popcount[mask >> 1] + (mask & 1);
    for (int group = 0; group < p.g; ++group)
    {
        row[1 << group].resize(count);
        for (int i = 0; i < count; ++i)
            row[1 << group][i] = p.group_distance[group].value[facility[i]];
    }

    std::vector<double> merged(count);
    for (int size = 2; size <= p.half; ++size)
        for (int mask = 1; mask < subset_count; ++mask)
        {
            if (popcount[mask] != size)
                continue;
            std::fill(merged.begin(), merged.end(), fp::kInf);
            const int pivot = mask & -mask;
            for (int left = (mask - 1) & mask; left; left = (left - 1) & mask)
            {
                const int right = mask ^ left;
                if (!right || !(left & pivot))
                    continue;
                for (int i = 0; i < count; ++i)
                    merged[i] = std::min(merged[i], row[left][i] + row[right][i]);
            }
            row[mask].assign(count, fp::kInf);
            for (int root = 0; root < count; ++root)
                for (int branch = 0; branch < count; ++branch)
                    row[mask][root] = std::min(
                        row[mask][root], metric[root][branch] + merged[branch]);
        }

    double best = fp::kInf;
    if (!(p.g & 1))
    {
        for (int left = 1; left < full_mask; ++left)
        {
            const int right = full_mask ^ left;
            if (left >= right || popcount[left] != p.half)
                continue;
            for (int i = 0; i < count; ++i)
                best = std::min(best, row[left][i] + row[right][i]);
        }
    }
    else
    {
        for (int group = 0; group < p.g; ++group)
        {
            const int singleton = 1 << group;
            const int remaining = full_mask ^ singleton;
            for (int left = (remaining - 1) & remaining;
                 left;
                 left = (left - 1) & remaining)
            {
                const int right = remaining ^ left;
                if (left >= right || popcount[left] != p.half)
                    continue;
                for (int i = 0; i < count; ++i)
                    best = std::min(
                        best, row[singleton][i] + row[left][i] + row[right][i]);
            }
        }
    }
    return best;
}

/** @brief 为单位权图构造按顶点连续、按当前上界安全截止的组距离视图。 */
void BuildUnitGroupDistanceView(Problem& p)
{
    if (!p.graph.all_edges_unit_weight || p.UsesBoundedGroupDistances() || !std::isfinite(p.best) ||
        p.best != std::floor(p.best) || p.best <= 0.0 || p.best > std::numeric_limits<std::uint16_t>::max())
        return;
    const auto cap = static_cast<std::uint16_t>(p.best);
    p.unit_group_distance_by_vertex.resize((static_cast<size_t>(p.graph.n) + 1) * p.g);
    for (int vertex = 1; vertex <= p.graph.n; ++vertex)
    {
        std::uint16_t* output = p.unit_group_distance_by_vertex.data() + static_cast<size_t>(vertex) * p.g;
        for (int group = 0; group < p.g; ++group)
        {
            const double value = p.group_distance[group].value[vertex];
            if (value < p.best && (value < 0.0 || value != std::floor(value)))
            {
                p.unit_group_distance_by_vertex.clear();
                p.unit_group_distance_by_vertex.shrink_to_fit();
                return;
            }
            output[group] = value < p.best ? static_cast<std::uint16_t>(value) : cap;
        }
    }
    p.tour.SetUnitDistanceView(p.unit_group_distance_by_vertex);
}

/** @brief 构造从每个根到各组路径上最少非查询候选顶点数的 0-1 距离。 */
void BuildUnitNonterminalGroupDistanceView(Problem& p)
{
    if (!p.graph.all_edges_unit_weight || !std::isfinite(p.best) || p.best != std::floor(p.best) ||
        p.best <= 0.0 || p.best >= std::numeric_limits<std::uint16_t>::max() || p.component_cover.root_component_group_mask.empty())
        return;
    const auto cap = static_cast<std::uint16_t>(p.best + 1.0);
    p.unit_nonterminal_group_distance_by_vertex.assign((static_cast<size_t>(p.graph.n) + 1) * p.g, cap);
    p.unit_nonterminal_first_group.assign(p.graph.n + 1, 255);
    p.unit_nonterminal_second_group.assign(p.graph.n + 1, 255);
    std::vector<std::uint16_t> distance(p.graph.n + 1, cap);
    std::vector<unsigned char> settled(p.graph.n + 1);
    for (int group = 0; group < p.g; ++group)
    {
        std::fill(distance.begin(), distance.end(), cap);
        std::fill(settled.begin(), settled.end(), 0);
        std::deque<int> queue;
        for (int terminal : p.query.groups[group])
        {
            if (distance[terminal] == 0)
                continue;
            distance[terminal] = 0;
            queue.push_back(terminal);
        }
        while (!queue.empty())
        {
            const int vertex = queue.front();
            queue.pop_front();
            if (settled[vertex])
                continue;
            settled[vertex] = 1;
            for (const AdjEdge& edge : p.graph.adj[vertex])
            {
                const bool next_is_candidate = p.component_cover.root_component_group_mask[edge.to] != 0;
                const auto next = static_cast<std::uint16_t>(distance[vertex] + !next_is_candidate);
                if (next >= cap || next >= distance[edge.to])
                    continue;
                distance[edge.to] = next;
                if (next_is_candidate)
                    queue.push_front(edge.to);
                else
                    queue.push_back(edge.to);
            }
        }
        for (int vertex = 1; vertex <= p.graph.n; ++vertex)
        {
            const size_t offset = static_cast<size_t>(vertex) * p.g;
            p.unit_nonterminal_group_distance_by_vertex[offset + group] = distance[vertex];
            unsigned char& first = p.unit_nonterminal_first_group[vertex];
            unsigned char& second = p.unit_nonterminal_second_group[vertex];
            if (first == 255 || distance[vertex] > p.unit_nonterminal_group_distance_by_vertex[offset + first])
            {
                second = first;
                first = static_cast<unsigned char>(group);
            }
            else if (second == 255 || distance[vertex] > p.unit_nonterminal_group_distance_by_vertex[offset + second])
                second = static_cast<unsigned char>(group);
        }
    }
}

/** @brief 先检查全局最远组，未命中时按当前距离 realization 扫描剩余组。 */
double FarthestRemaining(const Problem& p, int vertex, int original_mask)
{
    if (!original_mask)
        return 0.0;
    const int cached = p.farthest_group[vertex];
    if (!p.unit_group_distance_by_vertex.empty())
    {
        const std::uint16_t* distance = p.unit_group_distance_by_vertex.data() + static_cast<size_t>(vertex) * p.g;
        if (original_mask & (1 << cached))
            return distance[cached];
        std::uint16_t value = 0;
        for (int bits = original_mask; bits; bits &= bits - 1)
            value = std::max(value, distance[FirstBit(bits & -bits)]);
        return value;
    }
    if (original_mask & (1 << cached))
        return p.group_distance[cached][vertex];
    double value = 0.0;
    if (!p.UsesBoundedGroupDistances())
    {
        for (int bits = original_mask; bits; bits &= bits - 1)
            value = std::max(value, p.group_distance[FirstBit(bits & -bits)].value[vertex]);
    }
    else
    {
        for (int bits = original_mask; bits; bits &= bits - 1)
            value = std::max(value, p.group_distance[FirstBit(bits & -bits)][vertex]);
    }
    return value;
}

/** @brief 先检查全局最近组，未命中时扫描剩余组；距离 realization 可向下但不可向上。 */
double NearestRemaining(const Problem& p, int vertex, int original_mask)
{
    assert(p.graph.all_edges_unit_weight && !p.nearest_group.empty() && original_mask);
    const int cached = p.nearest_group[vertex];
    if (!p.unit_group_distance_by_vertex.empty())
    {
        const std::uint16_t* distance = p.unit_group_distance_by_vertex.data() + static_cast<size_t>(vertex) * p.g;
        if (original_mask & (1 << cached))
            return distance[cached];
        std::uint16_t value = std::numeric_limits<std::uint16_t>::max();
        for (int bits = original_mask; bits; bits &= bits - 1)
            value = std::min(value, distance[FirstBit(bits & -bits)]);
        return value;
    }
    if (original_mask & (1 << cached))
        return p.group_distance[cached][vertex];
    double value = fp::kInf;
    for (int bits = original_mask; bits; bits &= bits - 1)
        value = std::min(value, p.group_distance[FirstBit(bits & -bits)][vertex]);
    return value;
}

/** @brief 返回到剩余组任一路径不可避免的最多非查询候选顶点数，不计根。 */
double FarthestRequiredNonterminal(const Problem& p, int vertex, int original_mask)
{
    if (p.unit_nonterminal_group_distance_by_vertex.empty() || !original_mask)
        return 0.0;
    const std::uint16_t* distance = p.unit_nonterminal_group_distance_by_vertex.data() + static_cast<size_t>(vertex) * p.g;
    std::uint16_t value = 0;
    const int first = p.unit_nonterminal_first_group[vertex];
    const int second = p.unit_nonterminal_second_group[vertex];
    if (original_mask & (1 << first))
        value = distance[first];
    else if (second != 255 && (original_mask & (1 << second)))
        value = distance[second];
    else
        for (int bits = original_mask; bits; bits &= bits - 1)
            value = std::max(value, distance[FirstBit(bits & -bits)]);
    if (!p.component_cover.root_component_group_mask[vertex] && value)
        --value;
    return value;
}

/** @brief 返回 strict-unit DirectedCut 的 rooted-entry 安全证书；非适用配置无额外下界。 */
double RootedEntryCoverLower(const Problem& p, int vertex, int original_mask)
{
    if (!p.UsesDirectedCut() || !original_mask || !p.graph.all_edges_unit_weight)
        return 0.0;
    const int uncovered = original_mask & ~static_cast<int>(p.component_cover.root_component_group_mask[vertex]);
    if (!uncovered)
        return p.component_cover.SubsetLower(original_mask);
    return p.component_cover.UnitRootedEntryLower(original_mask, vertex, NearestRemaining(p, vertex, uncovered), FarthestRequiredNonterminal(p, vertex, uncovered));
}

/** @brief 从零开始计算 farthest/tour/可选 directed-cut 的统一 future 下界。 */
double FutureBound(const Problem& p, int vertex, int original_mask)
{
    return FutureBound(
        p, vertex, original_mask, FarthestRemaining(p, vertex, original_mask));
}

/** @brief 复用已通过廉价检查的 farthest，再计算较贵 tour 与可选 dual。 */
double PrimaryFutureBound(const Problem& p,
                          int vertex,
                          int original_mask,
                          double farthest)
{
    double value = std::max(farthest, RootedEntryCoverLower(p, vertex, original_mask));
    if (value < p.tour.UpperEnvelope(original_mask, farthest))
        value = std::max(value, p.tour.At(vertex, original_mask, p.group_distance));
    if (p.UsesDirectedCut())
        value = std::max(value, p.dual.At(vertex, original_mask));
    return CloseCompletionLower(p, value);
}

/** @brief 在主 future 上再取独立反序 packing 的最大值。 */
double SymmetricFutureBound(const Problem& p,
                            int vertex,
                            int original_mask,
                            double farthest)
{
    assert(p.symmetric_dual_ready);
    return CloseCompletionLower(p, std::max(PrimaryFutureBound(p, vertex, original_mask, farthest), SymmetricPackingBound(p, vertex, original_mask)));
}

/** @brief 读取已购买反序 packing 对指定 rooted completion 的安全下界。 */
double SymmetricPackingBound(const Problem& p, int vertex, int original_mask)
{
    assert(p.symmetric_dual_ready);
    return p.symmetric_dual.At(vertex, original_mask);
}

/** @brief 按反序 packing 是否已购买选择主 future 或两套 packing 的最大值。 */
double FutureBound(const Problem& p,
                   int vertex,
                   int original_mask,
                   double farthest)
{
    return p.symmetric_dual_ready ? SymmetricFutureBound(p, vertex, original_mask, farthest) : PrimaryFutureBound(p, vertex, original_mask, farthest);
}

/** @brief singleton 天然可用；多组状态必须显式 `ready`，空 mask 不算 ordinary。 */
bool OrdinaryAvailable(const Problem& p, int mask)
{
    return mask && (p.popcount[mask] == 1 || p.ordinary[mask].ready);
}

/**
 * @brief 建立一次查询的全部公共数据，并按增强开关增加 stronger certificates。
 *
 * 执行顺序固定为分量下界、组距离/根上界、锚映射、tour，再选择 Base 根路径
 * witness 或 DirectedCut dual/primal。任意阶段上下界闭合都安全提前返回。
 */
bool PrepareProblem(Problem& p)
{
    p.g = static_cast<int>(p.query.groups.size());
    p.half = p.g / 2;
    // 至多三组由共同 root-star 恒等式直接闭包，不存在 rooted future 消费者；
    // 这里只按证明得出的状态生命周期省去 O(n) 根分量掩码，不是经验分段。
    p.component_cover = ComputeComponentCover(p.graph, p.query, p.g > 3 && p.UsesDirectedCut() && p.graph.all_edges_unit_weight);
    if (p.component_cover.cover_number == 1)
    {
        p.best = 0.0;
        return true;
    }

    // 对至多三个组，最优 GST 等于 min_v sum_i d(v,K_i)：任意三终端树在
    // 分叉点处分解成三条路径给出下界，反向取三条最短路并集给出上界。
    // 全部配置先执行同一个 bounded root-star 包并直接返回；既然不会进入
    // directed-cut，下游需要的 complete-potential 表没有理由在基例中构造。
    if (p.g <= 3)
    {
        const DistanceRootInitialization exact = BuildDistanceRootInitialization(
            p.graph, p.query, DistanceRootRealization::BootstrappedBounded);
        p.best = exact.upper;
        return true;
    }

    // 调用者始终执行同一个距离—根初始化职责。两种 realization 都一次返回
    // GroupRow oracle、候选根和真实上界；bounded cutoff 的 SPT 启动器已经
    // 封装在相应 realization 内，不再形成 Base-only 的外层控制流。
    DistanceRootInitialization distance_root =
        BuildDistanceRootInitialization(
            p.graph,
            p.query,
            DescribeConfiguration(p.options).distance_root);
    p.group_distance = std::move(distance_root.group_distance);
    p.root = distance_root.root;
    p.best = std::min(p.best, distance_root.upper);
    // 上下界闭合是精确性终止条件，不能用 epsilon 把一个很小但为正的 gap
    // 当作 0。容差只允许用于恢复真实路径；恢复失败最多削弱上界，不会证明
    // 最优性。这里使用解析后 double 值的原始顺序比较。
    if (p.best == 0.0 || p.best <= p.component_cover.lower)
        return true;

    std::vector<int> roots{p.root};
    for (int root : p.component_cover.roots)
        if (std::find(roots.begin(), roots.end(), root) == roots.end())
            roots.push_back(root);
    p.root_path_union = BuildRootPathUnion(
        p.graph, p.query, p.group_distance, roots);
    p.best = std::min(p.best, p.root_path_union.upper);
    if (p.best <= p.component_cover.lower)
        return true;

    p.anchor_group = 0;
    for (int group = 1; group < p.g; ++group)
        if (p.group_distance[group][p.root] >
            p.group_distance[p.anchor_group][p.root])
            p.anchor_group = group;

    p.bit_to_group.clear();
    for (int group = 0; group < p.g; ++group)
        if (group != p.anchor_group)
            p.bit_to_group.push_back(group);
    p.nonanchor_count = p.g - 1;
    p.subset_count = 1 << p.nonanchor_count;
    p.full_mask = p.subset_count - 1;
    p.original_full_mask = (1 << p.g) - 1;
    p.anchor_bit = 1 << p.anchor_group;
    p.nonanchor_original_mask = p.original_full_mask ^ p.anchor_bit;
    p.popcount.assign(p.subset_count, 0);
    p.original_mask.assign(p.subset_count, 0);
    for (int mask = 1; mask < p.subset_count; ++mask)
    {
        p.popcount[mask] = p.popcount[mask >> 1] + (mask & 1);
        const int bit = FirstBit(mask & -mask);
        p.original_mask[mask] =
            p.original_mask[mask ^ (1 << bit)] | (1 << p.bit_to_group[bit]);
    }

    if (!p.component_cover.root_component_group_mask.empty())
        BuildExtremeGroupCache<true>(p);
    else
        BuildExtremeGroupCache<false>(p);

    std::vector<std::vector<double>> metric(
        p.g, std::vector<double>(p.g, fp::kInf));
    for (int left = 0; left < p.g; ++left)
        for (int right = 0; right < p.g; ++right)
            for (int vertex : p.query.groups[right])
                metric[left][right] = std::min(metric[left][right], p.group_distance[left][vertex]);
    p.tour.Build(metric);

    if (!p.UsesDirectedCut())
    {
        p.witness_tree = BuildRootPathWitness(
            p.graph, p.query, p.root_path_union, p.anchor_group);
    }
    else
    {
        std::vector<std::vector<double>> dense(p.g);
        for (int group = 0; group < p.g; ++group)
            dense[group].swap(p.group_distance[group].value);
        p.dual.BuildKeepingResidualChangedArcsWithPrimalEdges(
            p.graph, p.query, dense, p.root);
        for (int group = 0; group < p.g; ++group)
            dense[group].swap(p.group_distance[group].value);
        p.best = std::min(p.best, p.dual.PrimalUpper());
        p.best = std::min(
            p.best,
            BuildPrimalFacilityUpper(
                p, p.dual.Residual(), p.dual.PrimalEdgeWords()));
        p.witness_tree = BuildDualWitness(
            p.graph,
            p.query,
            p.dual.PrimalEdgeWords(),
            p.root,
            p.anchor_group);
        p.dual.ReleaseResidual();
    }

    // 当前 witness 已经与公共下界闭合时，后续任何上界生成都不可能改变答案。
    // 该精确 gate 对两种 realization 使用同一条件，直接消除已解查询的三元固定成本。
    if (p.best <= p.component_cover.lower)
        return true;

    // 两种配置在各自 witness 构造后共同执行同一真实路径生长上界。
    const PathGrowthUpper path_growth = BuildTripleSeededPathGrowthUpper(p.graph, p.query, p.group_distance, p.best);
    if (path_growth.upper < p.best)
        p.best = path_growth.upper;
    if (p.best <= p.component_cover.lower)
        return true;

    p.ordinary.assign(p.subset_count, {});
    p.ordinary_minimum.assign(p.subset_count, fp::kInf);
    BuildUnitGroupDistanceView(p);

    // 到此只完成当前配置的 witness 构造，不无条件执行树 DP。Base 的
    // root-path tree 与 DirectedCut 的 dual-primal tree 随后都交给跨 A1/D 的
    // 同一个 rent-or-buy 调度器，并从 rent=0 开始按同一 buy 公式购买。
    return false;
}

bool RefreshPurchasedPathGrowthCertificate(Problem& p)
{
    const PathGrowthUpper path_growth = BuildQuadSeededPathGrowthUpper(p.graph, p.query, p.group_distance, p.best);
    if (!(path_growth.upper < p.best))
        return false;
    p.certificate_support_edges = path_growth.edge_ids;
    const std::vector<std::uint64_t>& primal_words = p.dual.PrimalEdgeWords();
    for (size_t word_index = 0; word_index < primal_words.size(); ++word_index)
    {
        std::uint64_t bits = primal_words[word_index];
        while (bits)
        {
            const int bit = __builtin_ctzll(bits);
            const size_t edge_id = (word_index << 6) + static_cast<size_t>(bit);
            if (edge_id < p.graph.edges.size())
                p.certificate_support_edges.push_back(static_cast<int>(edge_id));
            bits &= bits - 1;
        }
    }
    std::sort(p.certificate_support_edges.begin(), p.certificate_support_edges.end());
    p.certificate_support_edges.erase(std::unique(p.certificate_support_edges.begin(), p.certificate_support_edges.end()), p.certificate_support_edges.end());
    std::vector<int> support_vertices;
    support_vertices.reserve(2 * p.certificate_support_edges.size());
    for (int edge_id : p.certificate_support_edges)
    {
        support_vertices.push_back(p.graph.edges[edge_id].u);
        support_vertices.push_back(p.graph.edges[edge_id].v);
    }
    std::sort(support_vertices.begin(), support_vertices.end());
    support_vertices.erase(std::unique(support_vertices.begin(), support_vertices.end()), support_vertices.end());
    p.certificate_support_vertex_count = support_vertices.size();
    p.best = path_growth.upper;
    return true;
}

}  // namespace gst::methods::abhss::internal
