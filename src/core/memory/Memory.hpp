#pragma once

#include <vector>
#include <Windows.h>
#include <string>

struct ProcessModule
{
	uintptr_t base, size;
};

// 用户态进程封装：OpenProcess + ReadProcessMemory 读取目标进程内存，
// 不再依赖内核驱动。
class pProcess
{
public:
	DWORD		  pid_ = 0; // process id
	HWND		  hwnd_ = nullptr; // window handle

public:
	bool AttachProcess(const char* process_name);
	bool UpdateHWND();
	void Close();

public:
	ProcessModule GetModule(const char* module_name);

	bool read_raw(uintptr_t address, void* buffer, size_t size)
	{
		if (!handle_ || !buffer || !size)
			return false;
		SIZE_T bytes_read = 0;
		if (!ReadProcessMemory(handle_, reinterpret_cast<LPCVOID>(address),
			buffer, size, &bytes_read))
			return false;
		return bytes_read == size;
	}

	template<class T>
	T read(uintptr_t address)
	{
		T buffer{};
		read_raw(address, &buffer, sizeof(T));
		return buffer; // 读取失败时返回零值，与原驱动模式语义一致
	}

private:
	HANDLE handle_ = nullptr; // 目标进程句柄（用户态读取用）

	uint32_t FindProcessIdByProcessName(const char* process_name);
	HWND GetWindowHandleFromProcessId(DWORD ProcessId);
	bool Attach(DWORD pid);
};
