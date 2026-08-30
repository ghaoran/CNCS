# CNCS 架构文档

## 1. 概述

CNCS (CS2 Neutral Cheat System) 是一个基于 C++20 的 CS2 外部辅助程序，采用 **纯用户态外部读取** 架构（`OpenProcess` + `ReadProcessMemory`），无需内核驱动。

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                    用户态进程 (CNCS.exe)                      │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │   Renderer  │  │   Frontend   │  │      Engine        │  │
│  │  (D3D11)    │  │  (ESP/Aim/   │  │  (Cache/Entity/    │  │
│  │             │  │   Menu)      │  │   Offsets)         │  │
│  └──────┬──────┘  └──────┬───────┘  └─────────┬──────────┘  │
│         │                │                    │             │
│         └────────────────┴────────────────────┘             │
│                          │                                   │
│                 ┌────────▼─────────┐                         │
│                 │ pProcess (用户态) │                          │
│                 └────────┬─────────┘                         │
└──────────────────────────│────────────────────────────────────┘
                           │ Win32 API
                           ▼
              ┌──────────────────────────────┐
              │ OpenProcess                  │ 打开目标进程句柄      │
              │ ReadProcessMemory            │ 只读目标进程内存      │
              │ CreateToolhelp32Snapshot     │ 进程/模块枚举        │
              └──────────────────────────────┘
```

---

## 3. 核心模块

### 3.1 核心引擎 (`src/core/engine/`)

| 文件 | 职责 |
|------|------|
| `Engine.hpp/.cpp` | 主引擎，进程附加、模块枚举、主循环 |
| `Cache.hpp/.cpp` | 双缓冲快照系统，无锁发布-订阅模式 |
| `classes/` | 游戏实体封装 (Player/Bomb/Weapon/Globals/Observer) |
| `types/` | 数学类型 (Vec2/Vec3/Matrix/Color) |

**关键设计**：
- 双缓冲快照：引擎线程构建 `Snapshot`，渲染线程通过 `shared_ptr` 零拷贝读取
- 自适应刷新：根据游戏帧率动态调整刷新间隔 (0.5×帧时间，1-5ms 限幅)
- Result<T,E> 错误处理：所有关键操作返回 `Result<T, cncs_error::Code>`

### 3.2 内存读取 (`src/core/memory/`)

`pProcess` 封装用户态读取：

| API | 用途 | 说明 |
|-----|------|------|
| `OpenProcess` | 打开目标进程句柄 | `PROCESS_VM_READ \| PROCESS_QUERY_INFORMATION` |
| `ReadProcessMemory` | 只读目标进程内存 | 不写游戏内存 |
| `CreateToolhelp32Snapshot` | 进程/模块枚举 | PID 查找、模块基址获取 |

### 3.3 可见性/掩体系统 (`src/core/visibility/`)

| 组件 | 功能 |
|------|------|
| `CollisionMesh` | 加载 .m2/.glb 碰撞网格，BVH 加速射线检测 |
| `Visibility` | 射线-AABB 相交测试，烟雾体素解析转 AABB 缓存 |

**多级加载策略**：外部 .m2 → .m1 → .glb → 内嵌资源 → 目录扫描

### 3.4 自瞄/预测系统 (`src/gui/frontend/aimbot/`)

| 组件 | 功能 |
|------|------|
| `Aimbot` | 主循环、目标选择、平滑移动、鼠标移动 |
| `KalmanFilter<6,3>` | 3D 位置/速度卡尔曼滤波 (常速度模型) |
| `Ballistics` | 弹道预测 (弹道下坠/飞行时间/阻力模型) |
| `RecoilCompensation` | 后坐力模式学习与补偿 |

### 3.5 渲染系统 (`src/gui/renderer/`)

- 解耦渲染线程：`condition_variable` 同步，主线程非阻塞
- ImGui + D3D11 覆盖层绘制
- HighResTimer：`CreateWaitableTimerEx(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION)` ~0.5ms 精度

---

## 4. 核心工具库 (`src/core/util/`)

| 文件 | 功能 |
|------|------|
| `Result.hpp` | `Result<T,E>` 单子类型，支持 `and_then`/`or_else`/`map`/`map_err` |
| `HighResTimer.hpp` | `CreateWaitableTimerEx(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION)` ~0.5ms 精度 |
| `KalmanFilter.hpp` | `KalmanFilter<6,3>` 3D 位置/速度跟踪，常速度模型 |
| `Ballistics.hpp` | 弹道预测、后坐力补偿、多点采样 |

---

## 5. 配置系统 (`src/config/`)

- `Current.hpp`：运行时配置 (150+ inline 变量)
- `Config.hpp/.cpp`：JSON 序列化/反序列化，nlohmann::json 内置转换
- 支持类型：`color_t`、`Vec2_t`、基础类型、嵌套命名空间

---

## 6. 错误处理体系

```cpp
namespace cncs_error {
    enum class Code : int {
        Success = 0,
        InvalidParameter = 1000,
        AccessDenied = 2005,
        ProcessNotFound = 3000,
        MemoryReadFailed = 4000,
        // ...
    };
}
```

所有关键操作返回 `Result<T, cncs_error::Code>`，支持 Monadic 操作：
```cpp
auto result = Engine::Init()
    .and_then([](auto) { return Renderer::Init(); })
    .and_then([](auto) { Renderer::StartRenderThread(); });
```

---

## 7. 构建系统

- **CMake 3.25+** + **vcpkg** 依赖管理
- **预编译头**：`pch_core` (核心) / `pch_engine` (引擎)
- **源码编码**：`/utf-8`（源码为无 BOM UTF-8）
- **安全编译选项**：`/GS /guard:cf /sdl /DYNAMICBASE /NXCOMPAT`
- **输出目录**：`build/bin/{Debug|Release}/CNCS.exe`

---

## 8. 部署要求

| 组件 | 要求 |
|------|------|
| **操作系统** | Windows 10 1903+ / Windows 11 |
| **运行权限** | 与游戏相同或更高权限（游戏以管理员运行时需管理员） |
| **游戏模式** | 全屏窗口化 |
| **依赖库** | vcpkg: nlohmann-json, zlib, fmt, spdlog, imgui, catch2 |

---

## 9. 扩展指南

### 添加新的 ESP 功能
1. 在 `src/gui/frontend/esp/Esp.cpp` 添加渲染逻辑
2. 在 `src/config/Current.hpp` 添加配置项
3. 在 `Config.cpp` 添加序列化/反序列化

### 添加新的自瞄逻辑
1. 在 `src/gui/frontend/aimbot/Aimbot.cpp` 修改 `try_aim` lambda
2. 使用 `KalmanFilter` 平滑目标
3. 使用 `Ballistics::predict_impact` 弹道预测

---

## 10. 故障排查

| 现象 | 可能原因 | 排查步骤 |
|------|----------|----------|
| 打开进程失败 | 权限不足 / 游戏未运行 | 以相同或更高权限运行本程序 |
| 找不到进程 | 游戏未运行 / 进程名错误 | 确认 `cs2.exe` 运行中 |
| 找不到模块 | 游戏未完全加载 | 增加 `AwaitModules` 重试次数 |
| 内存读取失败 | 地址无效 / 句柄失效 | 检查 `ReadProcessMemory` 返回值与目标地址 |
| 渲染闪烁 | 窗口位置同步问题 | 检查 `HandleWindowOrder` 逻辑 |
| 自瞄抖动 | 平滑参数不当 | 调整 `smoothing` / `speed_boost` |
