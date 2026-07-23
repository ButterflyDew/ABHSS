#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace
{

struct EdgeRecord
{
    std::uint32_t title;
    std::uint32_t person;
};
static_assert(sizeof(EdgeRecord) == 8, "unexpected edge-record padding");

struct SourceVertex
{
    std::uint32_t source_id = 0;
    bool is_title = false;
};

std::vector<std::string_view> SplitTabs(const std::string& line)
{
    std::vector<std::string_view> fields;
    std::size_t begin = 0;
    while (begin <= line.size())
    {
        const std::size_t end = line.find('\t', begin);
        fields.emplace_back(line.data() + begin,
                            (end == std::string::npos ? line.size() : end) - begin);
        if (end == std::string::npos)
        {
            break;
        }
        begin = end + 1;
    }
    return fields;
}

std::uint32_t ParseSourceId(std::string_view value, std::string_view prefix)
{
    if (value.size() <= prefix.size() || value.substr(0, prefix.size()) != prefix)
    {
        throw std::runtime_error("unexpected IMDb identifier: " + std::string(value));
    }
    std::uint64_t parsed = 0;
    const char* begin = value.data() + prefix.size();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end ||
        parsed > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error("invalid or oversized IMDb identifier: " + std::string(value));
    }
    return static_cast<std::uint32_t>(parsed);
}

std::vector<std::string> TokenizeAscii(std::string_view text)
{
    std::vector<std::string> tokens;
    std::string token;
    for (const unsigned char value : text)
    {
        const bool alpha = (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
        const bool digit = value >= '0' && value <= '9';
        if (alpha || digit)
        {
            token.push_back(static_cast<char>(value >= 'A' && value <= 'Z' ? value + 32U : value));
        }
        else if (!token.empty())
        {
            tokens.push_back(std::move(token));
            token.clear();
        }
    }
    if (!token.empty())
    {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

void AddFieldTokens(std::vector<std::string>& tokens, std::string_view field)
{
    if (field == R"(\N)")
    {
        return;
    }
    std::vector<std::string> added = TokenizeAscii(field);
    tokens.insert(tokens.end(), std::make_move_iterator(added.begin()),
                  std::make_move_iterator(added.end()));
}

void AtomicRename(const fs::path& temporary, const fs::path& destination)
{
    if (!fs::exists(temporary))
    {
        throw std::runtime_error("temporary output is missing: " + temporary.string());
    }
    std::error_code error;
    fs::remove(destination, error);
    error.clear();
    fs::rename(temporary, destination, error);
    if (error)
    {
        throw std::runtime_error("cannot replace " + destination.string() + ": " + error.message());
    }
}

std::uint32_t ParsePositive(const char* text, const char* name)
{
    std::uint64_t value = 0;
    const std::string_view input(text);
    const auto result = std::from_chars(input.data(), input.data() + input.size(), value);
    if (result.ec != std::errc() || result.ptr != input.data() + input.size() || value == 0 ||
        value > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error(std::string("invalid ") + name);
    }
    return static_cast<std::uint32_t>(value);
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 7 && argc != 8)
        {
            throw std::runtime_error(
                "usage: build_imdb_graph <title.basics.tsv> <title.principals.tsv> "
                "<name.basics.tsv> <output_dir> <minimum_label_frequency> <maximum_label_frequency> "
                "[--replace-existing]");
        }
        const fs::path title_path = argv[1];
        const fs::path principal_path = argv[2];
        const fs::path name_path = argv[3];
        const fs::path output_dir = argv[4];
        const std::uint32_t minimum_frequency = ParsePositive(argv[5], "minimum label frequency");
        const std::uint32_t maximum_frequency = ParsePositive(argv[6], "maximum label frequency");
        if (minimum_frequency > maximum_frequency)
        {
            throw std::runtime_error("minimum label frequency exceeds maximum");
        }
        const fs::path graph_path = output_dir / "graph.txt";
        const fs::path groups_path = output_dir / "candidate_groups.txt";
        const bool replace_existing = argc == 8 && std::string_view(argv[7]) == "--replace-existing";
        if (argc == 8 && !replace_existing)
        {
            throw std::runtime_error("unknown option; expected --replace-existing");
        }
        if (!replace_existing && (fs::exists(graph_path) || fs::exists(groups_path)))
        {
            throw std::runtime_error("output exists; refusing to overwrite a frozen interface file");
        }
        fs::create_directories(output_dir);

        std::ifstream principals(principal_path);
        if (!principals)
        {
            throw std::runtime_error("failed to open " + principal_path.string());
        }
        std::string line;
        if (!std::getline(principals, line) ||
            line != "tconst\tordering\tnconst\tcategory\tjob\tcharacters")
        {
            throw std::runtime_error("unexpected title.principals.tsv header");
        }
        std::unordered_map<std::uint32_t, std::uint32_t> title_ids;
        std::unordered_map<std::uint32_t, std::uint32_t> person_ids;
        title_ids.reserve(12'000'000);
        person_ids.reserve(15'000'000);
        std::vector<SourceVertex> source_vertices(1);
        std::vector<std::uint32_t> component_parent(1, 0);
        std::vector<std::uint32_t> component_size(1, 0);
        auto find_component = [&](std::uint32_t vertex)
        {
            std::uint32_t root = vertex;
            while (component_parent[root] != root)
            {
                root = component_parent[root];
            }
            while (component_parent[vertex] != vertex)
            {
                const std::uint32_t next = component_parent[vertex];
                component_parent[vertex] = root;
                vertex = next;
            }
            return root;
        };
        auto get_vertex = [&](std::unordered_map<std::uint32_t, std::uint32_t>& mapping,
                              std::uint32_t source_id, bool is_title)
        {
            const auto found = mapping.find(source_id);
            if (found != mapping.end())
            {
                return found->second;
            }
            if (source_vertices.size() > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::runtime_error("IMDb graph exceeds uint32 vertex identifiers");
            }
            const auto dense = static_cast<std::uint32_t>(source_vertices.size());
            source_vertices.push_back({source_id, is_title});
            component_parent.push_back(dense);
            component_size.push_back(1);
            mapping.emplace(source_id, dense);
            return dense;
        };

        const fs::path spool_path = output_dir / "imdb_edges.bin.tmp";
        std::ofstream spool(spool_path, std::ios::binary);
        std::uint64_t principal_rows = 0;
        while (std::getline(principals, line))
        {
            if (line.empty())
            {
                continue;
            }
            const auto fields = SplitTabs(line);
            if (fields.size() != 6)
            {
                throw std::runtime_error("malformed title.principals.tsv row " +
                                         std::to_string(principal_rows + 2U));
            }
            const std::uint32_t title_source = ParseSourceId(fields[0], "tt");
            const std::uint32_t person_source = ParseSourceId(fields[2], "nm");
            const EdgeRecord record{
                get_vertex(title_ids, title_source, true),
                get_vertex(person_ids, person_source, false),
            };
            std::uint32_t title_root = find_component(record.title);
            std::uint32_t person_root = find_component(record.person);
            if (title_root != person_root)
            {
                if (component_size[title_root] < component_size[person_root])
                {
                    std::swap(title_root, person_root);
                }
                component_parent[person_root] = title_root;
                component_size[title_root] += component_size[person_root];
            }
            spool.write(reinterpret_cast<const char*>(&record), sizeof(record));
            if (!spool)
            {
                throw std::runtime_error("failed while writing IMDb edge spool");
            }
            ++principal_rows;
            if (principal_rows % 10'000'000U == 0)
            {
                std::cout << "principal_edges=" << principal_rows << " titles=" << title_ids.size()
                          << " people=" << person_ids.size() << std::endl;
            }
        }
        spool.close();
        std::uint32_t largest_component = 0;
        std::uint32_t largest_component_size = 0;
        for (std::uint32_t vertex = 1; vertex < component_parent.size(); ++vertex)
        {
            const std::uint32_t root = find_component(vertex);
            if (component_size[root] > largest_component_size)
            {
                largest_component = root;
                largest_component_size = component_size[root];
            }
        }
        std::cout << "principal_edges=" << principal_rows << " titles=" << title_ids.size()
                  << " people=" << person_ids.size() << " vertices=" << source_vertices.size() - 1U
                  << std::endl;
        const std::size_t title_vertex_count = title_ids.size();
        const std::size_t person_vertex_count = person_ids.size();

        const fs::path graph_temporary = graph_path.string() + ".tmp";
        std::ifstream spool_input(spool_path, std::ios::binary);
        std::ofstream graph(graph_temporary, std::ios::binary);
        graph << source_vertices.size() - 1U << ' ' << principal_rows << '\n';
        EdgeRecord edge{};
        std::uint64_t graph_rows = 0;
        while (spool_input.read(reinterpret_cast<char*>(&edge), sizeof(edge)))
        {
            graph << edge.title << ' ' << edge.person << " 1\n";
            ++graph_rows;
        }
        if (!spool_input.eof() || graph_rows != principal_rows || !graph)
        {
            throw std::runtime_error("IMDb edge spool is truncated or graph write failed");
        }
        graph.close();
        spool_input.close();
        fs::remove(spool_path);
        AtomicRename(graph_temporary, graph_path);

        std::unordered_map<std::string, std::vector<std::uint32_t>> labels;
        labels.reserve(4'000'000);
        std::uint64_t title_rows = 0;
        std::uint64_t labeled_titles = 0;
        std::ifstream titles(title_path);
        if (!titles || !std::getline(titles, line) ||
            line != "tconst\ttitleType\tprimaryTitle\toriginalTitle\tisAdult\tstartYear\tendYear\truntimeMinutes\tgenres")
        {
            throw std::runtime_error("unexpected title.basics.tsv header");
        }
        while (std::getline(titles, line))
        {
            ++title_rows;
            const auto fields = SplitTabs(line);
            if (fields.size() != 9)
            {
                throw std::runtime_error("malformed title.basics.tsv row " +
                                         std::to_string(title_rows + 1U));
            }
            const std::uint32_t source_id = ParseSourceId(fields[0], "tt");
            const auto found = title_ids.find(source_id);
            if (found == title_ids.end())
            {
                continue;
            }
            ++labeled_titles;
            std::vector<std::string> tokens;
            AddFieldTokens(tokens, fields[1]);
            AddFieldTokens(tokens, fields[2]);
            AddFieldTokens(tokens, fields[3]);
            AddFieldTokens(tokens, fields[8]);
            std::sort(tokens.begin(), tokens.end());
            tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
            for (std::string& token : tokens)
            {
                labels[token].push_back(found->second);
            }
            if (title_rows % 2'000'000U == 0)
            {
                std::cout << "title_rows=" << title_rows << " labeled=" << labeled_titles
                          << " token_types=" << labels.size() << std::endl;
            }
        }
        title_ids.clear();
        title_ids.rehash(0);

        std::uint64_t name_rows = 0;
        std::uint64_t labeled_people = 0;
        std::ifstream names(name_path);
        if (!names || !std::getline(names, line) ||
            line != "nconst\tprimaryName\tbirthYear\tdeathYear\tprimaryProfession\tknownForTitles")
        {
            throw std::runtime_error("unexpected name.basics.tsv header");
        }
        while (std::getline(names, line))
        {
            ++name_rows;
            const auto fields = SplitTabs(line);
            if (fields.size() != 6)
            {
                throw std::runtime_error("malformed name.basics.tsv row " +
                                         std::to_string(name_rows + 1U));
            }
            const std::uint32_t source_id = ParseSourceId(fields[0], "nm");
            const auto found = person_ids.find(source_id);
            if (found == person_ids.end())
            {
                continue;
            }
            ++labeled_people;
            std::vector<std::string> tokens;
            AddFieldTokens(tokens, fields[1]);
            AddFieldTokens(tokens, fields[4]);
            std::sort(tokens.begin(), tokens.end());
            tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
            for (std::string& token : tokens)
            {
                labels[token].push_back(found->second);
            }
            if (name_rows % 2'000'000U == 0)
            {
                std::cout << "name_rows=" << name_rows << " labeled=" << labeled_people
                          << " token_types=" << labels.size() << std::endl;
            }
        }
        person_ids.clear();
        person_ids.rehash(0);

        std::vector<std::string> eligible_tokens;
        eligible_tokens.reserve(labels.size());
        std::uint64_t memberships = 0;
        for (auto& entry : labels)
        {
            auto& members = entry.second;
            std::sort(members.begin(), members.end());
            members.erase(std::unique(members.begin(), members.end()), members.end());
            if (members.size() >= minimum_frequency && members.size() <= maximum_frequency)
            {
                eligible_tokens.push_back(entry.first);
                memberships += members.size();
            }
        }
        std::sort(eligible_tokens.begin(), eligible_tokens.end());

        const fs::path groups_temporary = groups_path.string() + ".tmp";
        const fs::path group_map_path = output_dir / "source_group_ids.tsv";
        const fs::path group_map_temporary = group_map_path.string() + ".tmp";
        std::ofstream groups(groups_temporary);
        std::ofstream group_map(group_map_temporary);
        group_map << "group_id\ttoken\tsize\tlargest_component_members\n";
        for (std::size_t index = 0; index < eligible_tokens.size(); ++index)
        {
            const auto& members = labels.at(eligible_tokens[index]);
            groups << 'g' << index + 1U << ':';
            for (const std::uint32_t member : members)
            {
                groups << ' ' << member;
            }
            groups << '\n';
            const std::size_t largest_members = static_cast<std::size_t>(std::count_if(
                members.begin(), members.end(), [&](std::uint32_t vertex)
                {
                    return find_component(vertex) == largest_component;
                }));
            group_map << index + 1U << '\t' << eligible_tokens[index] << '\t' << members.size()
                      << '\t' << largest_members << '\n';
        }
        if (!groups || !group_map)
        {
            throw std::runtime_error("failed while writing IMDb candidate groups");
        }
        groups.close();
        group_map.close();
        AtomicRename(groups_temporary, groups_path);
        AtomicRename(group_map_temporary, group_map_path);

        const fs::path vertex_map_path = output_dir / "source_vertex_ids.tsv";
        const fs::path vertex_map_temporary = vertex_map_path.string() + ".tmp";
        std::ofstream vertex_map(vertex_map_temporary);
        vertex_map << "vertex_id\tsource_id\tvertex_type\n";
        for (std::size_t index = 1; index < source_vertices.size(); ++index)
        {
            const SourceVertex& vertex = source_vertices[index];
            vertex_map << index << '\t' << (vertex.is_title ? "tt" : "nm") << std::setfill('0')
                       << std::setw(7) << vertex.source_id << std::setfill(' ') << '\t'
                       << (vertex.is_title ? "title" : "person") << '\n';
        }
        vertex_map.close();
        AtomicRename(vertex_map_temporary, vertex_map_path);

        const fs::path stats_path = output_dir / "source_normalization.json";
        const fs::path stats_temporary = stats_path.string() + ".tmp";
        std::ofstream stats(stats_temporary);
        stats << "{\n"
              << "  \"schema_version\": 1,\n"
              << "  \"vertices\": " << source_vertices.size() - 1U << ",\n"
              << "  \"title_vertices\": " << title_vertex_count << ",\n"
              << "  \"person_vertices\": " << person_vertex_count << ",\n"
              << "  \"title_vertices_with_metadata\": " << labeled_titles << ",\n"
              << "  \"person_vertices_with_metadata\": " << labeled_people << ",\n"
              << "  \"vertices_without_metadata\": "
              << (source_vertices.size() - 1U - labeled_titles - labeled_people) << ",\n"
              << "  \"edges\": " << principal_rows << ",\n"
              << "  \"largest_connected_component_vertices\": " << largest_component_size << ",\n"
              << "  \"source_title_rows\": " << title_rows << ",\n"
              << "  \"source_name_rows\": " << name_rows << ",\n"
              << "  \"candidate_groups\": " << eligible_tokens.size() << ",\n"
              << "  \"candidate_group_memberships\": " << memberships << ",\n"
              << "  \"minimum_candidate_frequency\": " << minimum_frequency << ",\n"
              << "  \"maximum_candidate_frequency\": " << maximum_frequency << ",\n"
              << "  \"graph_model\": \"title-person bipartite multigraph; one unit edge per title.principals row\",\n"
              << "  \"label_fields\": \"titleType, primaryTitle, originalTitle, genres, primaryName, primaryProfession\",\n"
              << "  \"tokenization\": \"lower-case maximal ASCII alphanumeric runs, deduplicated per vertex\",\n"
              << "  \"vertex_mapping\": \"title then person first occurrence while scanning frozen title.principals.tsv\"\n"
              << "}\n";
        stats.close();
        AtomicRename(stats_temporary, stats_path);
        std::cout << "eligible_groups=" << eligible_tokens.size() << " memberships=" << memberships
                  << " label_frequency_range=" << minimum_frequency << ".." << maximum_frequency
                  << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "IMDb conversion failed: " << error.what() << std::endl;
        return 1;
    }
}
