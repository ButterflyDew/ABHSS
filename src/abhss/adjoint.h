#ifndef ABHSS_ADJOINT_H
#define ABHSS_ADJOINT_H

#include "forward.h"

namespace gst::methods::abhss::internal
{
/**
 * @brief 用补集转置终端和反向图闭包完成未物化的高层锚定状态。
 * @param problem 已完成预处理与 ordinary D 的查询上下文；必须启用
 *        DirectedCut 和 AdjointCompletion。
 * @param anchored 已物化到 `low_last` 的前向 A row。
 * @param low_last 前向保留的最高非锚组数。
 * @param high_last 完整前向格原本需要达到的最高非锚组数。
 * @param probe_method 稳定配置名，仅在诊断编译中写入 phase 记录。
 *
 * 函数从 `high_last` 递减到 `low_last+1` 生成 H，并在每张 row 完成后与
 * 低层 A、ordinary branch 做边界结算。它是基础完整前向完成式的可选
 * 加速操作；关闭该开关时统一入口直接执行完整前向 A。
 */
void SolveHighAdjoint(Problem& problem,
                      const std::vector<Row>& anchored,
                      int low_last,
                      int high_last,
                      const char* probe_method);

}  // namespace gst::methods::abhss::internal

#endif  // ABHSS_ADJOINT_H
