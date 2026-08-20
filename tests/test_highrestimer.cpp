#include <catch2/catch_test_macros.hpp>
#include <core/util/HighResTimer.hpp>
#include <chrono>
#include <thread>

TEST_CASE("HighResTimer 基本功能", "[HighResTimer]") {
    SECTION("SleepFor 基本睡眠") {
        util::HighResTimer timer;
        auto start = std::chrono::steady_clock::now();
        timer.SleepMs(10);
        auto elapsed = std::chrono::steady_clock::now() - start;
        
        // 应该在 8-15ms 之间 (允许一定误差)
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        REQUIRE(ms >= 8);
        REQUIRE(ms <= 20);
    }
    
    SECTION("SleepUs 微秒级睡眠") {
        util::HighResTimer timer;
        auto start = std::chrono::steady_clock::now();
        timer.SleepUs(5000); // 5ms
        auto elapsed = std::chrono::steady_clock::now() - start;
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        REQUIRE(ms >= 3);
        REQUIRE(ms <= 15);
    }
    
    SECTION("SleepUntil 时间点唤醒") {
        util::HighResTimer timer;
        auto wake_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
        
        auto start = std::chrono::steady_clock::now();
        timer.SleepUntil(wake_time);
        auto elapsed = std::chrono::steady_clock::now() - start;
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        REQUIRE(ms >= 15);
        REQUIRE(ms <= 30);
    }
    
    SECTION("静态方法") {
        auto start = std::chrono::steady_clock::now();
        util::HighResTimer::SleepMs(5);
        auto elapsed = std::chrono::steady_clock::now() - start;
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        REQUIRE(ms >= 3);
        REQUIRE(ms <= 15);
    }
    
    SECTION("移动语义") {
        util::HighResTimer timer1;
        util::HighResTimer timer2 = std::move(timer1);
        // timer1 应该为空，timer2 应该有效
        timer2.SleepMs(5); // 不应崩溃
    }
}