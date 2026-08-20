#include <catch2/catch_test_macros.hpp>
#include <core/util/Ballistics.hpp>
#include <core/engine/types/Vec3.hpp>

using namespace ballistics;

TEST_CASE("WeaponBallistics 预设值", "[Ballistics]") {
    SECTION("步枪默认值") {
        auto b = WeaponBallistics::from_weapon_index(7); // AK-47
        REQUIRE(b.initial_velocity > 0.0f);
        REQUIRE(b.drag_coefficient > 0.0f);
        REQUIRE(b.gravity > 0.0f);
    }
    
    SECTION("手枪较低初速") {
        auto rifle = WeaponBallistics::from_weapon_index(7);  // AK-47
        auto pistol = WeaponBallistics::from_weapon_index(1); // Glock
        REQUIRE(pistol.initial_velocity < rifle.initial_velocity);
    }
    
    SECTION("狙击枪高初速低阻力") {
        auto sniper = WeaponBallistics::from_weapon_index(9); // AWP
        auto rifle = WeaponBallistics::from_weapon_index(7);  // AK-47
        REQUIRE(sniper.initial_velocity > rifle.initial_velocity);
        REQUIRE(sniper.drag_coefficient < rifle.drag_coefficient);
    }
}

TEST_CASE("predict_impact 基本功能", "[Ballistics]") {
    SECTION("静止目标") {
        Vec3_t shooter = {0.0f, 0.0f, 0.0f};
        Vec3_t target = {1000.0f, 0.0f, 0.0f}; // 1000 units away
        Vec3_t target_vel = {0.0f, 0.0f, 0.0f};
        
        auto b = WeaponBallistics::from_weapon_index(7); // AK-47
        auto result = predict_impact(shooter, target, target_vel, b);
        
        REQUIRE(result.has_value());
        REQUIRE(result->valid);
        REQUIRE(result->time_to_impact > 0.0f);
        REQUIRE(result->bullet_drop >= 0.0f);
    }
    
    SECTION("移动目标") {
        Vec3_t shooter = {0.0f, 0.0f, 0.0f};
        Vec3_t target = {1000.0f, 0.0f, 0.0f};
        Vec3_t target_vel = {100.0f, 0.0f, 0.0f}; // 向侧移动
        
        auto b = WeaponBallistics::from_weapon_index(7);
        auto result = predict_impact(Vec3_t{0,0,0}, target, target_vel, b);
        
        REQUIRE(result.has_value());
        REQUIRE(result->valid);
        // 预测位置应该领先于当前位置
        REQUIRE(result->aim_point.x > 1000.0f);
    }
    
    SECTION("超出最大时间返回 nullopt") {
        Vec3_t shooter = {0.0f, 0.0f, 0.0f};
        Vec3_t target = {100000.0f, 0.0f, 0.0f}; // 极远距离
        Vec3_t target_vel = {0.0f, 0.0f, 0.0f};
        
        auto b = WeaponBallistics::from_weapon_index(7);
        auto result = predict_impact(Vec3_t{0,0,0}, target, Vec3_t{0,0,0}, b, 0.1f);
        
        // 超过最大时间应返回 nullopt 或 time 被限制
        if (result.has_value()) {
            REQUIRE(result->time_to_impact <= 0.1f + 0.001f);
        }
    }
}

TEST_CASE("RecoilCompensation 基本功能", "[Ballistics]") {
    SECTION("更新和获取补偿") {
        RecoilCompensation rc;
        Vec3_t punch = {1.0f, 0.5f, 0.0f};
        Vec3_t punch_vel = {0.1f, 0.05f, 0.0f};
        
        rc.update(punch, punch_vel);
        auto comp = rc.get_compensation(1);
        
        REQUIRE(comp.x == Approx(1.0f).margin(0.01f));
        REQUIRE(comp.y == Approx(0.5f).margin(0.01f));
    }
    
    SECTION("超出范围返回零") {
        RecoilCompensation rc;
        auto comp = rc.get_compensation(0);
        REQUIRE(comp.x == 0.0f && comp.y == 0.0f && comp.z == 0.0f);
        
        comp = rc.get_compensation(65);
        REQUIRE(comp.x == 0.0f && comp.y == 0.0f && comp.z == 0.0f);
    }
}