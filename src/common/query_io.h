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

/** @brief 将 query、g10、query_g10.txt 或显式路径解析为询问文件。 */
std::string ResolveQueryFile(const std::string& graph_folder, const std::string& query_selector = "");
/**
 * @brief 一次读入并验证全部询问，后续批量运行不重复读取查询文件。
 *
 * 这里只验证记录数、非负组数、正组大小与 token 完整性；顶点范围必须结合
 * 具体图检查，统一由 `IsQueryFeasible` 在求解前完成。
 */
std::vector<Query> LoadQueriesFromFolder(const std::string& graph_folder, const std::string& query_selector = "");

}  // namespace gst

#endif  // GST_QUERY_IO_H
