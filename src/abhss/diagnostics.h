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
// Probe-only timing and row accounting live in one header so formal binaries
// compile the whole path away.  Light, Heavy and Heavy-Forward consequently
// expose identical phase fields without paying clocks or counters in paper runs.
class ProbeTimer
{
public:
    ProbeTimer()
    {
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
        start_ = Clock::now();
#endif
    }

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

inline const char* ProbeFamilyMethod(const Problem& problem)
{
    return problem.UsesBoundedGroupDistances() ? "abhss_light_family"
                                               : "abhss_heavy_family";
}

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
