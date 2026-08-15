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
 * @param high_last H 物化的最高层；包含精确转置被省略 D 半格的辅助层。
 * @param probe_method 稳定配置名，仅在诊断编译中写入 phase 记录。
 *
 * 函数从 high_last 递减到 low_last+1 生成 H，并在每张 row 完成后与低层 A、ordinary branch
 * 做边界结算。ordinary 已完整保留到最高逻辑层；辅助 H 半格先实现被省略 D 半格的同一递推，
 * 再由递减 H 替换完整前向高层。关闭 AdjointCompletion 时
 * 统一入口恢复完整前向 A。
 */
void SolveHighAdjoint(Problem& problem,
                      const std::vector<Row>& anchored,
                      int low_last,
                      int high_last,
                      const char* probe_method);

}  // namespace gst::methods::abhss::internal

#endif  // ABHSS_ADJOINT_H
