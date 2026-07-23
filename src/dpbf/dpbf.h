#ifndef GST_DPBF_H
#define GST_DPBF_H

#include "../common/graph_io.h"
#include "../common/query_io.h"

namespace gst::methods::dpbf
{

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
};

/**
 * @brief 用规范 Dreyfus–Wagner/DPBF 递推求一条边权 GST 查询。
 *
 * 实现刻意保持透明并使用稠密全子集表，只作为 correctness baseline，
 * 不定位为大图性能竞争者；超过内部 80M cell 安全上限会明确拒绝。
 */
SolveResult SolveOneQuery(const Graph& graph, const Query& query);

}  // namespace gst::methods::dpbf

#endif  // GST_DPBF_H
