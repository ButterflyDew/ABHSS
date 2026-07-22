#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
struct EdgeRecord
{
    std::uint32_t u = 0;
    std::uint32_t v = 0;
    double weight = 0.0;
};

struct LoadedGraph
{
    std::uint64_t n = 0;
    std::vector<EdgeRecord> edges;
};

fs::path ResolveGraphFile(const fs::path& input)
{
    if (fs::is_regular_file(input))
        return input;
    for (const char* name : {"graph.txt", "Graph.txt"})
    {
        const fs::path candidate = input / name;
        if (fs::is_regular_file(candidate))
            return candidate;
    }
    throw std::runtime_error("Cannot resolve graph file from: " + input.string());
}

LoadedGraph Load(const fs::path& input)
{
    const fs::path path = ResolveGraphFile(input);
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open: " + path.string());

    std::uint64_t n = 0;
    std::uint64_t m = 0;
    file >> n >> m;
    if (!file || n == 0 || n > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Invalid header: " + path.string());

    LoadedGraph graph;
    graph.n = n;
    graph.edges.reserve(static_cast<std::size_t>(m));
    for (std::uint64_t index = 0; index < m; ++index)
    {
        std::uint64_t u = 0;
        std::uint64_t v = 0;
        double weight = 0.0;
        file >> u >> v >> weight;
        if (!file || u == 0 || v == 0 || u > n || v > n)
            throw std::runtime_error("Invalid edge " + std::to_string(index) + " in " + path.string());
        if (u > v)
            std::swap(u, v);
        graph.edges.push_back({static_cast<std::uint32_t>(u),
                               static_cast<std::uint32_t>(v), weight});
    }
    std::sort(graph.edges.begin(), graph.edges.end(), [](const EdgeRecord& left,
                                                          const EdgeRecord& right)
    {
        if (left.u != right.u)
            return left.u < right.u;
        if (left.v != right.v)
            return left.v < right.v;
        return left.weight < right.weight;
    });
    return graph;
}

bool SameEndpoint(const EdgeRecord& left, const EdgeRecord& right)
{
    return left.u == right.u && left.v == right.v;
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: compare_graphs <graph-or-folder-a> <graph-or-folder-b>\n";
            return 2;
        }
        LoadedGraph left = Load(argv[1]);
        LoadedGraph right = Load(argv[2]);

        const bool header_equal = left.n == right.n && left.edges.size() == right.edges.size();
        bool topology_equal = header_equal;
        bool weights_equal = header_equal;
        bool scalar_weights = header_equal;
        double reference_ratio = std::numeric_limits<double>::quiet_NaN();
        double maximum_absolute_difference = 0.0;
        std::size_t first_topology_mismatch = left.edges.size();

        if (header_equal)
        {
            for (std::size_t index = 0; index < left.edges.size(); ++index)
            {
                const EdgeRecord& a = left.edges[index];
                const EdgeRecord& b = right.edges[index];
                if (!SameEndpoint(a, b))
                {
                    topology_equal = false;
                    weights_equal = false;
                    scalar_weights = false;
                    first_topology_mismatch = index;
                    break;
                }
                const double difference = std::abs(a.weight - b.weight);
                maximum_absolute_difference = std::max(maximum_absolute_difference, difference);
                if (difference > 1e-12 * std::max({1.0, std::abs(a.weight), std::abs(b.weight)}))
                    weights_equal = false;

                if (a.weight == 0.0 || b.weight == 0.0)
                {
                    if (a.weight != b.weight)
                        scalar_weights = false;
                }
                else
                {
                    const double ratio = b.weight / a.weight;
                    if (std::isnan(reference_ratio))
                        reference_ratio = ratio;
                    else if (std::abs(ratio - reference_ratio) >
                             1e-10 * std::max({1.0, std::abs(ratio), std::abs(reference_ratio)}))
                        scalar_weights = false;
                }
            }
        }

        std::cout << std::boolalpha << std::setprecision(17)
                  << "vertices_a=" << left.n << '\n'
                  << "vertices_b=" << right.n << '\n'
                  << "edges_a=" << left.edges.size() << '\n'
                  << "edges_b=" << right.edges.size() << '\n'
                  << "header_equal=" << header_equal << '\n'
                  << "topology_multiset_equal=" << topology_equal << '\n'
                  << "weights_equal=" << weights_equal << '\n'
                  << "weights_constant_scale=" << (topology_equal && scalar_weights) << '\n'
                  << "b_over_a_weight_scale=" << reference_ratio << '\n'
                  << "max_abs_weight_difference=" << maximum_absolute_difference << '\n';
        if (!topology_equal && header_equal)
            std::cout << "first_sorted_topology_mismatch=" << first_topology_mismatch << '\n';
        return topology_equal ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
