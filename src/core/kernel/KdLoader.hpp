// ============================================================================
// KdLoader — 内核驱动加载与通信模块
// ============================================================================
// 负责:
//   1. 通过 SCM (Service Control Manager) 安装/启动/停止/删除内核驱动服务
//   2. 打开驱动设备句柄 (\\\\.\\CNCS)
//   3. 封装 IOCTL 调用，提供与原有 pProcess 兼容的读取接口
//
// 用法:
//   KdLoader kd;
//   if (kd.LoadDriver(L"CNCS_drv.sys")) {
//       kd.Open();
//       // 现在可以用 kd.Read<T>(pid, address) 读取内存
//   }
// ============================================================================

#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <cstdint>

class KdLoader {
public:
    KdLoader();
    ~KdLoader();

    // 禁止拷贝
    KdLoader(const KdLoader&) = delete;
    KdLoader& operator=(const KdLoader&) = delete;

    // ---- 驱动生命周期 ----

    // 安装并启动内核驱动服务
    // driver_path: .sys 文件的完整路径
    // 返回 true 表示驱动已加载就绪
    bool LoadDriver(const wchar_t* driver_path);

    // 停止并删除驱动服务
    void UnloadDriver();

    // 打开驱动设备句柄（LoadDriver 成功后调用）
    bool Open();

    // 关闭设备句柄
    void Close();

    // 驱动是否就绪
    bool IsReady() const { return device_handle_ != INVALID_HANDLE_VALUE; }

    // ---- 内存操作（通过驱动 IOCTL）----

    // 读取目标进程内存
    bool ReadMemory(uint64_t pid, uint64_t address, void* buffer, size_t size);

    // 模板读取（兼容原有 pProcess::read<T> 接口）
    template<class T>
    T Read(uint64_t pid, uint64_t address) {
        T buffer{};
        ReadMemory(pid, address, &buffer, sizeof(T));
        return buffer;
    }

    // 批量读取（兼容原有 pProcess::read_raw 接口）
    bool ReadRaw(uint64_t pid, uint64_t address, void* buffer, size_t size) {
        return ReadMemory(pid, address, buffer, size);
    }

    // ---- 模块查询 ----

    struct ModuleInfo {
        uint64_t base;
        uint64_t size;
    };

    // 获取目标进程的模块基址和大小
    bool GetModuleBase(uint64_t pid, const wchar_t* module_name, ModuleInfo& out);

    // ---- 进程查找 ----

    // 通过进程名查找 PID
    uint64_t FindProcess(const wchar_t* process_name);

    // ---- 状态 ----
    bool IsDriverLoaded() const { return driver_loaded_; }

private:
    HANDLE  device_handle_;
    bool    driver_loaded_;
    wchar_t service_name_[64];

    bool InstallService(const wchar_t* driver_path);
    bool StartService();
    void StopService();
    void RemoveService();
};
