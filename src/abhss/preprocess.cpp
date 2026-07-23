#include "internal.h"

#include <numeric>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace gst::methods::abhss::internal
{
namespace
{
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

int ArcIndex(int edge_id, int from, int to)
{
    return 2 * edge_id + (from < to ? 0 : 1);
}

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
            // A strict improvement is essential here.  Re-parenting an
            // equal-distance vertex across a zero-weight edge can point a
            // settled ancestor back to its descendant, create a parent cycle,
            // and enqueue the same final key twice.  Heap/adjacency order is
            // already deterministic, so equal candidates keep the first
            // valid predecessor.
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
        // Keep enough evidence in the exceptional path to distinguish an
        // actually disconnected edge set from duplicate equal-distance heap
        // pops.  This path is never executed by a valid witness and therefore
        // has no effect on formal timing.
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

double RowValue(const Row& row, int vertex)
{
    const auto it = std::lower_bound(row.vertex.begin(), row.vertex.end(), vertex);
    if (it == row.vertex.end() || *it != vertex)
        return fp::kInf;
    return row.value[static_cast<size_t>(it - row.vertex.begin())];
}

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

bool GroupRow::IsExact(int v) const
{
    if (!bounded)
        return true;
    if (dense)
        return value[v] < cutoff;
    const size_t word = static_cast<size_t>(v) >> 6;
    return word < bits.size() &&
           ((bits[word] >> (v & 63)) & std::uint64_t{1});
}

size_t GroupRow::ExactSize(int n) const
{
    return bounded ? exact_count : static_cast<size_t>(n);
}

ComponentCover ComputeComponentCover(const Graph& graph, const Query& query)
{
    ComponentCover result;
    const int g = static_cast<int>(query.groups.size());
    const int full_mask = (1 << g) - 1;

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
    auto Find = [&](auto&& self, int x) -> int
    {
        return parent[x] == x ? x : parent[x] = self(self, parent[x]);
    };
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

    std::vector<int> superset(full_mask + 1, -1);
    for (int component : components)
        superset[component_mask[component]] = component;
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

double BuildCanonicalSptUpper(const Graph& graph, const Query& query, int& best_root)
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
    for (const auto& [unused_size, root, unused_group] : order)
    {
        (void)unused_size;
        (void)unused_group;
        if (std::find(roots.begin(), roots.end(), root) != roots.end())
            continue;
        roots.push_back(root);

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

GroupTable BuildGroupDistances(const Graph& graph,
                               const Query& query,
                               bool bounded,
                               double cutoff)
{
    GroupTable table(query.groups.size());
    for (int group = 0; group < static_cast<int>(query.groups.size()); ++group)
    {
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

        std::sort(touched.begin(), touched.end());
        touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
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
    }
    return table;
}

double RootStarUpper(const GroupTable& distance, int n, int& root)
{
    if (distance.empty())
        return 0.0;
    double best = distance.front().bounded ? distance.front().cutoff : fp::kInf;
    const GroupRow* driver = &distance.front();
    for (const auto& row : distance)
        if (row.ExactSize(n) < driver->ExactSize(n))
            driver = &row;
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

void TourLowerBound::Build(const std::vector<std::vector<double>>& metric)
{
    group_count_ = static_cast<int>(metric.size());
    const int subset_count = 1 << group_count_;
    const int full_mask = subset_count - 1;
    std::vector<double> path(
        static_cast<size_t>(subset_count) * group_count_ * group_count_, fp::kInf);
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

    endpoints_.assign(subset_count, {});
    for (int mask = 1; mask < subset_count; ++mask)
    {
        if (!(mask & (mask - 1)))
            continue;
        for (int left_bits = mask; left_bits; left_bits &= left_bits - 1)
        {
            const int left = FirstBit(left_bits & -left_bits);
            for (int right_bits = mask & ~((1 << (left + 1)) - 1);
                 right_bits;
                 right_bits &= right_bits - 1)
            {
                const int right = FirstBit(right_bits & -right_bits);
                const double value = std::min(path[Index(mask, left, right)],
                                              path[Index(mask, right, left)]);
                if (value < fp::kInf)
                    endpoints_[mask].push_back({left, right, value});
            }
        }
    }
}

double TourLowerBound::At(int vertex, int mask, const GroupTable& distance) const
{
    if (!mask)
        return 0.0;
    if (!(mask & (mask - 1)))
        return distance[FirstBit(mask)][vertex];

    std::array<double, 16> fixed;
    fixed.fill(fp::kInf);
    for (const auto& endpoint : endpoints_[mask])
    {
        const double candidate = distance[endpoint.left][vertex] + endpoint.path +
                                 distance[endpoint.right][vertex];
        fixed[endpoint.left] = std::min(fixed[endpoint.left], candidate);
        fixed[endpoint.right] = std::min(fixed[endpoint.right], candidate);
    }
    double value = 0.0;
    for (int bits = mask; bits; bits &= bits - 1)
        value = std::max(value, fixed[FirstBit(bits & -bits)]);
    return value * 0.5;
}

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
                bool advanced = false;
                while (next_edge[vertex] < static_cast<int>(graph.adj[vertex].size()))
                {
                    const auto& edge = graph.adj[vertex][next_edge[vertex]++];
                    if (epoch[edge.to] == current_epoch ||
                        !fp::Eq(edge.w + distance[group][edge.to],
                                distance[group][vertex]))
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

WitnessTree BuildRootPathWitness(const Graph& graph,
                                 const Query& query,
                                 const RootPathUnion& paths,
                                 int anchor_group)
{
    return BuildWitnessFromEdges(
        graph, query, paths.edge_ids, paths.root, anchor_group);
}

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
                    block[mask] = p.group_distance[
                        p.bit_to_group[FirstBit(mask)]][vertex];
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

double BuildPrimalFacilityUpper(const Problem& p,
                                const std::vector<double>& residual,
                                const std::vector<std::uint64_t>& edge_words)
{
    auto IsTreeEdge = [&](int edge_id)
    {
        return (edge_words[static_cast<size_t>(edge_id) >> 6] >> (edge_id & 63)) & 1ULL;
    };
    std::vector<int> facility{p.root};
    for (const auto& edge : p.graph.edges)
        if (IsTreeEdge(edge.id))
        {
            facility.push_back(edge.u);
            facility.push_back(edge.v);
        }
    std::sort(facility.begin(), facility.end());
    facility.erase(std::unique(facility.begin(), facility.end()), facility.end());
    const int count = static_cast<int>(facility.size());
    std::vector<int> facility_index(p.graph.n + 1, -1);
    for (int i = 0; i < count; ++i)
        facility_index[facility[i]] = i;

    std::vector<double> scratch(p.graph.n + 1, fp::kInf);
    std::vector<int> touched;
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
            row[1 << group][i] = p.group_distance[group][facility[i]];
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

double FarthestRemaining(const Problem& p, int vertex, int original_mask)
{
    if (!original_mask)
        return 0.0;
    const int cached = p.farthest_group[vertex];
    if (original_mask & (1 << cached))
        return p.group_distance[cached][vertex];
    double value = 0.0;
    for (int bits = original_mask; bits; bits &= bits - 1)
        value = std::max(value,
                         p.group_distance[FirstBit(bits & -bits)][vertex]);
    return value;
}

double FutureBound(const Problem& p, int vertex, int original_mask)
{
    return FutureBound(
        p, vertex, original_mask, FarthestRemaining(p, vertex, original_mask));
}

double FutureBound(const Problem& p,
                   int vertex,
                   int original_mask,
                   double farthest)
{
    double value = std::max(
        farthest, p.tour.At(vertex, original_mask, p.group_distance));
    if (p.UsesDirectedCut())
        value = std::max(value, p.dual.At(vertex, original_mask));
    return value;
}

double OrdinaryValue(const Problem& p, int mask, int vertex)
{
    if (!mask)
        return 0.0;
    if (p.popcount[mask] == 1)
        return p.group_distance[p.bit_to_group[FirstBit(mask)]][vertex];
    return RowValue(p.ordinary[mask], vertex);
}

bool OrdinaryAvailable(const Problem& p, int mask)
{
    return mask && (p.popcount[mask] == 1 || p.ordinary[mask].ready);
}

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

    if (p.UsesBoundedGroupDistances())
        p.best = BuildCanonicalSptUpper(p.graph, p.query, p.root);
    const bool bounded_distances = p.UsesBoundedGroupDistances();
    p.group_distance = BuildGroupDistances(
        p.graph,
        p.query,
        bounded_distances,
        bounded_distances ? p.best : fp::kInf);
    const double star = RootStarUpper(p.group_distance, p.graph.n, p.root);
    p.best = std::min(p.best, star);
    if (p.best == 0.0 || fp::Le(p.best, p.component_cover.lower))
        return true;

    std::vector<int> roots{p.root};
    for (int root : p.component_cover.roots)
        if (std::find(roots.begin(), roots.end(), root) == roots.end())
            roots.push_back(root);
    p.root_path_union = BuildRootPathUnion(
        p.graph, p.query, p.group_distance, roots);
    p.best = std::min(p.best, p.root_path_union.upper);
    if (fp::Le(p.best, p.component_cover.lower))
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

    p.farthest_group.assign(p.graph.n + 1, 0);
    for (int vertex = 1; vertex <= p.graph.n; ++vertex)
        for (int group = 1; group < p.g; ++group)
            if (p.group_distance[group][vertex] >
                p.group_distance[p.farthest_group[vertex]][vertex])
                p.farthest_group[vertex] = static_cast<unsigned char>(group);

    std::vector<std::vector<double>> metric(
        p.g, std::vector<double>(p.g, fp::kInf));
    for (int left = 0; left < p.g; ++left)
        for (int right = 0; right < p.g; ++right)
            for (int vertex : p.query.groups[right])
                metric[left][right] = std::min(
                    metric[left][right], p.group_distance[left][vertex]);
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

    p.ordinary.assign(p.subset_count, {});
    p.ordinary_minimum.assign(p.subset_count, fp::kInf);
    return fp::Le(p.best, p.component_cover.lower);
}

}  // namespace gst::methods::abhss::internal
