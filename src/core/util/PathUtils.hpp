#pragma once

// -----------------------------------------------------------------------------
// 跨平台路径工具：定位可执行文件所在目录（配置、地图碰撞缓存都放在那里）。
// 原实现散落在 Config.cpp / Visibility.cpp，各自调用 Windows API，
// 导致 core 无法在非 Windows 工具链下编译（CI 的 clang-format/单元测试与
// Linux 静态检查全部失败）。现在统一到一处。
// -----------------------------------------------------------------------------

#include "core/util/Platform.hpp"

#include <filesystem>
#include <string>

#if CNCS_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif CNCS_PLATFORM_POSIX
#include <unistd.h>
#include <climits>
#endif

namespace util {

// 返回当前进程可执行文件所在目录；取不到信息时退回当前工作目录。
// 不会抛异常：任何失败都回落到 current_path()（本身带 error_code 重载）。
inline std::filesystem::path ExeDir() {
    namespace fs = std::filesystem;
    std::error_code ec;

#if CNCS_PLATFORM_WINDOWS
    char buf[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n != 0 && n < MAX_PATH)
        return fs::path(buf).parent_path();
#elif CNCS_PLATFORM_POSIX
    char buf[PATH_MAX] = {};
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        return fs::path(buf).parent_path();
    }
#endif

    fs::path cwd = fs::current_path(ec);
    return ec ? fs::path(".") : cwd;
}

// 在 ExeDir() 下拼接相对路径，用于 config.json / maps/*.m2 等旁挂资源。
inline std::filesystem::path ExePath(const std::string& leaf) {
    return ExeDir() / leaf;
}

} // namespace util
