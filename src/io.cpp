#include "graph.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <iterator>
#include <stdexcept>

namespace abhss {
namespace {

// 顺序扫描数字文件，8 MiB 缓冲只分配一次。
class Reader {
    FILE* file;
    std::vector<char> buffer;
    std::string token;
    size_t pos = 0, end = 0;

    // 取下一个字节；缓冲耗尽时批量读取，文件结束返回 EOF。
    int Get() {
        if (pos == end) {
            end = std::fread(buffer.data(), 1, buffer.size(), file);
            pos = 0;
            if (!end) return EOF;
        }
        return static_cast<unsigned char>(buffer[pos++]);
    }

    // 跳过空白，返回下一个数字的首字节。
    int Start() {
        int c;
        do { c = Get(); } while (c != EOF && c <= ' ');
        return c;
    }

public:
    // 输入按声明格式提供；只在文件无法打开时报告路径。
    explicit Reader(const std::string& path) : file(std::fopen(path.c_str(), "rb")), buffer(8U << 20) {
        if (!file) throw std::runtime_error("Cannot open " + path);
        token.reserve(32);
    }

    // 关闭本次读入的文件。
    ~Reader() { std::fclose(file); }

    // 读取非负十进制整数，用于规模、组大小和顶点编号。
    int Int() {
        int c = Start(), value = 0;
        if (c == '+') c = Get();
        while (c >= '0' && c <= '9') {
            value = value * 10 + c - '0';
            c = Get();
        }
        return value;
    }

    // 用标准库转换原始浮点 token，保留 double 的舍入精度。
    double Real() {
        token.clear();
        int c = Start();
        while (c > ' ') {
            token.push_back(static_cast<char>(c));
            c = Get();
        }
        const char* first = token.data() + (token.front() == '+');
        double value = 0;
        std::from_chars(first, token.data() + token.size(), value);
        return value;
    }
};

// 对一个组命中的连通分量排序去重，供组间求交。
std::vector<int> GroupComponents(const std::vector<int>& group, const std::vector<int>& component_of) {
    std::vector<int> result;
    result.reserve(group.size());
    for (int v : group) result.push_back(component_of[v]);
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

} // namespace

// 先读原边并统计度数，再一次分配邻接表；最后缓存图的连通分量。
Graph LoadGraph(const std::string& folder) {
    Reader in(folder + "/graph.txt");
    Graph graph;
    graph.n = in.Int();
    graph.m = in.Int();
    graph.edges.resize(graph.m);
    std::vector<size_t> degree(graph.n + 1);
    for (int i = 0; i < graph.m; ++i) {
        int u = in.Int(), v = in.Int();
        double w = in.Real();
        graph.edges[i] = {i, u, v, w};
        graph.minimum_edge_weight = std::min(graph.minimum_edge_weight, w);
        ++degree[u];
        ++degree[v];
    }

    // 保持输入边顺序，自环也保留两个邻接项。
    graph.adj.resize(graph.n + 1);
    for (int v = 1; v <= graph.n; ++v) graph.adj[v].reserve(degree[v]);
    for (const auto& e : graph.edges) {
        graph.adj[e.u].push_back({e.v, e.id, e.w});
        graph.adj[e.v].push_back({e.u, e.id, e.w});
    }

    // 连通分量只算一次，后续查询只对其组成员做集合交。
    graph.component_of.assign(graph.n + 1, -1);
    std::vector<int> queue;
    for (int root = 1; root <= graph.n; ++root) {
        if (graph.component_of[root] >= 0) continue;
        queue.clear();
        queue.push_back(root);
        graph.component_of[root] = graph.component_count;
        for (size_t i = 0; i < queue.size(); ++i) {
            for (const auto& e : graph.adj[queue[i]]) {
                if (graph.component_of[e.to] >= 0) continue;
                graph.component_of[e.to] = graph.component_count;
                queue.push_back(e.to);
            }
        }
        ++graph.component_count;
    }
    return graph;
}

// 查询文件以查询数开头，每条依次给出组数和各组成员。
std::vector<Query> LoadQueries(const std::string& file) {
    Reader in(file);
    std::vector<Query> queries(in.Int());
    for (auto& query : queries) {
        query.groups.resize(in.Int());
        for (auto& group : query.groups) {
            group.resize(in.Int());
            for (int& v : group) v = in.Int();
        }
    }
    return queries;
}

// 若一个连通分量同时含有所有组的候选点，就能在该分量内连接它们。
bool IsQueryFeasible(const Graph& graph, const Query& query) {
    if (query.groups.empty() || graph.component_count == 1) return true;
    std::vector<int> candidates = GroupComponents(query.groups.front(), graph.component_of);
    for (size_t i = 1; i < query.groups.size() && !candidates.empty(); ++i) {
        const auto current = GroupComponents(query.groups[i], graph.component_of);
        std::vector<int> intersection;
        intersection.reserve(std::min(candidates.size(), current.size()));
        std::set_intersection(candidates.begin(), candidates.end(), current.begin(), current.end(), std::back_inserter(intersection));
        candidates.swap(intersection);
    }
    return !candidates.empty();
}

} // namespace abhss
