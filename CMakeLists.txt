# CNCS - CS2 External Overlay
# Unified CMake build system replacing VS Solution + Zig build script
# Requires: CMake 3.25+, Visual Studio 2022 (MSVC v143+), vcpkg

cmake_minimum_required(VERSION 3.25)
project(CNCS
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "CS2 External Overlay with ESP, Aimbot, Triggerbot"
)

# -----------------------------------------------------------------------------
# Policies & Standards
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC-specific settings
if(MSVC)
    # Warning level 4 (equivalent to /W4) but suppress noisy ones
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /Zc:inline /Zc:strictStrings /Zc:throwingNew)
    # Disable specific warnings that are noisy in third-party code
    add_compile_options(
        /wd4201   # nameless struct/union (common in Windows headers)
        /wd4244   # conversion loss of data (intentional in math)
        /wd4267   # size_t to smaller type (intentional)
        /wd4996   # deprecated CRT functions (we use secure versions where needed)
        /wd4820   # padding added (struct alignment)
        /wd4710   # function not inlined
        /wd4711   # function selected for inlining
        /wd5045   # Spectre mitigation (we handle separately)
    )
    # Security hardening
    add_compile_options(/GS /guard:cf /sdl /DYNAMICBASE)
    add_link_options(/DYNAMICBASE /NXCOMPAT /GUARD:CF)
endif()

# -----------------------------------------------------------------------------
# Build Configurations
# -----------------------------------------------------------------------------
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# Debug: /MDd = debug CRT (defines _DEBUG automatically), /Zi = PDB, /Ob0 = no inline, /Od = no opt, /RTC1 = runtime checks
set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1 /D_DEBUG")

# Release: /MD = release CRT, /O2 = max speed, /Ob2 = inline any, /GL = LTCG, /DNDEBUG = no asserts
set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /GL /DNDEBUG")

# Whole Program Optimization for Release
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}")

# -----------------------------------------------------------------------------
# vcpkg Integration
# -----------------------------------------------------------------------------
# Expected vcpkg packages (see vcpkg.json):
#   - nlohmann-json
#   - zlib
#   - fmt

# Toolchain file should be passed via -DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake
if(NOT DEFINED ENV{VCPKG_ROOT} AND NOT CMAKE_TOOLCHAIN_FILE)
    message(WARNING "vcpkg not detected. Set VCPKG_ROOT or pass -DCMAKE_TOOLCHAIN_FILE")
endif()

# -----------------------------------------------------------------------------
# Source File Groups (for IDE organization)
# -----------------------------------------------------------------------------
# Explicit source file list - avoids picking up imgui examples, json tests, etc.
set(CNCS_SOURCES
    src/common.cpp
    src/config/Config.cpp
    src/core/engine/Engine.cpp
    src/core/engine/cache/Cache.cpp
    src/core/engine/classes/Bomb.cpp
    src/core/engine/classes/Game.cpp
    src/core/engine/classes/Globals.cpp
    src/core/engine/classes/ObserverServices.cpp
    src/core/engine/classes/Player.cpp
    src/core/engine/classes/Weapon.cpp
    src/core/kernel/KdLoader.cpp
    src/core/logger/LogHelper.cpp
    src/core/memory/Memory.cpp
    src/core/offsets/Dumper.cpp
    src/core/visibility/CollisionMesh.cpp
    src/core/visibility/Visibility.cpp
    src/external/imgui/imgui.cpp
    src/external/imgui/imgui_demo.cpp
    src/external/imgui/imgui_draw.cpp
    src/external/imgui/imgui_tables.cpp
    src/external/imgui/imgui_widgets.cpp
    src/external/imgui/backends/imgui_impl_win32.cpp
    src/external/imgui/backends/imgui_impl_dx11.cpp
    src/external/AsyncLogger/src/Logger.cpp
    src/external/AsyncLogger/src/LogCapture.cpp
    src/external/AsyncLogger/src/LogStream.cpp
    src/external/AsyncLogger/src/LogIntermediate.cpp
    src/external/AsyncLogger/src/LogMessage.cpp
    src/external/AsyncLogger/src/LogLevel.cpp
    src/gui/frontend/aimbot/Aimbot.cpp
    src/gui/frontend/esp/Esp.cpp
    src/gui/frontend/menu/Menu.cpp
    src/gui/frontend/overlays/Overlays.cpp
    src/gui/GuiUtils.cpp
    src/gui/renderer/Renderer.cpp
    src/gui/renderer/window/Window.cpp
    src/main.cpp
    src/pch_core.cpp
)

file(GLOB_RECURSE CNCS_HEADERS
    "src/*.hpp"
    "src/*.h"
    "src/**/*.hpp"
    "src/**/*.h"
)

source_group("Source Files" FILES ${CNCS_SOURCES})
source_group("Header Files" FILES ${CNCS_HEADERS})

# -----------------------------------------------------------------------------
# External Dependencies (via vcpkg)
# -----------------------------------------------------------------------------
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)
find_package(fmt REQUIRED)

# ImGui - we use our bundled version (src/external/imgui)

# -----------------------------------------------------------------------------
# Main Executable Target
# -----------------------------------------------------------------------------
add_executable(CNCS
    ${CNCS_SOURCES}
    ${CNCS_HEADERS}
    "src/resource.rc"
)

target_compile_definitions(CNCS PRIVATE
    _CONSOLE
    IMGUI_DEFINE_MATH_OPERATORS
    _WIN32_WINNT=0x0A00
    NOMINMAX
    UNICODE
    _UNICODE
)

# Include directories
target_include_directories(CNCS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external/AsyncLogger/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external/AsyncLogger/include/AsyncLogger
)

# Precompiled Headers
set(PCH_CORE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/pch_core.hpp")
set(PCH_CORE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/pch_core.cpp")

# Use target_precompile_headers (CMake 3.16+) for core PCH
target_precompile_headers(CNCS PRIVATE
    ${PCH_CORE_HEADER}
)

# Set the PCH header for the target
set_target_properties(CNCS PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Link libraries
target_link_libraries(CNCS PRIVATE
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
    fmt::fmt
    # System libraries
    d3d11
    dxgi
    d3dcompiler
    dwmapi
    gdi32
    user32
    imm32
    ole32
    ws2_32
    wldap32
    crypt32
    normaliz
    advapi32
    bcrypt
    setupapi
)

# MSVC-specific link options
if(MSVC)
    target_link_options(CNCS PRIVATE
        /SUBSYSTEM:CONSOLE
        /DEBUG:FULL
        /OPT:REF
        /OPT:ICF
    )
endif()

# -----------------------------------------------------------------------------
# Driver Project (Kernel Driver)
# -----------------------------------------------------------------------------
# Note: Kernel drivers require WDK + special CMake setup.
# This is a placeholder - actual driver build uses WDK build system.
# See driver/CNCS_drv.vcxproj for current driver build.

# -----------------------------------------------------------------------------
# Custom Targets
# -----------------------------------------------------------------------------
# Copy embedded collision meshes to output directory (only if maps/ exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/maps")
    file(GLOB MAP_GLB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/maps/*.glb")
    file(GLOB MAP_COLMESH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/maps/*.colmesh")
    if(MAP_GLB_FILES OR MAP_COLMESH_FILES)
        add_custom_command(TARGET CNCS POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CNCS>/maps"
        )
        foreach(f ${MAP_GLB_FILES} ${MAP_COLMESH_FILES})
            add_custom_command(TARGET CNCS POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${f}" "$<TARGET_FILE_DIR:CNCS>/maps/"
            )
        endforeach()
    endif()
endif()

# Copy driver .sys to output directory (built separately, only if exists)
set(DRIVER_SYS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/driver/x64/${CMAKE_BUILD_TYPE}/CNCS_drv.sys")
if(NOT EXISTS "${DRIVER_SYS_PATH}")
    set(DRIVER_SYS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/driver/x64/Release/CNCS_drv.sys")
endif()
if(EXISTS "${DRIVER_SYS_PATH}")
    add_custom_command(TARGET CNCS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DRIVER_SYS_PATH}"
            "$<TARGET_FILE_DIR:CNCS>/CNCS_drv.sys"
        COMMENT "Copying kernel driver to output directory"
    )
else()
    message(WARNING "Driver not found at ${DRIVER_SYS_PATH}, skipping driver copy")
endif()

# Copy driver .sys to output directory (built separately)
add_custom_command(TARGET CNCS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/driver/x64/Release/CNCS_drv.sys"
        "$<TARGET_FILE_DIR:CNCS>/CNCS_drv.sys"
    COMMENT "Copying kernel driver to output directory"
)

# -----------------------------------------------------------------------------
# Installation (optional)
# -----------------------------------------------------------------------------
install(TARGETS CNCS
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/maps")
    install(DIRECTORY maps/
        DESTINATION maps
        FILES_MATCHING PATTERN "*.glb" PATTERN "*.colmesh"
    )
endif()

# -----------------------------------------------------------------------------
# Testing (Catch2)
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(Catch2 3 REQUIRED)
    
    add_executable(CNCS_tests
        tests/test_cache.cpp
        tests/test_visibility.cpp
        tests/test_aimbot.cpp
        tests/test_config.cpp
        tests/test_kalman.cpp
        tests/test_ballistics.cpp
        tests/test_result.cpp
        tests/test_highrestimer.cpp
        tests/test_core.cpp
    )
    target_link_libraries(CNCS_tests PRIVATE Catch2::Catch2WithMain CNCS)
    add_test(NAME CNCS_tests COMMAND CNCS_tests)
endif()

# -----------------------------------------------------------------------------
# CPack for packaging
# -----------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "CNCS")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CS2 External Overlay")
set(CPACK_GENERATOR "ZIP")
include(CPack)

# -----------------------------------------------------------------------------
# Helper: Print build summary
# -----------------------------------------------------------------------------
message(STATUS "==========================================")
message(STATUS "CNCS Build Configuration")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Generator: ${CMAKE_GENERATOR}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Output: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "==========================================")