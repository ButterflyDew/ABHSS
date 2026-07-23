#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

class DisjointSet
{
public:
    explicit DisjointSet(std::uint32_t count) : parent_(count), size_(count, 1)
    {
        for (std::uint32_t vertex = 0; vertex < count; ++vertex)
        {
            parent_[vertex] = vertex;
        }
    }

    std::uint32_t Find(std::uint32_t vertex)
    {
        std::uint32_t root = vertex;
        while (parent_[root] != root)
        {
            root = parent_[root];
        }
        while (parent_[vertex] != vertex)
        {
            const std::uint32_t next = parent_[vertex];
            parent_[vertex] = root;
            vertex = next;
        }
        return root;
    }

    void Unite(std::uint32_t left, std::uint32_t right)
    {
        left = Find(left);
        right = Find(right);
        if (left == right)
        {
            return;
        }
        if (size_[left] < size_[right])
        {
            std::swap(left, right);
        }
        parent_[right] = left;
        size_[left] += size_[right];
    }

private:
    std::vector<std::uint32_t> parent_;
    std::vector<std::uint32_t> size_;
};

struct GraphAudit
{
    std::uint32_t vertices = 0;
    std::uint64_t edges = 0;
    std::uint32_t components = 0;
    std::uint32_t largest_component_vertices = 0;
};

struct QueryAudit
{
    fs::path path;
    std::uint64_t queries = 0;
    std::uint64_t feasible = 0;
    std::uint64_t infeasible = 0;
    std::uint32_t minimum_g = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t maximum_g = 0;
    std::vector<std::uint64_t> infeasible_indices;
};

std::string JsonEscape(const std::string& text)
{
    std::string result;
    result.reserve(text.size() + 8);
    for (const unsigned char character : text)
    {
        switch (character)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character < 0x20)
            {
                const char* digits = "0123456789abcdef";
                result += "\\u00";
                result += digits[(character >> 4) & 0x0f];
                result += digits[character & 0x0f];
            }
            else
            {
                result += static_cast<char>(character);
            }
        }
    }
    return result;
}

GraphAudit ReadGraph(const fs::path& graph_path, DisjointSet*& output_dsu)
{
    std::ifstream input(graph_path);
    if (!input)
    {
        throw std::runtime_error("cannot open graph: " + graph_path.string());
    }
    std::uint64_t vertices64 = 0;
    std::uint64_t edges = 0;
    if (!(input >> vertices64 >> edges) || vertices64 == 0 ||
        vertices64 > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error("invalid graph header: " + graph_path.string());
    }
    auto dsu = std::make_unique<DisjointSet>(static_cast<std::uint32_t>(vertices64));
    for (std::uint64_t edge = 0; edge < edges; ++edge)
    {
        std::uint64_t left = 0;
        std::uint64_t right = 0;
        double weight = 0;
        if (!(input >> left >> right >> weight))
        {
            throw std::runtime_error("truncated graph at edge " + std::to_string(edge + 1));
        }
        if (left == 0 || right == 0 || left > vertices64 || right > vertices64)
        {
            throw std::runtime_error("out-of-range graph endpoint");
        }
        dsu->Unite(static_cast<std::uint32_t>(left - 1),
                   static_cast<std::uint32_t>(right - 1));
    }

    std::vector<std::uint32_t> component_sizes(vertices64, 0);
    for (std::uint32_t vertex = 0; vertex < vertices64; ++vertex)
    {
        ++component_sizes[dsu->Find(vertex)];
    }
    GraphAudit audit;
    audit.vertices = static_cast<std::uint32_t>(vertices64);
    audit.edges = edges;
    for (const std::uint32_t size : component_sizes)
    {
        if (size)
        {
            ++audit.components;
            audit.largest_component_vertices =
                std::max(audit.largest_component_vertices, size);
        }
    }
    output_dsu = dsu.release();
    return audit;
}

QueryAudit AuditQueries(const fs::path& path, std::uint32_t vertices, DisjointSet& dsu)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("cannot open query file: " + path.string());
    }
    QueryAudit audit;
    audit.path = path;
    if (!(input >> audit.queries))
    {
        throw std::runtime_error("invalid query header: " + path.string());
    }
    for (std::uint64_t query_index = 1; query_index <= audit.queries; ++query_index)
    {
        std::uint32_t group_count = 0;
        if (!(input >> group_count) || group_count == 0)
        {
            throw std::runtime_error("invalid g at query " + std::to_string(query_index));
        }
        audit.minimum_g = std::min(audit.minimum_g, group_count);
        audit.maximum_g = std::max(audit.maximum_g, group_count);
        std::vector<std::uint32_t> common_components;
        for (std::uint32_t group = 0; group < group_count; ++group)
        {
            std::uint64_t group_size = 0;
            if (!(input >> group_size) || group_size == 0)
            {
                throw std::runtime_error("empty or malformed query group");
            }
            std::vector<std::uint32_t> group_components;
            group_components.reserve(static_cast<std::size_t>(group_size));
            for (std::uint64_t member_index = 0; member_index < group_size; ++member_index)
            {
                std::uint64_t member = 0;
                if (!(input >> member) || member == 0 || member > vertices)
                {
                    throw std::runtime_error("out-of-range query member");
                }
                group_components.push_back(dsu.Find(static_cast<std::uint32_t>(member - 1)));
            }
            std::sort(group_components.begin(), group_components.end());
            group_components.erase(
                std::unique(group_components.begin(), group_components.end()),
                group_components.end());
            if (group == 0)
            {
                common_components = std::move(group_components);
            }
            else
            {
                std::vector<std::uint32_t> intersection;
                intersection.reserve(std::min(common_components.size(), group_components.size()));
                std::set_intersection(
                    common_components.begin(), common_components.end(),
                    group_components.begin(), group_components.end(),
                    std::back_inserter(intersection));
                common_components = std::move(intersection);
            }
        }
        if (common_components.empty())
        {
            ++audit.infeasible;
            audit.infeasible_indices.push_back(query_index);
        }
        else
        {
            ++audit.feasible;
        }
    }
    if (audit.queries == 0)
    {
        audit.minimum_g = 0;
    }
    return audit;
}

void WriteJson(const fs::path& output_path, const fs::path& graph_path,
               const GraphAudit& graph, const std::vector<QueryAudit>& queries)
{
    std::ofstream output(output_path);
    if (!output)
    {
        throw std::runtime_error("cannot write output: " + output_path.string());
    }
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"graph_path\": \"" << JsonEscape(graph_path.generic_string()) << "\",\n"
           << "  \"vertices\": " << graph.vertices << ",\n"
           << "  \"edges\": " << graph.edges << ",\n"
           << "  \"connected_components\": " << graph.components << ",\n"
           << "  \"largest_component_vertices\": " << graph.largest_component_vertices << ",\n"
           << "  \"query_files\": [\n";
    for (std::size_t index = 0; index < queries.size(); ++index)
    {
        const QueryAudit& audit = queries[index];
        output << "    {\n"
               << "      \"path\": \"" << JsonEscape(audit.path.generic_string()) << "\",\n"
               << "      \"queries\": " << audit.queries << ",\n"
               << "      \"feasible\": " << audit.feasible << ",\n"
               << "      \"infeasible\": " << audit.infeasible << ",\n"
               << "      \"minimum_g\": " << audit.minimum_g << ",\n"
               << "      \"maximum_g\": " << audit.maximum_g << ",\n"
               << "      \"infeasible_query_indices\": [";
        for (std::size_t bad = 0; bad < audit.infeasible_indices.size(); ++bad)
        {
            if (bad)
            {
                output << ", ";
            }
            output << audit.infeasible_indices[bad];
        }
        output << "]\n    }" << (index + 1 == queries.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 3)
        {
            throw std::runtime_error(
                "usage: audit_query_feasibility <graph.txt> <output.json> [query.txt ...]");
        }
        const fs::path graph_path = argv[1];
        const fs::path output_path = argv[2];
        DisjointSet* raw_dsu = nullptr;
        const GraphAudit graph = ReadGraph(graph_path, raw_dsu);
        std::unique_ptr<DisjointSet> dsu(raw_dsu);
        std::vector<QueryAudit> audits;
        for (int index = 3; index < argc; ++index)
        {
            audits.push_back(AuditQueries(argv[index], graph.vertices, *dsu));
        }
        WriteJson(output_path, graph_path, graph, audits);
        std::uint64_t total = 0;
        std::uint64_t feasible = 0;
        for (const QueryAudit& audit : audits)
        {
            total += audit.queries;
            feasible += audit.feasible;
        }
        std::cout << "n=" << graph.vertices << " m=" << graph.edges
                  << " components=" << graph.components
                  << " largest_component=" << graph.largest_component_vertices
                  << " queries=" << total << " feasible=" << feasible << std::endl;
        return feasible == total ? 0 : 2;
    }
    catch (const std::exception& error)
    {
        std::cerr << "audit_query_feasibility: " << error.what() << std::endl;
        return 1;
    }
}
