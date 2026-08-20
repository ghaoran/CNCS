#include <catch2/catch_test_macros.hpp>
#include <core/visibility/Visibility.hpp>
#include <core/engine/types/Vec3.hpp>

TEST_CASE("Visibility 碰撞检测", "[Visibility]") {
    SECTION("射线与 AABB 相交") {
        Vec3_t from = {0.0f, 0.0f, 0.0f};
        Vec3_t to = {10.0f, 0.0f, 0.0f};
        
        Visibility vis;
        // 简单的 AABB 相交测试
        bool blocked = vis.SmokeBlocksRay(from, to);
        // 无烟雾时不应阻挡
        REQUIRE(!blocked);
    }
    
    SECTION("烟雾阻挡") {
        Visibility vis;
        vis.SetSmokes({{{5.0f, -1.0f, -1.0f}, {5.0f, 1.0f, 1.0f}}});
        
        Vec3_t from = {0.0f, 0.0f, 0.0f};
        Vec3_t to = {10.0f, 0.0f, 0.0f};
        
        bool blocked = vis.SmokeBlocksRay(from, to);
        // 射线穿过烟雾 AABB
        REQUIRE(blocked);
    }
    
    SECTION("射线绕过烟雾") {
        Visibility vis;
        vis.SetSmokes({{{5.0f, -1.0f, -1.0f}, {5.0f, 1.0f, 1.0f}}});
        
        Vec3_t from = {0.0f, 10.0f, 0.0f}; // Y=10，在烟雾上方
        Vec3_t to = {10.0f, 10.0f, 0.0f};
        
        bool blocked = vis.SmokeBlocksRay(from, to);
        // 射线在烟雾上方，不应阻挡
        REQUIRE(!blocked);
    }
}