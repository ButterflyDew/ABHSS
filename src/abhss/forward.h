#ifndef ABHSS_FORWARD_H
#define ABHSS_FORWARD_H

#include "core.h"

namespace gst::methods::abhss::internal
{
/**
 * @brief 公共前向锚定内核的物化计划。
 *
 * 基础配置请求完整前缀并在最后一层结算后释放；adjoint 增强只请求低层
 * 前缀并保留切分边界。计划在查询开始前由固定开关推导，不观察状态密度。
 */
struct ForwardAnchoredPlan
{
    int last_size = 0;
    bool retain_last_layer = true;
    const char* probe_method = nullptr;
    const char* probe_phase = "forward_anchor_layer";
};

/**
 * @brief 按计划生成前向锚定 A(S,v) row 并持续更新全局可行上界。
 * @param initial_rows 空表或 `subset_count` 大小的已精确闭包 early-A1 表。
 * @return 需要保留的前向 row；最后层是否保留由 plan 决定。
 *
 * `initial_rows` 按值接收并转移所有权，因为 ordinary 阶段结束后 early row
 * 不再有其他消费者。全部配置调用这一份 seed、图闭包和完成结算实现。
 */
std::vector<Row> BuildForwardAnchoredRows(
    Problem& problem,
    const ForwardAnchoredPlan& plan,
    std::vector<Row> initial_rows = {});

/**
 * @brief 在统一 start/end probe 边界内调用公共前向锚定内核。
 *
 * 该函数只负责阶段计时和 row 统计；正式未开启诊断时计时代码编译为空，
 * 因而不会给基础或增强配置引入不同的工程开销。
 */
std::vector<Row> RunForwardAnchoredStage(
    Problem& problem,
    const ForwardAnchoredPlan& plan,
    const char* start_phase,
    const char* end_phase,
    std::vector<Row> initial_rows = {});

}  // namespace gst::methods::abhss::internal

#endif  // ABHSS_FORWARD_H
