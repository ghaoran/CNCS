#include "Memory.hpp"
#include "core/kernel/KdLoader.hpp"

uint32_t pProcess::FindProcessIdByProcessName(const char* ProcessName)
{
	if (!kd_ || !kd_->IsReady())
		return 0;

	std::wstring wideProcessName;
	int wideCharLength = MultiByteToWideChar(CP_UTF8, 0, ProcessName, -1, nullptr, 0);
	if (wideCharLength > 0) {
		wideProcessName.resize(wideCharLength);
		MultiByteToWideChar(CP_UTF8, 0, ProcessName, -1, &wideProcessName[0], wideCharLength);
	}
	return (uint32_t)kd_->FindProcess(wideProcessName.c_str());
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

	if (!kd_ || !kd_->IsReady())
		return false;

	hwnd_ = GetWindowHandleFromProcessId(pid);
	if (!hwnd_)
		return false;

	// 通过驱动获取并验证 PID
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
	if (!kd_ || !kd_->IsReady())
		return { 0, 0 };

	std::wstring wideModule;
	int wideCharLength = MultiByteToWideChar(CP_UTF8, 0, lModule, -1, nullptr, 0);
	if (wideCharLength > 0)
	{
		wideModule.resize(wideCharLength);
		MultiByteToWideChar(CP_UTF8, 0, lModule, -1, &wideModule[0], wideCharLength);
	}

	KdLoader::ModuleInfo info{};
	if (kd_->GetModuleBase(pid_, wideModule.c_str(), info)) {
		return { (uintptr_t)info.base, (uintptr_t)info.size };
	}
	return { 0, 0 };
}

void pProcess::Close()
{
	// 内核模式下无需清理
}
