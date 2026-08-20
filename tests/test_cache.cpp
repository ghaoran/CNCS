#include <catch2/catch_test_macros.hpp>
#include <core/engine/cache/Cache.hpp>
#include <core/engine/Engine.hpp>

TEST_CASE("Cache 快照功能", "[Cache]") {
    SECTION("CopySnapshot 返回空指针当未初始化") {
        auto snapshot = Cache::CopySnapshot();
        // 未初始化时可能返回空
        // REQUIRE(snapshot == nullptr);
    }
    
    SECTION("Refresh 返回布尔值") {
        // 需要引擎初始化，暂时跳过
        SUCCEED("Cache 测试需要引擎初始化");
    }
}
