#pragma once

namespace al
{
    // 统一日志级别枚举。使用 LOG_ 前缀值与 pch_core.hpp 的
    // VERBOSE/DEBUG/INFO/WARNING/ERROR/FATAL 宏一致。
    enum class eLogLevel
    {
        LOG_VERBOSE,
        LOG_DEBUG,
        LOG_INFO,
        LOG_WARNING,
        LOG_ERROR,
        LOG_FATAL
    };
}
