#pragma once

class Globals {
public:
    Globals() {};

    bool Update();
    
    // Frame timing for adaptive cache refresh
    float GetFrameTime() const { return frame_time; }
    float GetTickRate() const { return tick_rate; }
    
public:
    int max_clients;
    float current_time;
    char map_name[32];
    bool in_match;
    
    // Frame timing (for adaptive refresh)
    float frame_time = 16.67f;  // ms, default ~60 FPS
    float tick_rate = 64.0f;    // server tick rate
    
private:
    uintptr_t address;
};