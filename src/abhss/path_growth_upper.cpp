#include "path_growth_upper.h"

namespace gst::methods::abhss::internal
{
namespace
{
/** @brief 枚举固定数量的早期组并用真实 tight paths 生长，返回最紧可行边并集。 */
PathGrowthUpper BuildSeededPathGrowthUpper(const Graph& graph, const Query& query, const GroupTable& distance, double cutoff, bool force_fourth)
{
    const int g = static_cast<int>(query.groups.size());
    const int full_mask = (1 << g) - 1;
    // membership[v] 记录顶点同时命中的全部组，使一条路径可一次覆盖多个组。
    std::vector<int> membership(graph.n + 1);
    for (int group = 0; group < g; ++group)
        for (int vertex : query.groups[group])
            membership[vertex] |= 1 << group;

    // 所有工作区在有序组三元组之间复用，结束时只清除本轮实际触碰的位置。
    std::vector<unsigned char> selected_edge(graph.m), selected_vertex(graph.n + 1);
    std::vector<int> epoch(graph.n + 1), next_edge(graph.n + 1), path, path_edges, tree_vertices, touched_edges;
    int current_epoch = 0;
    PathGrowthUpper result;
    double& best = result.upper;
    double selected_cost = 0.0;

    // lambda：登记当前连通树中的新顶点，并保存其稀疏清理位置。
    auto AddVertex = [&](int vertex)
    {
        if (!selected_vertex[vertex])
        {
            selected_vertex[vertex] = 1;
            tree_vertices.push_back(vertex);
        }
    };
    // lambda：按原图 edge_id 去重计费，同时把两个端点纳入当前树。
    auto AddEdge = [&](int edge_id)
    {
        if (!selected_edge[edge_id])
        {
            selected_edge[edge_id] = 1;
            touched_edges.push_back(edge_id);
            selected_cost += graph.edges[edge_id].w;
        }
        AddVertex(graph.edges[edge_id].u);
        AddVertex(graph.edges[edge_id].v);
    };
    // lambda：稀疏清空一棵候选树，并同步恢复累计费用。
    auto ClearTree = [&]()
    {
        for (int edge_id : touched_edges)
            selected_edge[edge_id] = 0;
        for (int vertex : tree_vertices)
            selected_vertex[vertex] = 0;
        touched_edges.clear();
        tree_vertices.clear();
        selected_cost = 0.0;
    };
    // lambda：只在候选需要分支或继续生长时统计当前树覆盖的组，避免每次加点都维护掩码。
    auto CollectCovered = [&]()
    {
        int covered = 0;
        for (int vertex : tree_vertices)
            covered |= membership[vertex];
        return covered;
    };
    // lambda：检查连接下一个目标组的必要费用后是否仍可能严格改善上界。
    auto CanImproveAfterConnection = [&](double mandatory_distance)
    {
        return selected_cost + mandatory_distance < std::min(best, cutoff);
    };
    // lambda：在目标组的 tight-edge 子图中恢复真实最短路；epoch 阻止零权环。
    auto Recover = [&](int root, int group)
    {
        if (!distance[group].IsExact(root))
            return false;
        ++current_epoch;
        path.assign(1, root);
        path_edges.clear();
        epoch[root] = current_epoch;
        next_edge[root] = 0;
        const int group_bit = 1 << group;
        while (!path.empty() && !(membership[path.back()] & group_bit))
        {
            const int vertex = path.back();
            bool advanced = false;
            while (next_edge[vertex] < static_cast<int>(graph.adj[vertex].size()))
            {
                const AdjEdge& edge = graph.adj[vertex][next_edge[vertex]++];
                if (epoch[edge.to] == current_epoch || !distance[group].IsExact(edge.to) || !fp::Eq(edge.w + distance[group][edge.to], distance[group][vertex]))
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
        if (path.empty())
            return false;
        AddVertex(root);
        for (int edge_id : path_edges)
            AddEdge(edge_id);
        return true;
    };

    // lambda：从当前真实树继续按最近未覆盖组生长，并登记可行上界。
    auto GrowAndRecord = [&](bool promising, int covered)
    {
        while (promising && covered != full_mask)
        {
            int next_group = -1, next_root = 0;
            double next_distance = fp::kInf;
            for (int group = 0; group < g; ++group)
            {
                if (covered & (1 << group))
                    continue;
                for (int vertex : tree_vertices)
                    if (distance[group].IsExact(vertex) && distance[group][vertex] < next_distance)
                    {
                        next_group = group;
                        next_root = vertex;
                        next_distance = distance[group][vertex];
                    }
            }
            // 当前树可视为零代价收缩点；任何覆盖下一组的连通扩展都至少再付
            // 树到该组的最短距离。必要条件失败时无需恢复注定不能改进的路径。
            if (next_group < 0 || !CanImproveAfterConnection(next_distance) || !Recover(next_root, next_group))
                break;
            promising = selected_cost < std::min(best, cutoff);
            covered = CollectCovered();
        }
        if (promising && covered == full_mask && selected_cost < best)
        {
            best = selected_cost;
            result.edge_ids = touched_edges;
        }
    };

    // 每个有序组对只恢复一次种子路径，再复用其真实边枚举全部第三组。
    std::vector<int> seed_edges;
    for (int first = 0; first < g; ++first)
        for (int second = 0; second < g; ++second)
        {
            if (first == second)
                continue;
            int start = 0;
            double start_distance = fp::kInf;
            for (int vertex : query.groups[second])
                if (distance[first].IsExact(vertex) && distance[first][vertex] < start_distance)
                {
                    start = vertex;
                    start_distance = distance[first][vertex];
                }
            // 种子树只有起点，故其必要连接费用正是 start_distance。
            if (!start || !CanImproveAfterConnection(start_distance))
                continue;
            AddVertex(start);
            if (!Recover(start, first))
            {
                ClearTree();
                continue;
            }
            if (!(selected_cost < std::min(best, cutoff)))
            {
                ClearTree();
                continue;
            }

            seed_edges = touched_edges;
            bool seed_loaded = true;
            for (int forced = 0; forced < g; ++forced)
            {
                if (forced == first || forced == second)
                    continue;
                if (!seed_loaded)
                {
                    AddVertex(start);
                    for (int edge_id : seed_edges)
                        AddEdge(edge_id);
                }
                seed_loaded = false;

                int covered = CollectCovered();
                bool promising = selected_cost < std::min(best, cutoff);

                // 第三组未被种子顺带覆盖时，显式连接它的最近真实路径。
                if (promising && !(covered & (1 << forced)))
                {
                    int forced_root = 0;
                    double forced_distance = fp::kInf;
                    for (int vertex : tree_vertices)
                        if (distance[forced].IsExact(vertex) && distance[forced][vertex] < forced_distance)
                        {
                            forced_root = vertex;
                            forced_distance = distance[forced][vertex];
                        }
                    if (!forced_root || !CanImproveAfterConnection(forced_distance) || !Recover(forced_root, forced))
                        promising = false;
                    else
                    {
                        promising = selected_cost < std::min(best, cutoff);
                        covered = CollectCovered();
                    }
                }

                // 证书刷新可再固定一个未覆盖第四组；预处理仍只运行三元版本。
                if (!force_fourth || (promising && covered == full_mask))
                    GrowAndRecord(promising, covered);
                else if (promising)
                {
                    const std::vector<int> third_edges = touched_edges;
                    const int third_covered = covered;
                    bool third_loaded = true;
                    for (int fourth = 0; fourth < g; ++fourth)
                    {
                        if (third_covered & (1 << fourth))
                            continue;
                        if (!third_loaded)
                        {
                            AddVertex(start);
                            for (int edge_id : third_edges)
                                AddEdge(edge_id);
                        }
                        third_loaded = false;
                        bool fourth_promising = selected_cost < std::min(best, cutoff);
                        int fourth_root = 0;
                        double fourth_distance = fp::kInf;
                        for (int vertex : tree_vertices)
                            if (distance[fourth].IsExact(vertex) && distance[fourth][vertex] < fourth_distance)
                            {
                                fourth_root = vertex;
                                fourth_distance = distance[fourth][vertex];
                            }
                        if (!fourth_root || !CanImproveAfterConnection(fourth_distance) || !Recover(fourth_root, fourth))
                            fourth_promising = false;
                        else
                            fourth_promising = selected_cost < std::min(best, cutoff);
                        const int fourth_covered = fourth_promising ? CollectCovered() : third_covered;
                        GrowAndRecord(fourth_promising, fourth_covered);
                        ClearTree();
                    }
                }

                ClearTree();
            }
        }

    return result;
}
}

PathGrowthUpper BuildTripleSeededPathGrowthUpper(const Graph& graph, const Query& query, const GroupTable& distance, double cutoff)
{
    return BuildSeededPathGrowthUpper(graph, query, distance, cutoff, false);
}

PathGrowthUpper BuildQuadSeededPathGrowthUpper(const Graph& graph, const Query& query, const GroupTable& distance, double cutoff)
{
    return BuildSeededPathGrowthUpper(graph, query, distance, cutoff, true);
}

}  // namespace gst::methods::abhss::internal
