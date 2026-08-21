#include "LogCapture.hpp"
#include "Logger.hpp"
#include "LogMessage.hpp"

namespace al
{
    LogCapture::LogCapture(const eLogLevel level, std::source_location&& location, std::optional<std::shared_ptr<LogStream> const> stream)
        : m_Level(level)
        , m_Timestamp(std::chrono::system_clock::now())
        , m_Location(std::move(location))
        , m_LogStream(std::move(stream))
    {
    }

    LogCapture::~LogCapture()
    {
        Logger::QueueMessage(std::make_shared<LogMessage>(
            m_Level, m_Timestamp, m_Location, m_Stream.str(), m_LogStream));
    }

    template<typename T>
    std::ostream& LogCapture::operator<< (const T& d)
    {
        m_Stream << d;
        return m_Stream;
    }
}