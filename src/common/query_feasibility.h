#ifndef GST_QUERY_FEASIBILITY_H
#define GST_QUERY_FEASIBILITY_H

#include "graph_io.h"
#include "query_io.h"

namespace gst
{

/**
 * @brief 判断是否存在一个与每个查询组均相交的无向连通分量。
 *
 * 直接复用图加载阶段建立的 `Graph::component_of`，复杂度由查询组成员数
 * 主导；输入文件须满足 README 中给出的顶点范围与非空组约定。
 */
bool IsQueryFeasible(const Graph& graph, const Query& query);

} // namespace gst

#endif // GST_QUERY_FEASIBILITY_H
