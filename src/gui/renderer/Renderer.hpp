#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

class Renderer {
public:
    ~Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    static bool Init();
    static void Destroy();
    static void Thread(); // Legacy - runs on calling thread
    static void StartRenderThread(); // New: starts background render thread
    static void StopRenderThread();  // New: stops background render thread

    static bool IsOpen();
    static bool IsFocused();
    
    // Frame synchronization
    static void WaitForFrame();      // Main thread waits for frame completion
    static void SignalFrameReady();  // Render thread signals frame ready
    
private:
    Renderer() {};

    static Renderer& GetInstance()
    {
        static Renderer i{};
        return i;
    }

    bool InitImpl();
    void ThreadImpl();      // Background render loop
    void DestroyImpl();

    // Private Impl methods (declared but defined in Renderer.cpp)
    void StartRenderThreadImpl();
    void StopRenderThreadImpl();
    void WaitForFrameImpl();
    void SignalFrameReadyImpl();

    void Render();
    bool HandleState();
    bool HandleWindowOrder();
    
private:
    bool isRunning = true;
    bool isOpen = false;
    bool isFocused = false;
    
    // Thread management
    std::thread render_thread_;
    std::mutex frame_mutex_;
    std::condition_variable frame_cv_;
    bool frame_ready_ = false;
    bool main_thread_waiting_ = false;
};