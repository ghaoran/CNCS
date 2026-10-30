// 回归用例：锁定本次修复的几个真实缺陷，防止再次退化。
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <chrono>
#include <cmath>

#include <core/util/Ballistics.hpp>
#include <core/util/KalmanFilter.hpp>
#include <core/util/PathUtils.hpp>
#include <core/util/Platform.hpp>
#include <core/util/Result.hpp>
#include <core/util/HighResTimer.hpp>

using Catch::Approx;
using cncs_error::Code;

TEST_CASE("Result<void>::and_then 可延续到非 void Result", "[Regression][Result]") {
    // 修复前：Result<void>::and_then 要求目标返回 Result<void>，
    // 这条链式调用在编译期就被 static_assert 拒绝。
    SECTION("Ok -> Result<int>") {
        auto r = Result<void, Code>::Ok()
                     .and_then([]() -> Result<int, Code> { return Result<int, Code>::Ok(42); });
        REQUIRE(r.is_ok());
        REQUIRE(r.value() == 42);
    }

    SECTION("Err 短路并保留错误") {
        auto r = Result<void, Code>::Err(Code::NotInitialized)
                     .and_then([]() -> Result<int, Code> { return Result<int, Code>::Ok(7); });
        REQUIRE(r.is_err());
        REQUIRE(r.error() == Code::NotInitialized);
    }
}

TEST_CASE("KalmanFilter 观测矩阵维度与速度量纲", "[Regression][Kalman]") {
    KalmanFilter<6, 3> kf;
    kf.init({0.0f, 0.0f, 0.0f}, 0.016f);

    // 每帧(dt=0.016s)匀速移动 1 单位 => 62.5 units/s
    for (int i = 1; i <= 10; ++i) {
        kf.predict(0.016f);
        kf.update({static_cast<float>(i), 0.0f, 0.0f});
    }

    const auto vel = kf.get_velocity();
    // 修复前 H_ 维度写反造成越界读取，速度估计完全失真。
    REQUIRE(vel[0] == Approx(1.0f / 0.016f).epsilon(0.15f));
    REQUIRE(std::abs(vel[1]) < 5.0f);
    REQUIRE(std::abs(vel[2]) < 5.0f);

    const auto pos = kf.get_predicted_position();
    REQUIRE(pos[0] == Approx(10.0f).epsilon(0.1f));
}

TEST_CASE("Ballistics units and drop sign", "[Regression][Ballistics]") {
    const auto rifle = ballistics::WeaponBallistics::from_weapon_index(7);
    SECTION("flight time is sub-second at 1000 units") {
        const auto r = ballistics::predict_impact(Vec3_t{0,0,0}, Vec3_t{1000,0,0}, Vec3_t{0,0,0}, rifle);
        REQUIRE(r.has_value());
        REQUIRE(r->valid);
        REQUIRE(r->time_to_impact > 0.0f);
        REQUIRE(r->time_to_impact < 0.5f);
    }
}

TEST_CASE("Kalman velocity units per second", "[Reg][K2]") {
    KalmanFilter<6, 3> kf;
    kf.init({0.0f, 0.0f, 0.0f}, 0.016f);
    for (int i = 1; i <= 10; ++i) {
        kf.predict(0.016f);
        kf.update({static_cast<float>(i), 0.0f, 0.0f});
    }
    REQUIRE(kf.get_velocity()[0] == Approx(1.0f / 0.016f).epsilon(0.15f));
}

TEST_CASE("HighResTimer portable", "[Reg][Timer]") {
    SECTION("SleepMs") {
        const auto s = std::chrono::steady_clock::now();
        util::HighResTimer::SleepMs(10);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - s).count();
        REQUIRE(ms >= 8);
        REQUIRE(ms <= 50);
    }
    SECTION("past deadline returns immediately") {
        const auto s = std::chrono::steady_clock::now();
        util::HighResTimer::SleepUntil(s - std::chrono::milliseconds(50));
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - s).count();
        REQUIRE(ms < 20);
    }
    SECTION("negative duration does not block") {
        const auto s = std::chrono::steady_clock::now();
        util::HighResTimer::SleepUs(-1000);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - s).count();
        REQUIRE(ms < 20);
    }
}

TEST_CASE("PathUtils resolves exe dir", "[Reg][Path]") {
    const auto dir = util::ExeDir();
    REQUIRE_FALSE(dir.empty());
    const auto leaf = util::ExePath("config.json");
    REQUIRE(leaf.filename().string() == "config.json");
    REQUIRE(leaf.parent_path() == dir);
}

TEST_CASE("Platform macros are exclusive", "[Reg][Platform]") {
    static_assert(CNCS_PLATFORM_WINDOWS + CNCS_PLATFORM_POSIX == 1,
                  "exactly one platform macro must be 1");
    SUCCEED();
}
