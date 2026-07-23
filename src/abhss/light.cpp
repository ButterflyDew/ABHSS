#include "forward.h"

#include <utility>

#include "diagnostics.h"
#include "pipeline.h"

namespace gst::methods::abhss
{
namespace
{
using namespace internal;

const char* LightProbeMethod()
{
#if defined(ABHSS_ABLATE_NO_EARLY)
    return "abhss_light_no_early";
#elif defined(ABHSS_ABLATE_NO_WITNESS)
    return "abhss_light_no_witness";
#else
    return "abhss_light";
#endif
}

}  // namespace

SolveResult SolveLightOneQuery(const Graph& graph, const Query& query)
{
    SolveResult trivial;
    if (ResolveQueryPrelude(graph, query, "ABHSSLight", trivial))
        return trivial;

    const char* method = LightProbeMethod();
    Problem problem(graph, query, SolverVariant::Light);
    if (PrepareWithProbe(problem, method))
        return {problem.best, true};

#if defined(ABHSS_ABLATE_NO_WITNESS)
    // Keep all preparation upper bounds but disable recurring tree evaluation.
    problem.witness_tree = {};
#endif

    EarlyAnchor early;
    EarlyAnchor* ordinary_early = nullptr;
#if defined(ABHSS_ABLATE_NO_EARLY)
    early.row.assign(problem.subset_count, {});
#else
    {
        ProbeTimer timer;
        EmitAbhssProbe(method, "early_anchor_start", problem);
        BuildEarlyAnchorRows(problem, early);
        EmitAbhssProbe(
            method, "early_anchor_end", problem, timer.Seconds(), &early.row);
    }
    ordinary_early = &early;
#endif
    BuildOrdinaryWithProbe(problem, ordinary_early, method);

    ForwardAnchoredPlan plan;
    plan.last_size = problem.half - 1;
    plan.retain_last_layer = false;
    plan.probe_method = method;
    plan.probe_phase = "anchored_layer";
    RunForwardAnchoredStage(problem,
                            plan,
                            "anchored_start",
                            "anchored_end",
                            std::move(early.row));
    return {problem.best, problem.best < fp::kInf / 4};
}

SolveResult SolveHeavyForwardOneQuery(const Graph& graph, const Query& query)
{
    SolveResult trivial;
    if (ResolveQueryPrelude(graph, query, "ABHSSHeavyForward", trivial))
        return trivial;

    constexpr const char* method = "abhss_heavy_forward";
    Problem problem(graph, query, SolverVariant::Heavy);
    if (PrepareWithProbe(problem, method))
        return {problem.best, true};

    BuildOrdinaryWithProbe(problem, nullptr, method);
    ForwardAnchoredPlan plan;
    plan.last_size = problem.half - 1;
    plan.retain_last_layer = false;
    plan.probe_method = method;
    plan.probe_phase = "anchored_layer";
    RunForwardAnchoredStage(
        problem, plan, "anchored_start", "anchored_end");
    return {problem.best, problem.best < fp::kInf / 4};
}

}  // namespace gst::methods::abhss
