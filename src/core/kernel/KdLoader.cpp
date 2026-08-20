// ============================================================================
// KdLoader 实现 — 内核驱动加载与 IOCTL 通信
// ============================================================================

#include "KdLoader.hpp"
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <cstdio>

#include "ioctl_defs.h"

// ---- 构造 / 析构 ----

KdLoader::KdLoader()
    : device_handle_(INVALID_HANDLE_VALUE)
    , driver_loaded_(false)
{
    wcscpy_s(service_name_, 64, L"CNCS_drv");
}

KdLoader::~KdLoader()
{
    Close();
    // 注意: 不自动卸载驱动服务，由调用者显式控制
}

// ---- 驱动生命周期 ----

bool KdLoader::LoadDriver(const wchar_t* driver_path)
{
    // 先尝试直接打开设备（驱动可能已加载）
    if (Open())
        return true;

    // 安装服务
    if (!InstallService(driver_path))
        return false;

    // 启动服务
    if (!StartService()) {
        RemoveService();
        return false;
    }

    driver_loaded_ = true;

    // 打开设备
    if (!Open()) {
        StopService();
        RemoveService();
        return false;
    }

    return true;
}

void KdLoader::UnloadDriver()
{
    Close();
    StopService();
    RemoveService();
    driver_loaded_ = false;
}

bool KdLoader::Open()
{
    if (device_handle_ != INVALID_HANDLE_VALUE)
        return true;

    device_handle_ = CreateFileA(
        CNCS_DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    return device_handle_ != INVALID_HANDLE_VALUE;
}

void KdLoader::Close()
{
    if (device_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(device_handle_);
        device_handle_ = INVALID_HANDLE_VALUE;
    }
}

// ---- SCM 服务管理 ----

bool KdLoader::InstallService(const wchar_t* driver_path)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm)
        return false;

    // 尝试打开已有服务
    SC_HANDLE svc = OpenServiceW(scm, service_name_, SERVICE_ALL_ACCESS);
    if (svc) {
        // 服务已存在，更新路径
        ChangeServiceConfigW(svc,
            SERVICE_KERNEL_DRIVER,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL,
            driver_path,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        return true;
    }

    // 创建新服务
    svc = CreateServiceW(
        scm,
        service_name_,
        service_name_,
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,      // 手动启动
        SERVICE_ERROR_NORMAL,
        driver_path,
        nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return true;
}

bool KdLoader::StartService()
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm)
        return false;

    SC_HANDLE svc = OpenServiceW(scm, service_name_, SERVICE_ALL_ACCESS);
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    BOOL ok = ::StartServiceW(svc, 0, nullptr);
    if (!ok) {
        DWORD err = GetLastError();
        // ERROR_SERVICE_ALREADY_RUNNING 或 ERROR_ALREADY_EXISTS 都算成功
        if (err == ERROR_SERVICE_ALREADY_RUNNING || err == ERROR_ALREADY_EXISTS) {
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            return true;
        }
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok != FALSE;
}

void KdLoader::StopService()
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm)
        return;

    SC_HANDLE svc = OpenServiceW(scm, service_name_, SERVICE_ALL_ACCESS);
    if (!svc) {
        CloseServiceHandle(scm);
        return;
    }

    SERVICE_STATUS status;
    ControlService(svc, SERVICE_CONTROL_STOP, &status);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
}

void KdLoader::RemoveService()
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm)
        return;

    SC_HANDLE svc = OpenServiceW(scm, service_name_, SERVICE_ALL_ACCESS);
    if (svc) {
        DeleteService(svc);
        CloseServiceHandle(svc);
    }

    CloseServiceHandle(scm);
}

// ---- IOCTL 通信 ----

bool KdLoader::ReadMemory(uint64_t pid, uint64_t address, void* buffer, size_t size)
{
    if (device_handle_ == INVALID_HANDLE_VALUE || !buffer || size == 0)
        return false;

    // 与内核驱动一致的上限，避免在客户端侧分配过大缓冲区
    if (size > CNCS_MAX_READ_SIZE)
        return false;

    const size_t total = sizeof(CNCS_READ_REQUEST) + size;

    // 避免大栈分配，统一使用向量（自动管理内存，防止栈溢出）
    std::vector<uint8_t> io_data(total);
    uint8_t* io_buf = io_data.data();

    CNCS_READ_REQUEST* req = (CNCS_READ_REQUEST*)io_buf;
    req->target_pid = pid;
    req->address    = address;
    req->size       = size;

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(
        device_handle_,
        IOCTL_CNCS_READ_MEMORY,
        io_buf, (DWORD)total,   // 输入
        io_buf, (DWORD)total,   // 输出 (METHOD_BUFFERED 共享)
        &bytesReturned,
        nullptr);

    if (!ok || bytesReturned < sizeof(CNCS_READ_REQUEST) + size)
        return false;

    // 拷贝读取到的数据
    memcpy(buffer, io_buf + sizeof(CNCS_READ_REQUEST), size);
    return true;
}

bool KdLoader::GetModuleBase(uint64_t pid, const wchar_t* module_name, ModuleInfo& out)
{
    if (device_handle_ == INVALID_HANDLE_VALUE || !module_name)
        return false;

    CNCS_MODULE_REQUEST req = {};
    req.target_pid = pid;
    wcsncpy_s(req.module_name, sizeof(req.module_name) / sizeof(wchar_t), module_name, _TRUNCATE);

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(
        device_handle_,
        IOCTL_CNCS_GET_BASE_MODULE,
        &req, sizeof(req),
        &req, sizeof(req),
        &bytesReturned,
        nullptr);

    if (!ok || bytesReturned < sizeof(CNCS_MODULE_REQUEST))
        return false;

    out.base = req.base;
    out.size = req.size;
    return out.base != 0;
}

uint64_t KdLoader::FindProcess(const wchar_t* process_name)
{
    if (device_handle_ == INVALID_HANDLE_VALUE || !process_name)
        return 0;

    CNCS_FIND_PROCESS_REQUEST req = {};
    wcsncpy_s(req.process_name, sizeof(req.process_name) / sizeof(wchar_t), process_name, _TRUNCATE);

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(
        device_handle_,
        IOCTL_CNCS_FIND_PROCESS,
        &req, sizeof(req),
        &req, sizeof(req),
        &bytesReturned,
        nullptr);

    if (!ok)
        return 0;

    return req.pid;
}
