#pragma once

#include <cmath>
#include <optional>
#include "core/engine/types/Vec3.hpp"
#include "core/util/KalmanFilter.hpp"

// 3D Ballistic Prediction for CS2
// Supports bullet drop, travel time, and target leading
namespace ballistics {

// Weapon ballistic data
struct WeaponBallistics {
    float initial_velocity = 0.0f;    // m/s
    float bullet_mass = 0.0f;         // kg (not used currently)
    float drag_coefficient = 0.0f;    // Drag coefficient
    float gravity = 9.81f;            // m/s^2
    
    // CS2 weapon data (approximate values in game units)
    // 1 unit = 0.0254 meters (1 inch)
    static WeaponBallistics from_weapon_index(int weapon_index) {
        WeaponBallistics b;
        
        // Default values for rifles
        b.initial_velocity = 750.0f * 39.37f; // ~750 m/s to units/s
        b.drag_coefficient = 0.001f;
        b.gravity = 9.81f * 39.37f; // ~386 units/s^2
        
        // Weapon-specific adjustments
        switch (weapon_index) {
            // Pistols
            case 1: case 2: case 3: case 4: case 30: case 32: case 36: case 61: case 63:
                b.initial_velocity = 350.0f * 39.37f; // ~350 m/s
                b.drag_coefficient = 0.002f;
                break;
            // SMGs
            case 17: case 19: case 23: case 24: case 26: case 33: case 34:
                b.initial_velocity = 400.0f * 39.37f;
                b.drag_coefficient = 0.0015f;
                break;
            // Rifles
            case 7: case 8: case 10: case 13: case 16: case 39: case 60:
                b.initial_velocity = 750.0f * 39.37f;
                b.drag_coefficient = 0.001f;
                break;
            // Snipers
            case 9: case 11: case 38: case 40:
                b.initial_velocity = 850.0f * 39.37f;
                b.drag_coefficient = 0.0008f;
                break;
            // Heavy
            case 14: case 28:
                b.initial_velocity = 715.0f * 39.37f;
                b.drag_coefficient = 0.0012f;
                break;
            default:
                break;
        }
        
        return b;
    }
};

// 3D target prediction with ballistics
struct PredictionResult {
    Vec3_t aim_point;           // Where to aim (predicted target position at impact)
    float time_to_impact = 0.0f; // Seconds until bullet hits
    float bullet_drop = 0.0f;    // Vertical drop in game units
    bool valid = false;
};

// Predict where to aim to hit a moving target
// shooter_pos: Local player eye position
// target_pos: Target current position
// target_vel: Target velocity (units/s)
// ballistics: Weapon ballistic properties
// max_time: Maximum prediction time (default 2 seconds)
inline std::optional<PredictionResult> predict_impact(
    const Vec3_t& shooter_pos,
    const Vec3_t& target_pos,
    const Vec3_t& target_vel,
    const WeaponBallistics& ballistics,
    float max_time = 2.0f
) {
    // 单位约定（这是原实现最大的坑）：
    //   位置/速度           -> 游戏单位(unit)，1 unit = 0.0254 m
    //   ballistics.initial_velocity -> units/s（from_weapon_index 已按 *39.37 换算）
    //   ballistics.gravity          -> units/s^2
    // 因此物理积分全程用游戏单位，毫不再做 unit<->m 来回换算；
    // 原先把"米"和"units/s"混用，时间被放大约 39 倍并直接被 max_time 截断。
    const Vec3_t rel_pos = target_pos - shooter_pos;
    const float distance = rel_pos.length();
    if (distance < 1.0f)
        return std::nullopt;

    const float v0 = ballistics.initial_velocity;
    if (v0 <= 0.0f || max_time <= 0.0f)
        return std::nullopt;

    const float k = ballistics.drag_coefficient;
    const float k_v0 = k * v0;

    // 水平距离 -> 飞行时间。
    // 二次阻力解析解：x(t) = ln(1 + k*v0*t)/k  =>  t = (exp(k*x) - 1)/(k*v0)
    // k == 0 时退化为匀速 t = x / v0。
    auto time_for_distance = [&](float horiz_units) -> float {
        if (horiz_units <= 0.0f)
            return 0.0f;
        if (k_v0 > 1e-8f)
            return (std::exp(k * horiz_units) - 1.0f) / k_v0;
        return horiz_units / v0;
    };

    // 目标在 t 时刻的位置（t 单位秒，target_vel 单位 units/s）。
    auto predict_target = [&](float t) -> Vec3_t {
        return target_pos + target_vel * t;
    };

    // 固定点迭代：先用静止假设估时，再用预测位置重算，收敛即止。
    float time = std::min(time_for_distance(distance), max_time);
    constexpr int kMaxIterations = 8;
    constexpr float kTolerance = 1e-4f; // 0.1ms

    for (int iter = 0; iter < kMaxIterations; ++iter) {
        const Vec3_t predicted = predict_target(time);
        const Vec3_t to_target = predicted - shooter_pos;
        const float horiz = std::sqrt(to_target.x * to_target.x + to_target.y * to_target.y);

        float new_time = time_for_distance(horiz);
        new_time = std::min(new_time, max_time);

        const float delta = std::fabs(new_time - time);
        time = new_time;
        if (delta < kTolerance)
            break;
    }

    const float final_time = std::min(time, max_time);
    const Vec3_t predicted_target = predict_target(final_time);

    // 重力下坠（游戏单位）。子弹初始垂直速度按 0 计算（平射基准），
    // 用半隐式欧拉积分更稳：先更新速度再累加位移。
    const float g = ballistics.gravity;
    float vz = 0.0f;
    float drop = 0.0f; // 正数 = 下坠量（与 PredictionResult 注释一致）
    constexpr int kSteps = 64;
    const float dt = final_time / static_cast<float>(kSteps);
    for (int i = 0; i < kSteps; ++i) {
        // 垂直方向同样受二次阻力影响：dvz/dt = -g - k*vz*|vz|
        vz -= (g + k * vz * std::fabs(vz)) * dt;
        drop -= vz * dt; // vz 为负（向下），取负得到正的下坠量
    }

    PredictionResult result;
    result.time_to_impact = final_time;
    result.bullet_drop = drop;                 // 正数：下坠多少游戏单位
    result.aim_point = predicted_target;
    result.aim_point.z += drop;                // 抬高瞄点以补偿下坠
    result.valid = true;
    return result;
}


// Recoil compensation
struct RecoilCompensation {
    Vec3_t punch_angle{0, 0, 0};      // Current view punch
    Vec3_t punch_angle_vel{0, 0, 0};  // Punch velocity
    std::array<Vec3_t, 64> recoil_history; // Last 64 frames
    int history_index = 0;
    
    void update(const Vec3_t& new_punch, const Vec3_t& new_punch_vel) {
        punch_angle = new_punch;
        punch_angle_vel = new_punch_vel;
        recoil_history[history_index] = new_punch;
        history_index = (history_index + 1) % 64;
    }
    
    // Get average recoil pattern for compensation
    Vec3_t get_compensation(int shots_fired) const {
        if (shots_fired <= 0 || shots_fired > 64) return {0, 0, 0};
        return recoil_history[(history_index - shots_fired + 64) % 64];
    }
    
    // Apply compensation to aim point
    Vec3_t apply(const Vec3_t& aim_point, int shots_fired) const {
        Vec3_t comp = get_compensation(shots_fired);
        // Convert punch angles to aim offset
        // punch is in degrees, convert to radians
        float pitch_rad = -comp.x * M_PI / 180.0f; // Negative because punch up = aim down
        float yaw_rad = comp.y * M_PI / 180.0f;
        
        // Simple compensation (would need view matrix for proper 3D)
        return aim_point; // Placeholder - full implementation needs view matrix
    }
};

// Multi-point sampling for anti-jitter
// Samples multiple points on hitbox and averages
inline Vec3_t multi_point_aim(const std::array<Vec3_t, 9>& head_points, 
                              const Vec3_t& eye_pos,
                              const WeaponBallistics& ballistics) {
    // For each point, calculate prediction
    // Return the one with highest hit probability (closest to center, visible)
    Vec3_t best_point = head_points[0]; // Default to center
    float best_score = 0.0f;
    
    for (const auto& point : head_points) {
        // Score based on distance to head center and visibility
        float score = 1.0f / (1.0f + (point - head_points[0]).length() * 0.1f);
        
        if (score > best_score) {
            best_score = score;
            best_point = point;
        }
    }
    
    return best_point;
}

} // namespace ballistics