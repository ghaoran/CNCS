// GUI 通用辅助函数实现。
#include "GuiUtils.hpp"

#include <Windows.h>
#include <filesystem>
#include <string>

namespace {
// 常见 Windows 中文字体（按兼容性优先级）。
// 微软雅黑 (msyh.ttc) 覆盖现代系统；黑体 (simhei.ttf) / 宋体 (simsun.ttc) 为后备。
constexpr const wchar_t* kCjkFontCandidates[] = {
    L"msyh.ttc",
    L"msyh.ttf",
    L"simhei.ttf",
    L"simsun.ttc",
};
} // namespace

const char* GetCJKFontPath() {
    using std::filesystem::path;

    static std::string s_cached;

    if (!s_cached.empty())
        return s_cached.c_str();

    // 系统字体目录通常为 %WINDIR%\Fonts，回退到 C:\Windows\Fonts。
    wchar_t windir[MAX_PATH] = {};
    GetWindowsDirectoryW(windir, MAX_PATH);
    const path font_dir = path(windir) / L"Fonts";

    std::string found;
    for (const wchar_t* name : kCjkFontCandidates) {
        const path p = font_dir / name;
        if (std::filesystem::exists(p)) {
            // ImGui 期望 const char*（本地 ANSI）。将宽路径转 UTF-8。
            const std::wstring w = p.wstring();
            const int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                                nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                found.assign(len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                    found.data(), len, nullptr, nullptr);
            }
            break;
        }
    }

    if (found.empty())
        s_cached = ""; // 未找到：返回空串，调用方回退。
    else
        s_cached = std::move(found);

    return s_cached.c_str();
}
