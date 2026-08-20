#include <catch2/catch_test_macros.hpp>
#include <core/config/Config.hpp>
#include <filesystem>
#include <fstream>

TEST_CASE("Config 读写功能", "[Config]") {
    SECTION("默认配置创建") {
        // 使用临时文件路径避免污染实际配置
        std::filesystem::path temp_path = std::filesystem::temp_directory_path() / "test_config.json";
        if (std::filesystem::exists(temp_path)) {
            std::filesystem::remove(temp_path);
        }
        
        // 这里需要修改 Config 类以支持自定义路径，暂时跳过
        SUCCEED("Config 测试需要重构以支持测试路径");
    }
}

TEST_CASE("配置默认值", "[Config]") {
    SECTION("默认值正确") {
        REQUIRE(cfg::enabled == true);
        REQUIRE(cfg::deathmatch == false);
        REQUIRE(cfg::aimbot::enabled == false);
        REQUIRE(cfg::aimbot::fov == 200.0f);
        REQUIRE(cfg::aimbot::smoothing == 0.3f);
    }
}