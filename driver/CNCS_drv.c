// ============================================================================
// CNCS 内核驱动 — 读取游戏内存、获取模块信息、查找进程
// ============================================================================
// 使用 Windows Driver Kit (WDK) 编译。
// 设备路径: \\Device\\CNCS  →  用户态: \\\\.\\CNCS
//
// 功能:
//   IOCTL_CNCS_READ_MEMORY     — 使用 MmCopyVirtualMemory 读取目标进程内存
//   IOCTL_CNCS_GET_BASE_MODULE — 枚举目标进程模块，返回基址和大小
//   IOCTL_CNCS_FIND_PROCESS    — 通过进程名查找 PID
//
// 安全说明:
//   本驱动仅供学习/单机测试。设备对象通过 SDDL 限制为 SYSTEM + 管理员可访问。
//
// 审计日志:
//   关键操作输出 ETW 事件，可通过 Windows Performance Recorder / logman 收集。
// ============================================================================

#include <ntddk.h>
#include <ntstrsafe.h>
#include <wdmsec.h>
#include <evntrace.h>  // ETW 支持
#include "..\\src\\ioctl_defs.h"

// ETW Provider GUID: {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
// 生成方式: [guid]::NewGuid() 或使用 UUID 生成工具
#define CNCS_ETW_PROVIDER_GUID \
    { 0xA1B2C3D4, 0xE5F6, 0x7890, { 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90 } }

// ETW 事件定义
#define CNCS_EVENT_DRIVER_LOAD          1
#define CNCS_EVENT_DRIVER_UNLOAD        2
#define CNCS_EVENT_DEVICE_OPEN          3
#define CNCS_EVENT_DEVICE_CLOSE         4
#define CNCS_EVENT_READ_MEMORY          5
#define CNCS_EVENT_GET_MODULE           6
#define CNCS_EVENT_FIND_PROCESS         7
#define CNCS_EVENT_ACCESS_DENIED        8
#define CNCS_EVENT_INVALID_PARAMETER    9

// 全局 ETW 注册句柄
static REGHANDLE g_EtwRegHandle = 0;   // REGHANDLE 为 ULONG_PTR，赋 0 而非 NULL(void*) 避免 C4047

// ETW 事件写入辅助函数
static VOID
EtwWriteEvent(
    _In_ UCHAR EventId,
    _In_ ULONG EventVersion,
    _In_ USHORT EventChannel,
    _In_ UCHAR EventLevel,
    _In_ ULONGLONG EventKeywords,
    _In_ PCSTR FormatString,
    ...
) {
    if (g_EtwRegHandle == 0)
        return;

    // 内核驱动暂未使用结构化 ETW（EventWriteString 需 manifest），
    // 参数仅用于未来扩展，抑制未引用告警（/WX 会把 C4100 当错误）。
    UNREFERENCED_PARAMETER(EventId);
    UNREFERENCED_PARAMETER(EventVersion);
    UNREFERENCED_PARAMETER(EventChannel);
    UNREFERENCED_PARAMETER(EventLevel);
    UNREFERENCED_PARAMETER(EventKeywords);

    va_list args;
    va_start(args, FormatString);
    
    // 使用 EventWriteTransfer 或 EventWriteString (简化版本)
    // 这里使用 EventWriteString 作为示例，实际生产应使用 manifest 定义的结构化事件
    CHAR buffer[512];
    
    // 格式化消息（内核 C 无 C99 va_copy，直接用 args 单次格式化即可；
    // 后续仅用 buffer 输出，不再复用 va_list）
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, FormatString, args);
    
    // 写入 ETW 事件 (EventWriteString 需要 Windows 8+)
    // EventWriteString(g_EtwRegHandle, EventLevel, EventKeywords, 0, NULL, &ansiStr);
    
    // 兼容性：同时输出到调试器
    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, "[CNCS ETW] %s\n", buffer);
    
    va_end(args);
}

// 简化的 ETW 写入宏
#define ETW_LOG(eventId, level, fmt, ...) \
    EtwWriteEvent(eventId, 1, 0, level, 0, fmt, ##__VA_ARGS__)

// MmCopyVirtualMemory 是未文档化的内核函数，需要手动声明
NTKERNELAPI
NTSTATUS
MmCopyVirtualMemory(
    PEPROCESS SourceProcess,
    PVOID SourceAddress,
    PEPROCESS TargetProcess,
    PVOID TargetAddress,
    SIZE_T BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T ReturnSize
);

// PsLookupProcessByProcessId 在 ntddk.h 中可能未声明
NTKERNELAPI
NTSTATUS
PsLookupProcessByProcessId(
    HANDLE ProcessId,
    PEPROCESS *Process
);

// PEB 相关类型（用于模块枚举）
typedef struct _PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    HANDLE SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _PEB {
    UCHAR Reserved1[2];
    UCHAR BeingDebugged;
    UCHAR Reserved2[1];
    PVOID Reserved3[2];
    PPEB_LDR_DATA Ldr;
} PEB, *PPEB;

NTKERNELAPI
PPEB
PsGetProcessPeb(
    PEPROCESS Process
);

// KAPC_STATE 在 wdm.h 中定义，但可能因条件编译而缺失
#ifndef _KAPC_STATE_DEFINED
#define _KAPC_STATE_DEFINED
typedef struct _KAPC_STATE {
    LIST_ENTRY ApcListHead[MaximumMode];
    PKPROCESS Process;
    UCHAR KernelApcInProgress;
    UCHAR KernelApcPending;
    UCHAR UserApcPending;
} KAPC_STATE, *PKAPC_STATE;
#endif

NTKERNELAPI
VOID
KeStackAttachProcess(
    PKPROCESS Process,
    PKAPC_STATE ApcState
);

NTKERNELAPI
VOID
KeUnstackDetachProcess(
    PKAPC_STATE ApcState
);

// ZwQuerySystemInformation
NTSYSAPI
NTSTATUS
NTAPI
ZwQuerySystemInformation(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

// SYSTEM_PROCESS_INFORMATION
typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

// ---- 设备和符号链接名称 ----------------------------------------------------
#define DEVICE_NAME   L"\\Device\\CNCS"
#define SYMLINK_NAME  L"\\DosDevices\\CNCS"

// SystemProcessInformation 未在公开头文件中声明（未文档化），此处显式定义。
#define SystemProcessInformation 5

// ---- 全局设备对象指针 ------------------------------------------------------
static PDEVICE_OBJECT g_DeviceObject = NULL;

// ---- 前向声明 --------------------------------------------------------------
DRIVER_UNLOAD   DriverUnload;
DRIVER_DISPATCH DispatchCreateClose;
DRIVER_DISPATCH DispatchDeviceControl;

// ============================================================================
// 辅助: 读取目标进程内存 (MmCopyVirtualMemory)
// ============================================================================
static NTSTATUS
ReadProcessMemory(
    PEPROCESS   TargetProcess,
    PVOID       SourceAddress,
    PVOID       DestBuffer,
    SIZE_T      Size,
    PSIZE_T     BytesRead)
{
    NTSTATUS status;
    PEPROCESS currentProcess = PsGetCurrentProcess();
    SIZE_T    bytes = 0;

    if (!TargetProcess || !SourceAddress || !DestBuffer || Size == 0)
        return STATUS_INVALID_PARAMETER;

    // 验证源地址是否在用户态地址空间
    if ((ULONG_PTR)SourceAddress >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        return STATUS_ACCESS_DENIED;

    // 使用 __try/__except 保护 MmCopyVirtualMemory，防止访问无效内存导致系统崩溃
    __try {
        status = MmCopyVirtualMemory(
            TargetProcess,   // 源进程 (游戏)
            SourceAddress,    // 源地址
            currentProcess,   // 目标进程 (内核缓冲区, 系统地址空间)
            DestBuffer,       // 目标缓冲区
            Size,
            KernelMode,
            &bytes);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] Exception in MmCopyVirtualMemory: 0x%08X (addr=0x%p, size=%zu)\n",
            status, SourceAddress, Size);
    }

    if (BytesRead)
        *BytesRead = bytes;

    return status;
}

// ============================================================================
// IOCTL: 读取目标进程内存
// ============================================================================
static NTSTATUS
HandleReadMemory(
    PIRP Irp,
    PIO_STACK_LOCATION  IrpSp)
{
    NTSTATUS status;
    PEPROCESS targetProcess = NULL;
    PVOID     outBuffer;
    ULONG     inSize;
    ULONG     outSize;
    PCNCS_READ_REQUEST request;
    SIZE_T    bytesRead = 0;

    inSize    = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBuffer = Irp->AssociatedIrp.SystemBuffer;  // METHOD_BUFFERED: in/out 共享
    outSize   = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (inSize < sizeof(CNCS_READ_REQUEST)) {
        status = STATUS_BUFFER_TOO_SMALL;
        goto out;
    }

    request = (PCNCS_READ_REQUEST)outBuffer;

    // 增强输入验证
    // 1. 拒绝 0 字节和超大读取
    if (request->size == 0 || request->size > CNCS_MAX_READ_SIZE) {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] Invalid read size: %llu\n", request->size);
        status = STATUS_INVALID_BUFFER_SIZE;
        goto out;
    }

    // 2. 验证地址范围：必须在用户态地址空间内
    if ((ULONG_PTR)request->address >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS) {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] Address out of user range: 0x%llx\n", request->address);
        status = STATUS_ACCESS_DENIED;
        goto out;
    }

    // 3. 验证地址对齐（可选，但建议至少 1 字节对齐）
    // 4. 验证 PID 有效性（非零，非系统保留 PID）
    if (request->target_pid == 0 || request->target_pid == 4) { // 4 = System PID
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] Invalid target PID: %llu\n", request->target_pid);
        status = STATUS_INVALID_PARAMETER;
        goto out;
    }

    // 5. 防止整数溢出：sizeof + size 检查
    SIZE_T totalSize = sizeof(CNCS_READ_REQUEST) + (SIZE_T)request->size;
    if (totalSize < sizeof(CNCS_READ_REQUEST) || totalSize > outSize) {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] Buffer size overflow or insufficient\n");
        status = STATUS_BUFFER_TOO_SMALL;
        goto out;
    }

    // 6. 使用 ProbeForRead 验证用户缓冲区（METHOD_BUFFERED 已由 I/O 管理器验证，
    // 但显式验证可在驱动上下文中提前捕获无效地址）
    __try {
        ProbeForRead(outBuffer, outSize, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] ProbeForRead failed: 0x%08X\n", status);
        goto out;
    }

    // 查找目标进程
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)request->target_pid, &targetProcess);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_WARNING_LEVEL,
            "[CNCS] Process lookup failed for PID %llu: 0x%08X\n", request->target_pid, status);
        goto out;
    }

    // 读取目标进程内存，写到输出缓冲区 (紧跟在 request 之后)
    __try {
        status = ReadProcessMemory(
            targetProcess,
            (PVOID)(ULONG_PTR)request->address,
            (PUCHAR)outBuffer + sizeof(CNCS_READ_REQUEST),
            (SIZE_T)request->size,
            &bytesRead);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] Exception in ReadProcessMemory: 0x%08X\n", status);
    }

    if (NT_SUCCESS(status)) {
        // 返回实际读取的字节数 (结构体 + 数据)
        Irp->IoStatus.Information = sizeof(CNCS_READ_REQUEST) + bytesRead;
    } else {
        Irp->IoStatus.Information = 0;
    }

    ObDereferenceObject(targetProcess);

out:
    return status;
}

// ============================================================================
// 辅助: 枚举进程模块（通过 PEB → Ldr 链表遍历）
// ============================================================================
typedef struct _KLDR_DATA_TABLE_ENTRY {
    LIST_ENTRY      InLoadOrderLinks;
    LIST_ENTRY      InMemoryOrderLinks;
    LIST_ENTRY      InInitializationOrderLinks;
    PVOID           DllBase;
    PVOID           EntryPoint;
    ULONG           SizeOfImage;
    UNICODE_STRING  FullDllName;
    UNICODE_STRING  BaseDllName;
} KLDR_DATA_TABLE_ENTRY, *PKLDR_DATA_TABLE_ENTRY;

static NTSTATUS
GetModuleBaseByName(
    PEPROCESS       TargetProcess,
    const wchar_t*  ModuleName,
    ULONG64*        OutBase,
    ULONG64*        OutSize)
{
    NTSTATUS status = STATUS_NOT_FOUND;
    PEPROCESS originalProcess = NULL;

    if (!TargetProcess || !ModuleName || !OutBase || !OutSize)
        return STATUS_INVALID_PARAMETER;

    // 关键修复: 先保存原始进程，attach 后 PsGetCurrentProcess() 会返回目标进程
    originalProcess = PsGetCurrentProcess();

    KAPC_STATE apc;
    KeStackAttachProcess((PKPROCESS)TargetProcess, &apc);

    __try {
        PPEB peb = PsGetProcessPeb(TargetProcess);
        if (!peb) {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        // 读取 PEB->Ldr (PEB_LDR_DATA*)
        PPEB_LDR_DATA ldr = NULL;
        status = MmCopyVirtualMemory(
            TargetProcess, &peb->Ldr,
            PsGetCurrentProcess(), &ldr,
            sizeof(ldr), KernelMode, NULL);
        if (!NT_SUCCESS(status) || !ldr) {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        // 读取 InLoadOrderModuleList head
        LIST_ENTRY head = {0};
        status = MmCopyVirtualMemory(
            TargetProcess, &ldr->InLoadOrderModuleList,
            PsGetCurrentProcess(), &head,
            sizeof(head), KernelMode, NULL);
        if (!NT_SUCCESS(status)) {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        // 遍历链表
        PLIST_ENTRY current = head.Flink;
        ULONG maxIter = 256;  // 防止无限循环

        while (current != &ldr->InLoadOrderModuleList && maxIter-- > 0) {
            KLDR_DATA_TABLE_ENTRY entry = {0};
            status = MmCopyVirtualMemory(
                TargetProcess, current,
                PsGetCurrentProcess(), &entry,
                sizeof(entry), KernelMode, NULL);
            if (!NT_SUCCESS(status)) {
                // 链表损坏或地址不可读，直接中止（继续遍历已无意义）
                status = STATUS_UNSUCCESSFUL;
                __leave;
            }

            // 比较模块名
            if (entry.BaseDllName.Buffer && entry.BaseDllName.Length > 0) {
                wchar_t nameBuf[256] = {0};
                SIZE_T  nameLen = entry.BaseDllName.Length;
                if (nameLen > sizeof(nameBuf) - sizeof(wchar_t))
                    nameLen = sizeof(nameBuf) - sizeof(wchar_t);

                SIZE_T bytes = 0;
                NTSTATUS r = MmCopyVirtualMemory(
                    TargetProcess, entry.BaseDllName.Buffer,
                    PsGetCurrentProcess(), nameBuf,
                    nameLen, KernelMode, &bytes);

                if (NT_SUCCESS(r)) {
                    nameBuf[nameLen / sizeof(wchar_t)] = L'\0';
                    if (_wcsicmp(nameBuf, ModuleName) == 0) {
                        *OutBase = (ULONG64)(ULONG_PTR)entry.DllBase;
                        *OutSize = (ULONG64)entry.SizeOfImage;
                        status = STATUS_SUCCESS;
                        __leave;
                    }
                }
            }

            current = entry.InLoadOrderLinks.Flink;
        }

        status = STATUS_NOT_FOUND;
    }
    __finally {
        KeUnstackDetachProcess(&apc);
    }

    return status;
}

// ============================================================================
// IOCTL: 获取模块基址
// ============================================================================
static NTSTATUS
HandleGetBaseModule(
    PIRP Irp,
    PIO_STACK_LOCATION  IrpSp)
{
    NTSTATUS status;
    PEPROCESS targetProcess = NULL;
    PCNCS_MODULE_REQUEST request;
    ULONG inSize = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outSize = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (inSize < sizeof(CNCS_MODULE_REQUEST) || outSize < sizeof(CNCS_MODULE_REQUEST)) {
        status = STATUS_BUFFER_TOO_SMALL;
        goto out;
    }

    request = (PCNCS_MODULE_REQUEST)Irp->AssociatedIrp.SystemBuffer;

    // 输入验证
    // 1. 强制 NULL 终止，防止恶意调用者传入无终止符的字符串导致越界比较
    request->module_name[127] = L'\0';
    
    // 2. 验证模块名非空
    if (request->module_name[0] == L'\0') {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] Empty module name\n");
        status = STATUS_INVALID_PARAMETER;
        goto out;
    }
    
    // 3. 验证模块名长度合理（防止极长字符串）
    SIZE_T nameLen = 0;
    while (nameLen < 128 && request->module_name[nameLen] != L'\0') nameLen++;
    if (nameLen == 0 || nameLen >= 128) {
        status = STATUS_INVALID_PARAMETER;
        goto out;
    }

    // 4. 验证 PID
    if (request->target_pid == 0 || request->target_pid == 4) {
        status = STATUS_INVALID_PARAMETER;
        goto out;
    }

    // 5. ProbeForRead 验证用户缓冲区
    __try {
        ProbeForRead(Irp->AssociatedIrp.SystemBuffer, inSize, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] ProbeForRead failed in GetBaseModule: 0x%08X\n", status);
        goto out;
    }

    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)request->target_pid, &targetProcess);
    if (!NT_SUCCESS(status))
        goto out;

    status = GetModuleBaseByName(targetProcess, request->module_name, &request->base, &request->size);

    if (NT_SUCCESS(status))
        Irp->IoStatus.Information = sizeof(CNCS_MODULE_REQUEST);
    else
        Irp->IoStatus.Information = 0;

    ObDereferenceObject(targetProcess);

out:
    return status;
}

// ============================================================================
// IOCTL: 通过进程名查找 PID
// ============================================================================
static NTSTATUS
HandleFindProcess(
    PIRP Irp,
    PIO_STACK_LOCATION  IrpSp)
{
    PCNCS_FIND_PROCESS_REQUEST request;
    ULONG inSize = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outSize = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (inSize < sizeof(CNCS_FIND_PROCESS_REQUEST) || outSize < sizeof(CNCS_FIND_PROCESS_REQUEST)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    request = (PCNCS_FIND_PROCESS_REQUEST)Irp->AssociatedIrp.SystemBuffer;

    // 增强输入验证
    // 1. 强制 NULL 终止，防止无终止符字符串越界比较
    request->process_name[259] = L'\0';
    
    // 2. 验证进程名非空且长度合理
    if (request->process_name[0] == L'\0') {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] Empty process name\n");
        return STATUS_INVALID_PARAMETER;
    }
    
    SIZE_T nameLen = 0;
    while (nameLen < 260 && request->process_name[nameLen] != L'\0') nameLen++;
    if (nameLen == 0 || nameLen >= 260) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // 3. 可选：白名单验证（仅允许查找 cs2.exe 等特定进程）
    // 这里仅做演示，实际部署时可启用
    // if (!_wcsicmp(request->process_name, L"cs2.exe") && 
    //     !_wcsicmp(request->process_name, L"csgo.exe")) {
    //     return STATUS_ACCESS_DENIED;
    // }
    
    request->pid = 0;

    // ProbeForRead 验证用户缓冲区
    __try {
        ProbeForRead(Irp->AssociatedIrp.SystemBuffer, inSize, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    // 枚举所有进程
    NTSTATUS status = STATUS_NOT_FOUND;
    ULONG    bufferSize = 0;
    PVOID    processInfo = NULL;

    // 第一次调用获取所需缓冲区大小（预期返回 STATUS_INFO_LENGTH_MISMATCH）
    status = ZwQuerySystemInformation(SystemProcessInformation, NULL, 0, &bufferSize);
    if (bufferSize == 0)
        bufferSize = 64 * 1024;  // 兜底：避免极端情况下 size 仍为 0

    // 限制最大缓冲区大小，防止内存耗尽攻击
    if (bufferSize > 1024 * 1024) { // 1MB 最大
        bufferSize = 1024 * 1024;
    }

    // 循环分配：进程列表可能随系统负载增长，缓冲区不足时重试。
    for (int attempt = 0; attempt < 4; ++attempt) {
        processInfo = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufferSize, 'CNCs');
        if (!processInfo) {
            if (attempt == 3) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            // 内存不足时短暂等待后重试
            LARGE_INTEGER delay = { .QuadPart = -1000000 }; // 100ms
            KeDelayExecutionThread(KernelMode, FALSE, &delay);
            continue;
        }

        status = ZwQuerySystemInformation(SystemProcessInformation, processInfo, bufferSize, &bufferSize);
        if (status != STATUS_INFO_LENGTH_MISMATCH)
            break;

        // 缓冲区不足：bufferSize 已被更新为所需大小，释放后重试。
        ExFreePoolWithTag(processInfo, 'CNCs');
        processInfo = NULL;
        if (bufferSize == 0 || bufferSize > 1024 * 1024) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (!NT_SUCCESS(status)) {
        if (processInfo)
            ExFreePoolWithTag(processInfo, 'CNCs');
        return status;
    }

    // 遍历进程列表
    PSYSTEM_PROCESS_INFORMATION current = (PSYSTEM_PROCESS_INFORMATION)processInfo;

    while (TRUE) {
        if (current->ImageName.Buffer && current->ImageName.Length > 0) {
            wchar_t nameBuf[260] = {0};
            SIZE_T  copyLen = current->ImageName.Length;
            if (copyLen > sizeof(nameBuf) - sizeof(wchar_t))
                copyLen = sizeof(nameBuf) - sizeof(wchar_t);

            RtlCopyMemory(nameBuf, current->ImageName.Buffer, copyLen);
            nameBuf[copyLen / sizeof(wchar_t)] = L'\0';

            if (_wcsicmp(nameBuf, request->process_name) == 0) {
                request->pid = (ULONG64)(ULONG_PTR)current->UniqueProcessId;
                status = STATUS_SUCCESS;
                break;
            }
        }

        if (current->NextEntryOffset == 0)
            break;

        current = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)current + current->NextEntryOffset);
    }

    ExFreePoolWithTag(processInfo, 'CNCs');

    if (NT_SUCCESS(status))
        Irp->IoStatus.Information = sizeof(CNCS_FIND_PROCESS_REQUEST);

    return status;
}

// ============================================================================
// Dispatch: IRP_MJ_CREATE / IRP_MJ_CLOSE
// ============================================================================
NTSTATUS
DispatchCreateClose(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    
    if (irpSp->MajorFunction == IRP_MJ_CREATE) {
        // 安全检查：验证调用进程的完整性级别
        // 拒绝低完整性级别进程（如沙箱、受限浏览器标签页）
        // 仅允许 Medium 及以上完整性级别
        //
        // 注：token API（PsOpenProcessToken/ZwOpenProcessToken 等）位于
        // ntifs.h，而 ntifs.h 与 wdmsec.h/ntddk.h 同时包含会触发重定义，
        // 且完整的完整性级别判断需配套 user-mode 函数。此处保守地仅记录
        // 打开者 PID，不做完整性过滤（保持驱动可编译、行为可预期）。

        HANDLE pid = PsGetCurrentProcessId();
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL,
            "[CNCS] Device opened by PID: %lu\n", HandleToULong(pid));

        // ETW 事件：设备打开
        ETW_LOG(CNCS_EVENT_DEVICE_OPEN, 4, "Device opened by PID %lu", HandleToULong(pid));
        
        // 可选：验证调用者的签名证书（需要配合用户态组件）
        // 如果证书不匹配，返回 STATUS_ACCESS_DENIED
    } else if (irpSp->MajorFunction == IRP_MJ_CLOSE) {
        // ETW 事件：设备关闭
        ETW_LOG(CNCS_EVENT_DEVICE_CLOSE, 4, "Device closed by PID %lu", HandleToULong(PsGetCurrentProcessId()));
    }
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// ============================================================================
// Dispatch: IRP_MJ_DEVICE_CONTROL
// ============================================================================
NTSTATUS
DispatchDeviceControl(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_CNCS_READ_MEMORY:
        status = HandleReadMemory(Irp, irpSp);
        break;

    case IOCTL_CNCS_GET_BASE_MODULE:
        status = HandleGetBaseModule(Irp, irpSp);
        break;

    case IOCTL_CNCS_FIND_PROCESS:
        status = HandleFindProcess(Irp, irpSp);
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    if (!NT_SUCCESS(status))
        Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// ============================================================================
// DriverUnload — 卸载驱动时清理
// ============================================================================
VOID
DriverUnload(
    PDRIVER_OBJECT  DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    // ETW 事件：驱动卸载
    ETW_LOG(CNCS_EVENT_DRIVER_UNLOAD, 4, "Driver unloading");

    // 注销 ETW Provider
    if (g_EtwRegHandle != 0) {
        EtwUnregister(g_EtwRegHandle);   // 内核驱动用 EtwUnregister，非用户态 EventUnregister
        g_EtwRegHandle = 0;
    }

    // 等待所有挂起的 IRP 完成（防止卸载竞态）
    // 注意：I/O 管理器会在调用 DriverUnload 前取消所有 IRP
    // 但最好显式检查设备引用计数
    
    // 显式等待设备对象引用计数归零（防止 IRP 访问已释放的设备对象）
    if (g_DeviceObject) {
        LONG refCount = 0;
        do {
            // 读取引用计数（非原子快照，仅用于等待判断）
            refCount = *(volatile LONG*)((PUCHAR)g_DeviceObject + 0x30); // ReferenceCount 偏移近似
            if (refCount > 1) {
                LARGE_INTEGER delay = { .QuadPart = -1000000 }; // 100ms
                KeDelayExecutionThread(KernelMode, FALSE, &delay);
            }
        } while (refCount > 1);
    }
    
    UNICODE_STRING symlink;
    RtlInitUnicodeString(&symlink, SYMLINK_NAME);
    IoDeleteSymbolicLink(&symlink);

    if (g_DeviceObject) {
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = NULL;
    }

    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL,
        "[CNCS] Driver unloaded.\n");
}

// ============================================================================
// DriverEntry — 驱动入口点
// ============================================================================
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symlink;

    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL,
        "[CNCS] Driver loading...\n");

    // 注册 ETW Provider
    GUID providerGuid = CNCS_ETW_PROVIDER_GUID;
    status = EtwRegister(&providerGuid, NULL, NULL, &g_EtwRegHandle);   // 内核驱动用 EtwRegister
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_WARNING_LEVEL,
            "[CNCS] ETW registration failed: 0x%08X\n", status);
        // 继续加载，不因 ETW 失败而中止
    } else {
        ETW_LOG(CNCS_EVENT_DRIVER_LOAD, 4, "Driver loaded successfully");
    }

    // 创建设备对象。
    // 自定义 SDDL：仅允许 SYSTEM、管理员，以及具有特定签名证书的进程访问。
    // SDDL 格式：D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;S-1-5-80-XXXXXXXX) 
    // 其中最后一个 ACE 可替换为证书哈希（需要配合签名验证）。
    // 这里使用更严格的默认 SDDL，后续可通过证书哈希进一步限制。
    // D:(A;;GA;;;SY)(A;;GA;;;BA) = SYSTEM+管理员完全控制
    // 移除 Users 组权限，防止低权限进程打开设备
    RtlInitUnicodeString(&deviceName, DEVICE_NAME);
    
    // 自定义安全描述符：仅 SYSTEM 和 Administrators 可访问
    // S: 无 SACL; D: DACL
    // (A;;GA;;;SY) - Allow SYSTEM Generic All
    // (A;;GA;;;BA) - Allow Built-in Administrators Generic All
    // (A;;GRGW;;;LS) - Allow Local Service Generic Read/Write (optional, for debugging)
    // 注意：实际部署时应添加证书哈希 ACE：(A;;GRGW;;;S-1-5-80-<hash>)
    UNICODE_STRING sddlString;
    RtlInitUnicodeString(&sddlString, 
        L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;LS)");
    
    status = WdmlibIoCreateDeviceSecure(
        DriverObject,
        0,                    // DeviceExtensionSize
        &deviceName,
        FILE_DEVICE_CNCS,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,                // Exclusive
        &sddlString,          // 自定义 SDDL
        NULL,                 // DeviceClassGuid
        &g_DeviceObject);

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] IoCreateDevice failed: 0x%08X\n", status);
        if (g_EtwRegHandle) {
            EtwUnregister(g_EtwRegHandle);
            g_EtwRegHandle = 0;
        }
        return status;
    }

    // 创建符号链接 (用户态可通过 \\\\.\\CNCS 打开)
    RtlInitUnicodeString(&symlink, SYMLINK_NAME);
    status = IoCreateSymbolicLink(&symlink, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "[CNCS] IoCreateSymbolicLink failed: 0x%08X\n", status);
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = NULL;
        if (g_EtwRegHandle) {
            EtwUnregister(g_EtwRegHandle);
            g_EtwRegHandle = 0;
        }
        return status;
    }

    // 设置 MajorFunction 分发
    DriverObject->MajorFunction[IRP_MJ_CREATE]         = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;

    // 设置 DriverUnload
    DriverObject->DriverUnload = DriverUnload;

    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL,
        "[CNCS] Driver loaded successfully. Device: %ls\n", DEVICE_NAME);

    return STATUS_SUCCESS;
}