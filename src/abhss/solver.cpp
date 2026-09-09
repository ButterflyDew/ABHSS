#include "abhss.h"
#include "core.h"
#include "forward.h"
#include "adjoint.h"

namespace abhss {

// 依次完成共同预处理、A1、ordinary D，再以前向 A 或伴随 H 结算答案。
SolveResult SolveOneQuery(const Graph& graph, const Query& query, bool enhanced) {
    if (query.groups.empty()) return {0.0, true, 0};
    if (!IsQueryFeasible(graph, query)) return {};
    if (query.groups.size() == 1) return {0.0, true, 0};

    Problem problem(graph, query, enhanced);
    if (PrepareProblem(problem)) return {problem.best, true, problem.mask_vertex_states};

    // q 是平衡分解要求的最高锚定层。Enhanced 保留 A1，非空后缀由 H 完成。
    const int q = std::max(0, problem.half - 1);
    const int forward_last = enhanced ? std::min(1, q) : q;
    const bool use_h = forward_last < q;
    const int ordinary_last = use_h ? q : problem.half;

    // 两边的 witness rent 均从零开始；同一个 A1 构造器建立可复用的未来下界。
    WitnessUpperScheduler witness(problem);
    AnchoredSingletonFuture future;
    if (q > 0) BuildReusableAnchoredSingletonLayer(problem, future, witness);
    ResidualClosureScheduler closure(problem);
    closure.Account(witness.TotalWork());
    BuildOrdinaryRows(problem, q > 0 ? &future : nullptr, witness, closure, ordinary_last);
    future.ReleaseLookupCache();

    // H 非空时，辅助半层 H 精确替代省略的 D 半层；没有 H 消费者时不留末层 A。
    auto anchored = BuildForwardAnchoredRows(problem, forward_last, use_h, std::move(future.row));
    if (use_h) SolveHighAdjoint(problem, anchored, forward_last, problem.half);
    return {problem.best, problem.best < fp::kInf / 4, problem.mask_vertex_states};
}

} // namespace abhss
