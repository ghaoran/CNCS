#include "Renderer.hpp"
#include "window/Window.hpp"

#include "core/engine/Engine.hpp"
#include "gui/frontend/esp/Esp.hpp"
#include "gui/frontend/menu/Menu.hpp"
#include "gui/frontend/overlays/Overlays.hpp"
#include "gui/frontend/aimbot/Aimbot.hpp"

#include <thread>
#include <chrono>

namespace {
    // 配置写入线程：HandleState 每次保存时重启；程序退出时在 DestroyImpl
    // 中 join。此前是函数级 static，若退出时仍在写配置，joinable 的
    // std::thread 析构会触发 std::terminate。
    std::thread g_config_writer;
}

bool Renderer::Init() {
    return GetInstance().InitImpl();
}

void Renderer::Destroy() {
    return GetInstance().DestroyImpl();
}

void Renderer::Thread() {
    // Legacy: run on calling thread (blocking)
    return GetInstance().ThreadImpl();
}

void Renderer::StartRenderThread() {
    GetInstance().StartRenderThreadImpl();
}

void Renderer::StopRenderThread() {
    GetInstance().StopRenderThreadImpl();
}

bool Renderer::IsOpen() {
    return GetInstance().isOpen;
}

bool Renderer::IsRunning() {
    return GetInstance().isRunning;
}

bool Renderer::IsFocused() {
    return GetInstance().isFocused;
}

void Renderer::WaitForFrame() {
    GetInstance().WaitForFrameImpl();
}

void Renderer::SignalFrameReady() {
    GetInstance().SignalFrameReadyImpl();
}

bool Renderer::InitImpl() {
    if (!Window::SpawnWindow()) {
        LOGF(FATAL, "创建窗口失败");
        return false;
    }

    if (!Window::CreateDevice()) {
        LOGF(FATAL, "创建设备失败");
        return false;
    }

    if (!Window::CreateImGui()) {
        LOGF(FATAL, "创建 ImGui 失败");
        return false;
    }

    Menu::Init();
    Esp::Init();
    Overlays::Init();

    // Focus the game
    SetForegroundWindow(Engine::GetProcess()->hwnd_);

    if (cfg::settings::streamproof)
        Window::SetAffinity(Window::hwnd, WindowAffinity::Invisible);

    if (cfg::settings::vsync)
        Window::vsync = true;

    LOGF(INFO, "渲染器初始化成功...");
    return true;
}

void Renderer::StartRenderThreadImpl() {
    if (render_thread_.joinable()) {
        LOGF(WARNING, "渲染线程已在运行");
        return;
    }
    
    isRunning = true;
    render_thread_ = std::thread(&Renderer::ThreadImpl, this);
    LOGF(INFO, "渲染线程已启动");
}

void Renderer::StopRenderThreadImpl() {
    isRunning = false;
    
    // Signal the render thread to wake up if it's waiting
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        frame_ready_ = true;
    }
    frame_cv_.notify_one();
    
    if (render_thread_.joinable()) {
        render_thread_.join();
        LOGF(INFO, "渲染线程已停止");
    }
}

void Renderer::DestroyImpl() {
    StopRenderThreadImpl();

    // 等待未完成的配置写入，避免退出时 joinable 线程析构导致 terminate
    if (g_config_writer.joinable())
        g_config_writer.join();

    Window::DestroyImGui();
    Window::DestroyDevice();
    Window::DespawnWindow();
    
    LOGF(VERBOSE, "渲染器销毁完成");
}

void Renderer::ThreadImpl() {
    using clock = std::chrono::steady_clock;
    auto last_frame = clock::now();

    while (isRunning) {
        Render();

        // Signal frame completion（供主线程 WaitForFrame 使用）
        SignalFrameReadyImpl();

        // 热键全局响应：GetAsyncKeyState 本就是全局按键轮询，不依赖前台
        // 焦点。此前用 isFocused 门控，导致前台不是游戏/覆盖层时按键永远
        // 无响应、菜单打不开。
        const bool toggled = HandleState();
        if (toggled)
            continue; // It will cause flickering if we handle window order after window closes

        HandleWindowOrder();

        // 渲染节流：vsync 关闭时 Present(0,0) 不阻塞，本循环会以纯 CPU 速度空转
        // （既吃满一个核，也让按键轮询/窗口跟随抖动）。这里按目标帧率补足节流；
        // vsync 打开时由 Present 自身限速，不再额外等待。
        if (!Window::vsync.load()) { // atomic 需显式 load（拷贝赋值已删除）
            constexpr auto kTargetFrame = std::chrono::microseconds(1000000 / 144); // 144 FPS 上限
            const auto now = clock::now();
            const auto spent = now - last_frame;
            if (spent < kTargetFrame)
                std::this_thread::sleep_for(kTargetFrame - spent);
            last_frame = clock::now();
        } else {
            last_frame = clock::now();
        }

        // 游戏未聚焦时进一步降频，减少后台占用
        if (!isFocused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
}

void Renderer::WaitForFrameImpl() {
    std::unique_lock<std::mutex> lock(frame_mutex_);
    main_thread_waiting_ = true;
    frame_cv_.wait(lock, [this] { return frame_ready_ || !isRunning; });
    main_thread_waiting_ = false;
    frame_ready_ = false;
}

void Renderer::SignalFrameReadyImpl() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    frame_ready_ = true;
    if (main_thread_waiting_) {
        frame_cv_.notify_one();
    }
}

void Renderer::Render() {
    Window::StartRender();

    Aimbot::Run();
    Aimbot::RunTriggerbot();

    // 菜单打开时不再绘制 ESP / 覆盖层 / FOV 圈：
    // 这些元素都画在背景绘制列表上，会盖在菜单窗口之上，且与"调设置"的场景冲突。
    // 菜单关闭时恢复全部绘制。
    const bool menu_open = isOpen.load();
    if (!menu_open) {
        Esp::Render();
        Overlays::Render();
        Aimbot::Render();
        Menu::RenderStartupHelp();
    } else {
        Menu::Render();
    }

    Window::EndRender();
}

bool Renderer::HandleState() {
    isRunning = Window::shouldRun.load(); // atomic 间赋值需显式 load（atomic 拷贝赋值已删除）

    static bool was_holding = false;

    bool pressed_insert = (GetAsyncKeyState(VK_INSERT) & 0x8000);
    bool pressed_rshift = (GetAsyncKeyState(VK_RSHIFT) & 0x8000);

    bool pressed_end = (GetAsyncKeyState(VK_END) & 0x8000);

    // 只在"刚按下"的那一帧翻转，避免长按连续切换。
    const bool should_toggle = !was_holding && (pressed_insert || pressed_rshift);

    // End 键语义修正：原来 End 既翻转 isOpen 又立刻把 isRunning 置 false，
    // 实际效果是"按 End = 退出程序"，而不是界面上写的"关闭菜单"。
    // 现在 End 只负责关闭菜单（菜单已关时无副作用），不再终止渲染线程。
    const bool should_close = !was_holding && pressed_end && this->isOpen;

    if (should_toggle || should_close) {
        this->isOpen = should_toggle ? !isOpen.load() : false;

        if (this->isOpen) {
            // 菜单需要键盘/鼠标：窗口必须可见、可点、并在前台。
            ShowWindow(Window::hwnd, SW_SHOW);
            Window::SetClickthrough(Window::hwnd, false);
            SetForegroundWindow(Window::hwnd);
            // 覆盖层是全屏透明窗口，必须让 ImGui 拿到鼠标位置与点击，
            // 否则菜单能显示但点不动。
            Window::SetMouseCapture(Window::hwnd, true);
        } else {
            Window::SetMouseCapture(Window::hwnd, false);
            Window::SetClickthrough(Window::hwnd, true);
            // 关菜单时把焦点还给游戏；进程已退出/句柄失效时跳过
            if (auto p = Engine::GetProcess(); p && p->hwnd_)
                SetForegroundWindow(p->hwnd_);
        }

        LOGF(VERBOSE, "菜单状态切换为 {}（触发键: {}）",
             this->isOpen.load(),
             should_toggle ? "Insert/右Shift" : "End");

        // 使用独立线程确保进程退出时不会崩溃
        if (g_config_writer.joinable())
            g_config_writer.join();
        g_config_writer = std::thread(Config::Write);
    }

    was_holding = pressed_insert || pressed_rshift || pressed_end;
    return should_toggle || should_close;
}

bool Renderer::HandleWindowOrder() {
    auto p = Engine::GetProcess();

    if (!p || (!p->hwnd_ && !p->UpdateHWND()))
        return false;

    // Check if game window is still valid, if not, most likely game closed
    if (!IsWindow(p->hwnd_))
        this->isRunning = false;

    // 用 IsWindowVisible 作为唯一权威状态，不再维护 static overlay_visible。
    // 原先的 static 只在两个分支里翻转：一旦窗口在菜单未打开时被 SW_HIDE，
    // 之后进入"菜单已打开"的路径就再也不走 SW_SHOW —— 于是按 Insert 后
    // isOpen 已为 true、菜单也在绘制，但窗口仍处于隐藏状态，表现为"Menu 唤不出"。
    auto foreground = GetForegroundWindow();
    this->isFocused = (foreground == Window::hwnd || foreground == p->hwnd_);
    const bool window_visible = IsWindowVisible(Window::hwnd) != FALSE;

    // 只有"菜单关闭 且 游戏/覆盖层都不在前台"时才隐藏，避免无谓占用前台。
    if (!this->isFocused && !this->isOpen) {
        if (window_visible) {
            LOGF(VERBOSE, "游戏未聚焦且菜单关闭，隐藏叠加层窗口");
            ShowWindow(Window::hwnd, SW_HIDE);
        }
        return true;
    }

    // 菜单已打开或焦点回到游戏/覆盖层：必须确保窗口可见，否则菜单不可见。
    if (!window_visible) {
        LOGF(VERBOSE, "叠加层窗口应为可见状态（菜单打开或已聚焦），重新显示");
        ShowWindow(Window::hwnd, SW_SHOW);
        return true;
    }

    static RECT last_rect = { 0, 0, 0, 0 };

    RECT window_rect;
    if (!GetWindowRect(p->hwnd_, &window_rect))
        return false;

    // All good, no movements from the client
    if (memcmp(&window_rect, &last_rect, sizeof(RECT)) == 0)
        return true;

    RECT client_rect;
    if (!GetClientRect(p->hwnd_, &client_rect))
        return false;

    POINT top_left = { client_rect.left, client_rect.top };
    POINT bottom_right = { client_rect.right, client_rect.bottom };

    ClientToScreen(p->hwnd_, &top_left);
    ClientToScreen(p->hwnd_, &bottom_right);

    RECT screen_rect = { top_left.x, top_left.y, bottom_right.x, bottom_right.y };

    SetWindowPos(
        Window::hwnd,
        HWND_TOPMOST,
        screen_rect.left,
        screen_rect.top,
        screen_rect.right - screen_rect.left,
        screen_rect.bottom - screen_rect.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );

    last_rect = window_rect;

    return true;
}