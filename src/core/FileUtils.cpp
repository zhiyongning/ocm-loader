#include "FileUtils.hpp"
#include <cstdio>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <stringapiset.h>
#endif

// 检查目录是否存在（实现同前，略作调整为静态函数）
bool FileUtils::directoryExists(const std::string& dirPath) {
#ifdef _WIN32
    std::wstring wDirPath;
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, dirPath.c_str(), -1, nullptr, 0);
    if (wideLen == 0) return false;
    wDirPath.resize(wideLen);
    MultiByteToWideChar(CP_UTF8, 0, dirPath.c_str(), -1, &wDirPath[0], wideLen);

    HANDLE hFile = CreateFileW(wDirPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_DIRECTORY, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(hFile);
    return true;
#else
    struct stat info;
    if (stat(dirPath.c_str(), &info) != 0) {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
#endif
}

// 创建目录（实现同前，调整为静态函数）
bool FileUtils::createDirectory(const std::string& dirPath) {
#ifdef _WIN32
    std::wstring wDirPath;
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, dirPath.c_str(), -1, nullptr, 0);
    if (wideLen == 0) return false;
    wDirPath.resize(wideLen);
    MultiByteToWideChar(CP_UTF8, 0, dirPath.c_str(), -1, &wDirPath[0], wideLen);

    if (!CreateDirectoryW(wDirPath.c_str(), nullptr)) {
        DWORD err = GetLastError();
        return (err == ERROR_ALREADY_EXISTS);
    }
    return true;
#else
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    if (mkdir(dirPath.c_str(), mode) != 0) {
        return (errno == EEXIST);
    }
    return true;
#endif
}

// 对外接口：生成文件路径并确保目录存在
std::string FileUtils::getFilePath(const std::string& filename, const std::string& direcotryName) {
    std::string geodata_dir = std::string(".") + PATH_SEP + direcotryName;

    if (!directoryExists(geodata_dir)) {
        if (!createDirectory(geodata_dir)) {
            printf("Failed to create directory: %s\n", geodata_dir.c_str());
        }
    }

    return geodata_dir + PATH_SEP + filename;
}