// GUI 通用辅助函数声明。
// 集中放置被多个 GUI frontend 翻译单元共享的小工具（当前主要是 CJK 字体路径解析）。
#pragma once

// 返回可用的 CJK（中文）TrueType/OpenType 字体路径，供 ImGui AddFontFromFileTTF 使用。
// 依次探测 Windows 常见中文字体（微软雅黑 → 黑体 → 宋体），返回首先存在者；
// 若无则返回空串（"")，调用方对 AddFontFromFileTTF 失败应有回退。
// 返回值为静态存储的字符串指针，生命周期为程序全程，无需释放。
const char* GetCJKFontPath();
