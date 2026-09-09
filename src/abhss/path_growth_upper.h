#pragma once

#include "internal.h"

namespace abhss {
// 路径生长得到的真实可行上界及其连通原图边 witness。
struct PathGrowthUpper {
    double upper = fp::kInf;
    std::vector<int> edge_ids;
};

// 枚举有序组三元组，以一步前瞻后的真实最短路生长构造公共可行上界。
// 两种模式调用同一实现；cutoff 仅用于非负累计费用的单调安全终止。返回的有限值及 edge_ids 始终描述同一真实连通子图；无改进时 upper 为无穷。
PathGrowthUpper BuildTripleSeededPathGrowthUpper(const Graph& graph, const Query& query, const GroupTable& distance, double cutoff);

// 在已购买的证书刷新中再固定一个第四组，然后复用同一真实路径生长尾部。
PathGrowthUpper BuildQuadSeededPathGrowthUpper(const Graph& graph, const Query& query, const GroupTable& distance, double cutoff);
}
