#pragma once

// -----------------------------------------------------------------------------
// 高精度睡眠
//
// Windows: CreateWaitableTimerExW(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION)，
//          精度约 0.5ms（对比 std::this_thread::sleep_for 的 ~15ms）。
//          取不到高精度定时器时退化为普通 waitable timer，再退化为 sleep_for。
// POSIX  : clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME) 绝对时基睡眠，
//          不受相对睡眠累计漂移影响；被信号打断自动续睡。
//
// 对外 API 保持既有调用方式：SleepMs / SleepUs / SleepFor / SleepUntil。
// -----------------------------------------------------------------------------

#include "core/util/Platform.hpp"

#include <chrono>
#include <thread>

#if CNCS_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif CNCS_PLATFORM_POSIX
#include <cerrno>
#include <ctime>
#endif

namespace util {

class HighResTimer {
public:
    HighResTimer() {
#if CNCS_PLATFORM_WINDOWS
        // 首选高精度定时器；系统不支持（Win8 以下或被策略禁用）时退回普通定时器。
        timer_ = CreateWaitableTimerExW(nullptr, nullptr,
                                        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                        TIMER_ALL_ACCESS);
        if (!timer_)
            timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
#endif
    }

    ~HighResTimer() {
#if CNCS_PLATFORM_WINDOWS
        if (timer_) {
            CloseHandle(timer_);
            timer_ = nullptr;
        }
#endif
    }

    HighResTimer(const HighResTimer&) = delete;
    HighResTimer& operator=(const HighResTimer&) = delete;

    HighResTimer(HighResTimer&& other) noexcept
#if CNCS_PLATFORM_WINDOWS
        : timer_(other.timer_)
#endif
    {
#if CNCS_PLATFORM_WINDOWS
        other.timer_ = nullptr;
#endif
    }

    HighResTimer& operator=(HighResTimer&& other) noexcept {
#if CNCS_PLATFORM_WINDOWS
        if (this != &other) {
            if (timer_) CloseHandle(timer_);
            timer_ = other.timer_;
            other.timer_ = nullptr;
        }
#else
        (void)other;
#endif
        return *this;
    }

    // 睡眠指定时长。负值按 0 处理（立即返回）。
    static void SleepFor(std::chrono::microseconds duration) {
        if (duration <= std::chrono::microseconds::zero())
            return;

#if CNCS_PLATFORM_WINDOWS
        static HighResTimer timer;
        if (timer.timer_) {
            LARGE_INTEGER due_time;
            // 100ns 为单位，负值表示相对时间。
            due_time.QuadPart = -static_cast<LONGLONG>(duration.count() * 10);
            if (SetWaitableTimer(timer.timer_, &due_time, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(timer.timer_, INFINITE);
                return;
            }
        }
        std::this_thread::sleep_for(duration);
#elif CNCS_PLATFORM_POSIX
        SleepUntil(std::chrono::steady_clock::now() + duration);
#else
        std::this_thread::sleep_for(duration);
#endif
    }

    // 睡到指定时间点；已过期立即返回。
    static void SleepUntil(std::chrono::steady_clock::time_point wake_time) {
        const auto now = std::chrono::steady_clock::now();
        if (wake_time <= now)
            return;

        const auto remaining = wake_time - now;

#if CNCS_PLATFORM_WINDOWS
        SleepFor(std::chrono::duration_cast<std::chrono::microseconds>(remaining));
#elif CNCS_PLATFORM_POSIX
        // 绝对时基睡眠：循环调用不会累积相对睡眠漂移。
        struct timespec ts {};
        ts.tv_sec = static_cast<time_t>(
            std::chrono::duration_cast<std::chrono::seconds>(remaining).count());
        ts.tv_nsec = static_cast<long>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count() % 1000000000LL);
        while (::clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts) == EINTR) {
            // 被信号打断时用 ts 中剩余的时长继续。
        }
#else
        std::this_thread::sleep_for(remaining);
#endif
    }

    static void SleepMs(int milliseconds) {
        SleepFor(std::chrono::milliseconds(milliseconds));
    }

    static void SleepUs(int microseconds) {
        SleepFor(std::chrono::microseconds(microseconds));
    }

private:
#if CNCS_PLATFORM_WINDOWS
    HANDLE timer_ = nullptr;
#endif
};

} // namespace util
