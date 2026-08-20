#include "Aimbot.hpp"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <array>

#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/visibility/Visibility.hpp"
#include "gui/renderer/Renderer.hpp"
#include "core/util/KalmanFilter.hpp"
#include "core/util/Ballistics.hpp"

namespace {
    bool trigger_down = false;

    // mouse_event resolved dynamically via GetProcAddress so user32's input API
    // isn't a static IAT import; the names are XOR-decoded at runtime.
    using pMouseEvent = void(WINAPI*)(DWORD, DWORD, DWORD, DWORD, ULONG_PTR);

    pMouseEvent GetMouseEvent() {
        static const pMouseEvent fn = []() {
            constexpr char key = 0x5C;
            constexpr XorStr dll("user32.dll", key);
            constexpr XorStr name("mouse_event", key);
            char db[16], nb[16];
            for (size_t i = 0; i < sizeof(dll.data); ++i) db[i] = dll.data[i] ^ key;
            for (size_t i = 0; i < sizeof(name.data); ++i) nb[i] = name.data[i] ^ key;
            return (pMouseEvent)GetProcAddress(GetModuleHandleA(db), nb);
        }();
        return fn;
    }

    void PressTrigger() {
        if (!trigger_down) {
            GetMouseEvent()(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            trigger_down = true;
        }
    }

    void ReleaseTrigger() {
        if (trigger_down) {
            GetMouseEvent()(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            trigger_down = false;
        }
    }

    // mouse_event(MOUSEEVENTF_MOVE) is distorted by Windows pointer acceleration
    // ("Enhance pointer precision"), which makes the cursor miss the target.
    // Disable it once while the aimbot is active and restore it on process exit.
    int g_mouse_params[3] = {0, 0, 0};
    bool g_accel_disabled = false;

    void DisableMouseAcceleration() {
        if (g_accel_disabled)
            return;
        if (!SystemParametersInfoA(SPI_GETMOUSE, 0, g_mouse_params, 0))
            return;

        int params[3] = { g_mouse_params[0], g_mouse_params[1], 0 }; // accel off
        if (SystemParametersInfoA(SPI_SETMOUSE, 0, params, SPIF_SENDCHANGE)) {
            g_accel_disabled = true;
            std::atexit([] {
                SystemParametersInfoA(SPI_SETMOUSE, 0, g_mouse_params, SPIF_SENDCHANGE);
            });
        }
    }

    // Kalman filter for target tracking
    static Kalman3D s_kalman_filter;
    static bool s_kalman_initialized = false;
    static int s_last_target_index = -1;

    // Recoil compensation
    static ballistics::RecoilCompensation s_recoil_comp;
} // namespace

// ============================================================================
// Public API: Ballistic prediction
// ============================================================================
Aimbot::PredictionData Aimbot::PredictTarget(const Vec3_t& shooter_eye, 
                                              const Vec3_t& target_pos,
                                              const Vec3_t& target_vel,
                                              int weapon_index) {
    PredictionData result{};
    
    using namespace ballistics;
    WeaponBallistics ballistics = WeaponBallistics::from_weapon_index(weapon_index);
    
    auto prediction = predict_impact(shooter_eye, target_pos, target_vel, ballistics);
    if (prediction) {
        result.predicted_position = prediction->aim_point;
        result.time_to_impact = prediction->time_to_impact;
        result.bullet_drop = prediction->bullet_drop;
        result.valid = true;
    }
    
    return result;
}

Vec3_t Aimbot::CompensateRecoil(const Vec3_t& aim_point, int shots_fired) {
    return s_recoil_comp.apply(aim_point, shots_fired);
}

void Aimbot::UpdateRecoilPattern(const Vec3_t& punch_angle, const Vec3_t& punch_vel) {
    s_recoil_comp.update(punch_angle, punch_vel);
}

// ============================================================================
// Main aimbot loop
// ============================================================================
void Aimbot::Run() {
    // Frame-rate independent smoothing: measure the real frame interval.
    static auto last_time = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - last_time).count();
    last_time = now;
    dt = std::clamp(dt, 0.001f, 0.1f); // guard against hitches causing huge steps

    // Sub-pixel accumulator: mouse_event only accepts whole pixels, so we
    // accumulate fractional movement and emit a whole pixel once enough builds
    // up. This gives sub-pixel precision and avoids the 1px "forced step" that
    // caused jitter around the target.
    static float acc_x = 0.f;
    static float acc_y = 0.f;

    if (!cfg::aimbot::enabled)
        return;

    // Precise cursor movement needs pointer acceleration disabled.
    DisableMouseAcceleration();

    // Don't fight the user for the cursor while the menu is open.
    if (Renderer::IsOpen())
        return;

    if (cfg::aimbot::key != 0 && !(GetAsyncKeyState(cfg::aimbot::key) & 0x8000)) {
        acc_x = acc_y = 0.f;
        return;
    }

    auto snapshot = Cache::CopySnapshot();
    if (!snapshot)
        return;
    auto& local = snapshot->local;

    // Keep the collision mesh in sync with the current map (lazy reload on map change).
    Visibility::Get().UpdateMap(snapshot->globals.map_name);
    Visibility::Get().SetSmokes(snapshot->smokes);

    if (!local.alive) {
        acc_x = acc_y = 0.f;
        return;
    }

    auto p = Engine::GetProcess();
    if (!p || !p->hwnd_)
        return;

    RECT client{};
    if (!GetClientRect(p->hwnd_, &client))
        return;

    POINT tl{ client.left, client.top };
    if (!ClientToScreen(p->hwnd_, &tl))
        return;

    const Vec2_t screen_size{
        static_cast<float>(client.right - client.left),
        static_cast<float>(client.bottom - client.top)
    };

    // Crosshair sits at the center of the game window's client area.
    const float cx = tl.x + (client.right - client.left) * 0.5f;
    const float cy = tl.y + (client.bottom - client.top) * 0.5f;

    // Eye position (used by the cover checks and smart-bone selection).
    const float eye_height = local.ducking ? 46.f : 62.f;
    const Vec3_t eye = local.pos + Vec3_t(0.f, 0.f, eye_height);

    // Get local weapon for ballistics
    int local_weapon = local.weapon.item_index;

    // Project one player's aim bone to screen; fills the crosshair offset (px),
    // the target's screen-space velocity (px/s) for feed-forward, and its 3D
    // speed (units/s) for speed-aware smoothing.
    auto try_aim = [&](const Player& player, float& out_dx, float& out_dy, float& out_vx, float& out_vy, float& out_speed) -> bool {
        if (player.localplayer || !player.alive)
            return false;

        // 敌我不分（死亡竞赛）时跳过队友过滤
        if (!cfg::deathmatch && player.team == local.team)
            return false;

        const int aim_bone = cfg::aimbot::bone;
        Vec3_t aim_point;

        // 露瞄：按伤害优先级（头>颈>胸>骨盆）选第一个可见部位，做到"露哪打哪"。
        // 头部伤害最高且爆头致命，因此全暴露/非满血都优先打头。
        if (cfg::aimbot::smart_bone && player.skeleton_valid && Visibility::Get().IsLoaded()) {
            static constexpr int priority_bones[] = {
                bone_index::head, bone_index::neck, bone_index::chest, bone_index::pelvis
            };
            bool visible_bone = false;
            for (int b : priority_bones) {
                if (b >= 30)
                    continue;
                const Vec3_t bp = player.bone_list[b].pos;
                if (!Visibility::Get().RayBlocked(eye, bp)) {
                    aim_point = bp;
                    visible_bone = true;
                    break;
                }
            }
            if (!visible_bone)
                return false; // 完全遮挡，不瞄准
        } else {
            if (player.skeleton_valid && aim_bone >= 0 && aim_bone < 30) {
                aim_point = player.bone_list[aim_bone].pos;
            } else {
                float z = 64.f;
                switch (aim_bone) {
                case bone_index::pelvis: z = 32.f; break;
                case bone_index::chest:   z = 48.f; break;
                case bone_index::neck:    z = 58.f; break;
                default:                  z = 64.f; break;
                }
                aim_point = player.pos + Vec3_t(0.f, 0.f, z);
            }
        }

        const float vx = player.vel.x, vy = player.vel.y, vz = player.vel.z;
        out_speed = std::sqrt(vx * vx + vy * vy + vz * vz);

        // Ballistic prediction for moving targets
        if (out_speed > 10.f && cfg::aimbot::smart_bone) {
            // Use Kalman filter to smooth target position/velocity
            Kalman3D::MeasVec measurement = {aim_point.x, aim_point.y, aim_point.z};
            
            if (!s_kalman_initialized || s_last_target_index != player.index) {
                s_kalman_filter.init(measurement, dt);
                s_kalman_initialized = true;
                s_last_target_index = player.index;
            } else {
                s_kalman_filter.predict(dt);
                s_kalman_filter.update(measurement);
            }
            
            // Use filtered position for aim point
            auto filtered_pos = s_kalman_filter.get_predicted_position();
            aim_point = Vec3_t(filtered_pos[0], filtered_pos[1], filtered_pos[2]);
            
            // Get filtered velocity
            auto filtered_vel = s_kalman_filter.get_velocity();
            
            // Ballistic prediction
            auto prediction = PredictTarget(eye, aim_point, Vec3_t(filtered_vel[0], filtered_vel[1], filtered_vel[2]), local_weapon);
            if (prediction.valid) {
                aim_point = prediction.predicted_position;
            }
        }

        // Project the current aim point.
        Vec2_t screen_now;
        if (!snapshot->game.view_matrix.wts(aim_point, screen_size, screen_now))
            return false;

        out_dx = tl.x + screen_now.x - cx;
        out_dy = tl.y + screen_now.y - cy;

        // Screen-space velocity (px/s) via a short probe of the 3D velocity.
        // A small dead zone keeps stationary targets free of velocity noise.
        const float probe = 0.05f;
        if (out_speed < 10.f) {
            out_vx = 0.f;
            out_vy = 0.f;
        } else {
            Vec2_t screen_probe;
            if (snapshot->game.view_matrix.wts(aim_point + player.vel * probe, screen_size, screen_probe)) {
                out_vx = (screen_probe.x - screen_now.x) / probe;
                out_vy = (screen_probe.y - screen_now.y) / probe;
            } else {
                out_vx = 0.f;
                out_vy = 0.f;
            }
        }

        // Smoke check: always active when smoke data is available, independent
        // of visible_only and map collision mesh status.
        if (Visibility::Get().SmokeBlocksRay(eye, aim_point))
            return false;

        // Map geometry cover check: only when the user enables it AND
        // collision data is loaded for the current map.
        if (cfg::aimbot::visible_only && Visibility::Get().IsLoaded()) {
            if (Visibility::Get().RayBlocked(eye, aim_point))
                return false;
        }

        return true;
    };

    // Lock-on preference: keep the previous target as long as it is still valid,
    // so the aim doesn't flicker between two nearby enemies.
    static int last_target_index = -1;
    static int acc_target_index = -1;

    float best_dx = 0.f;
    float best_dy = 0.f;
    float best_vx = 0.f;
    float best_vy = 0.f;
    float best_speed = 0.f;
    int best_index = -1;
    bool found = false;

    // FOV hysteresis: a locked target stays locked inside a slightly wider FOV
    // so it doesn't flicker in/out when it hovers near the FOV edge. New targets
    // must enter the strict FOV.
    const float lock_fov = cfg::aimbot::fov * 1.3f;

    if (last_target_index >= 0) {
        for (const auto& player : snapshot->players) {
            if (player.index != last_target_index)
                continue;

            float dx, dy, vx, vy, sp;
            if (try_aim(player, dx, dy, vx, vy, sp) &&
                std::sqrt(dx * dx + dy * dy) <= lock_fov) {
                best_dx = dx;
                best_dy = dy;
                best_vx = vx;
                best_vy = vy;
                best_speed = sp;
                best_index = player.index;
                found = true;
            }
            break;
        }
    }

    // Otherwise pick the closest valid target.
    if (!found) {
        float best_dist = FLT_MAX;
        for (const auto& player : snapshot->players) {
            float dx, dy, vx, vy, sp;
            if (!try_aim(player, dx, dy, vx, vy, sp))
                continue;

            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > cfg::aimbot::fov)
                continue;

            if (dist < best_dist) {
                best_dist = dist;
                best_dx = dx;
                best_dy = dy;
                best_vx = vx;
                best_vy = vy;
                best_speed = sp;
                best_index = player.index;
                found = true;
            }
        }
    }

    last_target_index = found ? best_index : -1;

    if (!found) {
        acc_x = acc_y = 0.f;
        acc_target_index = -1;
        s_kalman_initialized = false;
        s_last_target_index = -1;
        return;
    }

    // Target switched: drop leftover fractional movement.
    if (acc_target_index != best_index) {
        acc_x = acc_y = 0.f;
        acc_target_index = best_index;
        s_kalman_initialized = false;
    }

    // Dead zone: only stop when the crosshair is already on the target AND the
    // target is essentially still. A moving target must keep being followed via
    // the feed-forward term even when the positional error is tiny.
    const float target_dist = std::sqrt(best_dx * best_dx + best_dy * best_dy);
    const float feed_mag = std::sqrt(best_vx * best_vx + best_vy * best_vy) * dt;
    const float dead_zone = 1.5f;
    if (target_dist < dead_zone && feed_mag < 0.5f) {
        acc_x = acc_y = 0.f;
        return;
    }

    // Dynamic smoothing: converge fast when far away, slow down near the target
    // so the cursor settles without overshooting/oscillating.
    const float t = std::clamp(cfg::aimbot::smoothing, 0.f, 1.f);
    const float user_scale = 1.f - t * 0.7f; // 1.0 (fast) .. 0.3 (smoothest)
    // ~32/s far away, ~16/s at 50px, ~9.5/s at 10px, 8/s at 0.
    const float base_speed = 8.f + 24.f * (target_dist / (target_dist + 150.f));
    // Speed-aware: strafing/running targets need faster convergence to keep up.
    const float speed_boost = 1.f + 0.6f * std::min(1.f, best_speed / 250.f);
    const float speed = base_speed * user_scale * speed_boost;
    const float factor = 1.f - std::exp(-speed * dt);

    // Feedback (converge to the target) + feed-forward (follow the target's
    // screen motion). The feedback aims at the PREDICTED position — current
    // offset plus one latency's worth of motion — so the aim doesn't trail by
    // the end-to-end read-memory → render → mouse pipeline lag. The feed-
    // forward term keeps it following that predicted point this frame.
    const float latency = 0.045f; // pipeline lag + slight motion lead
    float dx = (best_dx + best_vx * latency) * factor + best_vx * dt;
    float dy = (best_dy + best_vy * latency) * factor + best_vy * dt;

    // Recoil compensation
    if (local.weapon.item_index > 0) {
        // Get recoil compensation for current shot count
        // This would need shots_fired from weapon/player data
        // For now, use a simple approach
    }

    // Cap per-frame travel so a single frame never jumps too far at once.
    const float max_step = 80.f;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > max_step) {
        const float scale = max_step / len;
        dx *= scale;
        dy *= scale;
    }

    // Emit whole pixels only, keeping the remainder for the next frame.
    acc_x += dx;
    acc_y += dy;
    const int move_x = static_cast<int>(acc_x);
    const int move_y = static_cast<int>(acc_y);
    acc_x -= move_x;
    acc_y -= move_y;

    if (move_x || move_y)
        GetMouseEvent()(MOUSEEVENTF_MOVE, move_x, move_y, 0, 0);
}

// Auto-fire when the crosshair overlaps an enemy's on-screen bounds.
void Aimbot::RunTriggerbot() {
    if (!cfg::triggerbot::enabled || Renderer::IsOpen()) {
        ReleaseTrigger();
        return;
    }

    if (cfg::triggerbot::key != 0 && !(GetAsyncKeyState(cfg::triggerbot::key) & 0x8000)) {
        ReleaseTrigger();
        return;
    }

    auto snapshot = Cache::CopySnapshot();
    if (!snapshot) {
        ReleaseTrigger();
        return;
    }
    auto& local = snapshot->local;

    if (!local.alive) {
        ReleaseTrigger();
        return;
    }

    auto p = Engine::GetProcess();
    if (!p || !p->hwnd_) {
        ReleaseTrigger();
        return;
    }

    RECT client{};
    if (!GetClientRect(p->hwnd_, &client)) {
        ReleaseTrigger();
        return;
    }

    const Vec2_t screen_size{
        static_cast<float>(client.right - client.left),
        static_cast<float>(client.bottom - client.top)
    };
    const float center_x = (client.right - client.left) * 0.5f;
    const float center_y = (client.bottom - client.top) * 0.5f;

    bool on_target = false;
    for (auto& player : snapshot->players) {
        if (player.localplayer || !player.alive)
            continue;
        // 敌我不分（死亡竞赛）时跳过队友过滤
        if (!cfg::deathmatch && player.team == local.team)
            continue;

        // Cover check: skip targets hidden behind cover.
        if (cfg::triggerbot::visible_only && !player.visible)
            continue;

        std::pair<Vec2_t, Vec2_t> bounds;
        if (!player.GetBounds(snapshot->game.view_matrix, screen_size, bounds))
            continue;

        const float left = std::min(bounds.first.x, bounds.second.x);
        const float right = std::max(bounds.first.x, bounds.second.x);
        const float top = std::min(bounds.first.y, bounds.second.y);
        const float bottom = std::max(bounds.first.y, bounds.second.y);

        const float margin = 3.f;
        if (center_x >= left - margin && center_x <= right + margin &&
            center_y >= top - margin && center_y <= bottom + margin) {
            on_target = true;
            break;
        }
    }

    if (on_target)
        PressTrigger();
    else
        ReleaseTrigger();
}

// Draw the aim FOV circle at the crosshair.
void Aimbot::Render() {
    if (!cfg::aimbot::enabled || !cfg::aimbot::show_fov)
        return;

    auto& io = ImGui::GetIO();
    auto d = ImGui::GetBackgroundDrawList();

    const ImVec2 center{ io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f };
    d->AddCircle(center, cfg::aimbot::fov, IM_COL32(255, 255, 255, 70), 64, 1.0f);
    d->AddCircleFilled(center, 1.5f, IM_COL32(255, 255, 255, 120));
}
