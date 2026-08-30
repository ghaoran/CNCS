#pragma once

#include <atomic>
#include <d3d11.h>
#include <dwmapi.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")

enum class WindowAffinity {
	Disabled,
	Black,
	Invisible,
};

class Window {
public:
	static bool CreateDevice();
	static void DestroyDevice();

	static bool SpawnWindow();
	static void DespawnWindow();

	static bool CreateImGui();
	static void DestroyImGui();

	static void StartRender();
	static void EndRender();

	static bool vsync;
	static HWND hwnd;
	static HWND viewport;
	static WNDCLASSEX wc;

	static bool IsWindowInForeground(HWND window) { return GetForegroundWindow() == window; }
	static bool BringToForeground(HWND window) { return SetForegroundWindow(window); }

	static void SetTopMost(HWND window, bool up_down = true);
	static void SetClickthrough(HWND window, bool clickthrough = true);
	static void SetBounds(HWND window, RECT bounds);
	static bool SetAffinity(HWND window, WindowAffinity afi);
	static void SetForeground(HWND window);
	static void SetVSync(bool enable = false);
	static void SetParent(HWND window, HWND parent);

	static ID3D11Device* device;
	static ID3D11DeviceContext* device_context;
	static IDXGISwapChain* swap_chain;
	static ID3D11RenderTargetView* render_targetview;

	inline static RECT bounds;
	// vsync：主线程 Init 写、渲染线程 Present 读；shouldRun：渲染线程
	// 消息循环/窗口过程写读。均用 atomic 消除跨线程 data race。
	inline static std::atomic<bool> vsync{ false };
	inline static std::atomic<bool> shouldRun{ true };

};