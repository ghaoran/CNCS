
#include "LogHelper.hpp"
#include <AsyncLogger/Logger.hpp>
#include <AsyncLogger/LogCapture.hpp>
#include <AsyncLogger/LogLevel.hpp>
#include <AsyncLogger/LogMessage.hpp>
#include <sstream>
#include <iomanip>

#define ADD_COLOR_TO_STREAM(color) "\x1b[" << int(color) << "m"
#define RESET_STREAM_COLOR "\x1b[0m"
#define HEX(value) "0x" << std::hex << std::uppercase << DWORD64(value) << std::dec << std::nouppercase

// LogHelper 定义于 namespace al 内；这里用 using 使裸的 LogHelper:: 可用
// （main.cpp 已用 al::LogHelper:: 显式前缀，此处保持本文件原始调用风格）。
using namespace al;

bool LogHelper::Init()
{
    return GetInstance().InitImpl();
}

void LogHelper::Destroy()
{
    al::Logger::FlushQueue();
    al::Logger::Destroy();
}

void LogHelper::Free() {
    if (HWND console = GetConsoleWindow()) {
        FreeConsole();
        PostMessage(console, WM_CLOSE, 0, 0);
    }
}

bool LogHelper::InitImpl() {
    al::Logger::Init();

    if (auto handle = GetStdHandle(STD_OUTPUT_HANDLE); handle != nullptr)
    {
        SetConsoleTitleA("Hello, CNCS!");
        SetConsoleOutputCP(CP_UTF8);

        DWORD consoleMode;
        GetConsoleMode(handle, &consoleMode);

        // terminal like behaviour enable full color support
        consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        consoleMode &= ~(ENABLE_QUICK_EDIT_MODE);

        SetConsoleMode(handle, consoleMode);
    }

    m_ConsoleOut.open("CONOUT$", std::ios_base::out | std::ios_base::app);

    al::Logger::AddSink([this](al::LogMessagePtr msg) {
#ifndef _DEBUG
        if (msg->level == al::eLogLevel::LOG_VERBOSE)
            return;
#endif
        std::string formatted = this->FormatConsole(msg);

        m_ConsoleOut << formatted;
        m_ConsoleOut.flush();
    });

    return true;
}

std::string LogHelper::FormatConsole(const al::LogMessagePtr msg) {
        std::stringstream out;

#ifdef _DEBUG
        const auto timestamp = std::format("{0:%H:%M:%S}", msg->timestamp);
#else
        const auto timestamp = std::format("{0:%H:%M:%S}", std::chrono::floor<std::chrono::seconds>(msg->timestamp));
#endif
		const auto& location = msg->location;
		const auto level     = msg->level;
		const auto color     = GetColor(level);

		const auto file = std::filesystem::path(location.file_name()).filename().string();

#ifdef _DEBUG
		out << ADD_COLOR_TO_STREAM(LogColor::GRAY) << "[" << timestamp << "]" << ADD_COLOR_TO_STREAM(color) << "[" << GetLevelStr(level) << "/" << file << ":"
		    << location.line() << "] " << RESET_STREAM_COLOR << msg->message;
#else
        out << ADD_COLOR_TO_STREAM(LogColor::GRAY) << "[" << timestamp << "]" << ADD_COLOR_TO_STREAM(color) << "[" << GetLevelStr(level) << "] " << RESET_STREAM_COLOR << msg->message;
#endif

		return out.str();
}

LogColor LogHelper::GetColor(const al::eLogLevel level)
{
    switch (level)
    {
    case al::eLogLevel::LOG_VERBOSE: return LogColor::BLUE;
    case al::eLogLevel::LOG_DEBUG:   return LogColor::CYAN;
    case al::eLogLevel::LOG_INFO:    return LogColor::GREEN;
    case al::eLogLevel::LOG_WARNING: return LogColor::YELLOW;
    case al::eLogLevel::LOG_ERROR:   return LogColor::RED;
    case al::eLogLevel::LOG_FATAL:   return LogColor::MAGENTA;
    }
    return LogColor::WHITE;
}

const char* LogHelper::GetLevelStr(const al::eLogLevel level) {
    constexpr std::array<const char*, 6> levelStrings = {{"VRB", "DBG", "INF", "WRN", "ERR", "FTL"}};

    return levelStrings[static_cast<int>(level)];
}
