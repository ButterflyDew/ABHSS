#ifndef GST_MEMORY_USAGE_H
#define GST_MEMORY_USAGE_H

#include <atomic>
#include <cstdint>
#include <thread>

namespace gst
{

struct ProcessMemoryUsage
{
    std::uint64_t current_rss_bytes = 0;
    // 进程生存期 peak；多询问时不能直接当作逐询问 peak。
    std::uint64_t peak_rss_bytes = 0;
};

constexpr int kQueryRssSampleIntervalMs = 1;

// 读取当前进程 RSS 与生存期 peak，兼容 Windows/Linux。
ProcessMemoryUsage GetProcessMemoryUsage();
// 将字节转换为 MiB。
double BytesToMiB(std::uint64_t bytes);

class QueryPeakRssSampler
{
public:
    // 启动 1ms 采样线程，记录本条询问期间的绝对 RSS peak。
    QueryPeakRssSampler();
    ~QueryPeakRssSampler();

    QueryPeakRssSampler(const QueryPeakRssSampler&) = delete;
    QueryPeakRssSampler& operator=(const QueryPeakRssSampler&) = delete;

    // 停止并返回绝对 peak；主程序再减去图加载后的固定基线。
    std::uint64_t Stop();

private:
    void Sample();

    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> peak_rss_bytes_{0};
    std::thread worker_;
    bool stopped_ = false;
};

}  // namespace gst

#endif  // GST_MEMORY_USAGE_H
