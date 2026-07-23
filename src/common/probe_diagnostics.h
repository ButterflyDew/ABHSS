#ifndef GST_PROBE_DIAGNOSTICS_H
#define GST_PROBE_DIAGNOSTICS_H

#include <cstdlib>
#include <iostream>
#include <string>

namespace gst
{

// Probe 诊断必须在编译期显式启用。正式论文构建不定义该宏，编译器会把
// 整条诊断路径移除，不给计时二进制保留隐藏分支。
#if defined(GST_ENABLE_PROBE_DIAGNOSTICS)
/** @brief 只解析一次环境开关，决定已编译的 probe 是否真正输出事件。 */
inline bool ProbeDiagnosticsEnabled()
{
    static const bool enabled = []
    {
        const char* value = std::getenv("GST_PROBE_DIAGNOSTICS");
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }();
    return enabled;
}

/** @brief 以稳定前缀输出一条稀疏 probe 事件。 */
inline void EmitProbeDiagnostic(const std::string& fields)
{
    if (ProbeDiagnosticsEnabled())
        std::cout << "[ProbeDiag] " << fields << std::endl;
}
#else
/** @brief 正式构建的编译期常量关闭值。 */
inline constexpr bool ProbeDiagnosticsEnabled() { return false; }
/** @brief 正式构建中的空操作，由编译器完全消除。 */
inline void EmitProbeDiagnostic(const std::string&) {}
#endif

}  // namespace gst

#endif  // GST_PROBE_DIAGNOSTICS_H
