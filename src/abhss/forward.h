#ifndef ABHSS_FORWARD_H
#define ABHSS_FORWARD_H

#include "core.h"

namespace gst::methods::abhss::internal
{
/**
 * @brief 公共前向锚定内核的物化计划。
 *
 * Base 请求完整前缀并在最后一层结算后释放；Enhanced 只请求低层
 * 前缀并保留切分边界。计划在查询开始前由固定开关推导，不观察状态密度。
 */
struct ForwardAnchoredPlan
{
    int last_size = 0;
    // 仅当完整逻辑正层域为空时，直接以隐式 A(0) 做最终结算。该位由完整
    // completion schedule 给出，不能用 low prefix 为空或某个 g 阈值代替。
    bool complete_implicit_anchor = false;
    bool retain_last_layer = true;
};

/**
 * @brief 按计划生成前向锚定 A(S,v) row 并持续更新全局可行上界。
 * @param initial_rows 空表或 `subset_count` 大小、由本内核提前生成的 A1 表。
 * @return 需要保留的前向 row；最后层是否保留由 plan 决定。
 *
 * `initial_rows` 按值接收并转移所有权，因为 ordinary 阶段结束后 singleton
 * future 不再有其他消费者。两种模式调用同一份 seed、图闭包和结算实现。
 */
std::vector<Row> BuildForwardAnchoredRows(Problem& problem, const ForwardAnchoredPlan& plan, std::vector<Row> initial_rows = {});

} // namespace gst::methods::abhss::internal

#endif // ABHSS_FORWARD_H
