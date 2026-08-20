#include <catch2/catch_test_macros.hpp>
#include <core/util/Result.hpp>

using namespace cncs_error;

TEST_CASE("Result 基本功能", "[Result]") {
    SECTION("成功构造") {
        auto result = Result<int, Code>::Ok(42);
        REQUIRE(result.is_ok());
        REQUIRE(result.value() == 42);
        REQUIRE(static_cast<bool>(result) == true);
    }
    
    SECTION("错误构造") {
        auto result = Result<int, Code>::Err(Code::InvalidParameter);
        REQUIRE(result.is_err());
        REQUIRE(result.error() == Code::InvalidParameter);
        REQUIRE(static_cast<bool>(result) == false);
    }
    
    SECTION("move 语义") {
        std::string value = "test";
        auto result = Result<std::string, Code>::Ok(std::move(value));
        REQUIRE(result.is_ok());
        REQUIRE(result.value() == "test");
    }
}

TEST_CASE("Result Monadic 操作", "[Result]") {
    SECTION("and_then 链式调用") {
        auto result = Result<int, Code>::Ok(10)
            .and_then([](int x) -> Result<int, Code> { return Result<int, Code>::Ok(x * 2); })
            .and_then([](int x) -> Result<int, Code> { return Result<int, Code>::Ok(x + 5); });
        
        REQUIRE(result.is_ok());
        REQUIRE(result.value() == 25);
    }
    
    SECTION("and_then 遇到错误停止") {
        auto result = Result<int, Code>::Ok(10)
            .and_then([](int) -> Result<int, Code> { return Result<int, Code>::Err(Code::InvalidParameter); })
            .and_then([](int) -> Result<int, Code> { return Result<int, Code>::Ok(999); });
        
        REQUIRE(result.is_err());
        REQUIRE(result.error() == Code::InvalidParameter);
    }
    
    SECTION("or_else 恢复") {
        auto result = Result<int, Code>::Err(Code::InvalidParameter)
            .or_else([](Code) -> Result<int, Code> { return Result<int, Code>::Ok(42); });
        
        REQUIRE(result.is_ok());
        REQUIRE(result.value() == 42);
    }
    
    SECTION("map 变换") {
        auto result = Result<int, Code>::Ok(21)
            .map([](int x) { return x * 2; });
        
        REQUIRE(result.is_ok());
        REQUIRE(result.value() == 42);
    }
    
    SECTION("map_err 变换错误") {
        auto result = Result<int, Code>::Err(Code::InvalidParameter)
            .map_err([](Code) { return Code::MemoryReadFailed; });
        
        REQUIRE(result.is_err());
        REQUIRE(result.error() == Code::MemoryReadFailed);
    }
}

TEST_CASE("Result void 特化", "[Result]") {
    SECTION("成功") {
        auto result = Result<void, Code>::Ok();
        REQUIRE(result.is_ok());
    }
    
    SECTION("错误") {
        auto result = Result<void, Code>::Err(Code::InvalidParameter);
        REQUIRE(result.is_err());
    }
    
    SECTION("and_then void") {
        auto result = Result<void, Code>::Ok()
            .and_then([]() -> Result<int, Code> { return Result<int, Code>::Ok(42); });
        
        REQUIRE(result.is_ok());
        REQUIRE(result.value() == 42);
    }
}