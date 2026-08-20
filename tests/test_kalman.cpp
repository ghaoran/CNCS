#include <catch2/catch_test_macros.hpp>
#include <core/util/KalmanFilter.hpp>

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
        REQUIRE(pos[0] > 1.0f && pos[0] < 1.1f);
        REQUIRE(pos[1] > 1.0f && pos[1] < 1.1f);
        REQUIRE(pos[2] > 1.0f && pos[2] < 1.1f);
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
        
        auto vel = kf.get_velocity();
        // 速度应该接近 1 单位/帧
        REQUIRE(vel[0] > 0.5f && vel[0] < 2.0f);
        REQUIRE(std::abs(vel[1]) < 0.5f);
        REQUIRE(std::abs(vel[2]) < 0.5f);
    }
    
    SECTION("重置功能") {
        Kalman3D kf;
        Kalman3D::MeasVec meas = {100.0f, 100.0f, 100.0f};
        kf.init(meas, 0.016f);
        
        kf.reset();
        REQUIRE(!kf.is_initialized());
    }
}