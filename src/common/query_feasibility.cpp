#include "query_feasibility.h"

#include <algorithm>
#include <iterator>
#include <vector>

namespace gst
{
namespace
{

/** @brief 提取一个查询组命中的去重、递增连通分量编号。 */
std::vector<int> GroupComponents(const std::vector<int>& group, const std::vector<int>& component_of)
{
    std::vector<int> result;
    result.reserve(group.size());
    for (int vertex : group)
        result.push_back(component_of[vertex]);
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

} // namespace

/** @brief 判断是否存在一个同时命中所有查询组的无向连通分量。 */
bool IsQueryFeasible(const Graph& graph, const Query& query)
{
    if (query.groups.empty() || graph.component_count == 1)
        return true;

    std::vector<int> candidates = GroupComponents(query.groups.front(), graph.component_of);
    for (size_t group = 1; group < query.groups.size() && !candidates.empty(); ++group)
    {
        const std::vector<int> current = GroupComponents(query.groups[group], graph.component_of);
        std::vector<int> intersection;
        intersection.reserve(std::min(candidates.size(), current.size()));
        std::set_intersection(candidates.begin(), candidates.end(), current.begin(), current.end(), std::back_inserter(intersection));
        candidates.swap(intersection);
    }
    return !candidates.empty();
}

} // namespace gst
