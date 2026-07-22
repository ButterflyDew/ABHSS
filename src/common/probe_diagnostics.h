#ifndef GST_PROBE_DIAGNOSTICS_H
#define GST_PROBE_DIAGNOSTICS_H

#include <cstdlib>
#include <iostream>
#include <string>

namespace gst
{

// Probe diagnostics are deliberately opt-in.  Formal paper runs leave the
// instrumentation macro unset, so the compiler removes the entire path.
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
inline bool ProbeDiagnosticsEnabled()
{
    static const bool enabled = []
    {
        const char* value = std::getenv("GST_PROBE_DIAGNOSTICS");
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }();
    return enabled;
}

inline void EmitProbeDiagnostic(const std::string& fields)
{
    if (ProbeDiagnosticsEnabled())
        std::cout << "[ProbeDiag] " << fields << std::endl;
}
#else
inline constexpr bool ProbeDiagnosticsEnabled() { return false; }
inline void EmitProbeDiagnostic(const std::string&) {}
#endif

}  // namespace gst

#endif  // GST_PROBE_DIAGNOSTICS_H
