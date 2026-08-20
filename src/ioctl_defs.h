// ============================================================================
// CNCS — 共享 IOCTL 定义（内核驱动 ↔ 客户端）
// ============================================================================
// 本文件同时被内核驱动（C）和客户端（C++）包含。
// 所有结构体使用固定大小类型以确保 32/64 位兼容。

#pragma once

// 设备路径（客户端通过此路径打开驱动句柄）
#define CNCS_DEVICE_PATH   "\\\\.\\CNCS"

// 自定义设备类型（0x8000-0xFFFF 为用户自定义范围）
#define FILE_DEVICE_CNCS   0x8001

// IOCTL 功能码
#define IOCTL_CNCS_READ_MEMORY     CTL_CODE(FILE_DEVICE_CNCS, 0x0800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_CNCS_GET_BASE_MODULE CTL_CODE(FILE_DEVICE_CNCS, 0x0801, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_CNCS_FIND_PROCESS    CTL_CODE(FILE_DEVICE_CNCS, 0x0802, METHOD_BUFFERED, FILE_READ_ACCESS)

// 单次内存读取的上限（4MB）。
// 既防止 METHOD_BUFFERED 分配过大的非分页池，也防止恶意构造超大
// request->size 触发整数溢出绕过缓冲校验。实际最大需求是 Dumper 的
// 409600 字节扫描块，4MB 已留有充足余量。
#define CNCS_MAX_READ_SIZE (4 * 1024 * 1024)

// ============================================================================
// IOCTL 数据结构
// ============================================================================

// 读取进程内存
typedef struct _CNCS_READ_REQUEST {
    unsigned __int64 target_pid;       // [in]  目标进程 ID
    unsigned __int64 address;          // [in]  要读取的虚拟地址
    unsigned __int64 size;             // [in]  读取字节数
    // 输出数据紧跟在结构体之后（METHOD_BUFFERED 会分配足够空间）
} CNCS_READ_REQUEST, *PCNCS_READ_REQUEST;

// 获取模块基址
typedef struct _CNCS_MODULE_REQUEST {
    unsigned __int64 target_pid;       // [in]  目标进程 ID
    wchar_t          module_name[128]; // [in]  模块名（宽字符）
    unsigned __int64 base;             // [out] 模块基址
    unsigned __int64 size;             // [out] 模块大小
} CNCS_MODULE_REQUEST, *PCNCS_MODULE_REQUEST;

// 查找进程
typedef struct _CNCS_FIND_PROCESS_REQUEST {
    wchar_t          process_name[260]; // [in]  进程名（宽字符）
    unsigned __int64 pid;              // [out] 进程 ID
} CNCS_FIND_PROCESS_REQUEST, *PCNCS_FIND_PROCESS_REQUEST;
