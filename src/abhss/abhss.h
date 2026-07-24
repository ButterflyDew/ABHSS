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

/** @brief Enhanced 相对 Base 真正新增、没有 Base 对应物的安全证书操作。 */
enum class AddedOperation : std::uint32_t
{
    DirectedCutCertificate = std::uint32_t{1} << 0,
    FacilityUpperBound = std::uint32_t{1} << 1,
};

/** @brief 同一“组距离”职责的两种可互换实现。 */
enum class GroupDistanceRealization : std::uint8_t
{
    BoundedCutoff,
    CompletePotential,
};

/** @brief ordinary future 职责的两种可采纳证书实现。 */
enum class OrdinaryFutureRealization : std::uint8_t
{
    AnchoredSingletonCone,
    DirectedCutPotential,
};

/** @brief 初始/周期上界见证职责的两种实现。 */
enum class UpperWitnessRealization : std::uint8_t
{
    RootPathTree,
    DualPrimalTree,
};

/** @brief 高层锚定状态职责的前向与伴随实现。 */
enum class HighLayerRealization : std::uint8_t
{
    ForwardAnchoredA,
    AdjointH,
};

/**
 * @brief 把增强位解释为论文可陈述的“新增 + 同职责替换”执行契约。
 *
 * `added_operations` 只包含 Base 完全没有对应物的安全工作；其余四个字段
 * 都是相同逻辑职责的 realization，而不是额外算法阶段。特别地，Base 的
 * A1-cone future 被 DirectedCut potential 替换；A1 落在需物化的前向区间
 * 时，每种配置至多生成一次，若它落在 adjoint 高层则由 H 同职责替换。
 * 该结构同时约束配置合法性与配置回归，避免文档单独发明关系。
 */
struct ConfigurationProfile
{
    bool valid = true;
    std::uint32_t added_operations = 0;
    GroupDistanceRealization group_distance =
        GroupDistanceRealization::BoundedCutoff;
    OrdinaryFutureRealization ordinary_future =
        OrdinaryFutureRealization::AnchoredSingletonCone;
    UpperWitnessRealization upper_witness =
        UpperWitnessRealization::RootPathTree;
    HighLayerRealization high_layer =
        HighLayerRealization::ForwardAnchoredA;

    /** @brief 查询一个真正新增的安全操作是否存在。 */
    constexpr bool Adds(AddedOperation operation) const
    {
        return (added_operations & static_cast<std::uint32_t>(operation)) != 0;
    }
};

/**
 * @brief 从冻结开关生成唯一执行契约；不读取图、查询或运行时统计。
 */
constexpr ConfigurationProfile DescribeConfiguration(SolveOptions options)
{
    constexpr std::uint32_t known =
        EnhancementBit(Enhancement::DirectedCut) |
        EnhancementBit(Enhancement::AdjointCompletion);
    const bool directed = options.Enabled(Enhancement::DirectedCut);
    const bool adjoint = options.Enabled(Enhancement::AdjointCompletion);
    ConfigurationProfile profile;
    profile.valid = !(options.enhancements & ~known) && (!adjoint || directed);
    if (directed)
    {
        profile.added_operations =
            static_cast<std::uint32_t>(AddedOperation::DirectedCutCertificate) |
            static_cast<std::uint32_t>(AddedOperation::FacilityUpperBound);
        profile.group_distance = GroupDistanceRealization::CompletePotential;
        profile.ordinary_future =
            OrdinaryFutureRealization::DirectedCutPotential;
        profile.upper_witness = UpperWitnessRealization::DualPrimalTree;
    }
    if (adjoint)
        profile.high_layer = HighLayerRealization::AdjointH;
    return profile;
}

/**
 * @brief 由精确平衡完成域和冻结 high-layer realization 得到的层计划。
 *
 * `highest_layer` 是必须覆盖的最大锚定 mask 大小；正层定义域恰为
 * `1..highest_layer`。`forward_last_layer` 是公共前向 A 实际负责的前缀，
 * 非空后缀由 adjoint H 负责。该结构不包含按图或经验组数阈值的字段。
 */
struct AnchoredCompletionSchedule
{
    int highest_layer = 0;
    int forward_last_layer = 0;
    bool uses_adjoint = false;

    /** @brief 判断给定正层是否属于精确完成递推的逻辑定义域。 */
    constexpr bool ContainsLogicalLayer(int size) const
    {
        return size > 0 && size <= highest_layer;
    }

    /** @brief 判断逻辑层是否由公共前向 A realization 负责。 */
    constexpr bool UsesForwardA(int size) const
    {
        return ContainsLogicalLayer(size) && size <= forward_last_layer;
    }

    /** @brief 判断逻辑层是否由 adjoint H realization 负责。 */
    constexpr bool UsesAdjointH(int size) const
    {
        return ContainsLogicalLayer(size) && size > forward_last_layer;
    }
};

/**
 * @brief 从组数决定的递推域边界生成唯一层计划。
 *
 * 这里的整数除法来自平衡分解：`floor(g/2)-1` 是完整锚定格的最高层，
 * adjoint 再对该区间做固定的 meet-in-the-middle 切分。函数不读取图、
 * 查询内容、row 密度、耗时或内存，也不比较 `g` 与经验常数。
 */
constexpr AnchoredCompletionSchedule MakeAnchoredCompletionSchedule(
    int group_count,
    const ConfigurationProfile& profile)
{
    AnchoredCompletionSchedule schedule;
    const int half = group_count / 2;
    schedule.highest_layer = half > 0 ? half - 1 : 0;
    schedule.uses_adjoint =
        profile.high_layer == HighLayerRealization::AdjointH;
    schedule.forward_last_layer = schedule.uses_adjoint
                                      ? schedule.highest_layer / 2
                                      : schedule.highest_layer;
    return schedule;
}

/**
 * @brief 精确 GST 查询的最小返回结构。
 *
 * 有解时 `feasible=true` 且 `best_weight` 为精确最优值；无解时
 * `feasible=false` 且 `best_weight=-1`。`mask_vertex_states` 是本次查询中
 * 首次进入 D/A/H 状态行的不同状态项总数；状态族是键的一部分，因此
 * D(S,v)、A(S,v) 与 H(S,v) 即使数值 mask、vertex 相同也分别计数。辅助
 * 预处理、重复松弛和仅用于更新完整解的候选不计入。所有合法配置共享契约。
 */
struct SolveResult
{
    double best_weight = -1.0;
    bool feasible = false;
    std::uint64_t mask_vertex_states = 0;
};

/**
 * @brief 检查开关组合是否满足依赖关系。
 * @return 合法返回 true；未知增强位，或仅开 adjoint 而未开 directed-cut，
 * 均返回 false。
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
