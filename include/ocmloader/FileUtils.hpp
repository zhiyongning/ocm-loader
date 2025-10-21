#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>

// 定义路径分隔符（跨平台）
#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

class FileUtils {
public:
    // 对外接口：获取文件路径（自动创建geodata目录）
    static std::string getFilePath(const std::string& filename, const std::string& directoryName);

private:
    // 辅助函数：检查目录是否存在（私有，仅内部调用）
    static bool directoryExists(const std::string& dirPath);

    // 辅助函数：创建目录（私有，仅内部调用）
    static bool createDirectory(const std::string& dirPath);
};

#endif // FILE_UTILS_H