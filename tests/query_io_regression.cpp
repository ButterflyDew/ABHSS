#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/common/query_io.h"

namespace
{
/** @brief 断言格式错误的查询文件在分配求解状态前即被读取器拒绝。 */
void ExpectRejected(const std::string& path, const std::string& label)
{
    try
    {
        (void)gst::LoadQueriesFromFolder(".", path);
    }
    catch (const std::runtime_error&)
    {
        return;
    }
    throw std::runtime_error(label + " query fixture was unexpectedly accepted");
}
}  // namespace

/** @brief 覆盖合法批量读取以及计数、截断和尾随 token 输入错误。 */
int main()
{
    const std::string root =
        std::string(GST_TEST_SOURCE_DIR) + "/tests/fixtures/query_io";
    const std::vector<gst::Query> valid =
        gst::LoadQueriesFromFolder(".", root + "/valid_multi.txt");
    if (valid.size() != 2 || valid[0].groups.size() != 2 ||
        valid[0].groups[0] != std::vector<int>({1, 3}) ||
        valid[0].groups[1] != std::vector<int>({2}) ||
        !valid[1].groups.empty())
        throw std::runtime_error("valid multi-query fixture was parsed incorrectly");

    ExpectRejected(root + "/negative_query_count.txt", "negative-query-count");
    ExpectRejected(root + "/negative_group_count.txt", "negative-group-count");
    ExpectRejected(root + "/empty_group.txt", "empty-group");
    ExpectRejected(root + "/truncated_query.txt", "truncated-query");
    ExpectRejected(root + "/trailing_token.txt", "trailing-token");
    std::cout << "query reader preserved valid batches and rejected malformed inputs\n";
    return 0;
}
