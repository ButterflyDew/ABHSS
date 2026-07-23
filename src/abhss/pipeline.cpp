#include "pipeline.h"

#include <stdexcept>
#include <string>

#include "diagnostics.h"
#include "../common/query_feasibility.h"

namespace gst::methods::abhss::internal
{
bool ResolveQueryPrelude(const Graph& graph,
                         const Query& query,
                         const char* method,
                         SolveResult& answer)
{
    const int g = static_cast<int>(query.groups.size());
    if (!g)
    {
        answer = {0.0, true};
        return true;
    }
    if (g > 16)
        throw std::runtime_error(std::string(method) +
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

bool PrepareWithProbe(Problem& problem, const char* method)
{
    ProbeTimer timer;
    EmitAbhssProbe(method, "prepare_start", problem);
    const bool closed = PrepareProblem(problem);
    EmitAbhssProbe(method,
                   closed ? "prepare_closed" : "prepare_end",
                   problem,
                   timer.Seconds());
    return closed;
}

void BuildOrdinaryWithProbe(Problem& problem,
                            EarlyAnchor* early,
                            const char* method)
{
    ProbeTimer timer;
    EmitAbhssProbe(method, "ordinary_start", problem);
    BuildOrdinaryRows(problem, early);
    EmitAbhssProbe(
        method, "ordinary_end", problem, timer.Seconds(), &problem.ordinary);
}

}  // namespace gst::methods::abhss::internal
