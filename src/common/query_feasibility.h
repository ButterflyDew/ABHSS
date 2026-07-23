#ifndef GST_QUERY_FEASIBILITY_H
#define GST_QUERY_FEASIBILITY_H

#include "graph_io.h"
#include "query_io.h"

namespace gst
{

/**
 * @brief 判断是否存在一个与每个查询组均相交的无向连通分量。
 *
 * 正式加载的图直接读取 `Graph::component_of`，复杂度由查询组成员数主导；
 * 手工构造且未建立缓存的小图会在本次调用内回退计算分量。函数同时验证
 * 全部查询顶点范围，空查询可行，任何空组使非空查询不可行。
 */
bool IsQueryFeasible(const Graph& graph, const Query& query);

}  // namespace gst

#endif  // GST_QUERY_FEASIBILITY_H
