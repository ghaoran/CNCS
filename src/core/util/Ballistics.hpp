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
    // Convert to meters for physics calculations
    const float units_to_meters = 0.0254f;
    const float meters_to_units = 39.3701f;
    
    Vec3_t rel_pos = target_pos - shooter_pos;
    float distance = rel_pos.length();
    
    if (distance < 1.0f) return std::nullopt;
    
    // Initial estimate: time = distance / velocity
    float time = distance / (ballistics.initial_velocity * units_to_meters);
    time = std::min(time, max_time);
    
    // Iterative solver for bullet drop and travel time
    // Using simple drag model: dv/dt = -k*v^2 - g (for vertical)
    // For horizontal: dv/dt = -k*v^2
    
    const int max_iterations = 10;
    const float tolerance = 0.01f; // 1cm in meters
    
    for (int iter = 0; iter < max_iterations; ++iter) {
        // Predict target position at time t
        Vec3_t predicted_target = target_pos + target_vel * (time * meters_to_units);
        Vec3_t to_target = predicted_target - shooter_pos;
        float horiz_dist = sqrt(to_target.x * to_target.x + to_target.y * to_target.y);
        float vert_dist = to_target.z;
        
        // Calculate bullet trajectory
        // Horizontal motion with drag
        float v0 = ballistics.initial_velocity;
        float k = ballistics.drag_coefficient;
        
        // Analytical solution for quadratic drag: v(t) = v0 / (1 + k*v0*t)
        // Distance: x(t) = (1/k) * ln(1 + k*v0*t)
        // Invert: t = (exp(k*x) - 1) / (k*v0)
        
        float horiz_dist_m = horiz_dist * units_to_meters;
        
        // Time to travel horizontal distance
        float k_v0 = k * v0;
        if (k_v0 > 0.0f && horiz_dist_m > 0.0f) {
            time = (expf(k * horiz_dist_m) - 1.0f) / k_v0;
        } else {
            time = horiz_dist_m / v0;
        }
        
        // Clamp time
        time = std::min(time, max_time);
        
        // Bullet drop due to gravity (with drag)
        // Vertical: dvz/dt = -g - k*vz*|vz|
        // Approximate: drop = 0.5 * g * t^2 (without drag) or numerical integration
        float g = ballistics.gravity * units_to_meters; // m/s^2
        
        // Simple integration for vertical drop
        float vz = 0.0f; // Initial vertical velocity (aiming horizontally)
        float drop = 0.0f;
        float dt = time / 100.0f;
        for (int i = 0; i < 100; ++i) {
            vz -= (g + k * vz * fabsf(vz)) * dt;
            drop += vz * dt;
        }
        
        // Check convergence
        float new_horiz_dist = (target_pos + target_vel * (time * meters_to_units) - shooter_pos).length();
        float new_horiz_dist_m = new_horiz_dist * units_to_meters;
        
        // Solve for time again with new distance
        float new_time;
        if (k_v0 > 0.0f && new_horiz_dist_m > 0.0f) {
            new_time = (expf(k * new_horiz_dist_m) - 1.0f) / k_v0;
        } else {
            new_time = new_horiz_dist_m / v0;
        }
        
        if (fabsf(new_time - time) < tolerance / v0) {
            time = new_time;
            break;
        }
        time = new_time;
    }
    
    // Final prediction
    Vec3_t predicted_target = target_pos + target_vel * (time * meters_to_units);
    Vec3_t to_target = predicted_target - shooter_pos;
    float horiz_dist = sqrt(to_target.x * to_target.x + to_target.y * to_target.y);
    float horiz_dist_m = horiz_dist * units_to_meters;
    
    // Final time
    float k_v0 = ballistics.drag_coefficient * ballistics.initial_velocity;
    float final_time;
    if (k_v0 > 0.0f && horiz_dist_m > 0.0f) {
        final_time = (expf(ballistics.drag_coefficient * horiz_dist_m) - 1.0f) / k_v0;
    } else {
        final_time = horiz_dist_m / ballistics.initial_velocity;
    }
    final_time = std::min(final_time, max_time);
    
    // Final drop calculation
    float g = ballistics.gravity * units_to_meters;
    float vz = 0.0f;
    float drop = 0.0f;
    float dt = final_time / 100.0f;
    for (int i = 0; i < 100; ++i) {
        vz -= (g + ballistics.drag_coefficient * vz * fabsf(vz)) * dt;
        drop += vz * dt;
    }
    
    // Compensate aim point
    Vec3_t aim_point = predicted_target;
    aim_point.z -= drop * meters_to_units; // Compensate for drop
    
    PredictionResult result;
    result.aim_point = aim_point;
    result.time_to_impact = final_time;
    result.bullet_drop = drop * meters_to_units;
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