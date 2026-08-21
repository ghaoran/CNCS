#include "LogStream.hpp"

namespace al
{
    LogStream::LogStream() = default;

    LogStream::LogStream(const std::string& name, bool enabled)
        : m_Name(name)
        , m_Enabled(enabled)
    {
    }
}