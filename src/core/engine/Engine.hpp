#pragma once
#include <thread>
#include <atomic>
#include <memory>
#include "config/Config.hpp"
#include "core/memory/Memory.hpp"
#include "core/kernel/KdLoader.hpp"
#include "core/util/Result.hpp"
#include "core/util/HighResTimer.hpp"

class Engine {
public:
    ~Engine()                           = default;
    Engine(const Engine&)            = delete;
    Engine(Engine&&)                 = delete;
    Engine& operator=(const Engine&) = delete;
    Engine& operator=(Engine&&)      = delete;

    static Result<void, cncs_error::Code> Init();
    static void Shutdown();
    static ProcessModule GetClient();
    static ProcessModule GetEngine();
    static std::shared_ptr<pProcess> GetProcess();
    static KdLoader* GetKdLoader();
    static const HighResTimer& GetHighResTimer();

private:
    Engine() {};

    static Engine& GetInstance()
    {
        static Engine i{};
        return i;
    }

    Result<void, cncs_error::Code> InitImpl();
    void ShutdownImpl();

    Result<void, cncs_error::Code> InitKernelDriver();

    Result<void, cncs_error::Code> AwaitProcess();
    Result<void, cncs_error::Code> AwaitModules();

    void Thread();

private:
    std::shared_ptr<pProcess> process;
    ProcessModule client;
    ProcessModule engine;
    KdLoader kd_loader;
    HighResTimer high_res_timer_;

    // 引擎线程生命周期：running 原子标志 + 可 join 的线程对象。
    std::atomic<bool> running{ false };
    std::thread engine_thread;
};
