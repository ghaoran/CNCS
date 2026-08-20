#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <source_location>
#include <memory>
#include <format>

namespace al
{
    class LogStream;

    struct LogMessage
    {
        eLogLevel level;
        std::chrono::system_clock::time_point timestamp;
        std::source_location location;
        std::string message;
        std::shared_ptr<LogStream> stream;
    };

    using LogMessagePtr = std::shared_ptr<LogMessage>;
}