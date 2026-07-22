#ifndef GST_OUTPUT_MANAGER_H
#define GST_OUTPUT_MANAGER_H

#include <string>

namespace gst
{

class OutputManager
{
public:
    // 创建结果目录，main_result_filename 通常为 weights.txt。
    OutputManager(std::string result_dir, std::string main_result_filename);

    // 开始一个新 run：保留旧 run，空两行后追加 header。
    void BeginResultRun(const std::string& filename, const std::string& header) const;
    // 向主结果文件追加一条询问的 time/weight/memory。
    void AppendMainResultLine(const std::string& line) const;
    // 通用追加操作，目前只由上述两个接口调用。
    void AppendResultLine(const std::string& filename, const std::string& line) const;

private:
    std::string result_dir_;
    std::string main_result_filename_;
};

}  // namespace gst

#endif  // GST_OUTPUT_MANAGER_H
