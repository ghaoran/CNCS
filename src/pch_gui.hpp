// GUI Module Precompiled Header - GUI-specific heavy headers
// Include this in GUI module source files only

#pragma once

#include "pch_core.hpp"

// ImGui
#include "external/imgui/imgui.h"
#include "external/imgui/imgui_internal.h"
#include "external/imgui/backends/imgui_impl_dx11.h"
#include "external/imgui/backends/imgui_impl_win32.h"

// GUI Frontend
#include "gui/frontend/aimbot/Aimbot.hpp"
#include "gui/frontend/esp/Esp.hpp"
#include "gui/frontend/menu/Menu.hpp"
#include "gui/frontend/overlays/Overlays.hpp"

// GUI Renderer
#include "gui/renderer/Renderer.hpp"
#include "gui/renderer/window/Window.hpp"

// Engine (for snapshots)
#include "core/engine/cache/Cache.hpp"
#include "core/engine/Engine.hpp"
#include "core/visibility/Visibility.hpp"

// Config
#include "config/Current.hpp"