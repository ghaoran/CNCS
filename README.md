# 🕹️ CNCS — CS2 External Overlay

CS2 外部辅助工具（汉化版）。包含 ESP 透视、自瞄（鼠标模拟，不写内存）、自动扳机、雷达、掩体判断等功能。

> ⚠️ **仅供学习研究使用，请勿用于商业用途或破坏游戏公平性。使用风险自负。**

---

## 🎯 功能特性

### 🎮 核心功能
- **ESP 透视**：方框、骨骼、血条、护甲、姓名、武器、弹药、状态标志
- **自瞄系统**：可配置瞄准部位（头/颈/胸/骨盆）、FOV 范围、平滑度、智能部位选择
- **弹道预测**：子弹下坠/飞行时间/空气阻力模型、卡尔曼滤波目标平滑
- **后坐力补偿**：学习后坐力模式、自动补偿
- **自动扳机**：准星对准敌人自动开火、可配置触发键/可见性检查
- **掩体判断**：真·视线检测（地图碰撞网格 + 烟雾体素 AABB）
- **世界功能**：观战列表、炸弹计时、雷达、十字准星、速度图表

### 🛡️ 安全与隐蔽
- **内核驱动只读**：仅 `MmCopyVirtualMemory` 读取内存，不写游戏内存
- **完整性校验**：代码段 CRC32、关键函数哈希、数据哨兵值
- **反调试检测**：PEB BeingDebugged、NtGlobalFlag、硬件断点、时间检测
- **内存保护**：关键数据 Guard Pages、敏感数据零化、关键代码段运行时加密
- **通信加密**：IOCTL 动态码、请求/响应 AES-GCM 加密、ECDH 会话密钥

### ⚡ 性能优化
- **双缓冲快照**：引擎线程构建，渲染线程零拷贝读取 (`shared_ptr` 无锁发布)
- **自适应缓存刷新**：根据游戏帧率动态调整 (0.5×帧时间，1-5ms 限幅)
- **渲染线程解耦**：`condition_variable` 同步，主线程非阻塞
- **高精度定时器**：`CreateWaitableTimerEx(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION)` ~0.5ms 精度
- **指数退避重试**：进程/模块等待从固定 5s 改为 1s→5s 指数退避

---

## 🚀 快速开始

### 环境要求
- **Windows 10 1903+** / **Windows 11**
- **Visual Studio 2022** (Community/Professional/Enterprise) + C++ 桌面开发工作负载
- **CMake 3.25+**
- **vcpkg** (包管理器)
- **Windows 10 SDK** (10.0.22621+)
- **Windows Driver Kit (WDK)** 10.0.22621+ (驱动开发)

### 依赖安装
```bash
# 克隆项目
git clone --recursive https://github.com/your-org/CNCS.git
cd CNCS

# 安装 vcpkg 依赖
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install nlohmann-json zlib fmt spdlog imgui[dx11-binding] catch2 --triplet x64-windows
```

### 构建项目
```powershell
# 主程序构建 (Release + 测试)
.\build.ps1 -Configuration Release -RunTests

# Debug 构建
.\build.ps1 -Configuration Debug -RunTests

# 或使用批处理脚本
.\build.bat Release
```

### 驱动构建与签名
```powershell
# 测试签名模式 (需 bcdedit /set testsigning on + 重启)
.\driver\build_driver.bat Release testsign

# 生产签名 (需 EV 证书)
.\driver\build_driver.bat Release sign certthumbprint=<拇指印>
```

---

## 🎮 运行要求

1. **游戏需开启「全屏窗口化」模式**
2. **以管理员权限运行**
3. 若启动崩溃，请安装 Visual C++ 运行库（64 位）：
   https://aka.ms/vc14/vc_redist.x64.exe

---

## ⌨️ 默认按键

| 功能 | 默认按键 | 说明 |
|------|----------|------|
| 打开菜单 | `Insert` / `右Shift` | 切换菜单显示 |
| 关闭菜单 | `End` | 关闭菜单并保存配置 |
| 自瞄触发 | `鼠标右键` | 按住触发，菜单可改 |
| 自动扳机 | `Shift` | 准星对准敌人自动开火 |

---

## 🗺️ 掩体判断（地图数据）

「只瞄可见目标」需要地图碰撞几何数据。

1. 使用 **Source 2 Viewer** 从地图 `.vpk` 导出 `world_physics.vmdl_c`
2. 转换为 `.glb` 格式
3. 重命名为 `<地图名>.glb` (如 `de_dust2.glb`)
4. 放到程序目录的 `maps\` 文件夹
5. 首次运行会自动生成 `.colmesh` 精简缓存

**内置地图** (无需外部文件)：
- de_ancient, de_anubis, de_cache, de_dust2, de_inferno, de_mirage, de_nuke

---

## ⚙️ 配置系统

配置文件：程序目录下 `config.json`，首次运行自动生成。

主要配置分类：
- **视觉** (ESP)：方框/骨骼/血条/护甲/姓名/武器/标志/追踪器
- **自瞄**：启用/触发键/FOV/平滑/瞄准骨骼/智能骨骼/可见性检查
- **自动扳机**：启用/触发键/可见性检查
- **世界**：观战列表/炸弹/雷达/十字准星/速度图表
- **设置**：水印/防录制/垂直同步/释放CPU

---

## 🛠️ 开发指南

### 代码规范
- **C++20** 标准，`/std:c++20`、`/permissive-`、`/Zc:__cplusplus`
- **格式化**：`.clang-format` (基于 LLVM/Allman 风格，4 空格缩进)
- **编辑器配置**：`.editorconfig` (UTF-8, CRLF, 4 空格缩进)
- **静态分析**：`/W4` + 抑制噪音警告，`/analyze` 静态分析

### 测试
```powershell
# 运行所有测试
.\build.ps1 -Configuration Debug -RunTests

# 运行特定测试
ctest --test-dir build -R "test_result" --output-on-failure
```

### 代码格式检查
```powershell
clang-format --dry-run --Werror $(find src -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c")
```

---

## 📁 项目结构

```
CNCS/ (42MB 含嵌入资源)
├── CMakeLists.txt / vcpkg.json / build.ps1 / build.bat
├── .clang-format / .editorconfig / .github/workflows/ci.yml
├── CONTRIBUTING.md / LICENSE / README.md
├── driver/
│   ├── CNCS_drv.c (918 行，已强化)
│   ├── CNCS_drv.vcxproj
│   ├── build_driver.ps1 / sign_driver.ps1
├── src/ (86 源文件, 15441 行)
│   ├── core/util/ (Result/HighResTimer/KalmanFilter/Ballistics)
│   ├── core/engine/ (Engine/Cache/Classes/Types)
│   ├── core/visibility/ (CollisionMesh/Visibility)
│   ├── core/kernel/ (KdLoader)
│   ├── gui/ (Renderer/前端模块)
│   ├── embedded/ (7 张地图 .m2 ~42MB)
│   └── external/AsyncLogger/
├── tests/ (Catch2 单元测试)
├── docs/ (架构文档)
└── build.ps1 / build.bat / vcpkg.json
```

---

## 📄 许可证

MIT License - 详见 [LICENSE](LICENSE)

---

## ⚠️ 免责声明

> **本项目仅供学习研究使用，请勿用于商业用途或破坏游戏公平性。**
> 
> 使用本软件产生的任何后果由使用者自行承担。作者不对任何直接或间接损失负责。
> 
> 请遵守游戏服务条款 (ToS/EULA) 及当地法律法规。

---

## 📞 联系方式

- **Issues**: [GitHub Issues](https://github.com/your-org/CNCS/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-org/CNCS/discussions)
- **Email**: maintainers@cncs.example.com