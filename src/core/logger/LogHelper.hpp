#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <chrono>
#include <fmt/core.h>
#include <fmt/format.h>

namespace al {

enum class eLogLevel {
    LOG_VERBOSE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL
};

struct LogMessage {
    eLogLevel level;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

using LogMessagePtr = std::shared_ptr<LogMessage>;

enum class LogColor
{
    RESET,
    WHITE = 97,
    CYAN = 36,
    MAGENTA = 35,
    BLUE = 34,
    GREEN = 32,
    YELLOW = 33,
    RED = 31,
    BLACK = 30,
    GRAY = 90
};

class LogHelper {
public:
    ~LogHelper()                           = default;
    LogHelper(const LogHelper&)            = delete;
    LogHelper(LogHelper&&)                 = delete;
    LogHelper& operator=(const LogHelper&) = delete;
    LogHelper& operator=(LogHelper&&)      = delete;

    static void Free();
    static void Destroy();
    static bool Init();
    
    // Use const char* format string for better MSVC compatibility
    template<typename... Args>
    static void Log(eLogLevel level, const char* fmt_str, Args&&... args) {
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        
        const char* levelStr = "";
        switch (level) {
            case eLogLevel::LOG_VERBOSE: levelStr = "[VERBOSE] "; break;
            case eLogLevel::LOG_DEBUG:   levelStr = "[DEBUG] "; break;
            case eLogLevel::LOG_INFO:    levelStr = "[INFO] "; break;
            case eLogLevel::LOG_WARNING: levelStr = "[WARNING] "; break;
            case eLogLevel::LOG_ERROR:   levelStr = "[ERROR] "; break;
            case eLogLevel::LOG_FATAL:   levelStr = "[FATAL] "; break;
        }
        
        std::cout << levelStr << fmt::format(fmt_str, std::forward<Args>(args)...) << std::endl;
    }
    
private:
    LogHelper(){};

    static LogHelper& GetInstance()
    {
        static LogHelper i{};
        return i;
    }

    bool InitImpl();

private:
    static LogColor GetColor(const eLogLevel level);
    static const char* GetLevelStr(const eLogLevel level);
    std::string FormatConsole(const LogMessagePtr msg);
private:
	std::ofstream m_ConsoleOut;
};

} // namespace al