#include "Renderer.hpp"
#include "window/Window.hpp"

#include "core/engine/Engine.hpp"
#include "gui/frontend/esp/Esp.hpp"
#include "gui/frontend/menu/Menu.hpp"
#include "gui/frontend/overlays/Overlays.hpp"
#include "gui/frontend/aimbot/Aimbot.hpp"

#include <thread>
#include <chrono>

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
    
    Window::DestroyImGui();
    Window::DestroyDevice();
    Window::DespawnWindow();
    
    LOGF(VERBOSE, "渲染器销毁完成");
}

void Renderer::ThreadImpl() {
    while (isRunning) {
        Render();
        
        // Signal frame completion
        SignalFrameReadyImpl();
        
        // If the game is not focused dont do states, 
        // or will start focusing game & overlay
        if (this->isFocused && HandleState())
            continue; // It will cause flickering if we handle window order after window closes

        HandleWindowOrder();
        
        // Precise sleep when game is unfocused to reduce CPU usage
        if (!isFocused) {
            // Use waitable timer for high-precision sleep (~1ms resolution)
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

    Esp::Render();
    Overlays::Render();

    Aimbot::Render();

    Menu::RenderStartupHelp();
    if (isOpen) Menu::Render();

    Window::EndRender();
}

bool Renderer::HandleState() {
    isRunning = Window::shouldRun; // From the window event handler

    static bool was_holding = false;

    bool pressed_insert = (GetAsyncKeyState(VK_INSERT) & 0x8000);
    bool pressed_rshift = (GetAsyncKeyState(VK_RSHIFT) & 0x8000);

    bool pressed_end = (GetAsyncKeyState(VK_END) & 0x8000);

    bool should_toggle = !was_holding && (pressed_insert || pressed_rshift);

    if (should_toggle || pressed_end) { // Toggle when pressing end to trigger the config save :v
        this->isOpen = !isOpen;

        // Release cursor when opening the menu
        // Sometimes flashes the render as its handling the window order
        if (this->isOpen)
            SetForegroundWindow(Window::hwnd);
        else
            SetForegroundWindow(Engine::GetProcess()->hwnd_);

        Window::SetClickthrough(Window::hwnd, !this->isOpen);
        LOGF(VERBOSE, "捕获到 Insert 或右Shift，菜单状态切换为 {}", this->isOpen);

        // 使用静态线程确保进程退出时不会崩溃
        static std::thread config_writer;
        if (config_writer.joinable())
            config_writer.join();
        config_writer = std::thread(Config::Write);
    }

    if (pressed_end)
        this->isRunning = false;

    was_holding = pressed_insert || pressed_rshift;
    return should_toggle;
}

bool Renderer::HandleWindowOrder() {
    auto p = Engine::GetProcess();

    if (!p || (!p->hwnd_ && !p->UpdateHWND()))
        return false;

    // Check if game window is still valid, if not, most likely game closed
    if (!IsWindow(p->hwnd_))
        this->isRunning = false;

    static bool overlay_visible = true;
    auto foreground = GetForegroundWindow();
    this->isFocused = (foreground == Window::hwnd || foreground == p->hwnd_);

    if (!this->isFocused && overlay_visible) {
        LOGF(VERBOSE, "游戏未聚焦，隐藏叠加层窗口");
        ShowWindow(Window::hwnd, SW_HIDE);
        overlay_visible = false;
        return true;
    }

    if (!overlay_visible && this->isFocused) {  
        LOGF(VERBOSE, "游戏已聚焦，显示叠加层窗口");
        ShowWindow(Window::hwnd, SW_SHOW);
        overlay_visible = true;
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