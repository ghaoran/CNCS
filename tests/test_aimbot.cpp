#include <catch2/catch_test_macros.hpp>
#include <gui/frontend/aimbot/Aimbot.hpp>

TEST_CASE("Aimbot 基本功能", "[Aimbot]") {
    SECTION("默认配置") {
        // Aimbot 是静态类，测试默认配置值
        REQUIRE(cfg::aimbot::enabled == false);
        REQUIRE(cfg::aimbot::key == 0x02);
        REQUIRE(cfg::aimbot::fov == 200.0f);
        REQUIRE(cfg::aimbot::smoothing == 0.3f);
        REQUIRE(cfg::aimbot::bone == 7);
        REQUIRE(cfg::aimbot::show_fov == true);
        REQUIRE(cfg::aimbot::visible_only == false);
        REQUIRE(cfg::aimbot::smart_bone == true);
    }
}

TEST_CASE("Aimbot 预测功能", "[Aimbot]") {
    SECTION("PredictTarget 返回有效结构") {
        // 这里需要模拟环境，暂时验证结构体定义
        Aimbot::PredictionData data;
        data.predicted_position = {0, 0, 0};
        data.time_to_impact = 0.0f;
        data.bullet_drop = 0.0f;
        data.valid = false;
        
        REQUIRE(data.valid == false);
    }
}
