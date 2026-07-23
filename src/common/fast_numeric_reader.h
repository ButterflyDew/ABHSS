#ifndef GST_FAST_NUMERIC_READER_H
#define GST_FAST_NUMERIC_READER_H

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>
#include <vector>

namespace gst::io
{

/**
 * @brief 面向纯数字大文件的 8 MiB 缓冲 ASCII token 读取器。
 *
 * `operator>>` 会为每个字段重复执行 locale、sentry 与格式状态检查，在
 * Orkut 的 1.17 亿条边上开销很大。本类只负责无分配词法读取；顶点范围、
 * 非负边权、声明条数等语义仍由调用者验证。求解器和离线可行性审计共用
 * 同一实现，防止正式加载已经加速而 freeze 工具仍停留在慢路径。
 */
class FastNumericReader
{
public:
    /** @brief 以二进制模式打开文件并分配一次可复用缓冲区。 */
    explicit FastNumericReader(const std::filesystem::path& path)
        : input_(path, std::ios::binary), buffer_(8U << 20)
    {
    }

    /** @brief 返回文件是否成功打开；不提前触发读取。 */
    bool IsOpen() const
    {
        return input_.is_open();
    }

    /** @brief 读取十进制 `int`；非法 token 或溢出时返回 false。 */
    bool ReadInt(int& result)
    {
        char ch = 0;
        if (!SkipWhitespace(ch))
            return false;

        bool negative = false;
        if (ch == '-' || ch == '+')
        {
            negative = ch == '-';
            Consume();
            if (!Peek(ch))
                return false;
        }
        if (ch < '0' || ch > '9')
            return false;

        const long long limit = negative
                                    ? -static_cast<long long>(
                                          std::numeric_limits<int>::min())
                                    : std::numeric_limits<int>::max();
        long long value = 0;
        do
        {
            const int digit = ch - '0';
            if (value > (limit - digit) / 10)
                return false;
            value = value * 10 + digit;
            Consume();
        } while (Peek(ch) && ch >= '0' && ch <= '9');

        if (Peek(ch) && !IsWhitespace(ch))
            return false;
        result = static_cast<int>(negative ? -value : value);
        return true;
    }

    /** @brief 读取十进制 `uint64_t`；负号、非法 token 或溢出均被拒绝。 */
    bool ReadUInt64(std::uint64_t& result)
    {
        char ch = 0;
        if (!SkipWhitespace(ch))
            return false;
        if (ch == '+')
        {
            Consume();
            if (!Peek(ch))
                return false;
        }
        if (ch < '0' || ch > '9')
            return false;

        std::uint64_t value = 0;
        constexpr std::uint64_t limit =
            std::numeric_limits<std::uint64_t>::max();
        do
        {
            const unsigned digit = static_cast<unsigned>(ch - '0');
            if (value > (limit - digit) / 10)
                return false;
            value = value * 10 + digit;
            Consume();
        } while (Peek(ch) && ch >= '0' && ch <= '9');

        if (Peek(ch) && !IsWhitespace(ch))
            return false;
        result = value;
        return true;
    }

    /**
     * @brief 读取有限 `double`，保持标准库十进制到二进制的舍入语义。
     *
     * 现代工具链走无分配 `from_chars`；GCC 9/10 的 libstdc++ 缺失该浮点
     * 实现时由 CMake 探针选择 `strtod`。两条路径都要求 token 被完整消费。
     */
    bool ReadDouble(double& result)
    {
        char ch = 0;
        if (!SkipWhitespace(ch))
            return false;

        char token[129];
        std::size_t length = 0;
        do
        {
            if (length + 1 == sizeof(token))
                return false;
            token[length++] = ch;
            Consume();
        } while (Peek(ch) && !IsWhitespace(ch));

        const char* first = token;
        if (length && token[0] == '+')
            ++first;
        const char* last = token + length;
        bool has_digit = false;
        for (const char* cursor = first; cursor != last; ++cursor)
        {
            has_digit = has_digit || (*cursor >= '0' && *cursor <= '9');
            if ((*cursor < '0' || *cursor > '9') && *cursor != '+' &&
                *cursor != '-' && *cursor != '.' && *cursor != 'e' &&
                *cursor != 'E')
                return false;
        }
        if (!has_digit)
            return false;
#if defined(GST_HAVE_FLOAT_FROM_CHARS)
        const auto parsed =
            std::from_chars(first, last, result, std::chars_format::general);
        return parsed.ec == std::errc{} && parsed.ptr == last &&
               std::isfinite(result);
#else
        // C/C++ 程序在未调用 setlocale 时使用 C 数值 locale，因此冻结接口
        // 仍只接受点号小数；这里不改变进程全局 locale。
        token[length] = '\0';
        char* parsed_end = nullptr;
        result = std::strtod(first, &parsed_end);
        return parsed_end == last && std::isfinite(result);
#endif
    }

    /** @brief 跳过尾部 ASCII 空白后判断是否真正 EOF。 */
    bool AtEnd()
    {
        char ch = 0;
        return !SkipWhitespace(ch);
    }

private:
    /** @brief 判断本仓库数字接口允许的 ASCII token 分隔符。 */
    static bool IsWhitespace(char ch)
    {
        return static_cast<unsigned char>(ch) <=
               static_cast<unsigned char>(' ');
    }

    /** @brief 缓冲耗尽时批量补充数据，并返回是否读到至少一个字节。 */
    bool Refill()
    {
        input_.read(buffer_.data(),
                    static_cast<std::streamsize>(buffer_.size()));
        limit_ = static_cast<std::size_t>(input_.gcount());
        position_ = 0;
        return limit_ != 0;
    }

    /** @brief 查看但不消费下一个字节；跨缓冲边界时自动 refill。 */
    bool Peek(char& ch)
    {
        if (position_ == limit_ && !Refill())
            return false;
        ch = buffer_[position_];
        return true;
    }

    /** @brief 消费一个已经由 `Peek` 确认存在的字节。 */
    void Consume()
    {
        ++position_;
    }

    /** @brief 跳过任意 ASCII 空白并返回首个 token 字节。 */
    bool SkipWhitespace(char& ch)
    {
        while (Peek(ch))
        {
            if (!IsWhitespace(ch))
                return true;
            Consume();
        }
        return false;
    }

    std::ifstream input_;
    std::vector<char> buffer_;
    std::size_t position_ = 0;
    std::size_t limit_ = 0;
};

}  // namespace gst::io

#endif  // GST_FAST_NUMERIC_READER_H
