#include "Memory.hpp"
#include <tlhelp32.h>

uint32_t pProcess::FindProcessIdByProcessName(const char* ProcessName)
{
	std::wstring wideProcessName;
	int wideCharLength = MultiByteToWideChar(CP_UTF8, 0, ProcessName, -1, nullptr, 0);
	if (wideCharLength > 0) {
		wideProcessName.resize(wideCharLength);
		MultiByteToWideChar(CP_UTF8, 0, ProcessName, -1, &wideProcessName[0], wideCharLength);
	}

	// 用户态：通过 Toolhelp 进程快照按映像名查找 PID
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return 0;

	uint32_t pid = 0;
	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	if (Process32FirstW(snapshot, &entry)) {
		do {
			if (_wcsicmp(entry.szExeFile, wideProcessName.c_str()) == 0) {
				pid = entry.th32ProcessID;
				break;
			}
		} while (Process32NextW(snapshot, &entry));
	}
	CloseHandle(snapshot);
	return pid;
}

HWND pProcess::GetWindowHandleFromProcessId(DWORD ProcessId) {
	HWND hwnd = NULL;
	do {
		hwnd = FindWindowEx(NULL, hwnd, NULL, NULL);
		DWORD pid = 0;
		GetWindowThreadProcessId(hwnd, &pid);
		if (pid == ProcessId) {
			TCHAR windowTitle[MAX_PATH];
			GetWindowText(hwnd, windowTitle, MAX_PATH);
			if (IsWindowVisible(hwnd) && windowTitle[0] != '\0') {
				return hwnd;
			}
		}
	} while (hwnd != NULL);
	return NULL; // No main window found for the given process ID
}

bool pProcess::Attach(DWORD pid)
{
	if (!pid)
		return false;

	hwnd_ = GetWindowHandleFromProcessId(pid);
	if (!hwnd_)
		return false;

	// 用户态：打开进程句柄供 ReadProcessMemory 使用
	handle_ = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
	if (!handle_)
		return false;

	pid_ = pid;
	return true;
}

bool pProcess::AttachProcess(const char* ProcessName)
{
	pid_ = FindProcessIdByProcessName(ProcessName);
	return Attach(pid_);
}

bool pProcess::UpdateHWND()
{
	hwnd_ = this->GetWindowHandleFromProcessId(pid_);
	return hwnd_ != nullptr;
}

ProcessModule pProcess::GetModule(const char* lModule)
{
	if (!handle_)
		return { 0, 0 };

	std::wstring wideModule;
	int wideCharLength = MultiByteToWideChar(CP_UTF8, 0, lModule, -1, nullptr, 0);
	if (wideCharLength > 0)
	{
		wideModule.resize(wideCharLength);
		MultiByteToWideChar(CP_UTF8, 0, lModule, -1, &wideModule[0], wideCharLength);
	}

	// 用户态：通过 Toolhelp 模块快照获取模块基址与大小
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid_);
	if (snapshot == INVALID_HANDLE_VALUE)
		return { 0, 0 };

	ProcessModule result{ 0, 0 };
	MODULEENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	if (Module32FirstW(snapshot, &entry)) {
		do {
			if (_wcsicmp(entry.szModule, wideModule.c_str()) == 0) {
				result.base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
				result.size = static_cast<uintptr_t>(entry.modBaseSize);
				break;
			}
		} while (Module32NextW(snapshot, &entry));
	}
	CloseHandle(snapshot);
	return result;
}

void pProcess::Close()
{
	// 关闭用户态进程句柄
	if (handle_) {
		CloseHandle(handle_);
		handle_ = nullptr;
	}
}
