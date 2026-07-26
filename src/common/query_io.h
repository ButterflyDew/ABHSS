#ifndef GST_QUERY_IO_H
#define GST_QUERY_IO_H

#include <string>
#include <vector>

namespace gst
{

struct Query
{
    // groups[i] 存储第 i 个查询组的候选顶点。
    std::vector<std::vector<int>> groups;
};

/** @brief 用大块数字缓冲一次读入查询文件，批处理期间不再重复解析。 */
std::vector<Query> LoadQueries(const std::string& query_file);

} // namespace gst

#endif // GST_QUERY_IO_H
