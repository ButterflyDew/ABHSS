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
 * @brief 按冻结配置调度公共 A1，并返回 ordinary 阶段可读的 future 视图。
 *
 * 只要完整锚定状态格包含 A1，所有合法配置都在 D 前生成这一公共层，使它
 * 服务全部非平凡 D 层，并在随后移交给前向 A 内核。该调度器与 A1 构造
 * 都不读取 enhancement profile；DirectedCut/Enhanced 只在后续 ordinary
 * future 中额外取 dual potential 的最大值。条件式树 DP 若收紧上界，只会
 * 让共同构造器丢弃未完成 pass 并整轮重启；最终逻辑 row 仍只发布一次。
 */
AnchoredSingletonFuture* ScheduleAnchoredSingletonFuture(
    Problem& problem,
    const AnchoredCompletionSchedule& schedule,
    AnchoredSingletonFuture& singleton_future,
    WitnessUpperScheduler& witness_scheduler,
    const char* probe_method)
{
    // A1 不在逻辑状态格中时没有可登记的层；这发生于平衡完成式只需隐式
    // A(0) 的查询。除此之外所有配置一律执行同一 A1 操作，不使用 g 阈值。
    if (!schedule.ContainsLogicalLayer(1))
        return nullptr;

    ProbeTimer timer;
    EmitAbhssProbe(probe_method, "singleton_anchor_start", problem);
    BuildReusableAnchoredSingletonLayer(
        problem, singleton_future, witness_scheduler);
    EmitAbhssProbe(probe_method,
                   "singleton_anchor_end",
                   problem,
                   timer.Seconds(),
                   &singleton_future.row);
    return &singleton_future;
}

/**
 * @brief 在没有非空 H 后缀时使用完整前向 A 格完成查询。
 *
 * 所有在当前逻辑域没有非空 H 消费者的配置都把提前调度且已经精确闭包的
 * 同一 A1 row 移交给公共前向内核。最后一层只用于结算答案，不保留 payload。
 */
void CompleteWithForwardGrid(Problem& problem,
                             const AnchoredCompletionSchedule& schedule,
                             std::vector<Row> initial_rows,
                             const char* probe_method)
{
    ForwardAnchoredPlan plan;
    plan.last_size = schedule.forward_last_layer;
    plan.complete_implicit_anchor =
        !schedule.ContainsLogicalLayer(1);
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
 * 切分点由平衡分解所需的最高逻辑层确定，不观察图名、row 密度或运行时间。该函数是
 * `AdjointCompletion` 开关唯一的调度入口；关闭开关即回到
 * `CompleteWithForwardGrid`，不会保留第二套 ordinary 递推。
 */
void CompleteWithAdjoint(Problem& problem,
                         const AnchoredCompletionSchedule& schedule,
                         std::vector<Row> initial_rows,
                         const char* probe_method)
{
    const int high_last = schedule.highest_layer;
    const int low_last = schedule.forward_last_layer;

    ForwardAnchoredPlan plan;
    plan.last_size = low_last;
    plan.complete_implicit_anchor =
        !schedule.ContainsLogicalLayer(1);
    plan.retain_last_layer = true;
    plan.probe_method = probe_method;
    plan.probe_phase = "low_anchor_layer";
    std::vector<Row> anchored = RunForwardAnchoredStage(
        problem,
        plan,
        "low_anchor_start",
        "low_anchor_end",
        std::move(initial_rows));

    ProbeTimer timer;
    EmitAbhssProbe(probe_method, "adjoint_start", problem);
    SolveHighAdjoint(
        problem,
        anchored,
        low_last,
        high_last,
        schedule.requires_three_block_terminal,
        probe_method);
    EmitAbhssProbe(
        probe_method, "adjoint_end", problem, timer.Seconds());
}
}  // namespace

/** @brief 实现公开的配置依赖检查，并拒绝未定义的高位。 */
bool IsValid(const SolveOptions& options)
{
    return DescribeConfiguration(options).valid;
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

    const ConfigurationProfile profile = DescribeConfiguration(options);
    const AnchoredCompletionSchedule completion_schedule =
        MakeAnchoredCompletionSchedule(problem.g, profile);

    // 预处理完成后才创建；构造函数只计算共同 buy，rent 严格从 0 开始。
    // 同一个对象随后跨越公共 A1 与 ordinary D，防止两配置各自维护调度器。
    WitnessUpperScheduler witness_scheduler(problem);
    AnchoredSingletonFuture singleton_future;
    AnchoredSingletonFuture* ordinary_singleton_future =
        ScheduleAnchoredSingletonFuture(
            problem,
            completion_schedule,
            singleton_future,
            witness_scheduler,
            probe_method);
    // A1 本身仍逐项执行共同代码；这里只在 A1 完成后把已经发生的公共搜索
    // 工作一次性交给增强证书调度器，使困难查询不必用弱 dual 重做多张 D row。
    ResidualClosureScheduler closure_scheduler(problem);
    closure_scheduler.Account(witness_scheduler.TotalWork());
    BuildOrdinaryWithProbe(
        problem,
        ordinary_singleton_future,
        witness_scheduler,
        closure_scheduler,
        completion_schedule.ordinary_last_layer,
        probe_method);
    singleton_future.ReleaseLookupCache();

    // Adjoint realization 的逻辑后缀为空时，完整前向完成就是同一职责的空域退化；
    // 直接复用公共入口，避免保留一张没有 H 消费者的末层 A row。
    if (completion_schedule.UsesAdjointH(completion_schedule.highest_layer))
        CompleteWithAdjoint(
            problem,
            completion_schedule,
            std::move(singleton_future.row),
            probe_method);
    else
        CompleteWithForwardGrid(
            problem,
            completion_schedule,
            std::move(singleton_future.row),
            probe_method);

    return {problem.best,
            problem.best < fp::kInf / 4,
            problem.mask_vertex_states};
}

}  // namespace gst::methods::abhss
