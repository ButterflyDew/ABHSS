#ifndef PRUNEDDP_H
#define PRUNEDDP_H

#include "../common/graph_io.h"
#include "../common/query_io.h"

namespace gst::methods::pruned_dp
{

// PrunedDP++ 论文未公开源码，稀疏状态存储的具体容器不明确。
// Hash 是本仓库的默认复现口径；Dense 仅用于消融。
enum class StateStorage
{
    Hash,
    Dense,
};

// 三个选项是论文复现歧义，不是 ABHSS 的算法开关。
struct PrunedDpOptions
{
    StateStorage state_storage = StateStorage::Hash;
    bool use_mst_upper_bound = true;
    // Safe 精确默认值保留原始 admissible 下界，并在发现更小 g-value 时
    // reopen 状态。论文 pathmax 行为仍可用 --lb2-pathmax=on 显式复现。
    bool enforce_lb2_pathmax = false;
};

struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
};

// 使用默认论文复现口径求解一条 GST 询问。
SolveResult SolveOneQuery(const Graph& graph, const Query& query);
// 使用显式复现选项求解，用于论文主对照与消融。
SolveResult SolveOneQuery(const Graph& graph,
                          const Query& query,
                          const PrunedDpOptions& options);

// 返回稳定的 CLI/header 存储名。
const char* StateStorageName(StateStorage storage);

}  // namespace gst::methods::pruned_dp

#endif  // PRUNEDDP_H
