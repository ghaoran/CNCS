#pragma once
#include <string>
#include <source_location>

namespace al
{
    struct LogIntermediate
    {
        LogIntermediate() = default;
        LogIntermediate(const char* str, std::source_location loc = std::source_location::current())
            : FormatString(str), Location(std::move(loc)) {}

        std::string FormatString;
        std::source_location Location;
    };

    template<typename ...Args>
    inline LogIntermediate operator+(const LogIntermediate& lhs, Args&&... args)
    {
        return LogIntermediate(std::move(lhs.FormatString), lhs.Location);
    }
}