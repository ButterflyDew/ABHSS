#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define BOOST_ALL_NO_LIB 1
#define BOOST_JSON_NO_LIB 1
#include <boost/json.hpp>
#include <boost/json/src.hpp>

namespace fs = std::filesystem;
namespace json = boost::json;

namespace
{

constexpr std::uint32_t kDefaultMinimumFrequency = 50;
constexpr std::uint32_t kDefaultMaximumFrequency = 4800;
constexpr std::uint64_t kProgressRows = 100000;

struct Statistics
{
    std::uint64_t paper_records = 0;
    std::uint64_t authors_with_source_id = 0;
    std::uint64_t authors_with_name_org_fallback = 0;
    std::uint64_t authors_missing_identity = 0;
    std::uint64_t duplicate_authors_within_paper = 0;
    std::uint64_t authorship_edges = 0;
    std::uint64_t citation_edges = 0;
    std::uint64_t references_outside_snapshot = 0;
    std::uint64_t duplicate_references_within_paper = 0;
};

class GraphWriter
{
public:
    explicit GraphWriter(const fs::path& path)
    {
        file_ = std::fopen(path.string().c_str(), "wb+");
        if (file_ == nullptr)
        {
            throw std::runtime_error("cannot open graph temporary file: " + path.string());
        }
        buffer_.resize(8U * 1024U * 1024U);
        std::setvbuf(file_, buffer_.data(), _IOFBF, buffer_.size());
        WriteHeader(0, 0);
    }

    GraphWriter(const GraphWriter&) = delete;
    GraphWriter& operator=(const GraphWriter&) = delete;

    ~GraphWriter()
    {
        if (file_ != nullptr)
        {
            std::fclose(file_);
        }
    }

    void WriteEdge(std::uint32_t left, std::uint32_t right)
    {
        char line[64];
        char* current = line;
        auto first = std::to_chars(current, line + sizeof(line), left);
        if (first.ec != std::errc())
        {
            throw std::runtime_error("failed formatting graph edge");
        }
        current = first.ptr;
        *current++ = ' ';
        auto second = std::to_chars(current, line + sizeof(line), right);
        if (second.ec != std::errc())
        {
            throw std::runtime_error("failed formatting graph edge");
        }
        current = second.ptr;
        *current++ = ' ';
        *current++ = '1';
        *current++ = '\n';
        if (std::fwrite(line, 1, static_cast<std::size_t>(current - line), file_) !=
            static_cast<std::size_t>(current - line))
        {
            throw std::runtime_error("failed writing graph edge");
        }
    }

    void Finalize(std::uint32_t vertices, std::uint64_t edges)
    {
        if (std::fflush(file_) != 0 || std::fseek(file_, 0, SEEK_SET) != 0)
        {
            throw std::runtime_error("failed seeking graph header");
        }
        WriteHeader(vertices, edges);
        if (std::fflush(file_) != 0)
        {
            throw std::runtime_error("failed flushing graph");
        }
    }

    void Close()
    {
        if (file_ != nullptr)
        {
            if (std::fclose(file_) != 0)
            {
                file_ = nullptr;
                throw std::runtime_error("failed closing graph");
            }
            file_ = nullptr;
        }
    }

private:
    void WriteHeader(std::uint32_t vertices, std::uint64_t edges)
    {
        char line[64];
        const int length = std::snprintf(
            line, sizeof(line), "%020llu %020llu\n",
            static_cast<unsigned long long>(vertices),
            static_cast<unsigned long long>(edges));
        if (length != 42 || std::fwrite(line, 1, static_cast<std::size_t>(length), file_) !=
                                static_cast<std::size_t>(length))
        {
            throw std::runtime_error("failed writing graph header");
        }
    }

    std::FILE* file_ = nullptr;
    std::vector<char> buffer_;
};

std::unordered_set<std::string> LoadStopwords(const fs::path& path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("cannot open stopword file: " + path.string());
    }
    std::unordered_set<std::string> result;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line[0] != '#')
        {
            result.insert(line);
        }
    }
    return result;
}

std::string_view StringField(const json::object& object, std::string_view name)
{
    const json::value* value = object.if_contains(name);
    if (value == nullptr || !value->is_string())
    {
        return {};
    }
    return value->as_string();
}

std::string NormalizeIdentity(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    bool pending_space = false;
    for (unsigned char byte : value)
    {
        if (byte <= 0x20)
        {
            pending_space = !result.empty();
            continue;
        }
        if (pending_space)
        {
            result.push_back(' ');
            pending_space = false;
        }
        if (byte >= 'A' && byte <= 'Z')
        {
            byte = static_cast<unsigned char>(byte - 'A' + 'a');
        }
        result.push_back(static_cast<char>(byte));
    }
    return result;
}

void AddTokens(std::string_view value,
               const std::unordered_set<std::string>& stopwords,
               std::vector<std::string>& output)
{
    std::string token;
    for (std::size_t index = 0; index <= value.size(); ++index)
    {
        const unsigned char byte = index < value.size() ?
            static_cast<unsigned char>(value[index]) : 0;
        const bool alphanumeric =
            (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9');
        if (alphanumeric)
        {
            token.push_back(static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte - 'A' + 'a' : byte));
        }
        else if (!token.empty())
        {
            if (token.size() >= 2 && stopwords.find(token) == stopwords.end())
            {
                output.push_back(std::move(token));
                token.clear();
            }
            else
            {
                token.clear();
            }
        }
    }
}

void Deduplicate(std::vector<std::string>& tokens)
{
    std::sort(tokens.begin(), tokens.end());
    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
}

std::vector<std::string> PaperTokens(
    const json::object& record,
    const std::unordered_set<std::string>& stopwords)
{
    std::vector<std::string> tokens;
    AddTokens(StringField(record, "title"), stopwords, tokens);
    AddTokens(StringField(record, "venue"), stopwords, tokens);
    AddTokens(StringField(record, "doc_type"), stopwords, tokens);
    const json::value* keywords = record.if_contains("keywords");
    if (keywords != nullptr && keywords->is_array())
    {
        for (const json::value& keyword : keywords->as_array())
        {
            if (keyword.is_string())
            {
                AddTokens(keyword.as_string(), stopwords, tokens);
            }
        }
    }
    Deduplicate(tokens);
    return tokens;
}

std::vector<std::string> AuthorTokens(
    const json::object& author,
    const std::unordered_set<std::string>& stopwords)
{
    std::vector<std::string> tokens;
    AddTokens(StringField(author, "name"), stopwords, tokens);
    Deduplicate(tokens);
    return tokens;
}

std::pair<std::string, int> AuthorKey(const json::object& author)
{
    const std::string_view source_id = StringField(author, "id");
    if (!source_id.empty())
    {
        return {"id:" + std::string(source_id), 1};
    }
    const std::string name = NormalizeIdentity(StringField(author, "name"));
    const std::string organization = NormalizeIdentity(StringField(author, "org"));
    if (name.empty() && organization.empty())
    {
        return {{}, 0};
    }
    return {"fallback:" + name + '\x1f' + organization, 2};
}

void CountTokens(const std::vector<std::string>& tokens,
                 std::unordered_map<std::string, std::uint32_t>& counts,
                 std::uint32_t cap)
{
    for (const std::string& token : tokens)
    {
        auto [iterator, inserted] = counts.try_emplace(token, 1);
        if (!inserted && iterator->second < cap)
        {
            ++iterator->second;
        }
    }
}

json::object ParseRecord(const std::string& line, std::uint64_t line_number)
{
    json::error_code error;
    json::value value = json::parse(line, error);
    if (error || !value.is_object())
    {
        throw std::runtime_error("invalid JSON object at row " + std::to_string(line_number));
    }
    return std::move(value.as_object());
}

void ConfigureInputBuffer(std::ifstream& input, std::vector<char>& buffer)
{
    buffer.resize(8U * 1024U * 1024U);
    input.rdbuf()->pubsetbuf(buffer.data(), static_cast<std::streamsize>(buffer.size()));
}

void WriteStatistics(const fs::path& path,
                     const Statistics& statistics,
                     std::uint32_t vertices,
                     std::uint64_t paper_vertices,
                     std::uint64_t author_vertices,
                     std::size_t candidate_groups,
                     std::uint64_t memberships)
{
    const fs::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary);
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"dataset\": \"DBLP-AMiner-V18\",\n"
           << "  \"vertices\": " << vertices << ",\n"
           << "  \"paper_vertices\": " << paper_vertices << ",\n"
           << "  \"author_vertices\": " << author_vertices << ",\n"
           << "  \"edges\": " << statistics.authorship_edges + statistics.citation_edges << ",\n"
           << "  \"authorship_edges\": " << statistics.authorship_edges << ",\n"
           << "  \"citation_edges\": " << statistics.citation_edges << ",\n"
           << "  \"candidate_groups\": " << candidate_groups << ",\n"
           << "  \"candidate_group_memberships\": " << memberships << ",\n"
           << "  \"authors_with_source_id\": " << statistics.authors_with_source_id << ",\n"
           << "  \"authors_with_name_org_fallback\": " << statistics.authors_with_name_org_fallback << ",\n"
           << "  \"authors_missing_identity\": " << statistics.authors_missing_identity << ",\n"
           << "  \"duplicate_authors_within_paper\": " << statistics.duplicate_authors_within_paper << ",\n"
           << "  \"references_outside_snapshot\": " << statistics.references_outside_snapshot << ",\n"
           << "  \"duplicate_references_within_paper\": " << statistics.duplicate_references_within_paper << "\n"
           << "}\n";
    output.close();
    std::error_code error;
    fs::remove(path, error);
    error.clear();
    fs::rename(temporary, path, error);
    if (error)
    {
        throw std::runtime_error("cannot replace " + path.string() + ": " + error.message());
    }
}

void ReplaceFile(const fs::path& temporary, const fs::path& destination)
{
    std::error_code error;
    fs::remove(destination, error);
    error.clear();
    fs::rename(temporary, destination, error);
    if (error)
    {
        throw std::runtime_error("cannot replace " + destination.string() + ": " + error.message());
    }
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 4 || argc > 6)
        {
            std::cerr << "Usage: build_aminer_dblp_graph <DBLP-V18.jsonl> <output-dir> "
                         "<stopwords.txt> [min-frequency] [max-frequency]\n";
            return 2;
        }
        const fs::path jsonl = argv[1];
        const fs::path output_directory = argv[2];
        const fs::path stopword_path = argv[3];
        const std::uint32_t minimum_frequency = argc >= 5 ?
            static_cast<std::uint32_t>(std::stoul(argv[4])) : kDefaultMinimumFrequency;
        const std::uint32_t maximum_frequency = argc >= 6 ?
            static_cast<std::uint32_t>(std::stoul(argv[5])) : kDefaultMaximumFrequency;
        if (minimum_frequency == 0 || minimum_frequency > maximum_frequency)
        {
            throw std::runtime_error("invalid frequency bounds");
        }
        fs::create_directories(output_directory);
        const auto stopwords = LoadStopwords(stopword_path);
        std::unordered_map<std::string, std::uint32_t> paper_ids;
        std::unordered_map<std::string, std::uint32_t> author_ids;
        std::unordered_map<std::string, std::uint32_t> label_counts;
        paper_ids.reserve(10000000);
        author_ids.reserve(8000000);
        label_counts.reserve(2000000);
        std::uint32_t vertex_count = 0;
        Statistics statistics;
        const fs::path graph_path = output_directory / "graph.txt";
        const fs::path graph_temporary = output_directory / "graph.txt.tmp";
        GraphWriter graph(graph_temporary);

        std::ifstream input(jsonl, std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("cannot open JSONL: " + jsonl.string());
        }
        std::vector<char> input_buffer;
        ConfigureInputBuffer(input, input_buffer);
        std::string line;
        while (std::getline(input, line))
        {
            ++statistics.paper_records;
            json::object record = ParseRecord(line, statistics.paper_records);
            const std::string_view source_paper_id = StringField(record, "id");
            if (source_paper_id.empty())
            {
                throw std::runtime_error("paper has no id at row " + std::to_string(statistics.paper_records));
            }
            if (vertex_count == std::numeric_limits<std::uint32_t>::max())
            {
                throw std::runtime_error("vertex count exceeds uint32");
            }
            const std::uint32_t paper_vertex = ++vertex_count;
            if (!paper_ids.emplace(std::string(source_paper_id), paper_vertex).second)
            {
                throw std::runtime_error("duplicate paper id at row " + std::to_string(statistics.paper_records));
            }
            CountTokens(PaperTokens(record, stopwords), label_counts, maximum_frequency + 1);

            const json::value* authors_value = record.if_contains("authors");
            std::vector<std::uint32_t> seen_authors;
            if (authors_value != nullptr && authors_value->is_array())
            {
                for (const json::value& value : authors_value->as_array())
                {
                    if (!value.is_object())
                    {
                        ++statistics.authors_missing_identity;
                        continue;
                    }
                    const json::object& author = value.as_object();
                    auto [key, identity_kind] = AuthorKey(author);
                    if (identity_kind == 0)
                    {
                        ++statistics.authors_missing_identity;
                        continue;
                    }
                    auto iterator = author_ids.find(key);
                    std::uint32_t author_vertex = 0;
                    if (iterator == author_ids.end())
                    {
                        if (vertex_count == std::numeric_limits<std::uint32_t>::max())
                        {
                            throw std::runtime_error("vertex count exceeds uint32");
                        }
                        author_vertex = ++vertex_count;
                        author_ids.emplace(std::move(key), author_vertex);
                        if (identity_kind == 1)
                        {
                            ++statistics.authors_with_source_id;
                        }
                        else
                        {
                            ++statistics.authors_with_name_org_fallback;
                        }
                        CountTokens(AuthorTokens(author, stopwords), label_counts, maximum_frequency + 1);
                    }
                    else
                    {
                        author_vertex = iterator->second;
                    }
                    if (std::find(seen_authors.begin(), seen_authors.end(), author_vertex) != seen_authors.end())
                    {
                        ++statistics.duplicate_authors_within_paper;
                        continue;
                    }
                    seen_authors.push_back(author_vertex);
                    graph.WriteEdge(std::min(paper_vertex, author_vertex), std::max(paper_vertex, author_vertex));
                    ++statistics.authorship_edges;
                }
            }
            if (statistics.paper_records % kProgressRows == 0)
            {
                std::cout << "[pass1] rows=" << statistics.paper_records
                          << " vertices=" << vertex_count
                          << " authorship_edges=" << statistics.authorship_edges << std::endl;
            }
        }
        input.close();

        std::vector<std::string> group_tokens;
        group_tokens.reserve(label_counts.size());
        for (const auto& [token, frequency] : label_counts)
        {
            if (frequency >= minimum_frequency && frequency <= maximum_frequency)
            {
                group_tokens.push_back(token);
            }
        }
        std::sort(group_tokens.begin(), group_tokens.end());
        std::vector<std::uint32_t> group_frequencies;
        group_frequencies.reserve(group_tokens.size());
        for (const std::string& token : group_tokens)
        {
            group_frequencies.push_back(label_counts.at(token));
        }
        std::unordered_map<std::string, std::uint32_t> token_to_group;
        token_to_group.reserve(group_tokens.size() * 2);
        for (std::uint32_t index = 0; index < group_tokens.size(); ++index)
        {
            token_to_group.emplace(group_tokens[index], index);
        }
        label_counts.clear();
        label_counts.rehash(0);
        std::vector<std::vector<std::uint32_t>> memberships(group_tokens.size());
        for (std::uint32_t index = 0; index < group_tokens.size(); ++index)
        {
            memberships[index].reserve(group_frequencies[index]);
        }
        std::vector<std::uint8_t> seen_author_labels(static_cast<std::size_t>(vertex_count) + 1, 0);
        std::cout << "[pass1] done papers=" << paper_ids.size()
                  << " authors=" << author_ids.size()
                  << " vertices=" << vertex_count
                  << " eligible_labels=" << group_tokens.size() << std::endl;

        std::ifstream input2(jsonl, std::ios::binary);
        if (!input2)
        {
            throw std::runtime_error("cannot reopen JSONL: " + jsonl.string());
        }
        std::vector<char> input_buffer2;
        ConfigureInputBuffer(input2, input_buffer2);
        std::uint64_t pass2_rows = 0;
        while (std::getline(input2, line))
        {
            ++pass2_rows;
            json::object record = ParseRecord(line, pass2_rows);
            const auto paper_iterator = paper_ids.find(std::string(StringField(record, "id")));
            if (paper_iterator == paper_ids.end())
            {
                throw std::runtime_error("pass2 paper id missing at row " + std::to_string(pass2_rows));
            }
            const std::uint32_t paper_vertex = paper_iterator->second;
            for (const std::string& token : PaperTokens(record, stopwords))
            {
                const auto group = token_to_group.find(token);
                if (group != token_to_group.end())
                {
                    memberships[group->second].push_back(paper_vertex);
                }
            }

            const json::value* authors_value = record.if_contains("authors");
            if (authors_value != nullptr && authors_value->is_array())
            {
                for (const json::value& value : authors_value->as_array())
                {
                    if (!value.is_object())
                    {
                        continue;
                    }
                    const json::object& author = value.as_object();
                    auto [key, identity_kind] = AuthorKey(author);
                    if (identity_kind == 0)
                    {
                        continue;
                    }
                    const auto author_iterator = author_ids.find(key);
                    if (author_iterator == author_ids.end())
                    {
                        throw std::runtime_error("pass2 author identity missing");
                    }
                    const std::uint32_t author_vertex = author_iterator->second;
                    if (seen_author_labels[author_vertex])
                    {
                        continue;
                    }
                    seen_author_labels[author_vertex] = 1;
                    for (const std::string& token : AuthorTokens(author, stopwords))
                    {
                        const auto group = token_to_group.find(token);
                        if (group != token_to_group.end())
                        {
                            memberships[group->second].push_back(author_vertex);
                        }
                    }
                }
            }

            const json::value* references_value = record.if_contains("references");
            if (references_value != nullptr && references_value->is_array())
            {
                std::vector<std::string_view> references;
                references.reserve(references_value->as_array().size());
                for (const json::value& value : references_value->as_array())
                {
                    if (value.is_string() && value.as_string() != StringField(record, "id"))
                    {
                        references.push_back(value.as_string());
                    }
                }
                std::sort(references.begin(), references.end());
                const auto unique_end = std::unique(references.begin(), references.end());
                statistics.duplicate_references_within_paper +=
                    static_cast<std::uint64_t>(references.end() - unique_end);
                references.erase(unique_end, references.end());
                for (std::string_view reference : references)
                {
                    const auto target = paper_ids.find(std::string(reference));
                    if (target == paper_ids.end())
                    {
                        ++statistics.references_outside_snapshot;
                        continue;
                    }
                    graph.WriteEdge(std::min(paper_vertex, target->second),
                                    std::max(paper_vertex, target->second));
                    ++statistics.citation_edges;
                }
            }
            if (pass2_rows % kProgressRows == 0)
            {
                std::cout << "[pass2] rows=" << pass2_rows
                          << " citation_edges=" << statistics.citation_edges << std::endl;
            }
        }
        input2.close();
        graph.Finalize(vertex_count, statistics.authorship_edges + statistics.citation_edges);
        graph.Close();
        ReplaceFile(graph_temporary, graph_path);

        const fs::path candidate_path = output_directory / "candidate_groups.txt";
        const fs::path candidate_temporary = output_directory / "candidate_groups.txt.tmp";
        const fs::path metadata_path = output_directory / "source_group_ids.tsv";
        const fs::path metadata_temporary = output_directory / "source_group_ids.tsv.tmp";
        std::ofstream candidates(candidate_temporary, std::ios::binary);
        std::ofstream metadata(metadata_temporary, std::ios::binary);
        if (!candidates || !metadata)
        {
            throw std::runtime_error("cannot open candidate-group outputs");
        }
        std::vector<char> candidate_buffer(8U * 1024U * 1024U);
        candidates.rdbuf()->pubsetbuf(candidate_buffer.data(),
                                      static_cast<std::streamsize>(candidate_buffer.size()));
        metadata << "group_id\ttoken\tsize\n";
        std::uint64_t membership_count = 0;
        for (std::size_t index = 0; index < group_tokens.size(); ++index)
        {
            const auto& members = memberships[index];
            if (members.size() != group_frequencies[index])
            {
                throw std::runtime_error("candidate-group frequency mismatch for token " +
                                         group_tokens[index]);
            }
            candidates << 'g' << index + 1 << ':';
            for (std::uint32_t vertex : members)
            {
                candidates << ' ' << vertex;
            }
            candidates << '\n';
            metadata << index + 1 << '\t' << group_tokens[index] << '\t' << members.size() << '\n';
            membership_count += members.size();
        }
        candidates.close();
        metadata.close();
        if (!candidates || !metadata)
        {
            throw std::runtime_error("failed writing candidate-group outputs");
        }
        ReplaceFile(candidate_temporary, candidate_path);
        ReplaceFile(metadata_temporary, metadata_path);
        WriteStatistics(output_directory / "build_stats.json", statistics, vertex_count,
                        paper_ids.size(), author_ids.size(), group_tokens.size(), membership_count);
        std::cout << "[done] vertices=" << vertex_count
                  << " edges=" << statistics.authorship_edges + statistics.citation_edges
                  << " groups=" << group_tokens.size()
                  << " memberships=" << membership_count << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "AMiner DBLP conversion failed: " << error.what() << '\n';
        return 1;
    }
}
