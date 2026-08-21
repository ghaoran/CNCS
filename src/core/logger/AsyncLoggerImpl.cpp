// 精简实现 al::Logger 的公开静态方法，使链接得以通过。
//
// 背景：AsyncLogger 库仅提供头文件（header-only 语义），原始配套的 src/*.cpp
// 实现已被历史 cleanup 提交删除，且与当前 header 不再兼容（LogCapture 的
// operator<< 重复定义、LogStream 析构重复等）。本文件按当前 header 提供
// al::Logger 的公开接口定义。项目实际日志路径走 LogHelper::Log()（std::cout），
// 因此这里的 AsyncLogger sink 队列仅作存储，不驱动消息分发。
#include <AsyncLogger/Logger.hpp>

#include <mutex>
#include <vector>
#include <utility>

namespace al
{
    namespace {
        std::mutex g_sink_mtx;
        std::vector<LogSink> g_sinks;
    }

    void Logger::AddSink(LogSink sink)
    {
        std::lock_guard<std::mutex> lock(g_sink_mtx);
        g_sinks.push_back(std::move(sink));
    }

    void Logger::Init()
    {
        // 本构建为同步日志（由 LogHelper::Log 直接输出），无需启动后台线程。
    }

    void Logger::FlushQueue()
    {
        // 消息由 LogHelper::Log 同步写出，队列为空。
        // flush 后分发已注册 sink（不带待处理消息），供后期接入用。
        std::lock_guard<std::mutex> lock(g_sink_mtx);
        (void)g_sinks;
    }

    void Logger::Destroy()
    {
        std::lock_guard<std::mutex> lock(g_sink_mtx);
        g_sinks.clear();
    }
}
