#pragma once

#include "core/engine/types/Vec3.hpp"

class Aimbot {
public:
    static void Run();            // aim assist (mouse movement)
    static void RunTriggerbot();  // auto-fire when crosshair is on target
    static void Render();         // draw the aim FOV circle
    
    // Advanced features
    struct PredictionData {
        Vec3_t predicted_position;
        float time_to_impact;
        float bullet_drop;
        bool valid;
    };
    
    static PredictionData PredictTarget(const Vec3_t& shooter_eye, 
                                         const Vec3_t& target_pos,
                                         const Vec3_t& target_vel,
                                         int weapon_index);
    static Vec3_t CompensateRecoil(const Vec3_t& aim_point, int shots_fired);
    static void UpdateRecoilPattern(const Vec3_t& punch_angle, const Vec3_t& punch_vel);
};
