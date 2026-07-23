#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{

constexpr const char* kRdfType = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
constexpr const char* kRdfsLabel = "http://www.w3.org/2000/01/rdf-schema#label";

struct EdgeRecord
{
    std::uint32_t u;
    std::uint32_t v;
    std::uint32_t predicate;
};
static_assert(sizeof(EdgeRecord) == 12, "unexpected binary edge-record padding");

struct ResourceTriple
{
    std::string subject;
    std::string predicate;
    std::string object;
};

bool ParseUri(const std::string& line, std::size_t& cursor, std::string& value)
{
    while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t'))
    {
        ++cursor;
    }
    if (cursor >= line.size() || line[cursor] != '<')
    {
        return false;
    }
    const std::size_t end = line.find('>', cursor + 1);
    if (end == std::string::npos)
    {
        return false;
    }
    value.assign(line, cursor + 1, end - cursor - 1);
    cursor = end + 1;
    return true;
}

bool ParseResourceTriple(const std::string& line, ResourceTriple& triple)
{
    std::size_t cursor = 0;
    return ParseUri(line, cursor, triple.subject) && ParseUri(line, cursor, triple.predicate) &&
           ParseUri(line, cursor, triple.object);
}

void AppendUtf8(std::string& output, std::uint32_t codepoint)
{
    if (codepoint <= 0x7fU)
    {
        output.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint <= 0x7ffU)
    {
        output.push_back(static_cast<char>(0xc0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
    }
    else if (codepoint <= 0xffffU)
    {
        output.push_back(static_cast<char>(0xe0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
    }
    else if (codepoint <= 0x10ffffU)
    {
        output.push_back(static_cast<char>(0xf0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3fU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
    }
}

int HexValue(char value)
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f')
    {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F')
    {
        return value - 'A' + 10;
    }
    return -1;
}

bool ParseLiteral(const std::string& line, std::size_t cursor, std::string& value)
{
    while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t'))
    {
        ++cursor;
    }
    if (cursor >= line.size() || line[cursor] != '"')
    {
        return false;
    }
    ++cursor;
    value.clear();
    while (cursor < line.size())
    {
        const char current = line[cursor++];
        if (current == '"')
        {
            return true;
        }
        if (current != '\\')
        {
            value.push_back(current);
            continue;
        }
        if (cursor >= line.size())
        {
            return false;
        }
        const char escaped = line[cursor++];
        switch (escaped)
        {
        case 't': value.push_back('\t'); break;
        case 'b': value.push_back('\b'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 'f': value.push_back('\f'); break;
        case '"': value.push_back('"'); break;
        case '\'': value.push_back('\''); break;
        case '\\': value.push_back('\\'); break;
        case 'u':
        case 'U':
        {
            const int digits = escaped == 'u' ? 4 : 8;
            if (cursor + static_cast<std::size_t>(digits) > line.size())
            {
                return false;
            }
            std::uint32_t codepoint = 0;
            for (int i = 0; i < digits; ++i)
            {
                const int hex = HexValue(line[cursor++]);
                if (hex < 0)
                {
                    return false;
                }
                codepoint = (codepoint << 4U) | static_cast<std::uint32_t>(hex);
            }
            AppendUtf8(value, codepoint);
            break;
        }
        default:
            value.push_back(escaped);
            break;
        }
    }
    return false;
}

bool ParseLabelTriple(const std::string& line, std::string& subject, std::string& label)
{
    std::size_t cursor = 0;
    std::string predicate;
    if (!ParseUri(line, cursor, subject) || !ParseUri(line, cursor, predicate) ||
        predicate != kRdfsLabel)
    {
        return false;
    }
    return ParseLiteral(line, cursor, label);
}

std::vector<std::string> TokenizeAscii(const std::string& text)
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
    std::sort(tokens.begin(), tokens.end());
    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
    return tokens;
}

std::unordered_set<std::string> LoadStopwords(const fs::path& path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("failed to open stopword file " + path.string());
    }
    std::unordered_set<std::string> result;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (!line.empty() && line[0] != '#')
        {
            result.insert(line);
        }
    }
    return result;
}

std::unordered_set<std::string> LoadVocabulary(
    const fs::path& path,
    const std::unordered_set<std::string>& stopwords)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("failed to open query vocabulary source " + path.string());
    }
    std::unordered_set<std::string> result;
    std::string line;
    while (std::getline(input, line))
    {
        const std::size_t tab = line.find('\t');
        const std::string text = tab == std::string::npos ? line : line.substr(tab + 1);
        for (std::string& token : TokenizeAscii(text))
        {
            if (!stopwords.count(token))
            {
                result.insert(std::move(token));
            }
        }
    }
    return result;
}

void AtomicRename(const fs::path& temporary, const fs::path& destination)
{
    if (!fs::exists(temporary))
    {
        throw std::runtime_error("temporary output is missing: " + temporary.string());
    }
    fs::rename(temporary, destination);
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 5 && argc != 6)
        {
            throw std::runtime_error(
                "usage: build_rdf_graph <graph.nt/ttl> <labels.nt/ttl> <output_dir> "
                "<stopwords.txt> [query_vocabulary.txt]");
        }
        const fs::path graph_source = argv[1];
        const fs::path label_source = argv[2];
        const fs::path output_dir = argv[3];
        const fs::path stopword_path = argv[4];
        const bool filtered_vocabulary = argc == 6;
        const fs::path vocabulary_path = filtered_vocabulary ? fs::path(argv[5]) : fs::path();
        const fs::path graph_path = output_dir / "graph.txt";
        const fs::path groups_path = output_dir / "candidate_groups.txt";
        if (fs::exists(graph_path) || fs::exists(groups_path))
        {
            throw std::runtime_error("output exists; refusing to overwrite a frozen interface file");
        }
        fs::create_directories(output_dir);

        std::ifstream graph_input(graph_source);
        if (!graph_input)
        {
            throw std::runtime_error("failed to open " + graph_source.string());
        }
        std::unordered_map<std::string, std::uint32_t> vertex_ids;
        std::vector<std::string> vertex_uris(1);
        std::unordered_map<std::string, std::uint32_t> predicate_ids;
        std::vector<std::string> predicate_uris;
        std::vector<std::uint64_t> predicate_frequencies;
        vertex_ids.reserve(8'000'000);
        predicate_ids.reserve(2048);

        const fs::path spool_path = output_dir / "rdf_edges.bin.tmp";
        std::ofstream spool_output(spool_path, std::ios::binary);
        if (!spool_output)
        {
            throw std::runtime_error("failed to create binary edge spool");
        }
        auto get_vertex = [&](const std::string& uri)
        {
            const auto found = vertex_ids.find(uri);
            if (found != vertex_ids.end())
            {
                return found->second;
            }
            if (vertex_uris.size() > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::runtime_error("RDF graph exceeds uint32 vertex identifiers");
            }
            const auto id = static_cast<std::uint32_t>(vertex_uris.size());
            vertex_uris.push_back(uri);
            vertex_ids.emplace(vertex_uris.back(), id);
            return id;
        };
        auto get_predicate = [&](const std::string& uri)
        {
            const auto found = predicate_ids.find(uri);
            if (found != predicate_ids.end())
            {
                return found->second;
            }
            const auto id = static_cast<std::uint32_t>(predicate_uris.size());
            predicate_uris.push_back(uri);
            predicate_frequencies.push_back(0);
            predicate_ids.emplace(predicate_uris.back(), id);
            return id;
        };

        std::string line;
        ResourceTriple triple;
        std::uint64_t source_rows = 0;
        std::uint64_t malformed_rows = 0;
        std::uint64_t literal_or_blank_object_rows = 0;
        std::uint64_t rdf_type_rows = 0;
        std::uint64_t self_loops = 0;
        std::uint64_t edges = 0;
        while (std::getline(graph_input, line))
        {
            ++source_rows;
            if (!ParseResourceTriple(line, triple))
            {
                if (!line.empty() && line[0] == '<')
                {
                    ++literal_or_blank_object_rows;
                }
                else if (!line.empty())
                {
                    ++malformed_rows;
                }
                continue;
            }
            if (triple.predicate == kRdfType)
            {
                ++rdf_type_rows;
                continue;
            }
            const std::uint32_t u = get_vertex(triple.subject);
            const std::uint32_t v = get_vertex(triple.object);
            if (u == v)
            {
                ++self_loops;
                continue;
            }
            const std::uint32_t predicate = get_predicate(triple.predicate);
            ++predicate_frequencies[predicate];
            const EdgeRecord record{u, v, predicate};
            spool_output.write(reinterpret_cast<const char*>(&record), sizeof(record));
            if (!spool_output)
            {
                throw std::runtime_error("failed while writing binary edge spool");
            }
            ++edges;
            if (edges % 5'000'000U == 0)
            {
                std::cout << "rdf_edges=" << edges << " vertices=" << vertex_ids.size() << std::endl;
            }
        }
        spool_output.close();
        if (malformed_rows)
        {
            throw std::runtime_error("encountered " + std::to_string(malformed_rows) +
                                     " malformed non-empty RDF rows");
        }
        std::cout << "rdf_rows=" << source_rows << " vertices=" << vertex_ids.size()
                  << " edges=" << edges << " predicates=" << predicate_uris.size()
                  << " rdf_type=" << rdf_type_rows << " non_uri_object="
                  << literal_or_blank_object_rows << " self_loops_removed=" << self_loops << std::endl;

        const fs::path graph_temporary = graph_path.string() + ".tmp";
        std::ifstream spool_input(spool_path, std::ios::binary);
        std::ofstream graph_output(graph_temporary, std::ios::binary);
        if (!spool_input || !graph_output)
        {
            throw std::runtime_error("failed to reopen edge spool or graph output");
        }
        graph_output << vertex_ids.size() << ' ' << edges << '\n' << std::fixed << std::setprecision(10);
        EdgeRecord record{};
        std::uint64_t written_edges = 0;
        while (spool_input.read(reinterpret_cast<char*>(&record), sizeof(record)))
        {
            graph_output << record.u << ' ' << record.v << ' '
                         << std::log1p(static_cast<double>(predicate_frequencies.at(record.predicate))) << '\n';
            ++written_edges;
        }
        if (!spool_input.eof() || written_edges != edges || !graph_output)
        {
            throw std::runtime_error("binary edge spool is truncated or graph write failed");
        }
        graph_output.close();
        spool_input.close();
        fs::remove(spool_path);
        AtomicRename(graph_temporary, graph_path);

        const auto stopwords = LoadStopwords(stopword_path);
        const auto vocabulary = filtered_vocabulary
                                    ? LoadVocabulary(vocabulary_path, {})
                                    : std::unordered_set<std::string>();
        std::unordered_map<std::string, std::vector<std::uint32_t>> token_groups;
        if (filtered_vocabulary)
        {
            token_groups.reserve(vocabulary.size() * 2U);
            for (const std::string& token : vocabulary)
            {
                token_groups.emplace(token, std::vector<std::uint32_t>());
            }
        }
        else
        {
            token_groups.reserve(1'000'000);
        }
        std::ifstream label_input(label_source);
        if (!label_input)
        {
            throw std::runtime_error("failed to open " + label_source.string());
        }
        std::uint64_t label_rows = 0;
        std::uint64_t labels_on_graph_vertices = 0;
        std::uint64_t token_memberships_before_dedup = 0;
        std::string subject;
        std::string label;
        while (std::getline(label_input, line))
        {
            if (!ParseLabelTriple(line, subject, label))
            {
                continue;
            }
            ++label_rows;
            const auto vertex = vertex_ids.find(subject);
            if (vertex == vertex_ids.end())
            {
                continue;
            }
            ++labels_on_graph_vertices;
            for (std::string& token : TokenizeAscii(label))
            {
                if (!filtered_vocabulary && stopwords.count(token))
                {
                    continue;
                }
                if (filtered_vocabulary)
                {
                    const auto group = token_groups.find(token);
                    if (group != token_groups.end())
                    {
                        group->second.push_back(vertex->second);
                        ++token_memberships_before_dedup;
                    }
                }
                else
                {
                    token_groups[token].push_back(vertex->second);
                    ++token_memberships_before_dedup;
                }
            }
        }
        vertex_ids.clear();
        vertex_ids.rehash(0);

        std::vector<std::string> tokens;
        tokens.reserve(token_groups.size());
        for (auto& entry : token_groups)
        {
            std::vector<std::uint32_t>& members = entry.second;
            std::sort(members.begin(), members.end());
            members.erase(std::unique(members.begin(), members.end()), members.end());
            if (!members.empty())
            {
                tokens.push_back(entry.first);
            }
        }
        std::sort(tokens.begin(), tokens.end());

        const fs::path groups_temporary = groups_path.string() + ".tmp";
        const fs::path group_map_path = output_dir / "source_group_ids.tsv";
        const fs::path group_map_temporary = group_map_path.string() + ".tmp";
        std::ofstream groups_output(groups_temporary);
        std::ofstream group_map_output(group_map_temporary);
        group_map_output << "group_id\ttoken\tsize\n";
        std::uint64_t final_memberships = 0;
        for (std::size_t index = 0; index < tokens.size(); ++index)
        {
            const auto& members = token_groups.at(tokens[index]);
            groups_output << 'g' << index + 1U << ':';
            for (const std::uint32_t vertex : members)
            {
                groups_output << ' ' << vertex;
            }
            groups_output << '\n';
            group_map_output << index + 1U << '\t' << tokens[index] << '\t' << members.size() << '\n';
            final_memberships += members.size();
        }
        if (!groups_output || !group_map_output)
        {
            throw std::runtime_error("failed while writing candidate groups");
        }
        groups_output.close();
        group_map_output.close();
        AtomicRename(groups_temporary, groups_path);
        AtomicRename(group_map_temporary, group_map_path);

        const fs::path vertex_map_path = output_dir / "source_vertex_ids.tsv";
        const fs::path vertex_map_temporary = vertex_map_path.string() + ".tmp";
        std::ofstream vertex_map(vertex_map_temporary);
        vertex_map << "vertex_id\turi\n";
        for (std::size_t index = 1; index < vertex_uris.size(); ++index)
        {
            vertex_map << index << '\t' << vertex_uris[index] << '\n';
        }
        vertex_map.close();
        AtomicRename(vertex_map_temporary, vertex_map_path);

        const fs::path relation_path = output_dir / "relation_frequencies.tsv";
        const fs::path relation_temporary = relation_path.string() + ".tmp";
        std::vector<std::size_t> relation_order(predicate_uris.size());
        for (std::size_t index = 0; index < relation_order.size(); ++index)
        {
            relation_order[index] = index;
        }
        std::sort(relation_order.begin(), relation_order.end(), [&](std::size_t left, std::size_t right)
        {
            return predicate_uris[left] < predicate_uris[right];
        });
        std::ofstream relation_output(relation_temporary);
        relation_output << "predicate\tfrequency\tweight_ln1p_frequency\n" << std::fixed
                        << std::setprecision(10);
        for (const std::size_t index : relation_order)
        {
            relation_output << predicate_uris[index] << '\t' << predicate_frequencies[index] << '\t'
                            << std::log1p(static_cast<double>(predicate_frequencies[index])) << '\n';
        }
        relation_output.close();
        AtomicRename(relation_temporary, relation_path);

        const fs::path stats_path = output_dir / "source_normalization.json";
        const fs::path stats_temporary = stats_path.string() + ".tmp";
        std::ofstream stats(stats_temporary);
        stats << "{\n"
              << "  \"schema_version\": 1,\n"
              << "  \"rdf_rows\": " << source_rows << ",\n"
              << "  \"vertices\": " << vertex_uris.size() - 1U << ",\n"
              << "  \"edges\": " << edges << ",\n"
              << "  \"predicate_types\": " << predicate_uris.size() << ",\n"
              << "  \"rdf_type_rows_discarded\": " << rdf_type_rows << ",\n"
              << "  \"literal_or_blank_object_rows_discarded\": " << literal_or_blank_object_rows << ",\n"
              << "  \"self_loops_removed\": " << self_loops << ",\n"
              << "  \"label_rows\": " << label_rows << ",\n"
              << "  \"labels_on_graph_vertices\": " << labels_on_graph_vertices << ",\n"
              << "  \"candidate_groups\": " << tokens.size() << ",\n"
              << "  \"candidate_group_memberships\": " << final_memberships << ",\n"
              << "  \"memberships_before_deduplication\": " << token_memberships_before_dedup << ",\n"
              << "  \"vocabulary_mode\": \"" << (filtered_vocabulary ? "source-query tokens" : "all label tokens") << "\",\n"
              << "  \"tokenization\": \"lower-case maximal ASCII alphanumeric runs; per-label and per-group deduplication; all-token mode removes the frozen stopword list, source-query-vocabulary mode preserves exactly the supplied vocabulary\",\n"
              << "  \"vertex_mapping\": \"first occurrence among retained URI-URI non-rdf:type triples\",\n"
              << "  \"edge_weight\": \"ln(1 + retained frequency of predicate)\",\n"
              << "  \"parallel_edges\": \"retained\"\n"
              << "}\n";
        stats.close();
        AtomicRename(stats_temporary, stats_path);
        std::cout << "labels=" << label_rows << " graph_labels=" << labels_on_graph_vertices
                  << " groups=" << tokens.size() << " memberships=" << final_memberships << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "RDF conversion failed: " << error.what() << std::endl;
        return 1;
    }
}
