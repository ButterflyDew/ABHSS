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
// 固定小数格式，避免不同平台的默认精度影响结果文件。
std::string FormatDouble(double value, int precision = 10)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

// 生成仅用于结果批次 header 的本地时间。
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

// query.txt 对应 default，query_g10.txt 对应 query_g10，与原仓库目录口径一致。
std::string RunSubdir(const fs::path& query_file)
{
    std::string stem = query_file.stem().string();
    std::string lower = stem;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lower == "query" ? "default" : stem;
}

// 方法名同时用于 run header 和结果子目录。
const char* MethodName()
{
#if defined(ABHSS_BUILD_PRUNEDDP)
    return "PrunedDP";
#elif defined(ABHSS_BUILD_DPBF)
    return "DPBF";
#elif defined(ABHSS_BUILD_BASIC_PLUS)
    return "BasicPlus";
#elif defined(ABHSS_BUILD_GPU4GST_PRUNEDDP)
    return "GPU4GST-PrunedDP-Artifact";
#elif defined(ABHSS_BUILD_LIGHT_NO_EARLY)
    return "ABHSS-Light-NoEarly";
#elif defined(ABHSS_BUILD_LIGHT_NO_WITNESS)
    return "ABHSS-Light-NoWitness";
#elif defined(ABHSS_BUILD_HEAVY_FORWARD)
    return "ABHSS-Heavy-Forward";
#elif defined(ABHSS_BUILD_LIGHT)
    return "ABHSS-Light";
#else
    return "ABHSS-Heavy";
#endif
}

#if defined(ABHSS_BUILD_PRUNEDDP)
// 只接受三个已记录的 PrunedDP++ 复现选项，拒绝静默忽略未知参数。
gst::methods::pruned_dp::PrunedDpOptions ParsePrunedOptions(int argc, char** argv)
{
    gst::methods::pruned_dp::PrunedDpOptions options;
    for (int index = 7; index < argc; ++index)
    {
        const std::string argument = argv[index];
        auto ParseOnOff = [&](const std::string& prefix)
        {
            const std::string value = argument.substr(prefix.size());
            if (value == "on")
                return true;
            if (value == "off")
                return false;
            throw std::runtime_error(prefix + " expects on or off.");
        };

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
            options.use_mst_upper_bound = ParseOnOff(mst);
        else if (argument.rfind(pathmax, 0) == 0)
            options.enforce_lb2_pathmax = ParseOnOff(pathmax);
        else
            throw std::runtime_error("Unknown PrunedDP option: " + argument);
    }
    return options;
}
#endif
}  // namespace

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
#else
        if (argc > 7)
            throw std::runtime_error("This method has no runtime algorithm options.");
#endif

        const std::string graph_folder =
            gst::ResolveGraphFolder(data_root, graph_selector);
        const std::string graph_name = fs::path(graph_folder).filename().string();
        const std::string query_file =
            gst::ResolveQueryFile(graph_folder, query_selector);
        const std::string run_subdir = RunSubdir(query_file);
        const fs::path result_dir =
            fs::path(result_root) / graph_name / MethodName() / run_subdir;

        // 图和全部询问只加载一次；query_begin/query_limit 仅控制循环范围。
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries =
            gst::LoadQueriesFromFolder(graph_folder, query_selector);
#if defined(ABHSS_BUILD_GPU4GST_PRUNEDDP)
        // Convert the unified graph into the artifact's integer adjacency
        // representation before the per-query deadline and memory baseline.
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
               << " method=" << MethodName()
               << " data_root=" << data_root
               << " graph_folder=" << graph_folder
               << " query_file=" << fs::path(query_file).filename().string()
               << " run_subdir=" << run_subdir
               << " original_query_count=" << queries.size()
               << " query_begin=" << query_begin
               << " query_limit=" << query_limit
               << " query_count=" << end - begin
               << " result_dir=" << result_dir.string()
               << " memory_baseline_rss_mb="
               << FormatDouble(gst::BytesToMiB(loaded_memory.current_rss_bytes), 3)
               << " memory_metric=query_processing_peak_rss_overhead_mb"
               << " rss_sample_interval_ms=" << gst::kQueryRssSampleIntervalMs;
#if defined(ABHSS_BUILD_PRUNEDDP)
        header << " state_storage="
               << gst::methods::pruned_dp::StateStorageName(pruned_options.state_storage)
               << " mst_upper=" << (pruned_options.use_mst_upper_bound ? "on" : "off")
               << " lb2_pathmax=" << (pruned_options.enforce_lb2_pathmax ? "on" : "off");
#endif
        output.BeginResultRun("weights.txt", header.str());

        // The experiment supervisor starts the per-instance 10,000-second
        // deadline only after graph/query loading has completed.  std::endl
        // is intentional: the marker must reach a pipe before solving starts.
        std::cout << "[Ready] graph=" << graph_name
                  << " query_begin=" << query_begin
                  << " query_count=" << end - begin << std::endl;

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
#elif defined(ABHSS_BUILD_HEAVY_FORWARD)
            const auto answer = gst::methods::abhss::SolveHeavyForwardOneQuery(
                graph, queries[index]);
#elif defined(ABHSS_BUILD_LIGHT)
            const auto answer = gst::methods::abhss::SolveLightOneQuery(
                graph, queries[index]);
#else
            const auto answer = gst::methods::abhss::SolveHeavyOneQuery(
                graph, queries[index]);
#endif
            best = answer.best_weight;
            feasible = answer.feasible;

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
                FormatDouble(gst::BytesToMiB(query_peak), 3));

            std::cout << "[Query " << index + 1 << "] time="
                      << FormatDouble(seconds, 6) << " sec, weight=" << weight
                      << ", query_memory_peak="
                      << FormatDouble(gst::BytesToMiB(query_peak), 3)
                      << " MiB" << std::endl;
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
