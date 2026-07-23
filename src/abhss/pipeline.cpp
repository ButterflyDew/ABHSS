#include "pipeline.h"

#include <stdexcept>
#include <string>

#include "diagnostics.h"
#include "../common/query_feasibility.h"

namespace gst::methods::abhss::internal
{
/** @brief 实现公共平凡/不可行查询前置处理；详细契约见 pipeline.h。 */
bool ResolveQueryPrelude(const Graph& graph,
                         const Query& query,
                         const char* algorithm_name,
                         SolveResult& answer)
{
    const int g = static_cast<int>(query.groups.size());
    if (!g)
    {
        answer = {0.0, true};
        return true;
    }
    if (g > 16)
        throw std::runtime_error(std::string(algorithm_name) +
                                 " supports group count <= 16.");
    if (!IsQueryFeasible(graph, query))
    {
        answer = {};
        return true;
    }
    if (g == 1)
    {
        answer = {0.0, true};
        return true;
    }
    return false;
}

/** @brief 以统一诊断边界调用 `PrepareProblem`，不改变其闭合语义。 */
bool PrepareWithProbe(Problem& problem, const char* probe_method)
{
    ProbeTimer timer;
    EmitAbhssProbe(probe_method, "prepare_start", problem);
    const bool closed = PrepareProblem(problem);
    EmitAbhssProbe(probe_method,
                   closed ? "prepare_closed" : "prepare_end",
                   problem,
                   timer.Seconds());
    return closed;
}

/** @brief 以统一诊断边界调用 ordinary 构造，并统计最终 row payload。 */
void BuildOrdinaryWithProbe(Problem& problem,
                            EarlyAnchor* early,
                            const char* probe_method)
{
    ProbeTimer timer;
    EmitAbhssProbe(probe_method, "ordinary_start", problem);
    BuildOrdinaryRows(problem, early);
    EmitAbhssProbe(
        probe_method,
        "ordinary_end",
        problem,
        timer.Seconds(),
        &problem.ordinary);
}

}  // namespace gst::methods::abhss::internal
