#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include "abhss/abhss.h"
#include "common/graph_io.h"
#include "common/query_io.h"

// 竞赛式批处理入口：参数只保留输入、Base/Enhanced 选择和查询区间。
int main(int argc, char** argv)
{
    if (argc < 4 || argc > 6)
    {
        std::cerr << "Usage: abhss <graph_folder> <query_file> <base|enhanced> [first_query] [query_count]\n";
        return 1;
    }

    // 模式只有最终论文需要的 Base 和 Enhanced，不保留中间消融状态。
    const std::string mode = argv[3];
    if (mode != "base" && mode != "enhanced")
    {
        std::cerr << "Mode must be base or enhanced.\n";
        return 1;
    }
    const bool enhanced = mode == "enhanced";

    // 图和查询在计时区外各读一次；正式接口仍沿用 graph.txt 与查询文本格式。
    const std::string graph_folder = argv[1];
    const std::string query_file = argv[2];
    const gst::Graph graph = gst::LoadGraph(graph_folder);
    const std::vector<gst::Query> queries = gst::LoadQueries(query_file);
    const int first = argc >= 5 ? std::stoi(argv[4]) - 1 : 0;
    const int count = argc >= 6 ? std::stoi(argv[5]) : static_cast<int>(queries.size()) - first;
    if (first < 0 || first > static_cast<int>(queries.size()) || count < 0)
    {
        std::cerr << "Invalid query range.\n";
        return 1;
    }
    const int last = std::min(static_cast<int>(queries.size()), first + count);

    // 每行固定输出查询号、solver-only 时间、精确权值和实际 D/A/H 状态数。
    std::cout << std::fixed << std::setprecision(10);
    for (int index = first; index < last; ++index)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto answer = gst::methods::abhss::SolveOneQuery(graph, queries[index], enhanced);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << index + 1 << ' ' << std::setprecision(6) << seconds << ' ' << std::setprecision(10);
        if (answer.feasible)
            std::cout << answer.best_weight;
        else
            std::cout << -1;
        std::cout << ' ' << answer.mask_vertex_states << '\n';
    }
    return 0;
}
