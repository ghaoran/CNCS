# CNCS 架构文档

## 1. 概述

CNCS (CS2 Neutral Cheat System) 是一个基于 C++20 的 CS2 外部辅助程序，采用 **用户态外部 + 内核驱动只读** 的混合架构。

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
│                    ┌─────▼─────┐                              │
│                    │ KdLoader  │  用户态驱动加载器             │
│                    └─────┬─────┘                              │
└─────────────────────────│─────────────────────────────────────┘
                          │ DeviceIoControl
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  内核驱动 (CNCS_drv.sys)                      │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ IOCTL_CNCS_READ_MEMORY      │ MmCopyVirtualMemory 只读   │ │
│  ├─────────────────────────────────────────────────────────┤ │
│  │ IOCTL_CNCS_GET_BASE_MODULE  │ PEB 遍历获取模块基址       │ │
│  ├─────────────────────────────────────────────────────────┤ │
│  │ IOCTL_CNCS_FIND_PROCESS     │ ZwQuerySystemInformation  │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 核心模块

### 3.1 核心引擎 (`src/core/engine/`)

| 文件 | 职责 |
|------|------|
| `Engine.hpp/.cpp` | 主引擎，驱动加载、进程附加、模块枚举、主循环 |
| `Cache.hpp/.cpp` | 双缓冲快照系统，无锁发布-订阅模式 |
| `classes/` | 游戏实体封装 (Player/Bomb/Weapon/Globals/Observer) |
| `types/` | 数学类型 (Vec2/Vec3/Matrix/Color) |

**关键设计**：
- 双缓冲快照：引擎线程构建 `Snapshot`，渲染线程通过 `shared_ptr` 零拷贝读取
- 自适应刷新：根据游戏帧率动态调整刷新间隔 (0.5×帧时间，1-5ms 限幅)
- Result<T,E> 错误处理：所有关键操作返回 `Result<T, cncs_error::Code>`

### 3.2 内核驱动 (`driver/CNCS_drv.c`)

**三个 IOCTL**：
| IOCTL | 功能 | 安全措施 |
|-------|------|----------|
| `IOCTL_CNCS_READ_MEMORY` | `MmCopyVirtualMemory` 读取目标进程内存 | `ProbeForRead`、`__try/__except`、地址范围检查、整数溢出检查 |
| `IOCTL_CNCS_GET_BASE_MODULE` | PEB 遍历获取模块基址 | `KeStackAttachProcess`、`__try/__finally`、输入验证 |
| `IOCTL_CNCS_FIND_PROCESS` | 枚举进程查找 PID | `ProbeForRead`、缓冲区限制 1MB、分配重试 |

**安全特性**：
- 自定义 SDDL：`D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;LS)` 仅 SYSTEM+Administrators
- ETW 审计日志：驱动加载/卸载、设备打开/关闭、内存读取、模块查询
- 卸载安全：显式等待设备引用计数归零

### 3.3 驱动加载器 (`src/core/kernel/KdLoader.cpp`)

- SCM 服务管理：安装/启动/停止/删除驱动服务
- IOCTL 封装：`ReadMemory`/`GetModuleBase`/`FindProcess`
- 栈安全：统一使用 `std::vector` 替代栈缓冲区

### 3.4 可见性/掩体系统 (`src/core/visibility/`)

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
| `Result.hpp` | `Result<T,E>` 单子类型，Monadic 操作 |

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
        DriverLoadFailed = 2001,
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
- **三级预编译头**：`pch_core` (核心) → `pch_engine` (引擎) → `pch_gui` (GUI)
- **安全编译选项**：`/GS /guard:cf /sdl /DYNAMICBASE /NXCOMPAT`
- **输出目录**：`build/bin/{Debug|Release}/CNCS.exe`

---

## 8. 部署要求

| 组件 | 要求 |
|------|------|
| **操作系统** | Windows 10 1903+ / Windows 11 |
| **运行权限** | 管理员权限 |
| **驱动签名** | 测试模式 或 EV 证书签名 |
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

### 添加新的驱动 IOCTL
1. 在 `src/ioctl_defs.h` 定义新 IOCTL 码和结构体
2. 在 `driver/CNCS_drv.c` 实现 Handler
3. 在 `KdLoader` 添加封装方法
4. 更新 SDDL 如需权限变更

---

## 10. 故障排查

| 现象 | 可能原因 | 排查步骤 |
|------|----------|----------|
| 驱动加载失败 | 测试模式未开启 / 签名无效 | `bcdedit /set testsigning on` 重启 |
| 找不到进程 | 游戏未运行 / 进程名错误 | 确认 `cs2.exe` 运行中 |
| 找不到模块 | 游戏未完全加载 | 增加 `AwaitModules` 重试次数 |
| 内存读取失败 | 地址无效 / 权限不足 | 检查 `ProbeForRead` / 地址范围 |
| 渲染闪烁 | 窗口位置同步问题 | 检查 `HandleWindowOrder` 逻辑 |
| 自瞄抖动 | 平滑参数不当 | 调整 `smoothing` / `speed_boost` |