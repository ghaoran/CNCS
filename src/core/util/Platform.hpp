#pragma once

// -----------------------------------------------------------------------------
// 平台 / 编译器探测。core 模块统一通过本文件获取平台常量：
//   1) __forceinline 在 GCC/Clang 上也有定义（always_inline），不再只在 MSVC 下可用；
//   2) 平台判定只用 CNCS_PLATFORM_WINDOWS，避免散落的 #ifdef _WIN32。
// -----------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
#define CNCS_PLATFORM_WINDOWS 1
#else
#define CNCS_PLATFORM_WINDOWS 0
#endif

#if CNCS_PLATFORM_WINDOWS
#define CNCS_PLATFORM_POSIX 0
#else
#define CNCS_PLATFORM_POSIX 1
#endif

#if defined(_MSC_VER)
// MSVC: __forceinline 是编译器关键字，无需宏定义。
#elif !defined(__forceinline)
#if defined(__GNUC__) || defined(__clang__)
#define __forceinline inline __attribute__((always_inline))
#else
#define __forceinline inline
#endif
#endif
