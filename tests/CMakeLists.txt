# Catch2 Unit Tests
cmake_minimum_required(VERSION 3.25)

project(CNCS_Tests LANGUAGES CXX)

# Find Catch2
find_package(Catch2 3 REQUIRED)

# Test sources (no duplicates)
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
)

# Create test executable
add_executable(CNCS_Tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(CNCS_Tests PRIVATE
    Catch2::Catch2WithMain
    CNCS
)

# Add test
add_test(NAME CNCS_Tests COMMAND CNCS_Tests)

# Compile options
target_compile_options(CNCS_Tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /Zc:__cplusplus>
)

# Include directories
target_include_directories(CNCS_Tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/util
    ${CMAKE_SOURCE_DIR}/src/core/engine
    ${CMAKE_SOURCE_DIR}/src/core/visibility
)