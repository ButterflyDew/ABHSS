#include "abhss.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "adjoint.h"
#include "diagnostics.h"
#include "pipeline.h"

namespace gst::methods::abhss
{
namespace
{
using namespace internal;

constexpr std::uint32_t kKnownEnhancements =
    EnhancementBit(Enhancement::DirectedCut) |
    EnhancementBit(Enhancement::AdjointCompletion);

/**
 * @brief 将合法配置映射到稳定的 probe 方法名。
 *
 * probe 名只用于工程诊断；正式关闭诊断时调用会被优化掉。名称显式包含
 * configuration，避免再次把基础/增强配置误读为两个算法实现。
 */
const char* ProbeMethodName(const SolveOptions& options)
{
    if (options.Enabled(Enhancement::AdjointCompletion))
        return "abhss_config_enhanced";
    if (options.Enabled(Enhancement::DirectedCut))
        return "abhss_config_directed_cut_only";
    return "abhss_config_base";
}

/**
 * @brief 为基础配置构造 early-A1，并返回 ordinary 阶段可读的证书指针。
 *
 * 开启 directed-cut 后，完整组距离和对偶势已经承担 future 下界与初始
 * primal 上界，故中间/完整增强配置跳过该基础固定成本。返回指针仅在
 * `early` 的生命周期内使用，不发生所有权转移。
 */
EarlyAnchor* BuildOptionalEarlyAnchor(Problem& problem,
                                      EarlyAnchor& early,
                                      const char* probe_method)
{
    if (problem.UsesDirectedCut())
        return nullptr;

    ProbeTimer timer;
    EmitAbhssProbe(probe_method, "early_anchor_start", problem);
    BuildEarlyAnchorRows(problem, early);
    EmitAbhssProbe(probe_method,
                   "early_anchor_end",
                   problem,
                   timer.Seconds(),
                   &early.row);
    return &early;
}

/**
 * @brief 使用完整前向 A 格完成基础或 directed-cut-only 配置。
 *
 * 基础配置把已经精确闭包的 early-A1 row 移交给公共前向内核；只开
 * directed-cut 时 `initial_rows` 为空，恰好形成验证 adjoint 增益的消融。
 * 最后一层只用于结算答案，不保留 payload，与重构前行为一致。
 */
void CompleteWithForwardGrid(Problem& problem,
                             std::vector<Row> initial_rows,
                             const char* probe_method)
{
    ForwardAnchoredPlan plan;
    plan.last_size = problem.half - 1;
    plan.retain_last_layer = false;
    plan.probe_method = probe_method;
    plan.probe_phase = "full_anchor_layer";
    RunForwardAnchoredStage(problem,
                            plan,
                            "full_anchor_start",
                            "full_anchor_end",
                            std::move(initial_rows));
}

/**
 * @brief 使用低层前向 A 与高层 adjoint H 完成全部锚定状态。
 *
 * 切分点只由组数确定，不观察 row 密度或运行时间。该函数是
 * `AdjointCompletion` 开关唯一的调度入口；关闭开关即回到
 * `CompleteWithForwardGrid`，不会保留第二套 ordinary 递推。
 */
void CompleteWithAdjoint(Problem& problem, const char* probe_method)
{
    const int high_last = problem.half - 1;
    const int low_last = std::max(0, high_last / 2);

    ForwardAnchoredPlan plan;
    plan.last_size = low_last;
    plan.retain_last_layer = true;
    plan.probe_method = probe_method;
    plan.probe_phase = "low_anchor_layer";
    std::vector<Row> anchored = RunForwardAnchoredStage(
        problem, plan, "low_anchor_start", "low_anchor_end");

    ProbeTimer timer;
    EmitAbhssProbe(probe_method, "adjoint_start", problem);
    SolveHighAdjoint(
        problem, anchored, low_last, high_last, probe_method);
    EmitAbhssProbe(
        probe_method, "adjoint_end", problem, timer.Seconds());
}
}  // namespace

/** @brief 实现公开的配置依赖检查，并拒绝未定义的高位。 */
bool IsValid(const SolveOptions& options)
{
    if (options.enhancements & ~kKnownEnhancements)
        return false;
    return !options.Enabled(Enhancement::AdjointCompletion) ||
           options.Enabled(Enhancement::DirectedCut);
}

/** @brief 实现公开的稳定配置命名，不执行任何自动配置推断。 */
const char* ConfigurationName(const SolveOptions& options)
{
    if (!IsValid(options))
        return "invalid";
    if (options.Enabled(Enhancement::AdjointCompletion))
        return "enhanced";
    if (options.Enabled(Enhancement::DirectedCut))
        return "directed_cut_only";
    return "base";
}

/**
 * @brief 单一 ABHSS 入口：先执行公共 DP，再按预声明开关选择完成策略。
 *
 * 这里是配置分派的唯一位置。所有开关在 `Problem` 构造后只读；普通 D、
 * row 表示、下界接口和完成结算始终共用。因而从 Enhanced 关闭 adjoint、
 * 再关闭 directed-cut，会沿同一函数严格回到 Base 的调用序列。
 */
SolveResult SolveOneQuery(const Graph& graph,
                          const Query& query,
                          const SolveOptions& options)
{
    if (!IsValid(options))
        throw std::invalid_argument(
            "ABHSS adjoint-completion requires directed-cut and no unknown flags.");

    SolveResult trivial;
    if (ResolveQueryPrelude(graph, query, "ABHSS", trivial))
        return trivial;

    const char* probe_method = ProbeMethodName(options);
    Problem problem(graph, query, options);
    if (PrepareWithProbe(problem, probe_method))
        return {problem.best, true, problem.mask_vertex_states};

    EarlyAnchor early;
    EarlyAnchor* ordinary_early =
        BuildOptionalEarlyAnchor(problem, early, probe_method);
    BuildOrdinaryWithProbe(problem, ordinary_early, probe_method);

    if (problem.UsesAdjointCompletion())
        CompleteWithAdjoint(problem, probe_method);
    else
        CompleteWithForwardGrid(problem, std::move(early.row), probe_method);

    return {problem.best,
            problem.best < fp::kInf / 4,
            problem.mask_vertex_states};
}

}  // namespace gst::methods::abhss
