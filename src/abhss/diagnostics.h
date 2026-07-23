#ifndef ABHSS_DIAGNOSTICS_H
#define ABHSS_DIAGNOSTICS_H

#include "internal.h"
#include "../common/probe_diagnostics.h"

#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
#include <chrono>
#include <cstdint>
#include <sstream>
#endif

namespace gst::methods::abhss::internal
{
/**
 * @brief 只在 probe 编译中工作的阶段计时器。
 *
 * 未定义 `GST_ENABLE_PROBE_DIAGNOSTICS` 时构造与 `Seconds` 都编译为空路径，
 * 正式二进制不会因基础/增强配置的阶段记录支付时钟读取成本。
 */
class ProbeTimer
{
public:
    /** @brief 在诊断编译中记录 steady-clock 起点；正式编译不保存字段。 */
    ProbeTimer()
    {
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
        start_ = Clock::now();
#endif
    }

    /** @brief 返回构造后的秒数；正式编译固定返回 -1 表示未采样。 */
    double Seconds() const
    {
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
        return std::chrono::duration<double>(Clock::now() - start_).count();
#else
        return -1.0;
#endif
    }

private:
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_;
#endif
};

/** @brief 从冻结增强开关返回稳定的配置族 probe 名。 */
inline const char* ProbeFamilyMethod(const Problem& problem)
{
    if (problem.UsesAdjointCompletion())
        return "abhss_config_enhanced";
    if (problem.UsesDirectedCut())
        return "abhss_config_directed_cut_only";
    return "abhss_config_base";
}

/**
 * @brief 以统一键值格式输出一个 ABHSS 工程 probe 事件。
 *
 * 可选统计 ready row 数、标量数、层号和实际工作量。整个函数在正式编译中
 * 仅保留 `(void)`，确保诊断能力不会改变论文计时路径。
 */
inline void EmitAbhssProbe(const char* method,
                           const char* phase,
                           const Problem& problem,
                           double seconds = -1.0,
                           const std::vector<Row>* rows = nullptr,
                           int layer = -1,
                           long long layer_work = -1)
{
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
    if (!gst::ProbeDiagnosticsEnabled())
        return;
    std::ostringstream out;
    out << "method=" << method << " phase=" << phase
        << " g=" << problem.g << " best=" << problem.best;
    if (seconds >= 0.0)
        out << " seconds=" << seconds;
    if (layer >= 0)
        out << " layer=" << layer;
    if (rows != nullptr)
    {
        std::uint64_t ready = 0;
        std::uint64_t scalars = 0;
        for (const Row& row : *rows)
        {
            ready += row.ready ? 1 : 0;
            scalars += row.value.size();
        }
        out << " rows=" << ready << " scalars=" << scalars;
    }
    if (layer_work >= 0)
        out << " layer_work=" << layer_work;
    gst::EmitProbeDiagnostic(out.str());
#else
    (void)method;
    (void)phase;
    (void)problem;
    (void)seconds;
    (void)rows;
    (void)layer;
    (void)layer_work;
#endif
}

}  // namespace gst::methods::abhss::internal

#endif  // ABHSS_DIAGNOSTICS_H
