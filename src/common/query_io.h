#ifndef GST_QUERY_IO_H
#define GST_QUERY_IO_H

#include <string>
#include <vector>

namespace gst
{

struct Query
{
    // groups[i] 存储第 i 组候选节点集合。
    std::vector<std::vector<int>> groups;
};

// 将 query、g10、query_g10.txt 等选择器解析为图目录下的询问文件。
std::string ResolveQueryFile(const std::string& graph_folder, const std::string& query_selector = "");
// 一次读入全部询问，后续批量运行不重复加载图。
std::vector<Query> LoadQueriesFromFolder(const std::string& graph_folder, const std::string& query_selector = "");

}  // namespace gst

#endif  // GST_QUERY_IO_H
