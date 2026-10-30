#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <core/util/KalmanFilter.hpp>

// Catch2 v3 把 Approx 放在 Catch 命名空间下（v2 是全局），统一显式引入。
using Catch::Approx;

using Kalman3D = KalmanFilter<6, 3>;

TEST_CASE("KalmanFilter 基本功能", "[KalmanFilter]") {
    SECTION("初始化和预测") {
        Kalman3D kf;
        Kalman3D::MeasVec meas = {1.0f, 2.0f, 3.0f};
        
        kf.init(meas, 0.016f);
        REQUIRE(kf.is_initialized());
        
        auto pos = kf.get_predicted_position();
        REQUIRE(pos[0] == Approx(1.0f).margin(0.01f));
        REQUIRE(pos[1] == Approx(2.0f).margin(0.01f));
        REQUIRE(pos[2] == Approx(3.0f).margin(0.01f));
    }
    
    SECTION("预测步骤") {
        Kalman3D kf;
        Kalman3D::MeasVec meas = {0.0f, 0.0f, 0.0f};
        kf.init(meas, 0.016f);
        
        kf.predict(0.016f);
        auto pos = kf.get_predicted_position();
        // 位置不应改变（速度初始为0）
        REQUIRE(pos[0] == Approx(0.0f).margin(0.01f));
        REQUIRE(pos[1] == Approx(0.0f).margin(0.01f));
        REQUIRE(pos[2] == Approx(0.0f).margin(0.01f));
    }
    
    SECTION("更新步骤") {
        Kalman3D kf;
        Kalman3D::MeasVec meas = {1.0f, 1.0f, 1.0f};
        kf.init(meas, 0.016f);
        
        // 更新为新位置
        Kalman3D::MeasVec new_meas = {1.1f, 1.1f, 1.1f};
        kf.update(new_meas);
        
        auto pos = kf.get_predicted_position();
        // 位置应该向新测量值靠拢
        REQUIRE((pos[0] > 1.0f && pos[0] < 1.1f));
        REQUIRE((pos[1] > 1.0f && pos[1] < 1.1f));
        REQUIRE((pos[2] > 1.0f && pos[2] < 1.1f));
    }
    
    SECTION("速度估计") {
        Kalman3D kf;
        Kalman3D::MeasVec meas = {0.0f, 0.0f, 0.0f};
        kf.init(meas, 0.016f);
        
        // 模拟匀速运动
        for (int i = 1; i <= 10; ++i) {
            Kalman3D::MeasVec m = {float(i), 0.0f, 0.0f};
            kf.predict(0.016f);
            kf.update(m);
        }
        
        // 速度单位是 units/s，不是 units/frame：每帧(dt=0.016s)位移 1 单位
        // 对应 1 / 0.016 = 62.5 units/s。原断言按 units/frame 写，量纲错误。
        constexpr float kUnitsPerSecond = 1.0f / 0.016f; // 62.5
        auto vel = kf.get_velocity();
        REQUIRE(vel[0] == Approx(kUnitsPerSecond).epsilon(0.15f));
        REQUIRE(std::abs(vel[1]) < 5.0f);
        REQUIRE(std::abs(vel[2]) < 5.0f);
    }
    
    SECTION("重置功能") {
        Kalman3D kf;
        Kalman3D::MeasVec meas = {100.0f, 100.0f, 100.0f};
        kf.init(meas, 0.016f);
        
        kf.reset();
        REQUIRE(!kf.is_initialized());
    }
}