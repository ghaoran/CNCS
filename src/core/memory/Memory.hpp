#pragma once

#include <vector>
#include <Windows.h>
#include <string>

// 内核驱动通信模块（需完整定义：内联的 read/read_raw 调用了
// kd_ 的成员函数，仅前置声明会编译失败）
#include "core/kernel/KdLoader.hpp"

struct ProcessModule
{
	uintptr_t base, size;
};

class pProcess
{
public:
	DWORD		  pid_; // process id
	HWND		  hwnd_; // window handle

	// 内核驱动实例（必须有效才能工作）
	KdLoader*     kd_ = nullptr;

public:
	bool AttachProcess(const char* process_name);
	bool UpdateHWND();
	void Close();

	// 设置内核驱动加载器（必须在 AttachProcess 之前调用）
	void SetKernelDriver(KdLoader* kd) { kd_ = kd; }

public:
	ProcessModule GetModule(const char* module_name);

	bool read_raw(uintptr_t address, void* buffer, size_t size)
	{
		if (!kd_ || !kd_->IsReady())
			return false;
		return kd_->ReadRaw(pid_, address, buffer, size);
	}

	template<class T>
	T read(uintptr_t address)
	{
		if (!kd_ || !kd_->IsReady())
			return T{};
		return kd_->Read<T>(pid_, address);
	}

private:
	uint32_t FindProcessIdByProcessName(const char* process_name);
	HWND GetWindowHandleFromProcessId(DWORD ProcessId);
	bool Attach(DWORD pid);
};
