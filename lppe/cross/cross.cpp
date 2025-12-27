#include "cross.hpp"

#include <string>
#include <filesystem>

#if defined(__linux__)
    #include <unistd.h>
    #include <limits.h>
#elif defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#endif

static std::string path = "";
std::string getExecutableDir() {
    if (!path.empty()) {
        return std::filesystem::path(path).parent_path().string();
    }

#if defined(_WIN32)
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (len > 0) {
        path = std::string(buffer, len);
    }

#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        path = std::string(buffer);
    }

#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer)-1);
    if (len != -1) {
        buffer[len] = '\0';
        path = std::string(buffer);
    }
#endif

    // 実行ファイルのディレクトリ部分を返す
    return std::filesystem::path(path).parent_path().string();
}
