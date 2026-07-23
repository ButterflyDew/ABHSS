#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace
{

class FastInput
{
public:
    explicit FastInput(const fs::path& path)
    {
        file_ = std::fopen(path.string().c_str(), "rb");
        if (file_ == nullptr)
        {
            throw std::runtime_error("failed to open " + path.string());
        }
        buffer_.resize(1U << 20U);
    }

    ~FastInput()
    {
        if (file_ != nullptr)
        {
            std::fclose(file_);
        }
    }

    bool Read(std::uint64_t& value)
    {
        int c = NextNonSpace();
        if (c == EOF)
        {
            return false;
        }
        if (c < '0' || c > '9')
        {
            throw std::runtime_error("expected an unsigned integer");
        }
        value = 0;
        do
        {
            const auto digit = static_cast<std::uint64_t>(c - '0');
            if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U)
            {
                throw std::runtime_error("integer overflow in input");
            }
            value = value * 10U + digit;
            c = GetChar();
        } while (c >= '0' && c <= '9');
        if (c != EOF && c > ' ')
        {
            throw std::runtime_error("unexpected character after integer");
        }
        return true;
    }

private:
    int GetChar()
    {
        if (position_ == size_)
        {
            size_ = std::fread(buffer_.data(), 1, buffer_.size(), file_);
            position_ = 0;
            if (size_ == 0)
            {
                return EOF;
            }
        }
        return static_cast<unsigned char>(buffer_[position_++]);
    }

    int NextNonSpace()
    {
        int c = GetChar();
        while (c != EOF && c <= ' ')
        {
            c = GetChar();
        }
        return c;
    }

    std::FILE* file_ = nullptr;
    std::vector<char> buffer_;
    std::size_t position_ = 0;
    std::size_t size_ = 0;
};

class FastOutput
{
public:
    explicit FastOutput(const fs::path& path)
    {
        file_ = std::fopen(path.string().c_str(), "wb");
        if (file_ == nullptr)
        {
            throw std::runtime_error("failed to create " + path.string());
        }
        buffer_.resize(1U << 20U);
    }

    ~FastOutput()
    {
        Flush();
        if (file_ != nullptr)
        {
            std::fclose(file_);
        }
    }

    void UInt(std::uint64_t value)
    {
        char local[32];
        const auto result = std::to_chars(local, local + sizeof(local), value);
        if (result.ec != std::errc())
        {
            throw std::runtime_error("integer formatting failed");
        }
        Write(local, static_cast<std::size_t>(result.ptr - local));
    }

    void Char(char value)
    {
        Write(&value, 1);
    }

    void Flush()
    {
        if (position_ == 0 || file_ == nullptr)
        {
            return;
        }
        if (std::fwrite(buffer_.data(), 1, position_, file_) != position_)
        {
            throw std::runtime_error("write failed");
        }
        position_ = 0;
    }

private:
    void Write(const char* data, std::size_t length)
    {
        while (length != 0)
        {
            const std::size_t available = buffer_.size() - position_;
            if (available == 0)
            {
                Flush();
                continue;
            }
            const std::size_t amount = std::min(available, length);
            std::copy_n(data, amount, buffer_.data() + position_);
            data += amount;
            length -= amount;
            position_ += amount;
        }
    }

    std::FILE* file_ = nullptr;
    std::vector<char> buffer_;
    std::size_t position_ = 0;
};

struct Edge
{
    std::uint32_t u = 0;
    std::uint32_t v = 0;
};

std::uint64_t CommonNeighbors(const std::vector<std::uint32_t>& adjacency,
                              const std::vector<std::uint64_t>& offsets,
                              std::uint32_t u,
                              std::uint32_t v)
{
    std::uint64_t first = offsets[u];
    std::uint64_t first_end = offsets[u + 1U];
    std::uint64_t second = offsets[v];
    std::uint64_t second_end = offsets[v + 1U];
    std::uint64_t common = 0;
    while (first < first_end && second < second_end)
    {
        const std::uint32_t a = adjacency[static_cast<std::size_t>(first)];
        const std::uint32_t b = adjacency[static_cast<std::size_t>(second)];
        if (a < b)
        {
            ++first;
        }
        else if (b < a)
        {
            ++second;
        }
        else
        {
            ++common;
            ++first;
            ++second;
        }
    }
    return common;
}

std::size_t ParseThreads(const char* text)
{
    std::size_t value = 0;
    const std::string input(text);
    const auto result = std::from_chars(input.data(), input.data() + input.size(), value);
    if (result.ec != std::errc() || result.ptr != input.data() + input.size() || value == 0)
    {
        throw std::runtime_error("--threads requires a positive integer");
    }
    return value;
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 3)
        {
            throw std::runtime_error("usage: build_jaccard_graph <normalized-edges.txt> <graph.txt> [--threads N] [--force]");
        }
        const fs::path input_path = argv[1];
        const fs::path output_path = argv[2];
        std::size_t thread_count = std::max(1U, std::thread::hardware_concurrency());
        bool force = false;
        for (int index = 3; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--force")
            {
                force = true;
            }
            else if (argument == "--threads" && index + 1 < argc)
            {
                thread_count = ParseThreads(argv[++index]);
            }
            else
            {
                throw std::runtime_error("unknown or incomplete option: " + argument);
            }
        }
        if (fs::exists(output_path) && !force)
        {
            throw std::runtime_error("output already exists (use --force): " + output_path.string());
        }

        FastInput input(input_path);
        std::uint64_t n64 = 0;
        std::uint64_t m64 = 0;
        if (!input.Read(n64) || !input.Read(m64) || n64 == 0 || n64 > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::runtime_error("invalid normalized edge header");
        }
        if (m64 > std::numeric_limits<std::size_t>::max())
        {
            throw std::runtime_error("edge count exceeds addressable memory");
        }
        const auto n = static_cast<std::uint32_t>(n64);
        const auto m = static_cast<std::size_t>(m64);
        std::cout << "loading n=" << n << " m=" << m << std::endl;
        std::vector<Edge> edges(m);
        std::vector<std::uint64_t> degree(static_cast<std::size_t>(n) + 1U, 0);
        for (std::size_t index = 0; index < m; ++index)
        {
            std::uint64_t u64 = 0;
            std::uint64_t v64 = 0;
            if (!input.Read(u64) || !input.Read(v64) || u64 < 1 || u64 > n || v64 < 1 || v64 > n || u64 == v64)
            {
                throw std::runtime_error("invalid edge at zero-based row " + std::to_string(index));
            }
            const auto u = static_cast<std::uint32_t>(u64);
            const auto v = static_cast<std::uint32_t>(v64);
            edges[index] = {u, v};
            ++degree[u];
            ++degree[v];
        }
        std::uint64_t extra = 0;
        if (input.Read(extra))
        {
            throw std::runtime_error("normalized edge file has trailing integer tokens");
        }

        std::vector<std::uint64_t> offsets(static_cast<std::size_t>(n) + 2U, 0);
        for (std::uint32_t vertex = 1; vertex <= n; ++vertex)
        {
            offsets[vertex + 1U] = offsets[vertex] + degree[vertex];
        }
        if (offsets[n + 1U] != 2U * m64)
        {
            throw std::runtime_error("CSR degree sum mismatch");
        }
        std::vector<std::uint32_t> adjacency(static_cast<std::size_t>(2U * m64));
        std::vector<std::uint64_t> cursor = offsets;
        for (const Edge edge : edges)
        {
            adjacency[static_cast<std::size_t>(cursor[edge.u]++)] = edge.v;
            adjacency[static_cast<std::size_t>(cursor[edge.v]++)] = edge.u;
        }
        cursor.clear();
        cursor.shrink_to_fit();
        degree.clear();
        degree.shrink_to_fit();

        std::cout << "sorting adjacency with " << thread_count << " workers" << std::endl;
        std::atomic<std::uint32_t> next_vertex{1};
        std::atomic<std::uint32_t> duplicate_vertex{0};
        std::vector<std::thread> workers;
        workers.reserve(thread_count);
        for (std::size_t worker = 0; worker < thread_count; ++worker)
        {
            workers.emplace_back([&]()
            {
                while (true)
                {
                    const std::uint32_t begin_vertex = next_vertex.fetch_add(1024U);
                    if (begin_vertex > n)
                    {
                        return;
                    }
                    const std::uint32_t end_vertex = std::min<std::uint32_t>(n + 1U, begin_vertex + 1024U);
                    for (std::uint32_t vertex = begin_vertex; vertex < end_vertex; ++vertex)
                    {
                        auto begin = adjacency.begin() + static_cast<std::ptrdiff_t>(offsets[vertex]);
                        auto end = adjacency.begin() + static_cast<std::ptrdiff_t>(offsets[vertex + 1U]);
                        std::sort(begin, end);
                        if (std::adjacent_find(begin, end) != end)
                        {
                            std::uint32_t expected = 0;
                            duplicate_vertex.compare_exchange_strong(expected, vertex);
                        }
                    }
                }
            });
        }
        for (auto& worker : workers)
        {
            worker.join();
        }
        if (duplicate_vertex.load() != 0)
        {
            throw std::runtime_error("duplicate undirected edge detected at vertex " +
                                     std::to_string(duplicate_vertex.load()));
        }

        std::cout << "computing Jaccard weights" << std::endl;
        std::vector<unsigned char> weights(m, 0);
        std::atomic<std::size_t> next_edge{0};
        std::atomic<std::uint64_t> zero_weights{0};
        workers.clear();
        for (std::size_t worker = 0; worker < thread_count; ++worker)
        {
            workers.emplace_back([&]()
            {
                std::uint64_t local_zero = 0;
                while (true)
                {
                    const std::size_t begin = next_edge.fetch_add(16384U);
                    if (begin >= m)
                    {
                        break;
                    }
                    const std::size_t end = std::min(m, begin + 16384U);
                    for (std::size_t index = begin; index < end; ++index)
                    {
                        const Edge edge = edges[index];
                        const std::uint64_t common = CommonNeighbors(adjacency, offsets, edge.u, edge.v);
                        const std::uint64_t union_size = (offsets[edge.u + 1U] - offsets[edge.u]) +
                                                         (offsets[edge.v + 1U] - offsets[edge.v]) - common;
                        const auto weight = static_cast<unsigned char>((100U * (union_size - common)) / union_size);
                        weights[index] = weight;
                        local_zero += (weight == 0);
                    }
                }
                zero_weights.fetch_add(local_zero);
            });
        }
        for (auto& worker : workers)
        {
            worker.join();
        }

        fs::create_directories(output_path.parent_path());
        const fs::path temporary = output_path.string() + ".tmp";
        std::cout << "writing " << output_path << std::endl;
        {
            FastOutput output(temporary);
            output.UInt(n);
            output.Char(' ');
            output.UInt(m64);
            output.Char('\n');
            for (std::size_t index = 0; index < m; ++index)
            {
                output.UInt(edges[index].u);
                output.Char(' ');
                output.UInt(edges[index].v);
                output.Char(' ');
                output.UInt(weights[index]);
                output.Char('\n');
            }
            output.Flush();
        }
        std::error_code error;
        if (force)
        {
            fs::remove(output_path, error);
            error.clear();
        }
        fs::rename(temporary, output_path, error);
        if (error)
        {
            throw std::runtime_error("failed to install output: " + error.message());
        }
        std::cout << "complete zero_weight_edges=" << zero_weights.load() << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << std::endl;
        return 1;
    }
}
