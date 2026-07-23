#ifndef GST_GRAPH_IO_H
#define GST_GRAPH_IO_H

#include <limits>
#include <string>
#include <vector>

namespace gst
{

struct UndirectedEdge
{
    int id = -1;
    int u = 0;
    int v = 0;
    double w = 0.0;
};

struct AdjEdge
{
    int to = 0;
    int edge_id = -1;
    double w = 0.0;
};

struct Graph
{
    int n = 0;
    int m = 0;
    double minimum_edge_weight = std::numeric_limits<double>::infinity();
    std::vector<UndirectedEdge> edges;
    std::vector<std::vector<AdjEdge>> adj;

    // 图加载阶段一次性建立的无向连通分量索引。component_of[v] 为稠密的
    // [0, component_count) 编号；手工构造的小图可留空，由可行性检查回退
    // 计算。该索引属于所有方法共享的只读图数据，不计入逐查询算法时间。
    std::vector<int> component_of;
    int component_count = 0;
};

/**
 * @brief 按字典序列出 data_root 下的直接子目录。
 * @return 根不存在时返回空表；否则只返回目录名，不递归且不改动文件系统。
 *
 * 稳定排序使 1-based 数字选择器跨运行可复现；正式矩阵仍应优先用图名。
 */
std::vector<std::string> ListGraphFolders(const std::string& data_root);
/**
 * @brief 将直接目录路径、图名或 1-based 序号解析为真实图目录。
 * @throws std::runtime_error 当根下无图、序号越界或名称不存在时抛出。
 *
 * 空选择器优先选 `example`，仅用于手工 smoke；正式实验传稳定图名或路径。
 */
std::string ResolveGraphFolder(const std::string& data_root, const std::string& graph_selector);
/**
 * @brief 用大块数字扫描读取 graph.txt，并构造原边数组和双向邻接表。
 * @throws std::runtime_error 当文件缺失、header/edge token 非法、端点越界或
 *         权重为负/非有限值时抛出；容量不足时传播标准分配异常。
 *
 * 加载分三阶段：先保存原边并统计度数，再按精确容量建立邻接表，最后一次
 * 扫描邻接表建立连通分量缓存。该过程不改变边序、edge_id、邻接插入顺序或
 * 浮点解析语义；连通分量缓存避免每条查询重新执行 O(n+m) 图扫描。
 */
Graph LoadGraphFromFolder(const std::string& graph_folder);

}  // namespace gst

#endif  // GST_GRAPH_IO_H
