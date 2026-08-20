#include "Window.hpp"

ID3D11Device* Window::device = nullptr;
ID3D11DeviceContext* Window::device_context = nullptr;
IDXGISwapChain* Window::swap_chain = nullptr;
ID3D11RenderTargetView* Window::render_targetview = nullptr;

bool Window::vsync = false;
HWND Window::hwnd = nullptr;
HWND Window::viewport = nullptr;
WNDCLASSEX Window::wc = { };

extern LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

bool Window::CreateDevice()
{
	// First we setup our swap chain, this basically just holds a bunch of descriptors for the swap chain.
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));

	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	//sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 0;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

	// create device and swap chain
	HRESULT result = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		featureLevelArray,
		2,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&featureLevel,
		&device_context);

	// if the hardware isn't supported create with WARP (basically just a different renderer)
	if (result == DXGI_ERROR_UNSUPPORTED) {
		result = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0U,
			featureLevelArray,
			2, D3D11_SDK_VERSION,
			&sd,
			&swap_chain,
			&device,
			&featureLevel,
			&device_context);

		LOGF(FATAL, "DXGI_ERROR | 使用 D3D_DRIVER_TYPE_WARP 创建");
	}

	if (result != S_OK) {
		LOGF(FATAL, "设备异常");
		return false;
	}

	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer)
	{
		device->CreateRenderTargetView(back_buffer, nullptr, &render_targetview);
		back_buffer->Release();

		LOGF(VERBOSE, "设备已创建");
		return true;
	}

	LOGF(FATAL, "创建设备失败");
	return false;
}

void Window::DestroyDevice()
{
	if (device)
	{
		if (device_context)
			device_context->Release();
		if (render_targetview)
			render_targetview->Release();
		if (swap_chain)
			swap_chain->Release();
		device->Release();

		// If not might cause a crash due to dangling pointers
		device = nullptr;
		device_context = nullptr;
		render_targetview = nullptr;
		swap_chain = nullptr;

		LOGF(VERBOSE, "设备已释放");
	}
	else
		LOGF(WARNING, "未找到要销毁的设备");
}

bool Window::SpawnWindow()
{
	ImGui_ImplWin32_EnableDpiAwareness();
	wc.cbSize = sizeof(wc);
	//wc.style = CS_CLASSDC;
	wc.style = 0;
	wc.hInstance = GetModuleHandle(0);
	wc.lpszClassName = "wa";
	wc.lpfnWndProc = window_procedure;
	//wc.cbClsExtra = 0;
	//wc.cbWndExtra = 0;

	//wc.hIcon = 0;
	//wc.hIconSm = 0;
	//wc.hCursor = 0;
	//wc.hbrBackground = 0;
	//wc.lpszMenuName = 0;

	// register our class
	RegisterClassEx(&wc);

	int width = GetSystemMetrics(SM_CXSCREEN);
	int height = GetSystemMetrics(SM_CYSCREEN);

	hwnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
		"wa",
		"wa",
		WS_POPUP | WS_VISIBLE,
		0, 0, width, height,
		NULL,
		NULL,
		wc.hInstance,
		NULL
	);

	if (hwnd == NULL) {
		LOGF(FATAL, "创建窗口失败");
		return false;
	}
	
	if (!SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), BYTE(255), LWA_ALPHA)) {
		LOGF(FATAL, "设置分层窗口属性失败");
		return false;
	}

	MARGINS margins = { -1 };
	DwmExtendFrameIntoClientArea(hwnd, &margins);

	// show + update window
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	SetForeground(hwnd);

	LOGF(VERBOSE, "窗口已创建，句柄 {}，尺寸 {}宽 {}高", (uintptr_t)hwnd, width, height);
	return true;
}

void Window::DespawnWindow()
{
	DestroyWindow(hwnd);
	UnregisterClass(wc.lpszClassName, wc.hInstance);
	LOGF(VERBOSE, "窗口已销毁");
}

bool Window::CreateImGui()
{
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Set the ImGui IO to the Win32 window
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	
	io.IniFilename = nullptr; // Disable saving to .ini file

	io.ConfigViewportsNoTaskBarIcon = true; // Disable showing in taskbar completely
	io.ConfigViewportsNoAutoMerge = true;

	// Initialize ImGui for the Win32 library
	if (!ImGui_ImplWin32_Init(hwnd)) {
		LOGF(FATAL, "ImGui_ImplWin32_Init 失败");
		return false;
	}

	// Initialize ImGui for DirectX 11.
	if (!ImGui_ImplDX11_Init(device, device_context)) {
		LOGF(FATAL, "ImGui_ImplDX11_Init 失败");
		return false;
	}

	LOGF(VERBOSE, "ImGui 已初始化");
	return true;
}

void Window::DestroyImGui()
{
	// Cleanup ImGui by shutting down DirectX11, the Win32 Platform and Destroying the ImGui context.
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	LOGF(VERBOSE, "ImGui 已销毁");
}

void Window::StartRender()
{
	// handle windows messages
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_QUIT)
			shouldRun = false;
	}

	// begin a new frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void Window::EndRender()
{
	// Render ImGui
	ImGui::Render();

	// Make a color that's clear / transparent
	float color[4]{ 0, 0, 0, 0 };

	// Set the render target and then clear it
	device_context->OMSetRenderTargets(1, &render_targetview, nullptr);
	device_context->ClearRenderTargetView(render_targetview, color);

	// Render ImGui draw data.
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	auto io = ImGui::GetIO();

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
	
	if (vsync) // Present rendered frame with V-Sync
		swap_chain->Present(1U, 0U);
	else // Present rendered frame without V-Sync
		swap_chain->Present(0U, 0U);
}

void Window::SetTopMost(HWND window, bool up_down) {
	SetWindowPos(
		window,
		up_down ? HWND_TOPMOST : HWND_NOTOPMOST,
		0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER
	);
}

void Window::SetClickthrough(HWND window, bool clickthrough)
{
	LONG_PTR style = GetWindowLongPtr(window, GWL_EXSTYLE);

	style |= WS_EX_LAYERED;

	if (clickthrough)
		style |= WS_EX_TRANSPARENT;
	else
		style &= ~WS_EX_TRANSPARENT;

	SetWindowLongPtr(window, GWL_EXSTYLE, style);
}

void Window::SetBounds(HWND window, RECT bounds) {
	SetWindowPos(
		window,
		nullptr,
		bounds.left, bounds.top,
		bounds.right - bounds.left,
		bounds.bottom - bounds.top,
		SWP_NOZORDER | SWP_NOACTIVATE
	);

	UpdateWindow(window);
}

bool Window::SetAffinity(HWND window, WindowAffinity afi) {
	auto mode = WDA_NONE;
	std::string mode_str = "禁用";

	switch (afi) {
	case WindowAffinity::Black:
		mode = WDA_MONITOR;
		mode_str = "黑屏";
		break;
	case WindowAffinity::Invisible:
		mode = WDA_EXCLUDEFROMCAPTURE;
		mode_str = "不可见";
		break;
	default:
		mode = WDA_NONE;
		mode_str = "禁用";
		break;
	}

	auto status = SetWindowDisplayAffinity(window, mode);

	if (status)
		LOGF(VERBOSE, "设置窗口亲和性为 " + mode_str);
	else
		LOGF(FATAL, "设置窗口亲和性失败 " + mode_str);

	return status;
}

void Window::SetForeground(HWND window) {
	if (!IsWindowInForeground(window))
		BringToForeground(window);
}

void Window::SetVSync(bool enable) {
	vsync = enable;
	LOGF(VERBOSE, "垂直同步现在为 {}", (enable ? "启用" : "禁用"));
}


// declaration of the ImGui_ImplWin32_WndProcHandler function
// basically integrates ImGui with the Windows message loop so ImGui can process input and events
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// set up ImGui window procedure handler
	if (ImGui_ImplWin32_WndProcHandler(window, msg, wParam, lParam))
		return true;

	// switch that disables alt application and checks for if the user tries to close the window.
	switch (msg)
	{
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu (imgui uses it in their example :shrug:)
			return 0;
		break;
	case WM_DESTROY: // We dont handle this event
		LOGF(VERBOSE, "窗口过程 WM_DESTROY 事件触发"); // We dont want to exit if a child window is closed, as they are when changing tabs
		break;
	case WM_CLOSE:
		LOGF(VERBOSE, "窗口过程 WM_CLOSE 事件触发");
		//DestroyWindow(window); // Redirect the event to WM_DESTROY
		Window::shouldRun = false; // Exit render thread, it will cleanup on exit
		break;
	case WM_SIZE:
		//DEBUG_LOG(out, "Window procedure WM_SIZE event triggered");
		if (Window::device != nullptr && wParam != SIZE_MINIMIZED)
		{
			if (Window::render_targetview != nullptr) {
				Window::render_targetview->Release();
				Window::render_targetview = nullptr;
			}

			// Resize the swap chain
			Window::swap_chain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);

			// Recreate the render target view
			ID3D11Texture2D* back_buffer{ nullptr };
			Window::swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

			// if back buffer is obtained then we can create render target view and release the back buffer again
			if (back_buffer)
			{
				Window::device->CreateRenderTargetView(back_buffer, nullptr, &Window::render_targetview);
				back_buffer->Release();

				return true;
			}
		}
		return 0;
	case WM_DPICHANGED:
		LOGF(VERBOSE, "窗口过程 WM_DPICHANGED 事件触发");
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
		{
			const RECT* suggested_rect = (RECT*)lParam;

			SetWindowPos(
				Window::hwnd, 
				nullptr, 
				suggested_rect->left, suggested_rect->top, 
				suggested_rect->right - suggested_rect->left, 
				suggested_rect->bottom - suggested_rect->top, 
				SWP_NOZORDER | SWP_NOACTIVATE
			);
		}
		break;
	}

	return DefWindowProc(window, msg, wParam, lParam);
}
