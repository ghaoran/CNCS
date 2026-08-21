#pragma once

#include <windows.h>
#include <chrono>
#include <thread>

namespace util {

// High-resolution timer using Windows Waitable Timer
// Provides ~0.5ms precision vs ~15ms for std::this_thread::sleep_for
class HighResTimer {
public:
    HighResTimer() {
        // Create a high-resolution waitable timer
        timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!timer_) {
            // Fallback to standard timer
            timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        }
    }
    
    ~HighResTimer() {
        if (timer_) {
            CloseHandle(timer_);
        }
    }
    
    // Non-copyable, movable
    HighResTimer(const HighResTimer&) = delete;
    HighResTimer& operator=(const HighResTimer&) = delete;
    HighResTimer(HighResTimer&& other) noexcept : timer_(other.timer_) { other.timer_ = nullptr; }
    HighResTimer& operator=(HighResTimer&& other) noexcept {
        if (this != &other) {
            if (timer_) CloseHandle(timer_);
            timer_ = other.timer_;
            other.timer_ = nullptr;
        }
        return *this;
    }
    
    // Static convenience methods
    static void SleepFor(std::chrono::microseconds duration) {
        static HighResTimer timer;
        if (!timer.timer_) {
            std::this_thread::sleep_for(duration);
            return;
        }
        
        LARGE_INTEGER due_time;
        due_time.QuadPart = -static_cast<LONGLONG>(duration.count() * 10); // 100ns units, negative = relative
        
        if (!SetWaitableTimer(timer.timer_, &due_time, 0, nullptr, nullptr, FALSE)) {
            std::this_thread::sleep_for(duration);
            return;
        }
        
        WaitForSingleObject(timer.timer_, INFINITE);
    }
    
    static void SleepUntil(std::chrono::steady_clock::time_point wake_time) {
        auto now = std::chrono::steady_clock::now();
        if (wake_time <= now) return;
        SleepFor(std::chrono::duration_cast<std::chrono::microseconds>(wake_time - now));
    }
    
    static void SleepMs(int milliseconds) {
        SleepFor(std::chrono::milliseconds(milliseconds));
    }
    
    static void SleepUs(int microseconds) {
        SleepFor(std::chrono::microseconds(microseconds));
    }

private:
    HANDLE timer_ = nullptr;
};

} // namespace util