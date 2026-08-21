// Core Precompiled Header - Minimal, stable headers used across the entire project
// This replaces the monolithic common.hpp forced include

#pragma once

// Windows SDK configuration
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#define UNICODE
#define _UNICODE

// ImGui configuration
#define IMGUI_DEFINE_MATH_OPERATORS

// Enable M_PI constant in <cmath>
#define _USE_MATH_DEFINES

// Standard library - core headers only
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cfloat>

#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>

#include <vector>
#include <array>
#include <string>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <format>

// Windows headers
#include <windows.h>
#include <winuser.h>
#include <winnt.h>
#include <minwindef.h>
#include <winbase.h>
#include <processthreadsapi.h>
#include <psapi.h>
#include <tlhelp32.h>

// Third-party - stable headers
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/format.h>

// ImGui core (needed by Color.hpp, Vec2.hpp for ImColor/ImVec2/ImVec4)
#include "external/imgui/imgui.h"

// Project-wide type aliases
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using usize = size_t;
using isize = ptrdiff_t;

// Common namespace aliases
namespace fs = std::filesystem;
namespace chr = std::chrono;
using json = nlohmann::json;

// Logging macros (temporary - will migrate to spdlog/fmt)
#include <core/logger/LogHelper.hpp>

// 日志级别宏（供 LOGF 使用）
#define VERBOSE al::eLogLevel::LOG_VERBOSE
#define DEBUG   al::eLogLevel::LOG_DEBUG
#define INFO    al::eLogLevel::LOG_INFO
#define WARNING al::eLogLevel::LOG_WARNING
#define ERROR   al::eLogLevel::LOG_ERROR
#define FATAL   al::eLogLevel::LOG_FATAL

#define LOGF(level, fmt, ...) ::al::LogHelper::Log(level, fmt, ##__VA_ARGS__)

// Compile-time string encryption (kept from common.hpp)
template <size_t N>
struct XorStr {
    char data[N];
    constexpr XorStr(const char (&s)[N], char k) : data{} {
        for (size_t i = 0; i < N; ++i)
            data[i] = s[i] ^ k;
    }
    constexpr operator const char* () const { return data; }
};