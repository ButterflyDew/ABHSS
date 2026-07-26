#ifndef GST_FLOAT_COMPARE_H
#define GST_FLOAT_COMPARE_H

#include <cmath>

namespace gst::fp
{

constexpr double kEps = 1e-9;
constexpr double kInf = 1e100;

// 只在从浮点最短距离等式恢复真实路径时允许容差相等；上下界闭合始终用原始比较。
inline bool Eq(double a, double b, double eps = kEps)
{
    return std::fabs(a - b) <= eps;
}

} // namespace gst::fp

#endif // GST_FLOAT_COMPARE_H
