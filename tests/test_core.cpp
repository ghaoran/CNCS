#include <catch2/catch_test_macros.hpp>
#include <core/util/Result.hpp>
#include <string>

TEST_CASE("Result 类型转换", "[Result]") {
    SECTION("int 类型") {
        auto r = Result<int, cncs_error::Code>::Ok(42);
        REQUIRE(r.value() == 42);
    }
    
    SECTION("字符串类型") {
        auto r = Result<std::string, cncs_error::Code>::Ok("hello");
        REQUIRE(r.value() == "hello");
    }
    
    SECTION("自定义类型") {
        struct Point { int x, y; };
        auto r = Result<Point, cncs_error::Code>::Ok(Point{1, 2});
        REQUIRE(r.value().x == 1);
        REQUIRE(r.value().y == 2);
    }
}