# Catch2 单元测试配置
cmake_minimum_required(VERSION 3.25)

project(CNCS_Tests LANGUAGES CXX)

# 启用测试
enable_testing()

# 查找 Catch2
find_package(Catch2 3 REQUIRED)

# 测试源文件
set(TEST_SOURCES
    test_cache.cpp
    test_visibility.cpp
    test_aimbot.cpp
    test_config.cpp
    test_kalman.cpp
    test_ballistics.cpp
    test_result.cpp
    test_highrestimer.cpp
    test_core.cpp
    test_aimbot.cpp
    test_cache.cpp
)

# 创建测试可执行文件
add_executable(CNCS_Tests ${TEST_SOURCES})

# 链接库
target_link_libraries(CNCS_Tests PRIVATE
    Catch2::Catch2WithMain
    CNCS  # 链接主项目以测试内部类
)

# 添加测试
add_test(NAME CNCS_Tests COMMAND CNCS_Tests)

# 编译选项
target_compile_options(CNCS_Tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /Zc:__cplusplus>
)

# 包含目录
target_include_directories(CNCS_Tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/util
    ${CMAKE_SOURCE_DIR}/src/core/engine
    ${CMAKE_SOURCE_DIR}/src/core/visibility
)
