#include "internal.h"

#include <cassert>
#include <numeric>
#include <tuple>

namespace gst::methods::abhss::internal
{
namespace
{
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
 * 第一个父亲，防止已经 settled 的祖先被重新挂到后代而形成环。
 */
WitnessTree BuildWitnessFromEdges(const Graph& graph, const Query& query, const std::vector<int>& edge_ids, int fallback_root, int anchor_group)
{
    // 先把真实边端点压成局部连续编号，空边集仍保留候选根。
    std::vector<int> vertices{fallback_root};
    for (int edge_id : edge_ids)
    {
        vertices.push_back(graph.edges[edge_id].u);
        vertices.push_back(graph.edges[edge_id].v);
    }
    std::sort(vertices.begin(), vertices.end());
    vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());

    // 将原图顶点号映射到局部顶点表，不在表中时返回 -1。
    auto Index = [&](int vertex)
    {
        const auto it = std::lower_bound(vertices.begin(), vertices.end(), vertex);
        return it != vertices.end() && *it == vertex ? static_cast<int>(it - vertices.begin()) : -1;
    };

    int root = -1;
    for (int terminal : query.groups[anchor_group])
    {
        root = Index(terminal);
        if (root >= 0)
            break;
    }
    assert(root >= 0);

    // 在局部编号上构造 witness 诱导子图，边权仍取自原图。
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
    // 从锚终端严格重根；每个 settled 顶点的父指针此后不再改变。
    while (!heap.empty())
    {
        const auto [value, u] = heap.top();
        heap.pop();
        if (value != distance[u] || settled[u])
            continue;
        settled[u] = 1;
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
    return tree;
}
} // namespace

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

/** @brief 判断读取值是否为真实最短路，而不是有界表的 cutoff 证书。 */
bool GroupRow::IsExact(int v) const
{
    if (!bounded)
        return true;
    if (dense)
        return value[v] < cutoff;
    const size_t word = static_cast<size_t>(v) >> 6;
    return word < bits.size() && ((bits[word] >> (v & 63)) & std::uint64_t{1});
}

/** @brief 返回可安全参加精确合并的顶点数，供交集驱动器比较工作量。 */
size_t GroupRow::ExactSize(int n) const
{
    return bounded ? exact_count : static_cast<size_t>(n);
}

/**
 * @brief 在零权连通分量覆盖超图上计算全局下界和候选代表根。
 *
 * 覆盖数为 1 时可直接证明零代价可行；否则结合最小正边权给出不超过任意
 * 可行树的连接代价。返回根只用于构造上界，不影响下界有效性。
 */
ComponentCover ComputeComponentCover(const Graph& graph, const Query& query)
{
    ComponentCover result;
    const int g = static_cast<int>(query.groups.size());
    const int full_mask = (1 << g) - 1;

    // 一次扫描得到是否需要零权压缩，以及连接免费分量的最小正边代价。
    bool has_zero = false;
    double minimum_positive = fp::kInf;
    for (const auto& edge : graph.edges)
    {
        has_zero = has_zero || edge.w == 0.0;
        if (edge.w > 0.0)
            minimum_positive = std::min(minimum_positive, edge.w);
    }

    std::vector<int> parent(graph.n + 1);
    std::iota(parent.begin(), parent.end(), 0);
    // 并查集递归查找零权连通分量的根，同时做路径压缩。
    auto Find = [&](auto&& self, int x) -> int { return parent[x] == x ? x : parent[x] = self(self, parent[x]); };
    if (has_zero)
    {
        for (const auto& edge : graph.edges)
        {
            if (edge.w != 0.0)
                continue;
            int u = Find(Find, edge.u);
            int v = Find(Find, edge.v);
            if (u != v)
                parent[v] = u;
        }
        for (int v = 1; v <= graph.n; ++v)
            parent[v] = Find(Find, v);
    }

    // 把每个查询终端所在的零权分量映射为它能免费覆盖的组 mask。
    std::vector<int> component_mask(graph.n + 1);
    std::vector<int> representative(graph.n + 1);
    std::vector<int> components;
    for (int group = 0; group < g; ++group)
    {
        for (int vertex : query.groups[group])
        {
            const int component = has_zero ? parent[vertex] : vertex;
            if (!component_mask[component])
            {
                representative[component] = vertex;
                components.push_back(component);
            }
            component_mask[component] |= 1 << group;
        }
    }

    // 子集 zeta 传播记录能覆盖给定 block 的任一实际零权分量。
    std::vector<int> superset(full_mask + 1, -1);
    for (int component : components)
        superset[component_mask[component]] = component;
    for (int bit = 0; bit < g; ++bit)
        for (int mask = 0; mask <= full_mask; ++mask)
            if (!(mask & (1 << bit)) && superset[mask] < 0)
                superset[mask] = superset[mask | (1 << bit)];

    // 固定最低位消除 set-cover 拆分重复，并保存恢复代表根的选择。
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

    // 恢复一组代表根，并把 cover 数转换为全局可采纳下界。
    result.cover_number = cover[full_mask];
    for (int remaining = full_mask; remaining;)
    {
        const int block = choice[remaining];
        if (!block)
            break;
        result.roots.push_back(representative[superset[block]]);
        remaining ^= block;
    }
    if (minimum_positive < fp::kInf)
        result.lower = std::max(0, result.cover_number - 1) * minimum_positive;
    return result;
}

namespace
{
/**
 * @brief 为有界距离表示构造 cutoff 与候选根。
 *
 * 该步骤封装在统一距离—根职责内部，不是调用者额外调度的算法阶段。返回
 * 边并集由原图真实边组成，所以既可安全截断距离，也可作为
 * `DistanceRootInitialization::upper` 的初值。若非连通图中各组的规范最小
 * 终端没有落在同一个可行分量，本函数可返回无穷；随后以无穷 cutoff 执行
 * 的多源距离不会截断有限标签，共同 root-star 扫描会在调用者已验证存在的
 * 公共分量中取得有限上界。
 */
double BootstrapBoundedDistanceUpper(const Graph& graph, const Query& query, int& best_root)
{
    const int g = static_cast<int>(query.groups.size());
    const int full_mask = (1 << g) - 1;
    // 预先标记每个顶点同时命中的组，便于 Dijkstra settle 时更新覆盖 mask。
    std::vector<int> group_mask(graph.n + 1);
    for (int group = 0; group < g; ++group)
        for (int vertex : query.groups[group])
            group_mask[vertex] |= 1 << group;

    // 以小组和稳定最小顶点优先选择至多 g 个不同候选根。
    std::vector<std::tuple<size_t, int, int>> order;
    for (int group = 0; group < g; ++group)
        order.push_back({query.groups[group].size(), *std::min_element(query.groups[group].begin(), query.groups[group].end()), group});
    std::sort(order.begin(), order.end());

    double best = fp::kInf;
    std::vector<int> roots;
    for (const auto& [unused_size, root, unused_group] : order)
    {
        (void)unused_size;
        (void)unused_group;
        if (std::find(roots.begin(), roots.end(), root) != roots.end())
            continue;
        roots.push_back(root);

        // 从候选根做一次 SPT；首次命中新组时把真实根路径加入边并集。
        std::vector<double> distance(graph.n + 1, fp::kInf);
        std::vector<int> parent_edge(graph.n + 1, -1);
        std::vector<std::uint64_t> used((static_cast<size_t>(graph.m) + 63) / 64);
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
                    assert(edge_id >= 0);
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
        // 只保留能够覆盖全部组且真实去重边权更小的候选。
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
 * ranked-bitmap 的实际字节数；完整模式保存 n 个真实距离供有向割对偶势。
 */
GroupTable BuildGroupDistances(const Graph& graph, const Query& query, bool bounded, double cutoff)
{
    GroupTable table(query.groups.size());
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
    {
        // 以本组全部终端为零距离源；bounded 模式不扩展到 cutoff 之外。
        std::vector<double> distance(graph.n + 1, fp::kInf);
        std::vector<int> touched;
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
            for (const auto& edge : graph.adj[vertex])
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

        // 完整模式直接接管 dense 距离，供 Enhanced 的势函数与 DP 读取。
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

        // 有界模式比较真实字节数，确定保存 dense cutoff 还是 ranked bitmap。
        std::sort(touched.begin(), touched.end());
        touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
        row.exact_count = touched.size();
        const size_t word_count = (static_cast<size_t>(graph.n + 1) + 63) / 64;
        const size_t dense_bytes = static_cast<size_t>(graph.n + 1) * sizeof(double);
        const size_t sparse_bytes = touched.size() * (sizeof(int) + sizeof(double)) + word_count * (sizeof(std::uint64_t) + sizeof(std::uint32_t));
        if (dense_bytes <= sparse_bytes)
        {
            row.dense = true;
            for (double& value : distance)
                if (!(value < cutoff))
                    value = cutoff;
            row.value = std::move(distance);
            continue;
        }

        // ranked bitmap 同时保存 membership 和每个 64-bit word 的值数组前缀。
        row.vertex = std::move(touched);
        row.value.reserve(row.vertex.size());
        row.bits.assign(word_count, 0);
        for (int vertex : row.vertex)
        {
            row.value.push_back(distance[vertex]);
            row.bits[static_cast<size_t>(vertex) >> 6] |= std::uint64_t{1} << (vertex & 63);
        }
        row.rank.resize(word_count);
        std::uint32_t prefix = 0;
        for (size_t word = 0; word < word_count; ++word)
        {
            row.rank[word] = prefix;
            prefix += static_cast<std::uint32_t>(Popcount64(row.bits[word]));
        }
    }
    return table;
}

/** @brief 扫描所有共同根的组距离和，返回最小 star 上界并更新根。 */
double RootStarUpper(const GroupTable& distance, int n, int& root)
{
    if (distance.empty())
        return 0.0;
    double best = distance.front().bounded ? distance.front().cutoff : fp::kInf;
    const GroupRow* driver = &distance.front();
    for (const auto& row : distance)
        if (row.ExactSize(n) < driver->ExactSize(n))
            driver = &row;
    // 只枚举最短精确 row 的顶点，在其上求全组 star 和。Base 的 best 从 cutoff 开始，任一非精确读取都至少贡献 cutoff，故不可能被误收为上界。
    driver->ForEachExact(n,
                         [&](int vertex, double)
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

} // namespace

/**
 * @brief 一次构造距离 oracle、候选根和初始真实上界。
 *
 * Base 在函数内部先取得 cutoff 再构造有界组距离；若 cutoff 暂为无穷，
 * 多源搜索自然不截断。Enhanced 保存完整组距离。两种模式最后都执行相同
 * 的共同根 star 扫描，并返回同一结构。
 */
DistanceRootInitialization BuildDistanceRootInitialization(const Graph& graph, const Query& query, bool enhanced)
{
    DistanceRootInitialization result;
    const bool bounded = !enhanced;

    if (bounded)
        result.upper = BootstrapBoundedDistanceUpper(graph, query, result.root);
    result.group_distance = BuildGroupDistances(graph, query, bounded, bounded ? result.upper : fp::kInf);
    result.upper = std::min(result.upper, RootStarUpper(result.group_distance, graph.n, result.root));
    return result;
}

/** @brief 用 subset DP 构建每个组子集、每对固定端点的最短 Hamilton path。 */
void TourLowerBound::Build(const std::vector<std::vector<double>>& metric)
{
    group_count_ = static_cast<int>(metric.size());
    const int subset_count = 1 << group_count_;
    const int full_mask = subset_count - 1;
    std::vector<double> path(static_cast<size_t>(subset_count) * group_count_ * group_count_, fp::kInf);
    // 把 (mask,start,last) 压成一维下标，避免多层 vector 寻址。
    auto Index = [&](int mask, int start, int last) { return (static_cast<size_t>(mask) * group_count_ + start) * group_count_ + last; };
    // 固定 start 后按 mask 扩展 Hamilton path，last 表示当前另一端点。
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

    // 只保留至少含两个组的无序端点对及其双向较小路径值。
    endpoints_.assign(subset_count, {});
    for (int mask = 1; mask < subset_count; ++mask)
    {
        if (!(mask & (mask - 1)))
            continue;
        for (int left_bits = mask; left_bits; left_bits &= left_bits - 1)
        {
            const int left = FirstBit(left_bits & -left_bits);
            for (int right_bits = mask & ~((1 << (left + 1)) - 1); right_bits; right_bits &= right_bits - 1)
            {
                const int right = FirstBit(right_bits & -right_bits);
                const double value = std::min(path[Index(mask, left, right)], path[Index(mask, right, left)]);
                if (value < fp::kInf)
                    endpoints_[mask].push_back({left, right, value});
            }
        }
    }
}

/** @brief 把当前顶点接到预计算路径两端，返回剩余 mask 的 admissible tour 下界。 */
double TourLowerBound::At(int vertex, int mask, const GroupTable& distance) const
{
    if (!mask)
        return 0.0;
    if (!(mask & (mask - 1)))
        return distance[FirstBit(mask)][vertex];

    std::array<double, 16> fixed;
    fixed.fill(fp::kInf);
    // 对每个固定端点组，记录从当前顶点接入该 Hamilton path 的最小值。
    for (const auto& endpoint : endpoints_[mask])
    {
        const double candidate = distance[endpoint.left][vertex] + endpoint.path + distance[endpoint.right][vertex];
        fixed[endpoint.left] = std::min(fixed[endpoint.left], candidate);
        fixed[endpoint.right] = std::min(fixed[endpoint.right], candidate);
    }
    double value = 0.0;
    for (int bits = mask; bits; bits &= bits - 1)
        value = std::max(value, fixed[FirstBit(bits & -bits)]);
    return value * 0.5;
}

/**
 * @brief 恢复候选根到各组的最短路，按 edge id 去重并选择最便宜边并集。
 *
 * 恢复只使用 GroupRow 中的精确 predecessor cone；最终边并集必须连通并覆盖
 * 所有组，故其去重边权是合法上界且可进一步重根为 witness。
 */
RootPathUnion BuildRootPathUnion(const Graph& graph, const Query& query, const GroupTable& distance, const std::vector<int>& roots)
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

    // 对每个候选根逐组恢复一条最短路，并在本根内按原 edge id 去重。
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
                bool advanced = false;
                while (next_edge[vertex] < static_cast<int>(graph.adj[vertex].size()))
                {
                    const auto& edge = graph.adj[vertex][next_edge[vertex]++];
                    if (epoch[edge.to] == current_epoch || !fp::Eq(edge.w + distance[group][edge.to], distance[group][vertex]))
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

        // 只有全部组均恢复成功时才按真实选边求和并比较上界。
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
WitnessTree BuildRootPathWitness(const Graph& graph, const Query& query, const RootPathUnion& paths, int anchor_group)
{
    return BuildWitnessFromEdges(graph, query, paths.edge_ids, paths.root, anchor_group);
}

/** @brief 将有向割 primal bitmap 展开为 edge id，再构造同格式 witness。 */
WitnessTree BuildDualWitness(const Graph& graph, const Query& query, const std::vector<std::uint64_t>& edge_words, int root, int anchor_group)
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
double EvaluateWitnessTree(const WitnessTree& tree, const Problem& p, const std::vector<Row>& ordinary)
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
    // 加入虚拟超根，把局部 parent 表转成自顶向下的孩子表和遍历序列。
    for (int node = 0; node < root; ++node)
        children[tree.parent[node]].push_back({node, tree.parent_edge[node]});
    std::vector<int> order{root};
    for (size_t i = 0; i < order.size(); ++i)
        for (const auto& child : children[order[i]])
            order.push_back(child.id);

    std::vector<std::vector<double>> dp(root + 1, std::vector<double>(p.subset_count, fp::kInf));
    std::vector<double> block(p.subset_count, fp::kInf);
    std::vector<double> local(p.subset_count, fp::kInf);
    std::vector<double> merged(p.subset_count, fp::kInf);
    // 逆序处理真实树：先组合当前根可用的 rooted block，再卷入每棵孩子子树。
    for (auto it = order.rbegin(); it != order.rend(); ++it)
    {
        const int node = *it;
        dp[node][0] = 0.0;
        if (node != root)
        {
            // singleton 读取组距离，多组只读取已经 ready 的 ordinary 精确值。
            const int vertex = tree.vertex[node];
            std::fill(block.begin(), block.end(), fp::kInf);
            for (int mask = 1; mask < p.subset_count; ++mask)
            {
                if (p.popcount[mask] == 1)
                {
                    const GroupRow& singleton = p.group_distance[p.bit_to_group[FirstBit(mask)]];
                    if (!singleton.bounded || singleton.IsExact(vertex))
                        block[mask] = singleton[vertex];
                }
                else if (ordinary[mask].ready)
                    block[mask] = RowValue(ordinary[mask], vertex);
            }
            std::fill(local.begin(), local.end(), fp::kInf);
            local[0] = 0.0;
            // 固定 remaining 的最低位，只枚举一次局部 block 分解。
            for (int remaining = 1; remaining < p.subset_count; ++remaining)
            {
                const int first = remaining & -remaining;
                for (int part = remaining; part; part = (part - 1) & remaining)
                    if ((part & first) && block[part] < fp::kInf && local[remaining ^ part] < fp::kInf)
                        local[remaining] = std::min(local[remaining], local[remaining ^ part] + block[part]);
            }
            dp[node] = local;
        }
        // 分配给孩子的 mask 非空时才支付真实父边权。
        for (const auto& child : children[node])
        {
            std::fill(merged.begin(), merged.end(), fp::kInf);
            for (int mask = 0; mask < p.subset_count; ++mask)
                for (int below = mask;; below = (below - 1) & mask)
                {
                    if (dp[node][mask ^ below] < fp::kInf && dp[child.id][below] < fp::kInf)
                        merged[mask] = std::min(merged[mask], dp[node][mask ^ below] + dp[child.id][below] + (below ? child.edge : 0.0));
                    if (!below)
                        break;
                }
            dp[node].swap(merged);
        }
    }
    return dp[root][p.full_mask];
}

/**
 * @brief 在有向割 primal 设施点上构造可行支撑度量并做小型 subset DP。
 *
 * residual 与 primal bitmap 只在增强预处理阶段有效；函数返回后调用者即可
 * 释放 residual，不让 O(m) 临时数组进入 ordinary 主阶段。
 */
double BuildPrimalFacilityUpper(const Problem& p, const std::vector<double>& residual, const std::vector<std::uint64_t>& edge_words)
{
    // 从 primal 边位图中 O(1) 判断原图边是否属于 witness。
    auto IsTreeEdge = [&](int edge_id) { return (edge_words[static_cast<size_t>(edge_id) >> 6] >> (edge_id & 63)) & 1ULL; };
    std::vector<int> facility{p.root};
    for (const auto& edge : p.graph.edges)
        if (IsTreeEdge(edge.id))
        {
            facility.push_back(edge.u);
            facility.push_back(edge.v);
        }
    std::sort(facility.begin(), facility.end());
    facility.erase(std::unique(facility.begin(), facility.end()), facility.end());
    // 压缩 primal 涉及的 facilities，并建立原顶点到局部下标的 O(1) 映射。
    const int count = static_cast<int>(facility.size());
    std::vector<int> facility_index(p.graph.n + 1, -1);
    for (int i = 0; i < count; ++i)
        facility_index[facility[i]] = i;

    std::vector<double> scratch(p.graph.n + 1, fp::kInf);
    std::vector<int> touched;
    // 从一个 facility 跑 residual 可行路径上的 Dijkstra，只返回 facility 间距离。
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
    // 对每个 facility 运行一次支撑图最短路，形成小图完整度量。
    for (int i = 0; i < count; ++i)
        metric[i] = DistancesFrom(facility[i]);

    const int subset_count = 1 << p.g;
    const int full_mask = subset_count - 1;
    std::vector<int> popcount(subset_count);
    std::vector<std::vector<double>> row(subset_count);
    // singleton 仍读取原图组距离；小图 row 只按 facility 下标保存。
    for (int mask = 1; mask < subset_count; ++mask)
        popcount[mask] = popcount[mask >> 1] + (mask & 1);
    for (int group = 0; group < p.g; ++group)
    {
        row[1 << group].resize(count);
        for (int i = 0; i < count; ++i)
            row[1 << group][i] = p.group_distance[group][facility[i]];
    }

    std::vector<double> merged(count);
    // 按组子集大小做同根 split，再用 facility metric 移动根。
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
                    row[mask][root] = std::min(row[mask][root], metric[root][branch] + merged[branch]);
        }

    // 偶数组数用两个半块完成；奇数组数再外加一个 singleton。
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
            for (int left = (remaining - 1) & remaining; left; left = (left - 1) & remaining)
            {
                const int right = remaining ^ left;
                if (left >= right || popcount[left] != p.half)
                    continue;
                for (int i = 0; i < count; ++i)
                    best = std::min(best, row[singleton][i] + row[left][i] + row[right][i]);
            }
        }
    }
    return best;
}

/** @brief 先检查顶点缓存的全局最远组，未命中再扫描剩余原始组 mask。 */
double FarthestRemaining(const Problem& p, int vertex, int original_mask)
{
    if (!original_mask)
        return 0.0;
    const int cached = p.farthest_group[vertex];
    if (original_mask & (1 << cached))
        return p.group_distance[cached][vertex];
    double value = 0.0;
    for (int bits = original_mask; bits; bits &= bits - 1)
        value = std::max(value, p.group_distance[FirstBit(bits & -bits)][vertex]);
    return value;
}

/** @brief 从零开始计算 farthest、tour 和 Enhanced 对偶势的统一 future 下界。 */
double FutureBound(const Problem& p, int vertex, int original_mask)
{
    return FutureBound(p, vertex, original_mask, FarthestRemaining(p, vertex, original_mask));
}

/** @brief 复用已通过廉价检查的 farthest，再计算较贵 tour 与可选 dual。 */
double FutureBound(const Problem& p, int vertex, int original_mask, double farthest)
{
    double value = std::max(farthest, p.tour.At(vertex, original_mask, p.group_distance));
    if (p.enhanced)
        value = std::max(value, p.dual.At(vertex, original_mask));
    return value;
}

/** @brief 统一读取 D(0)=0、singleton 组距离和多组 ordinary 稀疏 row。 */
double OrdinaryValue(const Problem& p, int mask, int vertex)
{
    if (!mask)
        return 0.0;
    if (p.popcount[mask] == 1)
        return p.group_distance[p.bit_to_group[FirstBit(mask)]][vertex];
    return RowValue(p.ordinary[mask], vertex);
}

/** @brief singleton 天然可用；多组状态必须显式 `ready`，空 mask 不算 ordinary。 */
bool OrdinaryAvailable(const Problem& p, int mask)
{
    return mask && (p.popcount[mask] == 1 || p.ordinary[mask].ready);
}

/**
 * @brief 建立一次查询的公共数据，并为 Enhanced 增加更强证书。
 *
 * 执行顺序固定为分量下界、组距离/根上界、锚映射、tour，再选择 Base 根路径
 * witness 或有向割 dual/primal witness。任意阶段上下界闭合都安全提前返回。
 */
bool PrepareProblem(Problem& p)
{
    p.g = static_cast<int>(p.query.groups.size());
    p.half = p.g / 2;
    p.component_cover = ComputeComponentCover(p.graph, p.query);
    if (p.component_cover.cover_number == 1)
    {
        p.best = 0.0;
        return true;
    }

    // 两种模式都调用同一距离—根职责并一次取得组距离、候选根和真实上界；
    // 有界距离需要的 cutoff 构造封装在该职责内部。
    DistanceRootInitialization distance_root = BuildDistanceRootInitialization(p.graph, p.query, p.enhanced);
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
    p.root_path_union = BuildRootPathUnion(p.graph, p.query, p.group_distance, roots);
    p.best = std::min(p.best, p.root_path_union.upper);
    if (p.best <= p.component_cover.lower)
        return true;

    // 在候选根处固定最远组为永久锚组，再建立压缩 mask 与原组 mask 的映射。
    p.anchor_group = 0;
    for (int group = 1; group < p.g; ++group)
        if (p.group_distance[group][p.root] > p.group_distance[p.anchor_group][p.root])
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
        p.original_mask[mask] = p.original_mask[mask ^ (1 << bit)] | (1 << p.bit_to_group[bit]);
    }

    // 每个顶点缓存全组最远项；剩余 mask 命中该组时可 O(1) 读取 farthest。
    p.farthest_group.assign(p.graph.n + 1, 0);
    for (int vertex = 1; vertex <= p.graph.n; ++vertex)
        for (int group = 1; group < p.g; ++group)
            if (p.group_distance[group][vertex] > p.group_distance[p.farthest_group[vertex]][vertex])
                p.farthest_group[vertex] = static_cast<unsigned char>(group);

    // 从组距离形成组间松弛度量，并一次预计算全部固定端点 tour 下界。
    std::vector<std::vector<double>> metric(p.g, std::vector<double>(p.g, fp::kInf));
    for (int left = 0; left < p.g; ++left)
        for (int right = 0; right < p.g; ++right)
            for (int vertex : p.query.groups[right])
                metric[left][right] = std::min(metric[left][right], p.group_distance[left][vertex]);
    p.tour.Build(metric);

    // 两种模式只在 witness 来源处替换；树 DP 不在预处理中无条件执行。
    if (!p.enhanced)
    {
        p.witness_tree = BuildRootPathWitness(p.graph, p.query, p.root_path_union, p.anchor_group);
    }
    else
    {
        std::vector<std::vector<double>> dense(p.g);
        for (int group = 0; group < p.g; ++group)
            dense[group].swap(p.group_distance[group].value);
        p.dual.BuildKeepingResidualChangedArcsWithPrimalEdges(p.graph, p.query, dense, p.root);
        for (int group = 0; group < p.g; ++group)
            dense[group].swap(p.group_distance[group].value);
        p.best = std::min(p.best, p.dual.PrimalUpper());
        p.best = std::min(p.best, BuildPrimalFacilityUpper(p, p.dual.Residual(), p.dual.PrimalEdgeWords()));
        p.witness_tree = BuildDualWitness(p.graph, p.query, p.dual.PrimalEdgeWords(), p.root, p.anchor_group);
        p.dual.ReleaseResidual();
    }

    // 主 DP 容器在全部证书和 witness 就绪后才分配，singleton 仍由 GroupRow 隐式提供。
    p.ordinary.assign(p.subset_count, {});
    p.ordinary_minimum.assign(p.subset_count, fp::kInf);

    // 到此只完成当前模式的 witness 构造，不无条件执行树 DP。Base 的
    // root-path tree 与 Enhanced 的 dual-primal tree 随后都交给跨 A1/D 的
    // 同一个 rent-or-buy 调度器，并从 rent=0 开始按同一 buy 公式购买。
    return p.best <= p.component_cover.lower;
}

} // namespace gst::methods::abhss::internal
