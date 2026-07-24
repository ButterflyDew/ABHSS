#ifndef ABHSS_PIPELINE_H
#define ABHSS_PIPELINE_H

#include "core.h"

namespace gst::methods::abhss::internal
{
/**
 * @brief 处理空组、单组、组数上限和跨连通分量不可行等公共前置情况。
 * @param algorithm_name 只用于错误信息的算法族名，不影响配置或求解路径。
 * @param answer 若返回 true，则写入无需指数搜索即可确定的最终答案。
 * @return true 表示调用者应立即返回 `answer`；false 表示继续完整管线。
 */
bool ResolveQueryPrelude(const Graph& graph,
                         const Query& query,
                         const char* algorithm_name,
                         SolveResult& answer);

/**
 * @brief 在统一 probe 边界内执行全部配置共享的查询预处理。
 * @return true 表示上下界已闭合，`problem.best` 已是精确答案。
 */
bool PrepareWithProbe(Problem& problem, const char* probe_method);

/**
 * @brief 在统一 probe 边界内生成 ordinary D row。
 * @param singleton_future Base 中已存在的公共 A1 future 视图；若锚定格没有
 *        A1，或 DirectedCut 用 dual future 替换该职责，则传 nullptr。
 *
 * 该包装器只统一计时和诊断字段，不改变 `BuildOrdinaryRows` 的状态语义。
 */
void BuildOrdinaryWithProbe(Problem& problem,
                            AnchoredSingletonFuture* singleton_future,
                            const char* probe_method);

}  // namespace gst::methods::abhss::internal

#endif  // ABHSS_PIPELINE_H
