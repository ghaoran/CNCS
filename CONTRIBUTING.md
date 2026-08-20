# CNCS 贡献指南

感谢你对 CNCS 项目的贡献！请阅读以下指南以确保贡献流程顺利。

---

## 开发环境准备

### 必需工具
- **Visual Studio 2022** (Community/Professional/Enterprise) + C++ 桌面开发工作负载
- **CMake 3.25+**
- **vcpkg** (包管理器)
- **Windows 10 SDK** (10.0.22621+)
- **Windows Driver Kit (WDK)** 10.0.22621+ (驱动开发)

### 克隆与配置
```bash
git clone --recursive https://github.com/your-org/CNCS.git
cd CNCS

# 安装 vcpkg 依赖
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install nlohmann-json zlib fmt spdlog imgui[dx11-binding] catch2 --triplet x64-windows
```

---

## 分支策略

| 分支 | 用途 | 保护规则 |
|------|------|----------|
| `main` | 稳定发布分支 | 仅允许 PR 合并，需通过 CI |
| `develop` | 开发集成分支 | 允许直接推送，需通过 CI |
| `feature/*` | 功能开发分支 | 从 `develop` 创建，合并回 `develop` |
| `hotfix/*` | 热修复分支 | 从 `main` 创建，合并回 `main` 和 `develop` |
| `release/*` | 发布准备分支 | 从 `develop` 创建，合并回 `main` 和 `develop` |

---

## 提交规范

### Commit Message 格式
```
<type>(<scope>): <subject>

<body>

<footer>
```

### Type 类型
| 类型 | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档更新 |
| `style` | 代码格式调整 (不影响逻辑) |
| `refactor` | 重构 (非新功能/非修复) |
| `perf` | 性能优化 |
| `test` | 测试相关 |
| `chore` | 构建/工具/依赖更新 |
| `ci` | CI/CD 配置变更 |

### 示例
```
feat(aimbot): 添加卡尔曼滤波器目标平滑

引入 KalmanFilter<6,3> 对目标位置/速度进行平滑跟踪，
显著减少抖动并提高命中率。

Closes #123
```

---

## Pull Request 流程

1. **从 `develop` 创建特性分支**
   ```bash
   git checkout develop
   git pull origin develop
   git checkout -b feature/your-feature-name
   ```

2. **开发与测试**
   - 编写代码并添加单元测试
   - 运行本地测试：`.\build.ps1 -Configuration Debug -RunTests`
   - 代码格式检查：`clang-format --dry-run --Werror src/`

3. **提交与推送**
   ```bash
   git add .
   git commit -m "feat(scope): your commit message"
   git push origin feature/your-feature-name
   ```

4. **创建 Pull Request**
   - 目标分支：`develop`
   - 填写 PR 模板
   - 等待 CI 通过和代码审查

4. **代码审查清单**
   - [ ] 代码符合 `.clang-format` 规范
   - [ ] 新增功能有单元测试覆盖
   - [ ] 无新增编译警告 ( `/W4` )
   - [ ] 文档同步更新 (如有接口变更)
   - [ ] 无明显性能回退

---

## 代码规范

### C++ 规范 (C++20)
- 使用 `std::` 标准库，避免 `using namespace std;`
- 优先使用 `std::optional`、`std::variant`、`std::expected` (C++23)
- 资源管理使用 RAII (`std::unique_ptr`/`std::shared_ptr`/`std::vector`)
- 错误处理使用 `Result<T, E>` 单子类型，避免异常用于控制流
- 线程安全：优先使用 `std::atomic`、`std::mutex`、`std::condition_variable`

### 命名约定
| 类型 | 规范 | 示例 |
|------|------|------|
| 类/结构体 | PascalCase | `class PlayerManager` |
| 函数/方法 | PascalCase | `bool UpdatePlayer()` |
| 变量/参数 | camelCase | `float maxHealth` |
| 私有成员 | `m_` 前缀 + camelCase | `m_playerList` |
| 常量/枚举 | PascalCase | `const int MaxPlayers` |
| 宏/编译期常量 | UPPER_SNAKE_CASE | `#define MAX_PLAYERS 64` |
| 命名空间 | 小写单数 | `namespace ballistics` |

### 文件组织
```
模块目录/
├── ModuleName.hpp      # 公开接口
├── ModuleName.cpp      # 实现
├── ModuleName_fwd.hpp  # 前置声明 (如需)
└── tests/
    └── test_ModuleName.cpp
```

---

## 测试规范

### 单元测试要求
- **覆盖率目标**：核心模块 ≥ 80%
- **命名规范**：`test_<模块名>.cpp`，测试用例名 `TEST_CASE("功能描述", "[模块]")`
- **测试隔离**：每个测试用例独立，无副作用
- **边界测试**：必须包含边界值、异常输入、空输入测试

### 运行测试
```bash
# 构建并运行所有测试
.\build.ps1 -Configuration Debug -RunTests

# 运行特定测试
ctest --test-dir build -R "test_result" --output-on-failure
```

---

## 文档规范

### 代码注释
```cpp
/// @brief 简短功能描述
/// @param param1 参数说明
/// @return 返回值说明
/// @throws 异常说明 (如适用)
/// @see 相关函数/文档链接
ReturnType FunctionName(ParamType param1);
```

### 架构文档更新
- 重大架构变更需同步更新 `docs/ARCHITECTURE.md`
- 新增公开接口需更新 `docs/API.md` (如存在)

---

## 发布流程

### 版本号规范 (SemVer)
```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]

示例：
1.0.0          - 正式发布
1.1.0-rc.1     - 预发布候选
1.0.1-hotfix.1 - 热修复
```

### 发布步骤
1. 创建 `release/vX.Y.Z` 分支
2. 更新版本号 (`CMakeLists.txt` / `vcpkg.json`)
3. 更新 `CHANGELOG.md`
4. 合并到 `main` 并打 Tag `vX.Y.Z`
5. CI 自动构建并创建 GitHub Release

---

## 行为准则

- 尊重所有贡献者，保持专业和包容
- 拒绝任何形式的骚扰、歧视或人身攻击
- 专注于技术讨论，避免无关争论
- 如遇违规行为，请联系维护者处理

---

## 联系方式

- **Issues**: [GitHub Issues](https://github.com/your-org/CNCS/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-org/CNCS/discussions)
- **Email**: maintainers@cncs.example.com

---

> **注意**：本项目仅供学习研究使用，请勿用于商业用途或破坏游戏公平性。使用风险自负。