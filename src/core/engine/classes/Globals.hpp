#pragma once

class Globals {
public:
    Globals() {};

    bool Update();
    
    // Frame timing for adaptive cache refresh
    float GetFrameTime() const { return frame_time; }
    float GetTickRate() const { return tick_rate; }
    
public:
    int max_clients = 0;
    float current_time = 0.f;
    char map_name[32] = {};
    bool in_match = false;
    
    // Frame timing (for adaptive refresh)
    float frame_time = 16.67f;  // ms, default ~60 FPS
    float tick_rate = 64.0f;    // server tick rate
    
private:
    uintptr_t address = 0;
};