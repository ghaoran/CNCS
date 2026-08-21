#include "Logger.hpp"
#include "LogMessage.hpp"

namespace al
{
    void Logger::AddSink(LogSink sink)
    {
        GetInstance().m_Sinks.push_back(std::move(sink));
    }

    void Logger::Destroy()
    {
        GetInstance().DestroyImpl();
    }

    void Logger::Init()
    {
        GetInstance().InitImpl();
    }

    void Logger::FlushQueue()
    {
        GetInstance().FlushQueueImpl();
    }

    void Logger::PushMessage(const eLogLevel level, std::chrono::system_clock::time_point&& timestamp, std::source_location&& location, std::string&& message, std::optional<std::shared_ptr<LogStream> const>&& stream) noexcept
    {
        GetInstance().QueueMessage(std::make_shared<LogMessage>(
            level, std::move(timestamp), std::move(location), std::move(message), std::move(stream)));
    }

    void Logger::CallSinks(LogMessagePtr msgPtr)
    {
        for (auto& sink : m_Sinks)
            sink(msgPtr);
    }

    void Logger::DestroyImpl()
    {
        if (m_Running)
        {
            m_Running = false;
            if (m_LogWorker.joinable())
                m_LogWorker.join();
        }
    }

    void Logger::InitImpl()
    {
        if (!m_Running)
        {
            m_Running = true;
            m_LogWorker = std::thread([this]() {
                while (m_Running)
                {
                    std::function<void()> task;
                    if (m_Queue.try_and_pop(task))
                    {
                        task();
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            });
        }
    }

    void Logger::FlushQueueImpl()
    {
        std::function<void()> task;
        while (m_Queue.try_and_pop(task))
        {
            task();
        }
    }

    void Logger::QueueMessage(LogMessagePtr msgPtr)
    {
        m_Queue.push([this, msgPtr]() {
            CallSinks(msgPtr);
        });
    }
}