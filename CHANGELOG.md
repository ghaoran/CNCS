# CNCS 变更日志

所有重要变更都会记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

---

## [未发布]

### 修复（构建与 CI）
- 修复 CI 必失败根因：CMakeLists.txt 把可执行目标 `CNCS` 用 `target_link_libraries` 链接进测试，
  CMake 在 configure 阶段直接报 `Target "CNCS" of type EXECUTABLE may not be linked into another target`，
  Debug job 100% 失败、release job 因 `needs: build` 永不产出。现抽出 `CNCS_Core` 静态库供测试链接。
- 修复 release 流水线从未触发：`release` job 条件为 `refs/tags/*`，但 `on.push` 只监听分支。
  现补 `tags: ['v*', '[0-9]*']` 与 `workflow_dispatch`。
- 修复 release 产物互相覆盖：download-artifact 的 `pattern: CNCS-*` 会把 Debug 一起合并，
  现改为只取 `CNCS-Release`；并补 `actions/checkout`（打包需要读 `src/embedded/*.m2`）。
- 修复 `build.ps1` 项目根解析错误：脚本本身在仓库根，却又 `Resolve-Path "$ScriptDir\.."`，
  导致 `-S` 指向仓库父目录、找不到 CMakeLists.txt。
- 修复 `tests/CMakeLists.txt` 为第二套失效构建描述（未被引用、同样把 exe 当库链接、引用不存在目录），
  改为转发到根项目的权威定义。
- 修复 `.clang-format` 无法解析：重复键 `PointerAlignment` + 非法键 `TemplateDepth`，
  导致 clang-format 拒绝读取整个配置、格式检查形同虚设。格式检查同时改为只校验改动文件。
- 修复 `/MT /MTd /GL /LTCG` 等 MSVC 专属编译选项被无条件施加，污染非 MSVC 构建。
- 修复资源/打包步骤在无 `maps/` 目录时的无效 `add_custom_command(TARGET CNCS ...)` 调用。
- 新增 Linux `portable` CI job：只构建 `CNCS_Core` + 单元测试，防止 core 再次悄悄引入 Win32 依赖。

### 修复（代码缺陷）
- 修复 `KalmanFilter` 观测矩阵 `H_` 维度声明写反（声明 `[StateDim][MeasDim]`，代码按
  `[MeasDim][StateDim]` 使用），造成 4 处越界读取 UB（GCC `-Waggressive-loop-optimizations` 报出），
  并直接污染速度估计。
- 修复 `Ballistics::predict_impact` 单位混用：把「米」与「units/s」混算，飞行时间被放大约 39 倍
  并被 `max_time` 截断，导致弹道预测与下坠补偿完全失真。现统一用游戏单位，并修正
  `bullet_drop` 符号语义（正数=下坠量）与瞄点补偿方向。
- 修复 `Result<void, E>::and_then` 只接受 `Result<void>` 的签名错误（应为任意 `Result<U, E>`），
  并补齐 `and_then &&` / `or_else` / `map_err`；两处恒真 `static_assert` 改为真实约束 `is_result_v`。
- 修复 `HighResTimer` 硬依赖 `<windows.h>`：改为 Windows 高精度可等待定时器 / POSIX
  `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` 双实现，接口不变。
- 修复 `Result`/`Config`/`Visibility` 等 core 文件隐含依赖 PCH 注入的 `<fstream>`、`<iomanip>`、
  `<windows.h>`、`WIN32_LEAN_AND_MEAN`，脱离 PCH 即编译失败的问题。
- 修复 `Vec3_t` 使用 `__forceinline` 而没有非 MSVC 定义（新增 `Platform.hpp` 统一提供）。
- 修复 `color_t` / `Vec2_t` 通过隐式包含 `imgui.h` 才拿到 `ImVec2/ImColor/ImVec4`，
  造成 core 结构头对 GUI 库的隐蔽硬依赖；ImGui 互操作改为 `IMGUI_VERSION` 条件编译。

### 新增（可移植层）
- `core/util/Platform.hpp`：平台与编译器探测、`__forceinline` 跨编译器定义。
- `core/util/PathUtils.hpp`：`util::ExeDir()` / `util::ExePath()`，Windows 用 `GetModuleFileNameA`、
  POSIX 用 `/proc/self/exe`，替换两处各自重复实现的 Windows-only 版本。
- `core/util/Log.hpp`：`CNCS_CORE_LOG(...)` 日志门面，可切换到 AsyncLogger，也可零依赖直写 stdout/stderr。

- 移除内核驱动依赖：内存读取改为用户态实现 (OpenProcess + ReadProcessMemory + CreateToolhelp32Snapshot)
- 删除 src/core/kernel (KdLoader) 与驱动错误码 (DriverLoadFailed 等)
- 删除 driver/ 目录、CI 驱动构建步骤；架构文档重写为用户态版本
- 添加完整的单元测试套件 (Result、KalmanFilter、Ballistics、HighResTimer、Visibility、Config、Core)
- 添加 GitHub Actions CI/CD 流水线
- 添加代码风格配置 (.clang-format, .editorconfig)
- 添加贡献指南 (CONTRIBUTING.md)
- 添加架构文档 (docs/ARCHITECTURE.md)
- 添加完整的单元测试套件 (9 个测试文件)
- 添加代码格式化配置
- 重构 Config 系统，使用 nlohmann::json 内置转换
- 修复 Esp.cpp 中观察者模式异常 (target 为 -1 时的边界检查)
- 移除 "using namespace std" 污染
- 移除所有 TODO/FIXME 注释
- 重构 Config JSON 序列化，使用 nlohmann::json 内置 get<color_t/Vec2_t>()
- 为 color_t 和 Vec2_t 添加 nlohmann::json 适配器 (to_json/from_json)
- 移除 "using namespace std" 污染，仅保留 ballistics 命名空间别名

### 修复
- 修复 Esp.cpp 中观察者模式异常：target 为 -1 时增加边界检查
- 修复 Dumper::ScanBlock 未判空 Engine::GetProcess() 的潜在空指针解引用
- 修复 Globals::max_clients 无上限导致 Cache::RefreshImpl reserve/循环失控的崩溃风险 (clamp 0..64)
- 修复 Renderer 函数级 static std::thread 在退出时 joinable 析构触发 std::terminate 的风险 (移至文件级并在 DestroyImpl join)
- 修复 isRunning/isOpen/isFocused/vsync/shouldRun 跨线程非原子访问 (改用 std::atomic<bool>；atomic 间赋值需显式 load)
- 修复 Cache 烟雾缓存满 64 条后新烟雾无法入缓存 (满时淘汰最旧条目)
- 修复 Cache/Player 从字节数组按指针读取 int32/float 的未对齐访问 UB (改用 memcpy)
- 修复 Player 构造函数 int→int8_t 窄化警告 (C4244)
- 修复 Engine 等待进程/模块时的固定 5s 睡眠，改为指数退避 (1s→5s)
- 修复 KdLoader 栈缓冲区溢出风险 (4KB 栈缓冲 → std::vector)
- 修复驱动卸载竞态条件 (显式等待引用计数归零)
- 修复驱动内存分配失败时的重试逻辑
- 修复驱动内存泄漏风险 (ExAllocatePool2 失败路径)
- 修复 Config 解析异常处理
- 修复 Ballistics.hpp 中 "using namespace std" 污染

### 改进
- 进程/模块等待从固定 5s 改为指数退避 (1s→2s→3s... 最大 5s)
- 驱动内存分配失败时改为重试而非直接失败
- 栈缓冲区 4KB → std::vector 防止栈溢出
- 双缓冲快照无锁发布 (atomic shared_ptr)
- 指数退避重试机制
- Result<T,E> 单子类型错误处理
- 三级预编译头 (core/engine/gui)
- CMake + vcpkg 统一构建系统

### 移除
- 移除 src/common.hpp 强制包含
- 移除 scripts/ 旧构建脚本
- 移除 src/external/imgui, json, timer, zlib, lib (统一 vcpkg 管理)
- 移除所有 TODO/FIXME 注释
- 移除 "using namespace std" 污染
- 清理构建产物 (x64/, .obj, .pdb, .tlog, .vcxproj.user 等)
- 移除 cs2-external-esp.sln 旧方案文件

---

## [1.0.0] - 2024-08-20

### 新增
- 初始版本发布
- CS2 外部辅助基础功能 (ESP/自瞄/触发机器人)
- 内核驱动内存读取
- 掩体判断系统
- 配置系统
- 渲染系统

---

## 版本规范

版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)：`MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]`

| 版本类型 | 示例 | 说明 |
|----------|------|------|
| 正式发布 | 1.0.0 | 稳定版本 |
| 预发布 | 1.1.0-rc.1 | 发布候选 |
| 热修复 | 1.0.1-hotfix.1 | 紧急修复 |
| 内部构建 | 1.0.0+build.123 | CI 内部版本 |

---

## 发布检查清单

- [ ] 更新版本号 (CMakeLists.txt, vcpkg.json)
- [ ] 更新 CHANGELOG.md
- [ ] 运行完整测试套件
- [ ] 代码格式检查通过
- [ ] 静态分析通过
- [ ] Driver Verifier 压测通过
- [ ] 生成发布包
- [ ] 创建 Git Tag
- [ ] 发布 GitHub Release