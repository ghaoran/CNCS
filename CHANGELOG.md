# CNCS 变更日志

所有重要变更都会记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

---

## [未发布]

### 新增
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