#include "Menu.hpp"

#include "core/engine/cache/Cache.hpp"
#include "gui/renderer/Renderer.hpp" // Circular dependency
#include "gui/renderer/window/Window.hpp" // Circular dependency
#include "assets/fonts/Icons.h"

namespace {
    // 紧凑颜色选择器（无标签，悬停提示），后面自动 SameLine。
    void ColorPicker(const char* id, color_t& color, const char* tip) {
        ImGui::ColorEdit4(id, color.data(),
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
        if (tip && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tip);
        ImGui::SameLine();
    }

    // 队伍/敌人一对颜色选择。
    void TeamColorPair(const char* base, color_t& team, color_t& enemy, const char* tip) {
        ColorPicker((std::string(base) + "##team").c_str(), team, (std::string(tip) + "（队友）").c_str());
        ColorPicker((std::string(base) + "##enemy").c_str(), enemy, (std::string(tip) + "（敌人）").c_str());
        ImGui::NewLine();
    }
}

bool Menu::Init() {
	return GetInstance().InitImpl();
}

void Menu::Render() {
	return GetInstance().RenderImpl();
}

void Menu::RenderStartupHelp() {
	return GetInstance().RenderStartupHelpImpl();
}

ImVec2 Menu::GetPos() {
	return GetInstance().pos;
}

ImVec2 Menu::GetSize() {
	return GetInstance().size;
}

bool Menu::InitImpl() {
	SetupStyles();

	LOGF(INFO, "菜单初始化成功...");
	return true;
}

void Menu::RenderImpl() {
	if (!isSetup)
		return;

	auto& io = ImGui::GetIO();
	auto screen = io.DisplaySize;

#ifdef _DEBUG
	static auto title = "Hello, CNCS! [DEV]";
#else
	static auto title = "Hello, CNCS!";
#endif

	ImGui::SetNextWindowSize(ImVec2(700, 480), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(screen.x / 2 - 350, screen.y / 2 - 240), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
		ImGui::End();
		return;
	}

	this->pos = ImGui::GetWindowPos();
	this->size = ImGui::GetWindowSize();

	const ImVec4 accent = ImVec4(0.42f, 0.56f, 1.00f, 1.00f);

	// 分节标题
	auto section = [&](const char* title) {
		ImGui::Spacing();
		ImGui::TextColored(accent, "%s", title);
		ImGui::Separator();
		ImGui::Spacing();
	};

	// 触发键 / 部位下拉
	static const char* aim_keys[] = { "始终开启", "鼠标右键", "鼠标左键", "鼠标中键", "Shift", "Alt", "Ctrl", "E 键", "F 键" };
	static const int aim_key_values[] = { 0, VK_RBUTTON, VK_LBUTTON, VK_MBUTTON, VK_SHIFT, VK_MENU, VK_CONTROL, 'E', 'F' };
	static const char* aim_bones[] = { "头部", "脖子", "胸部", "骨盆" };
	static const int aim_bone_values[] = { bone_index::head, bone_index::neck, bone_index::chest, bone_index::pelvis };

	auto key_combo = [&](const char* id, int& key) {
		int cur = 0;
		for (int i = 0; i < IM_ARRAYSIZE(aim_key_values); i++)
			if (key == aim_key_values[i]) { cur = i; break; }
		if (ImGui::Combo(id, &cur, aim_keys, IM_ARRAYSIZE(aim_keys)))
			key = aim_key_values[cur];
	};

	auto bone_combo = [&](const char* id, int& bone) {
		int cur = 0;
		for (int i = 0; i < IM_ARRAYSIZE(aim_bone_values); i++)
			if (bone == aim_bone_values[i]) { cur = i; break; }
		if (ImGui::Combo(id, &cur, aim_bones, IM_ARRAYSIZE(aim_bones)))
			bone = aim_bone_values[cur];
	};

	// ================= 视觉 =================
	auto render_visual = [&]() {
		section("透视");
		ImGui::Checkbox("方框", &cfg::esp::box);
		ImGui::BeginDisabled(!cfg::esp::box);
		TeamColorPair("box", cfg::esp::colors::box_team, cfg::esp::colors::box_enemy, "方框颜色");
		ImGui::EndDisabled();
		ImGui::Checkbox("骨骼", &cfg::esp::skeleton);
		ImGui::BeginDisabled(!cfg::esp::skeleton);
		TeamColorPair("skeleton", cfg::esp::colors::skeleton_team, cfg::esp::colors::skeleton_enemy, "骨骼颜色");
		ImGui::EndDisabled();
		ImGui::Checkbox("头部追踪", &cfg::esp::head_tracker);
		ImGui::BeginDisabled(!cfg::esp::head_tracker);
		TeamColorPair("tracker", cfg::esp::colors::tracker_team, cfg::esp::colors::tracker_enemy, "头部追踪颜色");
		ImGui::EndDisabled();
		ImGui::Checkbox("射线", &cfg::esp::tracers);
		ImGui::BeginDisabled(!cfg::esp::tracers);
		TeamColorPair("tracer", cfg::esp::colors::tracer_team, cfg::esp::colors::tracer_enemy, "射线颜色");
		ImGui::EndDisabled();

		section("属性");
		ImGui::Checkbox("生命值", &cfg::esp::health);
		ImGui::BeginDisabled(!cfg::esp::health);
		ImGui::Checkbox("生命数值", &cfg::esp::health_number);
		ImGui::EndDisabled();
		ImGui::Checkbox("护甲", &cfg::esp::armor);

		section("玩家信息");
		ImGui::Checkbox("名字", &cfg::esp::flags::name);
		ImGui::SameLine();
		ImGui::Checkbox("武器", &cfg::esp::flags::weapon);
		ImGui::SameLine();
		ImGui::Checkbox("弹药", &cfg::esp::flags::ammo);
		ImGui::SameLine();
		ImGui::Checkbox("金钱", &cfg::esp::flags::money);
		ImGui::SameLine();
		ImGui::Checkbox("延迟", &cfg::esp::flags::ping);

		section("状态标志");
		ImGui::Checkbox("被闪", &cfg::esp::flags::flashed);
		ImGui::BeginDisabled(!cfg::esp::flags::flashed);
		TeamColorPair("flashed", cfg::esp::colors::flags::flashed_team, cfg::esp::colors::flags::flashed_enemy, "被闪颜色");
		ImGui::EndDisabled();
		ImGui::Checkbox("换弹", &cfg::esp::flags::reloading);
		ImGui::BeginDisabled(!cfg::esp::flags::reloading);
		TeamColorPair("reloading", cfg::esp::colors::flags::reloading_team, cfg::esp::colors::flags::reloading_enemy, "换弹颜色");
		ImGui::EndDisabled();
		ImGui::Checkbox("拆弹", &cfg::esp::flags::defusing);
		ImGui::BeginDisabled(!cfg::esp::flags::defusing);
		TeamColorPair("defusing", cfg::esp::colors::flags::defusing_team, cfg::esp::colors::flags::defusing_enemy, "拆弹颜色");
		ImGui::EndDisabled();
		ImGui::Checkbox("开镜", &cfg::esp::flags::scoped);
		ImGui::BeginDisabled(!cfg::esp::flags::scoped);
		TeamColorPair("scoped", cfg::esp::colors::flags::scoped_team, cfg::esp::colors::flags::scoped_enemy, "开镜颜色");
		ImGui::EndDisabled();
		ImGui::Checkbox("持有C4", &cfg::esp::flags::has_c4);
		ImGui::BeginDisabled(!cfg::esp::flags::has_c4);
		TeamColorPair("c4", cfg::esp::colors::flags::c4_team, cfg::esp::colors::flags::c4_enemy, "持有C4颜色");
		ImGui::EndDisabled();

		section("过滤");
		ImGui::Checkbox("已被发现", &cfg::esp::spotted);
		ImGui::SetItemTooltip("仅显示已被发现的敌人");
		ImGui::Checkbox("仅显示可见", &cfg::esp::visible_only);
		ImGui::SetItemTooltip("掩体判断：被墙壁遮挡的敌人不显示");
		ImGui::Checkbox("显示队友", &cfg::esp::team);
	};

	// ================= 战斗 =================
	auto render_combat = [&]() {
		section("自瞄");
		ImGui::Checkbox("启用##a", &cfg::aimbot::enabled);
		ImGui::BeginDisabled(!cfg::aimbot::enabled);
		key_combo("触发键##a", cfg::aimbot::key);
		bone_combo("瞄准部位", cfg::aimbot::bone);
		ImGui::SetItemTooltip("固定瞄准部位（开启下方「露瞄」后此选项被忽略）");
		ImGui::Checkbox("露瞄（智能部位）", &cfg::aimbot::smart_bone);
		ImGui::SetItemTooltip("自动选可见的最高伤害部位：头>颈>胸>骨盆\n露哪打哪，全露打头，非满血优先爆头致命");
		ImGui::SliderFloat("范围", &cfg::aimbot::fov, 10.f, 1000.f, "%.0f px");
		ImGui::SetItemTooltip("距离准星多远内锁定目标（像素）");
		ImGui::SliderFloat("平滑", &cfg::aimbot::smoothing, 0.f, 1.f, "%.2f");
		ImGui::SetItemTooltip("0 = 快速追踪，1 = 最平滑\n动态平滑：离目标远时快、接近时自动减速");
		ImGui::Checkbox("显示范围圈", &cfg::aimbot::show_fov);
		ImGui::Checkbox("只瞄可见目标", &cfg::aimbot::visible_only);
		ImGui::SetItemTooltip("掩体判断：被墙壁遮挡的敌人不锁定（露头就锁）");
		ImGui::EndDisabled();

		section("自动扳机");
		ImGui::Checkbox("启用##t", &cfg::triggerbot::enabled);
		ImGui::BeginDisabled(!cfg::triggerbot::enabled);
		key_combo("触发键##t", cfg::triggerbot::key);
		ImGui::Checkbox("只对可见开火", &cfg::triggerbot::visible_only);
		ImGui::SetItemTooltip("掩体判断：被遮挡的敌人不触发");
		ImGui::TextDisabled("准星对准敌人时自动开火");
		ImGui::EndDisabled();

		section("模式");
		ImGui::Checkbox("死亡竞赛", &cfg::deathmatch);
		ImGui::SetItemTooltip("敌我不分：所有玩家视为敌人（死亡竞赛模式）");
	};

	// ================= 世界 =================
	auto render_world = [&]() {
		section("炸弹");
		ImGui::Checkbox("炸弹ESP", &cfg::esp::bomb);
		ImGui::BeginDisabled(!cfg::esp::bomb);
		ImGui::SameLine();
		ColorPicker("bomb_color", cfg::esp::colors::bomb, "炸弹颜色");
		ImGui::NewLine();
		ImGui::EndDisabled();
		ImGui::Checkbox("炸弹位置", &cfg::world::bomb::location);
		ImGui::Checkbox("炸弹计时", &cfg::world::bomb::timer);

		section("观战列表");
		ImGui::Checkbox("启用##spectators", &cfg::world::spectators::enabled);
		ImGui::BeginDisabled(!cfg::world::spectators::enabled);
		ImGui::Checkbox("详细信息", &cfg::world::spectators::detailed);
		ImGui::Checkbox("仅显示观战我的人", &cfg::world::spectators::self_only);
		ImGui::EndDisabled();

		section("雷达");
		ImGui::Checkbox("启用##radar", &cfg::world::radar::enabled);
		ImGui::BeginDisabled(!cfg::world::radar::enabled);
		ImGui::SliderFloat("范围", &cfg::world::radar::range, 100.f, 8000.f, "%.0f 单位");
		ImGui::Checkbox("禁用旋转", &cfg::world::radar::no_rotate);
		ImGui::EndDisabled();

		section("杂项");
		ImGui::Checkbox("准星", &cfg::world::crosshair::enabled);
		ImGui::Checkbox("速度图表", &cfg::world::velocity::enabled);
#ifdef _DEBUG
		ImGui::BeginDisabled(!cfg::world::velocity::enabled);
		ImGui::SliderInt("采样率", &cfg::world::velocity::sample_rate, 1, 100);
		ImGui::SliderFloat("采样时长", &cfg::world::velocity::sample_length, 1.f, 20.f, "%.1f 秒");
		ImGui::EndDisabled();
#endif
	};

	// ================= 系统 =================
	auto render_system = [&]() {
		section("通用");
		ImGui::Checkbox("启用##global", &cfg::enabled);
		ImGui::SetItemTooltip("总开关：关闭后所有功能暂停");

		section("界面");
		ImGui::Checkbox("水印", &cfg::settings::watermark);
		if (ImGui::Checkbox("防截屏", &cfg::settings::streamproof)) {
			Window::SetAffinity(Window::hwnd,
				cfg::settings::streamproof ? WindowAffinity::Invisible : WindowAffinity::Disabled);
		}
		ImGui::SetItemTooltip("防止 OBS / 录屏捕获叠加层");

		section("性能");
		if (ImGui::Checkbox("垂直同步", &cfg::settings::vsync))
			Window::vsync = cfg::settings::vsync;
		ImGui::Checkbox("释放CPU", &cfg::settings::free_cpu);
		ImGui::SetItemTooltip("让引擎线程休眠以降低 CPU 占用");

		section("帮助");
		ImGui::TextWrapped(
			"按键：Insert / 右Shift 打开菜单，End 关闭\n\n"
			"性能不佳或卡顿时可尝试：\n"
			"\t- 关闭垂直同步（上方「性能」）\n"
			"\t- 游戏内：高级视频 > 垂直同步 > 已禁用\n"
			"\t- 最后手段：关闭「释放CPU」（提高延迟但更流畅）"
		);

#ifdef _DEBUG
		section("开发");
		if (ImGui::Checkbox("控制台", &cfg::dev::console))
			if (!cfg::dev::console) LogHelper::Free();
		static int key_out;
		if (ImGui::Button("设置菜单按键")) {
			for (int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; i++)
				if (ImGui::IsKeyPressed((ImGuiKey)i)) { key_out = i; LOGF(VERBOSE, "菜单按键已改为 {}", key_out); break; }
		}
		ImGui::SliderInt("缓存刷新率", &cfg::dev::cache_refresh_rate, 0, 100, "%dms");
		ImGui::Checkbox("强制显示标志", &cfg::dev::force_show_flags);
#endif
	};

	// ================= 布局 =================
	if (ImGui::BeginTabBar("##main_tabs", ImGuiTabBarFlags_None)) {
		if (ImGui::BeginTabItem(tabs[0].label.c_str())) {
			ImGui::BeginChild("##tab_visual", ImVec2(0, 0), false);
			render_visual();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(tabs[1].label.c_str())) {
			ImGui::BeginChild("##tab_combat", ImVec2(0, 0), false);
			render_combat();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(tabs[2].label.c_str())) {
			ImGui::BeginChild("##tab_world", ImVec2(0, 0), false);
			render_world();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(tabs[3].label.c_str())) {
			ImGui::BeginChild("##tab_system", ImVec2(0, 0), false);
			render_system();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

void Menu::SetupStyles() {
	ImGuiStyle& style = ImGui::GetStyle();

	// ---- 简约深色主题（统一蓝紫强调色）----
	const ImVec4 accent     = ImVec4(0.42f, 0.56f, 1.00f, 1.00f); // #6B8FFF
	const ImVec4 accent_dim = ImVec4(0.30f, 0.40f, 0.80f, 1.00f);
	const ImVec4 bg         = ImVec4(0.043f, 0.051f, 0.063f, 1.00f); // #0B0D10
	const ImVec4 surface    = ImVec4(0.063f, 0.075f, 0.094f, 1.00f); // #101318
	const ImVec4 surface2   = ImVec4(0.102f, 0.118f, 0.149f, 1.00f); // #1A1E26
	const ImVec4 surface3   = ImVec4(0.137f, 0.161f, 0.200f, 1.00f); // #232933
	const ImVec4 border     = ImVec4(0.125f, 0.145f, 0.173f, 1.00f); // #20252C
	const ImVec4 text       = ImVec4(0.91f, 0.92f, 0.93f, 1.00f);    // #E8EAED
	const ImVec4 text_dim   = ImVec4(0.36f, 0.39f, 0.43f, 1.00f);    // #5C636E

	style.Colors[ImGuiCol_Text] = text;
	style.Colors[ImGuiCol_TextDisabled] = text_dim;
	style.Colors[ImGuiCol_WindowBg] = bg;
	style.Colors[ImGuiCol_ChildBg] = surface;
	style.Colors[ImGuiCol_PopupBg] = surface2;
	style.Colors[ImGuiCol_Border] = border;
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_FrameBg] = surface2;
	style.Colors[ImGuiCol_FrameBgHovered] = surface3;
	style.Colors[ImGuiCol_FrameBgActive] = accent_dim;
	style.Colors[ImGuiCol_TitleBg] = bg;
	style.Colors[ImGuiCol_TitleBgActive] = bg;
	style.Colors[ImGuiCol_TitleBgCollapsed] = bg;
	style.Colors[ImGuiCol_MenuBarBg] = surface;
	style.Colors[ImGuiCol_ScrollbarBg] = bg;
	style.Colors[ImGuiCol_ScrollbarGrab] = surface3;
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = text_dim;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = accent_dim;
	style.Colors[ImGuiCol_CheckMark] = accent;
	style.Colors[ImGuiCol_SliderGrab] = accent;
	style.Colors[ImGuiCol_SliderGrabActive] = accent_dim;
	style.Colors[ImGuiCol_Button] = surface2;
	style.Colors[ImGuiCol_ButtonHovered] = surface3;
	style.Colors[ImGuiCol_ButtonActive] = accent_dim;
	style.Colors[ImGuiCol_Header] = surface2;
	style.Colors[ImGuiCol_HeaderHovered] = surface3;
	style.Colors[ImGuiCol_HeaderActive] = accent_dim;
	style.Colors[ImGuiCol_Separator] = border;
	style.Colors[ImGuiCol_SeparatorHovered] = accent_dim;
	style.Colors[ImGuiCol_SeparatorActive] = accent;
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
	style.Colors[ImGuiCol_ResizeGripHovered] = accent_dim;
	style.Colors[ImGuiCol_ResizeGripActive] = accent;
	style.Colors[ImGuiCol_Tab] = surface;
	style.Colors[ImGuiCol_TabHovered] = surface3;
	style.Colors[ImGuiCol_TabActive] = accent_dim;
	style.Colors[ImGuiCol_TabUnfocused] = surface;
	style.Colors[ImGuiCol_TabUnfocusedActive] = surface2;
	style.Colors[ImGuiCol_TextSelectedBg] = accent_dim;
	style.Colors[ImGuiCol_NavHighlight] = accent;
	style.Colors[ImGuiCol_DragDropTarget] = accent;

	// ---- 圆角 ----
	style.WindowRounding = 14.f;
	style.ChildRounding = 8.f;
	style.FrameRounding = 6.f;
	style.PopupRounding = 8.f;
	style.GrabRounding = 6.f;
	style.TabRounding = 6.f;
	style.ScrollbarRounding = 4.f;

	// ---- 间距（简约宽松）----
	style.WindowPadding = ImVec2(16, 14);
	style.FramePadding = ImVec2(10, 5);
	style.ItemSpacing = ImVec2(9, 6);
	style.ItemInnerSpacing = ImVec2(8, 4);
	style.IndentSpacing = 20.f;
	style.ScrollbarSize = 8.f;

	// ---- 边框 ----
	style.FrameBorderSize = 0.f;
	style.WindowBorderSize = 0.f;
	style.ChildBorderSize = 1.f;
	style.PopupBorderSize = 1.f;

	auto& io = ImGui::GetIO();
	io.Fonts->Clear();
	io.Fonts->AddFontFromFileTTF(GetCJKFontPath(), 17.0f);

	ImFontConfig merge_icon_cfg{};
	merge_icon_cfg.FontDataOwnedByAtlas = false;
	merge_icon_cfg.MergeMode = true;
	merge_icon_cfg.GlyphOffset = Vec2_t(0, 3.5f);

	static const ImWchar icon_ranges[] = { 0xE100, 0xE108, 0 };
	io.Fonts->AddFontFromMemoryTTF(icons_font, icons_font_len, 20.f, &merge_icon_cfg, icon_ranges);
}

void Menu::RenderStartupHelpImpl() {
	static bool has_opened_menu = false;

	if (has_opened_menu)
		return;

	auto& io = ImGui::GetIO();
	auto screen = io.DisplaySize;
	auto d = ImGui::GetBackgroundDrawList();

	if (Renderer::IsOpen())
		has_opened_menu = true;

	auto help = "按 Insert 或 右Shift 键 打开菜单"
		"\n\t\t\t\t按 End 键 关闭菜单";
	auto size = ImGui::CalcTextSize(help);

	d->AddText(
		ImVec2(screen.x / 2 - size.x / 2, 80),
		IM_COL32(255, 255, 255, 255),
		help
	);
}
