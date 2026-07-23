#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "common/graph_io.h"
#include "common/memory_usage.h"
#include "common/output_manager.h"
#include "common/query_io.h"

#if defined(ABHSS_BUILD_PRUNEDDP)
#include "pruneddp/pruneddp.h"
#elif defined(ABHSS_BUILD_DPBF)
#include "dpbf/dpbf.h"
#elif defined(ABHSS_BUILD_BASIC_PLUS)
#include "baselines/basic_plus.h"
#elif defined(ABHSS_BUILD_GPU4GST_PRUNEDDP)
#include "baselines/gpu4gst_pruneddp.h"
#else
#include "abhss/abhss.h"
#endif

namespace fs = std::filesystem;

namespace
{
/**
 * @brief 按固定小数位序列化浮点数，保证跨平台结果文件可直接比较。
 * @param value 待输出数值。
 * @param precision 小数位数；目标值默认十位，时间可由调用者改为六位。
 */
std::string FormatDouble(double value, int precision = 10)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

/** @brief 生成仅用于结果批次 header 的本地墙钟时间，不参与算法计时。 */
std::string CurrentTimeString()
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

/**
 * @brief 将查询文件名映射到稳定结果子目录。
 *
 * `query.txt` 映射为 `default`，其余文件保留 stem；该规则只组织输出，
 * 不参与方法或配置选择。
 */
std::string RunSubdir(const fs::path& query_file)
{
    std::string stem = query_file.stem().string();
    std::string lower = stem;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lower == "query" ? "default" : stem;
}

/**
 * @brief 返回编译期选定的算法族名称。
 *
 * ABHSS 的具体配置由运行时参数在查询开始前冻结，故这里只返回族名；
 * 其他 baseline 仍是一目标一方法。
 */
[[maybe_unused]] const char* CompiledMethodName()
{
#if defined(ABHSS_BUILD_PRUNEDDP)
    return "PrunedDP";
#elif defined(ABHSS_BUILD_DPBF)
    return "DPBF";
#elif defined(ABHSS_BUILD_BASIC_PLUS)
    return "BasicPlus";
#elif defined(ABHSS_BUILD_GPU4GST_PRUNEDDP)
    return "GPU4GST-PrunedDP-Artifact";
#else
    return "ABHSS";
#endif
}

/**
 * @brief 解析形如 `--name=on|off` 的布尔选项。
 * @throws std::runtime_error 当值不是 on/off 时抛出，避免静默采用默认值。
 */
[[maybe_unused]] bool ParseOnOff(const std::string& argument,
                                 const std::string& prefix)
{
    const std::string value = argument.substr(prefix.size());
    if (value == "on")
        return true;
    if (value == "off")
        return false;
    throw std::runtime_error(prefix + " expects on or off.");
}

#if defined(ABHSS_BUILD_PRUNEDDP)
/**
 * @brief 解析已登记的 PrunedDP++ 复现选项。
 *
 * 参数从六个公共位置参数之后开始；未知选项立即报错，以确保运行记录中的
 * 配置和实际执行完全一致。
 */
gst::methods::pruned_dp::PrunedDpOptions ParsePrunedOptions(int argc, char** argv)
{
    gst::methods::pruned_dp::PrunedDpOptions options;
    for (int index = 7; index < argc; ++index)
    {
        const std::string argument = argv[index];
        const std::string storage = "--state-storage=";
        const std::string mst = "--mst-upper=";
        const std::string pathmax = "--lb2-pathmax=";
        if (argument.rfind(storage, 0) == 0)
        {
            const std::string value = argument.substr(storage.size());
            if (value == "hash")
                options.state_storage = gst::methods::pruned_dp::StateStorage::Hash;
            else if (value == "dense")
                options.state_storage = gst::methods::pruned_dp::StateStorage::Dense;
            else
                throw std::runtime_error("--state-storage expects hash or dense.");
        }
        else if (argument.rfind(mst, 0) == 0)
            options.use_mst_upper_bound = ParseOnOff(argument, mst);
        else if (argument.rfind(pathmax, 0) == 0)
            options.enforce_lb2_pathmax = ParseOnOff(argument, pathmax);
        else
            throw std::runtime_error("Unknown PrunedDP option: " + argument);
    }
    return options;
}
#endif

#if defined(ABHSS_BUILD_ABHSS)
/**
 * @brief 解析同一 ABHSS 可执行文件的增强开关。
 *
 * `--enhancements=none|directed-cut|all` 提供稳定预设；两个显式 on/off
 * 参数可在预设之后覆盖单个开关，用于消融。函数最后统一检查依赖关系，
 * 因而 `adjoint=on,directed-cut=off` 不可能进入求解阶段。
 */
gst::methods::abhss::SolveOptions ParseAbhssOptions(int argc, char** argv)
{
    using gst::methods::abhss::Enhancement;
    using gst::methods::abhss::SolveOptions;

    SolveOptions options = SolveOptions::Base();
    for (int index = 7; index < argc; ++index)
    {
        const std::string argument = argv[index];
        const std::string preset = "--enhancements=";
        const std::string directed = "--directed-cut=";
        const std::string adjoint = "--adjoint-completion=";
        if (argument.rfind(preset, 0) == 0)
        {
            const std::string value = argument.substr(preset.size());
            if (value == "none")
                options = SolveOptions::Base();
            else if (value == "directed-cut")
                options = SolveOptions::DirectedCutOnly();
            else if (value == "all")
                options = SolveOptions::Enhanced();
            else
                throw std::runtime_error(
                    "--enhancements expects none, directed-cut, or all.");
        }
        else if (argument.rfind(directed, 0) == 0)
        {
            options = options.With(
                Enhancement::DirectedCut, ParseOnOff(argument, directed));
        }
        else if (argument.rfind(adjoint, 0) == 0)
        {
            options = options.With(
                Enhancement::AdjointCompletion, ParseOnOff(argument, adjoint));
        }
        else
            throw std::runtime_error("Unknown ABHSS option: " + argument);
    }
    if (!gst::methods::abhss::IsValid(options))
        throw std::runtime_error(
            "ABHSS adjoint-completion requires directed-cut=on.");
    return options;
}

/** @brief 将冻结配置转为人类可读的结果目录名，不影响算法分派。 */
std::string AbhssRunName(const gst::methods::abhss::SolveOptions& options)
{
    const std::string configuration =
        gst::methods::abhss::ConfigurationName(options);
    if (configuration == "base")
        return "ABHSS-Base";
    if (configuration == "enhanced")
        return "ABHSS-Enhanced";
    return "ABHSS-DirectedCutOnly";
}
#endif
}  // namespace

/**
 * @brief 统一的批量查询命令行入口。
 *
 * 函数先解析并冻结方法配置，再加载一次图和查询；单查询计时只包围 solver
 * 调用。任何配置解析、图 I/O 和结果目录操作都在计时区外，从而保证基础与
 * 增强配置以及所有 baseline 使用相同的时间边界。
 */
int main(int argc, char** argv)
{
    try
    {
        // 六个公共参数完全保留原仓库顺序，以便旧实验脚本直接复用。
        const std::string graph_selector = argc >= 2 ? argv[1] : "";
        const std::string result_root = argc >= 3 ? argv[2] : "result";
        const std::string query_selector = argc >= 4 ? argv[3] : "";
        const std::string data_root = argc >= 5 ? argv[4] : "data";
        const int query_begin = argc >= 6 ? std::stoi(argv[5]) : 1;
        const int query_limit = argc >= 7 ? std::stoi(argv[6]) : -1;

#if defined(ABHSS_BUILD_PRUNEDDP)
        const auto pruned_options = ParsePrunedOptions(argc, argv);
        const std::string method_name = CompiledMethodName();
#elif defined(ABHSS_BUILD_ABHSS)
        const auto abhss_options = ParseAbhssOptions(argc, argv);
        const std::string method_name = AbhssRunName(abhss_options);
#else
        if (argc > 7)
            throw std::runtime_error("This method has no runtime algorithm options.");
        const std::string method_name = CompiledMethodName();
#endif

        const std::string graph_folder =
            gst::ResolveGraphFolder(data_root, graph_selector);
        const std::string graph_name = fs::path(graph_folder).filename().string();
        const std::string query_file =
            gst::ResolveQueryFile(graph_folder, query_selector);
        const std::string run_subdir = RunSubdir(query_file);
        const fs::path result_dir =
            fs::path(result_root) / graph_name / method_name / run_subdir;

        // 图和全部询问只加载一次；两段墙钟只审计 I/O，不进入任何查询计时。
        const auto graph_load_start = std::chrono::steady_clock::now();
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const double graph_load_seconds = std::chrono::duration<double>(
                                              std::chrono::steady_clock::now() -
                                              graph_load_start)
                                              .count();
        const auto query_load_start = std::chrono::steady_clock::now();
        const std::vector<gst::Query> queries =
            gst::LoadQueriesFromFolder(graph_folder, query_selector);
        const double query_load_seconds = std::chrono::duration<double>(
                                              std::chrono::steady_clock::now() -
                                              query_load_start)
                                              .count();
#if defined(ABHSS_BUILD_GPU4GST_PRUNEDDP)
        // 在逐查询 deadline 与内存基线开始前，把统一图转换为作者 artifact
        // 所需的整数邻接表示，避免只给某个 baseline 计入一次性转换成本。
        gst::methods::gpu4gst_pruneddp::PrepareGraph(graph);
#endif
        if (query_begin < 1 || query_begin > static_cast<int>(queries.size()) + 1)
            throw std::runtime_error("query_begin out of range.");
        const int begin = query_begin - 1;
        const int end = query_limit < 0
                            ? static_cast<int>(queries.size())
                            : std::min(static_cast<int>(queries.size()), begin + query_limit);

        gst::OutputManager output(result_dir.string(), "weights.txt");
        const gst::ProcessMemoryUsage loaded_memory = gst::GetProcessMemoryUsage();
        std::ostringstream header;
        header << "# Run started=" << CurrentTimeString()
               << " graph=" << graph_name
               << " method=" << method_name
               << " data_root=" << data_root
               << " graph_folder=" << graph_folder
               << " query_file=" << fs::path(query_file).filename().string()
               << " run_subdir=" << run_subdir
               << " original_query_count=" << queries.size()
               << " query_begin=" << query_begin
               << " query_limit=" << query_limit
               << " query_count=" << end - begin
               << " result_dir=" << result_dir.string()
               << " graph_load_seconds=" << FormatDouble(graph_load_seconds, 6)
               << " query_load_seconds=" << FormatDouble(query_load_seconds, 6)
               << " memory_baseline_rss_mb="
               << FormatDouble(gst::BytesToMiB(loaded_memory.current_rss_bytes), 3)
               << " memory_metric=query_processing_peak_rss_overhead_mb"
               << " rss_sample_interval_ms=" << gst::kQueryRssSampleIntervalMs
               << " result_columns=query_seconds,weight,query_peak_rss_mib,mask_vertex_states"
               << " unavailable_integer_metric=-1";
#if defined(ABHSS_BUILD_PRUNEDDP)
        header << " state_storage="
               << gst::methods::pruned_dp::StateStorageName(pruned_options.state_storage)
               << " mst_upper=" << (pruned_options.use_mst_upper_bound ? "on" : "off")
               << " lb2_pathmax=" << (pruned_options.enforce_lb2_pathmax ? "on" : "off");
#elif defined(ABHSS_BUILD_ABHSS)
        header << " algorithm_family=ABHSS"
               << " configuration="
               << gst::methods::abhss::ConfigurationName(abhss_options)
               << " enhancement_mask=" << abhss_options.enhancements
               << " directed_cut="
               << (abhss_options.Enabled(
                       gst::methods::abhss::Enhancement::DirectedCut)
                       ? "on"
                       : "off")
               << " adjoint_completion="
               << (abhss_options.Enabled(
                       gst::methods::abhss::Enhancement::AdjointCompletion)
                       ? "on"
                       : "off");
#endif
        output.BeginResultRun("weights.txt", header.str());

        // supervisor 只在图/查询加载完成后启动每实例 10,000 秒 deadline。
        // 此处故意使用 std::endl，保证开始求解前 marker 已刷新到管道。
        std::cout << "[Ready] graph=" << graph_name
                  << " query_begin=" << query_begin
                  << " query_count=" << end - begin
                  << " graph_load_seconds="
                  << FormatDouble(graph_load_seconds, 6)
                  << " query_load_seconds="
                  << FormatDouble(query_load_seconds, 6) << std::endl;

        for (int index = begin; index < end; ++index)
        {
            gst::QueryPeakRssSampler memory_sampler;
            const auto start = std::chrono::steady_clock::now();

            double best = -1.0;
            bool feasible = false;
#if defined(ABHSS_BUILD_PRUNEDDP)
            const auto answer = gst::methods::pruned_dp::SolveOneQuery(
                graph, queries[index], pruned_options);
#elif defined(ABHSS_BUILD_DPBF)
            const auto answer = gst::methods::dpbf::SolveOneQuery(
                graph, queries[index]);
#elif defined(ABHSS_BUILD_BASIC_PLUS)
            const auto answer = gst::methods::basic_plus::SolveOneQuery(
                graph, queries[index]);
#elif defined(ABHSS_BUILD_GPU4GST_PRUNEDDP)
            const auto answer = gst::methods::gpu4gst_pruneddp::SolveOneQuery(
                graph, queries[index]);
#else
            const auto answer = gst::methods::abhss::SolveOneQuery(
                graph, queries[index], abhss_options);
#endif
            best = answer.best_weight;
            feasible = answer.feasible;
            // 正式性能方法都提供实际状态数；correctness-only/第三方 adapter
            // 尚无统一可比口径时写 -1，而不是用 0 冒充“没有生成状态”。
            std::string mask_vertex_states = "-1";
#if defined(ABHSS_BUILD_PRUNEDDP) || defined(ABHSS_BUILD_ABHSS)
            mask_vertex_states = std::to_string(answer.mask_vertex_states);
#endif

            const double seconds = std::chrono::duration<double>(
                                       std::chrono::steady_clock::now() - start)
                                       .count();
            const std::uint64_t absolute_peak = memory_sampler.Stop();
            const std::uint64_t query_peak =
                absolute_peak > loaded_memory.current_rss_bytes
                    ? absolute_peak - loaded_memory.current_rss_bytes
                    : 0;
            const std::string weight = feasible ? FormatDouble(best) : "-1";
            output.AppendMainResultLine(
                FormatDouble(seconds, 6) + " " + weight + " " +
                FormatDouble(gst::BytesToMiB(query_peak), 3) + " " +
                mask_vertex_states);

            std::cout << "[Query " << index + 1 << "] time="
                      << FormatDouble(seconds, 6) << " sec, weight=" << weight
                      << ", query_memory_peak="
                      << FormatDouble(gst::BytesToMiB(query_peak), 3)
                      << " MiB, mask_vertex_states=" << mask_vertex_states
                      << std::endl;
        }

        std::cout << "Graph: " << graph_name << ", queries: " << end - begin << '\n';
        std::cout << "Result file: " << (result_dir / "weights.txt").string() << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
