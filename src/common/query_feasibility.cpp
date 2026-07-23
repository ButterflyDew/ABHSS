#include "query_feasibility.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace gst
{

namespace
{
/** @brief 为没有加载期缓存的手工小图建立一次局部连通分量索引。 */
std::vector<int> ComputeComponents(const Graph& graph, int& component_count)
{
    std::vector<int> comp(graph.n + 1, -1);
    component_count = 0;
    std::vector<int> frontier;
    for (int s = 1; s <= graph.n; ++s)
    {
        if (comp[s] != -1)
            continue;
        comp[s] = component_count;
        frontier.clear();
        frontier.push_back(s);
        for (size_t index = 0; index < frontier.size(); ++index)
        {
            const int u = frontier[index];
            for (const auto& e : graph.adj[u])
            {
                if (comp[e.to] == -1)
                {
                    comp[e.to] = component_count;
                    frontier.push_back(e.to);
                }
            }
        }
        ++component_count;
    }
    return comp;
}

/** @brief 验证一个组并提取其去重、递增的连通分量编号。 */
std::vector<int> GroupComponents(const std::vector<int>& group,
                                 const std::vector<int>& component_of,
                                 int n)
{
    std::vector<int> result;
    result.reserve(group.size());
    for (int vertex : group)
    {
        if (vertex < 1 || vertex > n)
            throw std::runtime_error("Query vertex id out of range.");
        result.push_back(component_of[vertex]);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
}  // namespace

bool IsQueryFeasible(const Graph& graph, const Query& query)
{
    if (query.groups.empty())
        return true;
    if (graph.n <= 0 || graph.adj.size() != static_cast<size_t>(graph.n + 1))
        return false;

    int component_count = graph.component_count;
    std::vector<int> local_component;
    const std::vector<int>* component_of = &graph.component_of;
    if (component_count <= 0 ||
        component_of->size() != static_cast<size_t>(graph.n + 1))
    {
        local_component = ComputeComponents(graph, component_count);
        component_of = &local_component;
    }

    // 连通图是正式 workload 的主流情形；只需验证组非空和顶点范围。
    if (component_count == 1)
    {
        for (const auto& group : query.groups)
        {
            if (group.empty())
                return false;
            for (int vertex : group)
                if (vertex < 1 || vertex > graph.n)
                    throw std::runtime_error("Query vertex id out of range.");
        }
        return true;
    }

    // 在分量很多但组较小时，不分配 O(component_count) 的清零数组；逐组求
    // 已排序分量集合的交，复杂度为组成员排序与实际候选交集之和。
    std::vector<int> candidates =
        GroupComponents(query.groups.front(), *component_of, graph.n);
    for (size_t group = 1; group < query.groups.size() && !candidates.empty(); ++group)
    {
        const std::vector<int> current =
            GroupComponents(query.groups[group], *component_of, graph.n);
        std::vector<int> intersection;
        intersection.reserve(std::min(candidates.size(), current.size()));
        std::set_intersection(candidates.begin(),
                              candidates.end(),
                              current.begin(),
                              current.end(),
                              std::back_inserter(intersection));
        candidates.swap(intersection);
    }
    return !candidates.empty();
}

}  // namespace gst
