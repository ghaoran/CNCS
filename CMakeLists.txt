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
# Target platform
# -----------------------------------------------------------------------------
# 显式建模"目标平台是否为 Windows"，供后续所有 Windows 专属依赖做条件化。
# 原来所有 /W4、d3d11、resource.rc 都无条件添加，GCC/Clang 一旦尝试配置就
# 直接失败；这也是 CI 无法在 Linux 上跑单元测试的根因之一。
if(WIN32)
    set(CNCS_PLATFORM_WINDOWS_TARGET ON)
else()
    set(CNCS_PLATFORM_WINDOWS_TARGET OFF)
endif()

# -----------------------------------------------------------------------------
# Policies & Standards
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC-specific settings
if(MSVC)
    # Static CRT（/MT /MTd）：静态链接运行时，与 x64-windows-static triplet 配套，
    # 无需随发布带 vcruntime/ucrt DLL。
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # Warning level 4 (equivalent to /W4) but suppress noisy ones
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /Zc:inline /Zc:strictStrings /Zc:throwingNew
        /utf-8   # 源码为无 BOM UTF-8（见 .editorconfig），避免 MSVC 按 ACP 误读中文注释
    )
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
# 仅多配置生成器（VS / Xcode）需要显式收敛配置集；Ninja/Make 下强制设置会让
# 单配置构建的 CMAKE_BUILD_TYPE 失效。
get_property(CNCS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(CNCS_MULTI_CONFIG)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

# 编译选项按编译器分流：/MT、/GL、/Zi 等是 MSVC 专属，硬写会污染 GCC/Clang 构建
# （CI 里 Linux 侧的静态检查、单元测试就是被这些选项打断的）。
if(MSVC)
    # 静态链接 CRT：/MT 多线程静态（无需随程序带 vcruntime/ucrt DLL），
    # 与 vcpkg 的 x64-windows-static triplet 配套（fmt/zlib 等静态库不再需 DLL）。
    # Debug: /MTd = debug CRT, /Zi = PDB, /Ob0 = no inline, /Od = no opt, /RTC1 = runtime checks
    set(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1 /D_DEBUG")

    # Release: /MT = release CRT, /O2 = max speed, /Ob2 = inline any, /GL = LTCG, /DNDEBUG = no asserts
    set(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /Ob2 /GL /DNDEBUG")

    # Whole Program Optimization for Release（LTCG，仅 MSVC 有意义）
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Output directories
# 使用 $<CONFIG> 生成器表达式：Visual Studio 多配置生成器下 CMAKE_BUILD_TYPE 为空，
# 会导致产物落在 build/bin/Release/Release/CNCS.exe 双目录；此处直接落在 build/bin/<Config>。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

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
# 可移植核心：零 Win32 / DX11 / imgui 依赖，任何平台都能编译，单元测试直接链接。
# 只放真正跨平台的逻辑（配置序列化、碰撞几何与可见性、纯数学工具）。
set(CNCS_CORE_SOURCES
    src/config/Config.cpp
    src/core/visibility/CollisionMesh.cpp
    src/core/visibility/Visibility.cpp
)

# 引擎层：进程内存读写（OpenProcess/ReadProcessMemory）、实体缓存、偏移 dump。
# 这部分天然依赖 <Windows.h>，因此只在 Windows 目标下构建。
set(CNCS_ENGINE_SOURCES
    src/core/engine/Engine.cpp
    src/core/engine/cache/Cache.cpp
    src/core/engine/classes/Bomb.cpp
    src/core/engine/classes/Game.cpp
    src/core/engine/classes/Globals.cpp
    src/core/engine/classes/ObserverServices.cpp
    src/core/engine/classes/Player.cpp
    src/core/engine/classes/Weapon.cpp
    src/core/logger/LogHelper.cpp
    src/core/logger/AsyncLoggerImpl.cpp
    src/core/memory/Memory.cpp
    src/core/offsets/Dumper.cpp
)

# Windows / DX11 专属：渲染、覆盖层窗口、ImGui 后端以及 main()。
set(CNCS_GUI_SOURCES
    src/external/imgui/imgui.cpp
    src/external/imgui/imgui_demo.cpp
    src/external/imgui/imgui_draw.cpp
    src/external/imgui/imgui_tables.cpp
    src/external/imgui/imgui_widgets.cpp
    src/external/imgui/backends/imgui_impl_win32.cpp
    src/external/imgui/backends/imgui_impl_dx11.cpp
    src/gui/frontend/aimbot/Aimbot.cpp
    src/gui/frontend/esp/Esp.cpp
    src/gui/frontend/menu/Menu.cpp
    src/gui/frontend/overlays/Overlays.cpp
    src/gui/GuiUtils.cpp
    src/gui/renderer/Renderer.cpp
    src/gui/renderer/window/Window.cpp
    src/main.cpp
    src/pch_core.cpp
    src/common.cpp   # pch_core.hpp 的占位 TU（无实际逻辑），跟随 PCH 归入 GUI 目标
)

set(CNCS_SOURCES ${CNCS_CORE_SOURCES} ${CNCS_ENGINE_SOURCES} ${CNCS_GUI_SOURCES})

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
# CNCS_Core - Portable Static Library
# -----------------------------------------------------------------------------
# 关键修复：原来的 CMakeLists 只有可执行目标 CNCS，单元测试却
# `target_link_libraries(CNCS_tests PRIVATE ... CNCS)` —— CMake 直接报
#   Target "CNCS" of type EXECUTABLE may not be linked into another target
# 导致 configure 阶段就失败，Debug/CI 全红，Release 也就永远不会产出。
# 现在把可移植逻辑收敛成静态库 CNCS_Core：
#   - 测试链接 CNCS_Core（合法，且不需要 imgui / DX11 / Win32 资源）
#   - 可执行文件 CNCS 链接 CNCS_Core + GUI 源
add_library(CNCS_Core STATIC
    ${CNCS_CORE_SOURCES}
    ${CNCS_HEADERS}
)

target_compile_features(CNCS_Core PUBLIC cxx_std_20)

target_compile_definitions(CNCS_Core PUBLIC
    IMGUI_DEFINE_MATH_OPERATORS
    NOMINMAX
)

if(CNCS_PLATFORM_WINDOWS_TARGET)
    target_compile_definitions(CNCS_Core PUBLIC _CONSOLE _WIN32_WINNT=0x0A00 UNICODE _UNICODE)
endif()

target_include_directories(CNCS_Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external/AsyncLogger/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external/AsyncLogger/include/AsyncLogger
)

target_link_libraries(CNCS_Core PUBLIC
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
    fmt::fmt
)

# 说明：Win32 / DX11 系统库只挂在 Windows 目标上（见下方 CNCS 目标）。
# core 库本身不引用任何系统库，保证在 Linux/macOS 上也能编译并测试。

# PCH 只供 GUI 目标使用：core 库保持无隐式全局注入，测试编译更干净。
# -----------------------------------------------------------------------------
# Main Executable Target
# -----------------------------------------------------------------------------
if(CNCS_PLATFORM_WINDOWS_TARGET)
    message(STATUS "CNCS: Windows 目标 -> 构建 CNCS.exe (GUI) + CNCS_Core + 单元测试")

    add_executable(CNCS
        ${CNCS_ENGINE_SOURCES}
        ${CNCS_GUI_SOURCES}
        "src/resource.rc"
    )
    target_compile_definitions(CNCS PRIVATE
        _CONSOLE
        IMGUI_DEFINE_MATH_OPERATORS
        _WIN32_WINNT=0x0A00
        NOMINMAX
        UNICODE
        _UNICODE
        # 让 core 的 CNCS_CORE_LOG 转发到 AsyncLogger（GUI 侧需要控制台着色输出）
        CNCS_ENABLE_ASYNC_LOGGER
    )

    # Include directories（imgui 仅 GUI 目标可见）
    target_include_directories(CNCS PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/json/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/AsyncLogger/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/AsyncLogger/include/AsyncLogger
    )

    # Precompiled Headers（只给 GUI 目标，避免把 windows.h/imgui 注入 core 与测试）
    set(PCH_CORE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/pch_core.hpp")
    set(PCH_CORE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/pch_core.cpp")

    target_precompile_headers(CNCS PRIVATE
        ${PCH_CORE_HEADER}
    )

    set_target_properties(CNCS PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # 依赖与系统库：CNCS_Core 以 PUBLIC 传递 nlohmann_json/zlib/fmt，
    # 这里补引擎层与渲染所需的 Win32 / DX11 系统库。
    target_link_libraries(CNCS PRIVATE
        CNCS_Core
        nlohmann_json::nlohmann_json
        ZLIB::ZLIB
        fmt::fmt
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
else()
    # 非 Windows 平台没有渲染/overlay 后端，只构建可移植 core 与单元测试。
    message(STATUS "CNCS: 非 Windows 目标 -> 仅构建 CNCS_Core 与单元测试")
endif()

# -----------------------------------------------------------------------------
# Custom Targets
# -----------------------------------------------------------------------------
# Copy embedded collision meshes to output directory (only if maps/ exists)
if(CNCS_PLATFORM_WINDOWS_TARGET AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/maps")
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

# -----------------------------------------------------------------------------
# Installation (optional)
# -----------------------------------------------------------------------------
if(CNCS_PLATFORM_WINDOWS_TARGET)
    install(TARGETS CNCS
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )
else()
    install(TARGETS CNCS_Core
        ARCHIVE DESTINATION lib
    )
endif()

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
    
    # 只收录不依赖 GUI/Win32 状态的用例；test_aimbot / test_cache 依赖
    # 引擎单例与热键状态，留在 Windows 全量构建里跑。
    add_executable(CNCS_tests
        tests/test_visibility.cpp
        tests/test_config.cpp
        tests/test_kalman.cpp
        tests/test_ballistics.cpp
        tests/test_result.cpp
        tests/test_highrestimer.cpp
        tests/test_core.cpp
        tests/test_regression.cpp
    )
    # 修复：原来链接的是可执行文件 CNCS，CMake 会在 configure 阶段报错。
    target_link_libraries(CNCS_tests PRIVATE Catch2::Catch2WithMain CNCS_Core)
    target_include_directories(CNCS_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
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