#pragma once

// -----------------------------------------------------------------------------
// core 层轻量日志门面
//
// 背景：LOGF 宏原先只在 pch_core.hpp 中定义，而 pch_core.hpp 会强制注入
// <windows.h>、imgui.h、AsyncLogger 等重型依赖 —— 单元测试或任何非 GUI
// 目标一旦包含它就无法编译。这里给 core 模块一个极薄的日志入口：
//   - 定义 CNCS_ENABLE_ASYNC_LOGGER 时转发到 al::LogHelper::Log(...)
//   - 否则退化为 std::cout / std::cerr 直写
// 调用语法与 LOGF 一致：CNCS_CORE_LOG(INFO, "fmt {}", value)。
// -----------------------------------------------------------------------------

#include <fmt/format.h>

#include <iostream>
#include <string>
#include <utility>

#ifdef CNCS_ENABLE_ASYNC_LOGGER
#include "core/logger/LogHelper.hpp"
#endif

namespace cncs::log {

enum class Level { VERBOSE, DEBUG, INFO, WARNING, ERROR, FATAL };

inline const char* ToString(Level level) {
    switch (level) {
        case Level::VERBOSE: return "[VERBOSE] ";
        case Level::DEBUG:   return "[DEBUG]   ";
        case Level::INFO:    return "[INFO]    ";
        case Level::WARNING: return "[WARNING] ";
        case Level::ERROR:   return "[ERROR]   ";
        case Level::FATAL:   return "[FATAL]   ";
    }
    return "[LOG]     ";
}

inline bool IsErrStream(Level level) {
    return level == Level::WARNING || level == Level::ERROR || level == Level::FATAL;
}

template <typename... Args>
void WriteF(Level level, const std::string& fmt_str, Args&&... args) {
    std::ostream& os = IsErrStream(level) ? std::cerr : std::cout;
    os << ToString(level) << fmt::format(fmt::runtime(fmt_str), std::forward<Args>(args)...) << '\n';
}

} // namespace cncs::log

// core 层日志宏：LEVEL 直接写 VERBOSE/DEBUG/INFO/WARNING/ERROR/FATAL。
#ifndef CNCS_CORE_LOG
#ifdef CNCS_ENABLE_ASYNC_LOGGER
#define CNCS_CORE_LOG(LEVEL, ...) \
    ::al::LogHelper::Log(::al::eLogLevel::LOG_##LEVEL, __VA_ARGS__)
#else
#define CNCS_CORE_LOG(LEVEL, ...) \
    ::cncs::log::WriteF(::cncs::log::Level::LEVEL, __VA_ARGS__)
#endif
#endif
