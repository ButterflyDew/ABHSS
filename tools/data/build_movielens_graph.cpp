#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::uint64_t ParseUnsigned(const char* begin, const char* end, const char* field)
{
    std::uint64_t value = 0;
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end)
    {
        throw std::runtime_error(std::string("invalid ") + field);
    }
    return value;
}

std::vector<std::string> Split(const std::string& text, char separator)
{
    std::vector<std::string> values;
    std::size_t begin = 0;
    while (begin <= text.size())
    {
        const std::size_t end = text.find(separator, begin);
        values.push_back(text.substr(begin, end == std::string::npos ? text.size() - begin : end - begin));
        if (end == std::string::npos)
        {
            break;
        }
        begin = end + 1;
    }
    return values;
}

struct Rating
{
    std::uint32_t user = 0;
    std::uint32_t movie = 0;
    bool five_star = false;
};

Rating ParseRating(const std::string& line,
                   const std::unordered_map<std::uint32_t, std::uint32_t>& movie_to_dense)
{
    const std::size_t first = line.find(',');
    const std::size_t second = first == std::string::npos ? first : line.find(',', first + 1);
    const std::size_t third = second == std::string::npos ? second : line.find(',', second + 1);
    if (first == std::string::npos || second == std::string::npos || third == std::string::npos)
    {
        throw std::runtime_error("malformed ratings.csv row");
    }
    const auto user64 = ParseUnsigned(line.data(), line.data() + first, "userId");
    const auto movie64 = ParseUnsigned(line.data() + first + 1, line.data() + second, "movieId");
    if (user64 > std::numeric_limits<std::uint32_t>::max() || movie64 > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error("MovieLens identifier exceeds uint32");
    }
    const auto found = movie_to_dense.find(static_cast<std::uint32_t>(movie64));
    if (found == movie_to_dense.end())
    {
        throw std::runtime_error("ratings.csv references a movie absent from movies.csv");
    }
    const std::string rating = line.substr(second + 1, third - second - 1);
    return {static_cast<std::uint32_t>(user64), found->second, rating == "5.0"};
}

template <class Callback>
std::uint64_t ScanFiveStarUsers(const fs::path& ratings_path,
                                const std::unordered_map<std::uint32_t, std::uint32_t>& movie_to_dense,
                                Callback callback)
{
    std::ifstream input(ratings_path);
    if (!input)
    {
        throw std::runtime_error("failed to open " + ratings_path.string());
    }
    std::string line;
    if (!std::getline(input, line) || line != "userId,movieId,rating,timestamp")
    {
        throw std::runtime_error("unexpected ratings.csv header");
    }
    std::uint32_t current_user = 0;
    std::vector<std::uint32_t> five_star_movies;
    std::uint64_t rows = 0;
    while (std::getline(input, line))
    {
        if (line.empty())
        {
            continue;
        }
        const Rating rating = ParseRating(line, movie_to_dense);
        if (current_user != 0 && rating.user != current_user)
        {
            callback(current_user, five_star_movies);
            five_star_movies.clear();
        }
        if (current_user != 0 && rating.user < current_user)
        {
            throw std::runtime_error("ratings.csv is not ordered by userId");
        }
        current_user = rating.user;
        if (rating.five_star)
        {
            five_star_movies.push_back(rating.movie);
        }
        ++rows;
    }
    if (current_user != 0)
    {
        callback(current_user, five_star_movies);
    }
    return rows;
}

std::uint64_t EncodePair(std::uint32_t a, std::uint32_t b)
{
    if (a > b)
    {
        std::swap(a, b);
    }
    return (static_cast<std::uint64_t>(a) << 32U) | b;
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 5)
        {
            throw std::runtime_error(
                "usage: build_movielens_graph <ratings.csv> <movies.csv> <graph.txt> <candidate_groups.txt>");
        }
        const fs::path ratings_path = argv[1];
        const fs::path movies_path = argv[2];
        const fs::path graph_path = argv[3];
        const fs::path groups_path = argv[4];
        if (fs::exists(graph_path) || fs::exists(groups_path))
        {
            throw std::runtime_error("output exists; refusing to overwrite a frozen interface file");
        }

        std::ifstream movies_input(movies_path);
        if (!movies_input)
        {
            throw std::runtime_error("failed to open " + movies_path.string());
        }
        std::string line;
        if (!std::getline(movies_input, line) || line != "movieId,title,genres")
        {
            throw std::runtime_error("unexpected movies.csv header");
        }
        std::unordered_map<std::uint32_t, std::uint32_t> movie_to_dense;
        std::vector<std::uint32_t> dense_to_movie(1, 0);
        std::map<std::string, std::vector<std::uint32_t>> genre_groups;
        while (std::getline(movies_input, line))
        {
            if (line.empty())
            {
                continue;
            }
            const std::size_t first = line.find(',');
            const std::size_t last = line.rfind(',');
            if (first == std::string::npos || last == first)
            {
                throw std::runtime_error("malformed movies.csv row");
            }
            const auto movie64 = ParseUnsigned(line.data(), line.data() + first, "movieId");
            if (movie64 > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::runtime_error("movieId exceeds uint32");
            }
            const auto source_movie = static_cast<std::uint32_t>(movie64);
            const auto dense = static_cast<std::uint32_t>(dense_to_movie.size());
            if (!movie_to_dense.emplace(source_movie, dense).second)
            {
                throw std::runtime_error("duplicate movieId");
            }
            dense_to_movie.push_back(source_movie);
            const std::string genres = line.substr(last + 1);
            for (const std::string& genre : Split(genres, '|'))
            {
                genre_groups[genre].push_back(dense);
            }
        }
        const std::uint32_t n = static_cast<std::uint32_t>(dense_to_movie.size() - 1U);
        std::cout << "movies=" << n << " genres=" << genre_groups.size() << std::endl;

        std::uint64_t generated_pairs = 0;
        std::uint64_t five_star_ratings = 0;
        const std::uint64_t rating_rows = ScanFiveStarUsers(
            ratings_path, movie_to_dense,
            [&](std::uint32_t, std::vector<std::uint32_t>& movies)
            {
                std::sort(movies.begin(), movies.end());
                movies.erase(std::unique(movies.begin(), movies.end()), movies.end());
                five_star_ratings += movies.size();
                const std::uint64_t k = movies.size();
                generated_pairs += k * (k - 1U) / 2U;
            });
        std::cout << "rating_rows=" << rating_rows << " five_star_ratings=" << five_star_ratings
                  << " generated_pairs=" << generated_pairs << std::endl;
        if (generated_pairs > std::numeric_limits<std::size_t>::max())
        {
            throw std::runtime_error("generated pair list exceeds addressable memory");
        }
        std::vector<std::uint64_t> pairs;
        pairs.reserve(static_cast<std::size_t>(generated_pairs));
        const std::uint64_t second_rows = ScanFiveStarUsers(
            ratings_path, movie_to_dense,
            [&](std::uint32_t, std::vector<std::uint32_t>& movies)
            {
                std::sort(movies.begin(), movies.end());
                movies.erase(std::unique(movies.begin(), movies.end()), movies.end());
                for (std::size_t i = 0; i < movies.size(); ++i)
                {
                    for (std::size_t j = i + 1; j < movies.size(); ++j)
                    {
                        pairs.push_back(EncodePair(movies[i], movies[j]));
                    }
                }
            });
        if (second_rows != rating_rows || pairs.size() != generated_pairs)
        {
            throw std::runtime_error("MovieLens two-pass scan is inconsistent");
        }
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
        std::cout << "unique_edges=" << pairs.size() << std::endl;

        fs::create_directories(graph_path.parent_path());
        const fs::path graph_temporary = graph_path.string() + ".tmp";
        {
            std::ofstream output(graph_temporary, std::ios::binary);
            output.rdbuf()->pubsetbuf(nullptr, 1U << 20U);
            output << n << ' ' << pairs.size() << '\n';
            for (const std::uint64_t pair : pairs)
            {
                output << (pair >> 32U) << ' ' << static_cast<std::uint32_t>(pair) << " 1\n";
            }
            if (!output)
            {
                throw std::runtime_error("failed while writing graph.txt");
            }
        }
        fs::rename(graph_temporary, graph_path);

        const fs::path groups_temporary = groups_path.string() + ".tmp";
        const fs::path group_map_path = groups_path.parent_path() / "source_group_ids.tsv";
        const fs::path group_map_temporary = group_map_path.string() + ".tmp";
        {
            std::ofstream groups(groups_temporary);
            std::ofstream group_map(group_map_temporary);
            group_map << "group_id\tgenre\tsize\n";
            std::uint32_t group_id = 0;
            for (const auto& entry : genre_groups)
            {
                ++group_id;
                groups << 'g' << group_id << ':';
                for (const std::uint32_t vertex : entry.second)
                {
                    groups << ' ' << vertex;
                }
                groups << '\n';
                group_map << group_id << '\t' << entry.first << '\t' << entry.second.size() << '\n';
            }
        }
        fs::rename(groups_temporary, groups_path);
        fs::rename(group_map_temporary, group_map_path);

        const fs::path vertex_map_path = graph_path.parent_path() / "source_vertex_ids.tsv";
        const fs::path vertex_map_temporary = vertex_map_path.string() + ".tmp";
        {
            std::ofstream output(vertex_map_temporary);
            output << "vertex_id\tmovieId\n";
            for (std::uint32_t vertex = 1; vertex <= n; ++vertex)
            {
                output << vertex << '\t' << dense_to_movie[vertex] << '\n';
            }
        }
        fs::rename(vertex_map_temporary, vertex_map_path);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MovieLens conversion failed: " << error.what() << std::endl;
        return 1;
    }
}
