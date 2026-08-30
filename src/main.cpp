// Hello, CNCS! — CS2 external overlay.

#include "core/engine/Engine.hpp"
#include "gui/renderer/Renderer.hpp"
#include "core/util/Result.hpp"

#include <external/exception.hpp>
#include <thread>
#include <chrono>

int main()
{
    c_exception_handler::setup();

    al::LogHelper::Init();

    LOGF(INFO, "Hello, CNCS!");

    // Needs to be ran as ADMINISTRATOR
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        LOGF(WARNING, "无法将进程优先级设为 HIGH");

    auto engine_result = Engine::Init();
    if (!engine_result) {
        LOGF(FATAL, "引擎初始化失败: {} (代码: {})", 
             cncs_error::to_string(engine_result.error()), 
             static_cast<int>(engine_result.error()));
        std::cin.get();
        return 1;
    }

    if (!Renderer::Init()) {
        LOGF(FATAL, "渲染器初始化失败，无法继续执行");
        Engine::Shutdown();
        std::cin.get();
        return 1;
    }
    
    LOGF(INFO, "一切就绪，请确保游戏不是全屏模式（需全屏窗口化）！");
    LOGF(INFO, "用户态读取模式 — 内存读取通过 ReadProcessMemory 完成");

    // Start render thread (non-blocking)
    Renderer::StartRenderThread();

    // 等待渲染线程结束。注意不能用 IsOpen()（那是菜单开关状态，
    // 初始为 false 会导致启动即退出）；IsRunning() 在用户按 End、
    // 覆盖层窗口关闭或游戏窗口失效时转为 false。
    while (Renderer::IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Check if game window is still valid
        auto p = Engine::GetProcess();
        if (p && !IsWindow(p->hwnd_)) {
            LOGF(INFO, "游戏窗口已关闭，退出...");
            break;
        }
    }
    
    Renderer::StopRenderThread();

    LOGF(INFO, "正在清理...");
    Engine::Shutdown();
    Renderer::Destroy();
    LOGF(INFO, "结束，感谢使用！");
    al::LogHelper::Destroy();
    std::cin.get();
    return 0;
}
