#pragma once
#include <array>
#include "core/engine/classes/Bones.hpp"
#include "core/engine/classes/Weapon.hpp"
#include "core/engine/classes/ObserverServices.hpp"

class Player {
public:
    Player() {}
    Player(int index, uintptr_t el, uintptr_t le) 
        : index(index), entity_list(el), list_entry(le){}

    bool Update();
    bool GetBounds(view_matrix_t matrix, Vec2_t size, std::pair<Vec2_t, Vec2_t>& bounds, Vec2_t* head_screen = nullptr) const;
public:
    int8_t index = -1; // To use as invalid/un-initialize check

    Vec3_t pos;
    Vec3_t vel;

    int ping = 0;
    int team = 0;
    int health = 0;
    int armor = 0;
    int money = 0;

    bool bot = true;
    bool alive = false;
    bool scoped = false;
    bool flashed = false;
    bool ducking = false;
    bool spotted = false;
    uint64_t spotted_mask = 0; // full 64-bit EntitySpottedState_t::m_bSpottedByMask
    bool visible = false;      // visible to the local player (cover check)
    bool defusing = false;
    bool localplayer = false;
    bool has_c4 = false;

    char name[32];
    //std::string name;
    uint64_t steam_id{};

    Weapon weapon;
    int32_t ammo = -1;        // -1 = unknown / weapon read failed
    bool is_reloading = false;

    // Fixed-size skeleton buffer: avoids per-player heap allocations in the
    // per-frame cache copy. skeleton_valid mirrors "bone_list was filled"
    // (players that die or fail the read have it false, like the old empty()).
    std::array<bone_pos, 30> bone_list;
    bool skeleton_valid = false;

    uintptr_t pawn_controller_addr; // pawn handle（32 位实体句柄）
    ObserverServices observer_services;
private:
    uintptr_t list_entry;
    uintptr_t entity_list;

    uintptr_t pawn;
    uintptr_t controller;
private:
    bool GetPawn();
    bool GetController();

    bool UpdatePawn();
    bool UpdateWeapon();
    bool UpdateSkeleton();
    bool UpdateController();
    bool UpdateObserverServices();
};