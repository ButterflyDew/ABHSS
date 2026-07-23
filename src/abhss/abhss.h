#ifndef ABHSS_PUBLIC_H
#define ABHSS_PUBLIC_H

#include <cstdint>

#include "../common/graph_io.h"
#include "../common/query_io.h"

namespace gst::methods::abhss
{
/**
 * @brief ABHSS 可以按位开启的增强操作。
 *
 * 基础配置不启用任何增强；完整配置依次加入 directed-cut 对偶预处理和
 * adjoint 高层求值。后者依赖前者，因此合法配置形成
 * none -> directed-cut -> directed-cut+adjoint 的单调链。枚举值只描述
 * 可复现的算法操作，不读取图名、查询编号或运行时表现。
 */
enum class Enhancement : std::uint32_t
{
    DirectedCut = std::uint32_t{1} << 0,
    AdjointCompletion = std::uint32_t{1} << 1,
};

/** @brief 返回增强枚举对应的位掩码，集中处理强类型枚举到整数的转换。 */
constexpr std::uint32_t EnhancementBit(Enhancement enhancement)
{
    return static_cast<std::uint32_t>(enhancement);
}

/**
 * @brief 一次查询开始前冻结的 ABHSS 配置。
 *
 * `Base()` 是不启用增强的论文配置；`Enhanced()` 在同一算法上开启全部
 * 增强，复现重构前的高强度路径。配置在求解开始后只读，严禁依据中间状态或
 * 已消耗时间切换。`With` 返回新值，便于消融明确表达“从基础配置加操作”。
 */
struct SolveOptions
{
    std::uint32_t enhancements = 0;

    /** @brief 构造不启用增强的基础配置。 */
    static constexpr SolveOptions Base()
    {
        return {};
    }

    /** @brief 构造只启用 directed-cut 的中间消融配置。 */
    static constexpr SolveOptions DirectedCutOnly()
    {
        return {EnhancementBit(Enhancement::DirectedCut)};
    }

    /** @brief 构造启用全部增强的完整配置。 */
    static constexpr SolveOptions Enhanced()
    {
        return {EnhancementBit(Enhancement::DirectedCut) |
                EnhancementBit(Enhancement::AdjointCompletion)};
    }

    /** @brief 判断指定增强是否已开启；该查询不修改配置。 */
    constexpr bool Enabled(Enhancement enhancement) const
    {
        return (enhancements & EnhancementBit(enhancement)) != 0;
    }

    /**
     * @brief 返回开启或关闭一个增强后的配置副本。
     * @param enhancement 要修改的增强操作。
     * @param enabled true 表示开启，false 表示关闭。
     */
    constexpr SolveOptions With(Enhancement enhancement, bool enabled = true) const
    {
        SolveOptions result = *this;
        if (enabled)
            result.enhancements |= EnhancementBit(enhancement);
        else
            result.enhancements &= ~EnhancementBit(enhancement);
        return result;
    }
};

/**
 * @brief 精确 GST 查询的最小返回结构。
 *
 * 有解时 `feasible=true` 且 `best_weight` 为精确最优值；无解时
 * `feasible=false` 且 `best_weight=-1`。`mask_vertex_states` 是本次查询中
 * 首次进入 D/A/H 状态行的不同 `(mask,v)` 项总数；辅助预处理、重复松弛和
 * 仅用于更新完整解的候选不计入。所有合法增强配置共享该契约。
 */
struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    std::uint64_t mask_vertex_states = 0;
};

/**
 * @brief 检查开关组合是否满足依赖关系。
 * @return 合法返回 true；当前唯一非法情况是仅开 adjoint 而未开 directed-cut。
 */
bool IsValid(const SolveOptions& options);

/**
 * @brief 返回稳定的配置标识，供结果目录、probe 和实验元数据共同使用。
 *
 * 合法配置依次返回 `base`、`directed_cut_only` 或 `enhanced`；非法配置返回
 * `invalid`。返回值具有静态存储期，调用者无需管理内存。
 */
const char* ConfigurationName(const SolveOptions& options);

/**
 * @brief 使用冻结配置精确求解一条 Group Steiner Tree 查询。
 * @param graph 已完成一致性检查的无向非负边权图。
 * @param query 顶点组查询，支持 0 到 16 组并允许组间重叠。
 * @param options 查询开始前固定的增强开关；默认值即基础配置。
 * @throws std::invalid_argument 当增强依赖不合法时抛出。
 *
 * 函数不会根据数据集、组数、状态密度或已运行时间自动改配置，因此基础与
 * 完整配置是同一算法的两次预声明运行，而不是逐查询 oracle。
 */
SolveResult SolveOneQuery(const Graph& graph,
                          const Query& query,
                          const SolveOptions& options = SolveOptions::Base());

}  // namespace gst::methods::abhss

#endif  // ABHSS_PUBLIC_H
