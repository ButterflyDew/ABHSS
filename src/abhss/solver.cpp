#include "abhss.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include "../common/query_feasibility.h"
#include "adjoint.h"
#include "core.h"
#include "forward.h"
#include "internal.h"

namespace gst::methods::abhss
{
namespace
{
using namespace internal;

// 处理不需要指数 DP 的数学边界情形；返回 true 表示答案已经确定。
bool SolveTrivialQuery(const Graph& graph, const Query& query, SolveResult& answer)
{
    const int g = static_cast<int>(query.groups.size());
    if (g == 0 || g == 1)
    {
        answer = {0.0, true, 0};
        return true;
    }
    assert(g <= 16);
    if (!IsQueryFeasible(graph, query))
    {
        answer = {};
        return true;
    }
    return false;
}

// 若锚定格含正层，就在 ordinary D 前构造 Base/Enhanced 共用的 A1。
AnchoredSingletonFuture* BuildCommonA1(Problem& problem, int highest_layer, AnchoredSingletonFuture& future, WitnessUpperScheduler& scheduler)
{
    if (highest_layer == 0)
        return nullptr;
    BuildReusableAnchoredSingletonLayer(problem, future, scheduler);
    return &future;
}

// Base 用完整前向 A 格覆盖全部锚定层，最后一层只结算答案。
void FinishBase(Problem& problem, int highest_layer, std::vector<Row> first_layer)
{
    ForwardAnchoredPlan plan;
    plan.last_size = highest_layer;
    plan.complete_implicit_anchor = highest_layer == 0;
    plan.retain_last_layer = false;
    BuildForwardAnchoredRows(problem, plan, std::move(first_layer));
}

// Enhanced 保留低层前向 A，再用补集转置的 H 完成剩余高层。
void FinishEnhanced(Problem& problem, int highest_layer, std::vector<Row> first_layer)
{
    const int low_last = highest_layer == 0 ? 0 : std::max(1, highest_layer / 2);
    ForwardAnchoredPlan plan;
    plan.last_size = low_last;
    plan.complete_implicit_anchor = highest_layer == 0;
    plan.retain_last_layer = true;
    std::vector<Row> anchored = BuildForwardAnchoredRows(problem, plan, std::move(first_layer));
    if (low_last < highest_layer)
        SolveHighAdjoint(problem, anchored, low_last, highest_layer);
}
} // namespace

// 公共预处理、A1 和 ordinary D 只写一次，最后由 enhanced 选择完成方式。
SolveResult SolveOneQuery(const Graph& graph, const Query& query, bool enhanced)
{
    SolveResult trivial;
    if (SolveTrivialQuery(graph, query, trivial))
        return trivial;

    Problem problem(graph, query, enhanced);
    if (PrepareProblem(problem))
        return {problem.best, true, problem.mask_vertex_states};

    const int highest_layer = std::max(0, problem.g / 2 - 1);
    WitnessUpperScheduler scheduler(problem);
    AnchoredSingletonFuture singleton_future;
    AnchoredSingletonFuture* ordinary_future = BuildCommonA1(problem, highest_layer, singleton_future, scheduler);
    BuildOrdinaryRows(problem, ordinary_future, scheduler);
    singleton_future.ReleaseLookupCache();

    if (enhanced)
        FinishEnhanced(problem, highest_layer, std::move(singleton_future.row));
    else
        FinishBase(problem, highest_layer, std::move(singleton_future.row));

    return {problem.best, problem.best < fp::kInf / 4, problem.mask_vertex_states};
}

} // namespace gst::methods::abhss
