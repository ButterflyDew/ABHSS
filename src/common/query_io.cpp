#include "query_io.h"

#include <stdexcept>

#include "fast_numeric_reader.h"

namespace gst
{

/** @brief 按仓库查询格式顺序读入 q 条查询；正式输入默认已经满足格式约定。 */
std::vector<Query> LoadQueries(const std::string& query_file)
{
    io::FastNumericReader input(query_file);
    int query_count = 0;
    if (!input.IsOpen() || !input.ReadInt(query_count))
        throw std::runtime_error("Cannot read " + query_file);

    std::vector<Query> queries(query_count);
    for (Query& query : queries)
    {
        int group_count = 0;
        input.ReadInt(group_count);
        query.groups.resize(group_count);
        for (auto& group : query.groups)
        {
            int group_size = 0;
            input.ReadInt(group_size);
            group.resize(group_size);
            for (int& vertex : group)
                input.ReadInt(vertex);
        }
    }
    return queries;
}

} // namespace gst
