#include "Engine.hpp"

#include "core/offsets/Dumper.hpp"
#include "core/engine/cache/Cache.hpp"

Result<void, cncs_error::Code> Engine::Init() {
    return GetInstance().InitImpl();
}

void Engine::Shutdown() {
    GetInstance().ShutdownImpl();
}

ProcessModule Engine::GetClient() {
    return GetInstance().client;
}

ProcessModule Engine::GetEngine() {
    return GetInstance().engine;
}

std::shared_ptr<pProcess> Engine::GetProcess() {
    return GetInstance().process;
}

KdLoader* Engine::GetKdLoader() {
    return &GetInstance().kd_loader;
}

const HighResTimer& Engine::GetHighResTimer() {
    return GetInstance().high_res_timer_;
}

// ============================================================================
// 初始化内核驱动（必须成功，否则无法继续）
// ============================================================================
Result<void, cncs_error::Code> Engine::InitKernelDriver() {
    auto& kd = GetInstance().kd_loader;

    // 获取驱动 .sys 文件路径（与 EXE 同目录）
    wchar_t exe_path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);

    // 去掉文件名，拼接驱动文件名
    std::wstring driver_path(exe_path);
    auto pos = driver_path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        driver_path = driver_path.substr(0, pos + 1);
    driver_path += L"CNCS_drv.sys";

    LOGF(INFO, "正在加载内核驱动 (CNCS_drv.sys)...");

    if (!kd.LoadDriver(driver_path.c_str())) {
        LOGF(FATAL, "内核驱动加载失败！请确保：");
        LOGF(FATAL, "  1. 以管理员权限运行");
        LOGF(FATAL, "  2. 已开启测试签名模式 (bcdedit /set testsigning on)");
        LOGF(FATAL, "  3. CNCS_drv.sys 与程序在同一目录");
        return Result<void, cncs_error::Code>::Err(cncs_error::Code::DriverLoadFailed);
    }

    LOGF(INFO, "内核驱动加载成功");
    return Result<void, cncs_error::Code>::Ok();
}

Result<void, cncs_error::Code> Engine::InitImpl() {
    process = std::make_shared<pProcess>();

    // 加载内核驱动（必须成功）
    auto driver_result = InitKernelDriver();
    if (!driver_result) {
        return driver_result;
    }

    // 设置内核驱动到 process 对象
    process->SetKernelDriver(&kd_loader);
    LOGF(INFO, "内存读取模式: 内核驱动 (MmCopyVirtualMemory)");

    auto process_result = this->AwaitProcess();
    if (!process_result) {
        LOGF(FATAL, "未找到进程，请确保游戏已启动");
        return process_result;
    }

    auto modules_result = this->AwaitModules();
    if (!modules_result) {
        LOGF(FATAL, "游戏加载超时，请等游戏完全加载后重新打开");
        return modules_result;
    }

    if (!Dumper::Init()) {
        LOGF(FATAL, "转储游戏偏移失败");
        return Result<void, cncs_error::Code>::Err(cncs_error::Code::OffsetDumpFailed);
    }

    if (!Config::Read())
        LOGF(WARNING, "解析配置失败，使用默认值");

#ifdef _DEBUG
    if (!cfg::dev::console)
        al::LogHelper::Free();
#endif

    // 启动引擎线程（可 join，Shutdown 时先停止再卸载驱动）
    running = true;
    engine_thread = std::thread(&Engine::Thread, this);

    LOGF(INFO, "引擎初始化成功...");
    return Result<void, cncs_error::Code>::Ok();
}

void Engine::ShutdownImpl() {
    // 先停止引擎线程并等待其退出，避免它在驱动卸载后继续通过已关闭的
    // 句柄调用 DeviceIoControl（竞态 + 潜在句柄重用风险）。
    running = false;
    if (engine_thread.joinable())
        engine_thread.join();

    LOGF(INFO, "正在卸载内核驱动...");
    kd_loader.UnloadDriver();

    if (process)
        process->Close();
}

void Engine::Thread() {
    // High-resolution timer for precise frame pacing
    using namespace std::chrono;
    auto last_frame = steady_clock::now();
    
    while (running) {
        auto start = steady_clock::now();
        
        // Calculate target frame time based on adaptive cache refresh
        // We want to run at ~2x the game's frame rate
        float target_ms = 2.0f; // fallback
        if (auto snap = Cache::CopySnapshot(); snap) {
            float frame_ms = snap->globals.frame_time;
            target_ms = std::clamp(frame_ms * 0.5f, 1.0f, 5.0f);
        }
        
        Cache::Refresh();

        if (cfg::settings::free_cpu) {
            auto elapsed = duration_cast<microseconds>(steady_clock::now() - start);
            auto target_duration = microseconds(static_cast<int64_t>(target_ms * 1000));
            
            if (elapsed < target_duration) {
                auto sleep_duration = target_duration - elapsed;
                // Use sleep_until for better precision than sleep_for
                auto wake_time = steady_clock::now() + sleep_duration;
                std::this_thread::sleep_until(wake_time);
            }
        }
    }
}

Result<void, cncs_error::Code> Engine::AwaitProcess() {
    if (!process)
        return Result<void, cncs_error::Code>::Err(cncs_error::Code::ProcessNotFound);

    int attempts = 0;
    constexpr int max_attempts = 30; // 最多等待 ~60 秒（指数退避）
    do {
        if (process->AttachProcess("cs2.exe"))
            break;

        if (!attempts)
            LOGF(INFO, "等待游戏启动...");

        if (attempts >= max_attempts)
            return Result<void, cncs_error::Code>::Err(cncs_error::Code::ProcessAttachFailed);
        
        // 指数退避：1s, 2s, 3s... 最大 5s
        int sleep_ms = std::min(1000 * (attempts + 1), 5000);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        attempts++;
    } while (true);

    return Result<void, cncs_error::Code>::Ok();
}

Result<void, cncs_error::Code> Engine::AwaitModules() {
    if (!process || !process->pid_)
        return Result<void, cncs_error::Code>::Err(cncs_error::Code::ProcessNotFound);

    LOGF(INFO, "等待游戏模块加载...");

    int attempts = 0;
    constexpr int max_attempts = 20; // 最多等待 ~40 秒（指数退避）
    do {
        this->client = process->GetModule("client.dll");
        this->engine = process->GetModule("engine2.dll");

        if (this->client.base && this->engine.base)
            break;

        if (attempts >= max_attempts)
            return Result<void, cncs_error::Code>::Err(cncs_error::Code::ModuleNotFound);
        
        // 指数退避：1s, 2s, 3s... 最大 5s
        int sleep_ms = std::min(1000 * (attempts + 1), 5000);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        attempts++;
    } while (true);

    return Result<void, cncs_error::Code>::Ok();
}
