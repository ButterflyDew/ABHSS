#ifndef ABHSS_PATH_GROWTH_UPPER_H
#define ABHSS_PATH_GROWTH_UPPER_H

#include "internal.h"

namespace gst::methods::abhss::internal
{
/** @brief 路径生长得到的真实可行上界及其连通原图边 witness。 */
struct PathGrowthUpper
{
    double upper = fp::kInf;
    std::vector<int> edge_ids;
};

/**
 * @brief 枚举有序组三元组，以一步前瞻后的真实最短路生长构造公共可行上界。
 *
 * 所有合法配置调用同一实现；cutoff 仅用于非负累计费用的单调安全终止。
 * 返回的有限值及 edge_ids 始终描述同一真实连通子图；无改进时 upper 为无穷。
 */
PathGrowthUpper BuildTripleSeededPathGrowthUpper(const Graph& graph, const Query& query, const GroupTable& distance, double cutoff);

/** @brief 在已购买的证书刷新中再固定一个第四组，然后复用同一真实路径生长尾部。 */
PathGrowthUpper BuildQuadSeededPathGrowthUpper(const Graph& graph, const Query& query, const GroupTable& distance, double cutoff);
}

#endif
