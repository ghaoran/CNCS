#include "Cache.hpp"

#include <cstring>
#include <cmath>
#include <string>

#include "core/engine/Engine.hpp" // Circular dep
#include "core/offsets/Dumper.hpp"

using namespace std::chrono;

namespace {
    // Smoke grenade constants
    constexpr float SMOKE_ESTIMATED_RADIUS = 150.f;    // 烟雾云半径 (units)
    constexpr float SMOKE_ESTIMATED_HEIGHT = 350.f;    // 烟雾云高度 (units)
    constexpr int32_t MAX_VOXEL_SIZE = 8192;           // 最大 voxel 缓冲区大小
    constexpr int MAX_SMOKE_CACHE_ENTRIES = 64;        // 最大烟雾缓存条目数
    constexpr int MAX_ENTITY_ITERATIONS = 256;         // 最大实体遍历次数
    constexpr int VOXEL_GRID_SIZE = 64;                // voxel 网格尺寸 (64x64x64)
    constexpr int VOXEL_CELL_SIZE = 8;                 // voxel 单元格大小 (units)
    constexpr int SMOKE_NAME_CHECK_CHARS = 8;          // 烟雾类名检查字符数

    // 从 CS2 烟雾 voxel bitfield 计算 AABB（Axis-Aligned Bounding Box）。
    // 每个 byte 含 8 个 voxel bit，voxel 格子边长 8 units，以引爆点为中心。
    // 成功返回 true 并填充 out；无占用体素返回 false。
    bool ComputeSmokeBox(const uint8_t* vbuf, int32_t size, const Vec3_t& origin, SmokeBox& out) {
        float mn_x = 1e9f, mn_y = 1e9f, mn_z = 1e9f;
        float mx_x = -1e9f, mx_y = -1e9f, mx_z = -1e9f;
        int count = 0;
        for (int32_t bi = 0; bi < size; ++bi) {
            const uint8_t byte = vbuf[bi];
            if (!byte)
                continue;
            for (int32_t bit = 0; bit < 8; ++bit) {
                if (!(byte & (1 << bit)))
                    continue;
                const int32_t idx = bi * 8 + bit;
                // 64×64×64 grid 展平：x 最快、z 最慢（每维 6 bit）。
                const float vx = (float)((idx >> 0) & 0x3F);
                const float vy = (float)((idx >> 6) & 0x3F);
                const float vz = (float)((idx >> 12) & 0x3F);
                const float wx = origin.x + (vx - 32.f) * 8.f;
                const float wy = origin.y + (vy - 32.f) * 8.f;
                const float wz = origin.z + (vz - 32.f) * 8.f;
                if (wx < mn_x) mn_x = wx;
                if (wy < mn_y) mn_y = wy;
                if (wz < mn_z) mn_z = wz;
                if (wx > mx_x) mx_x = wx;
                if (wy > mx_y) mx_y = wy;
                if (wz > mx_z) mx_z = wz;
                ++count;
            }
        }
        if (count == 0)
            return false;
        out.min = Vec3_t(mn_x, mn_y, mn_z);
        out.max = Vec3_t(mx_x, mx_y, mx_z);
        return true;
    }
}

bool Cache::Refresh() {
    return Get().RefreshImpl();
}

std::shared_ptr<const Snapshot> Cache::CopySnapshot() {
    // Only bumps a refcount — no deep copy of the 64-player vector.
    std::lock_guard<std::mutex> lock(Get().mtx);
    return Get().current;
}

bool Cache::RefreshImpl() {
    auto p = Engine::GetProcess();

    if (!p)
        return false;

    auto now = steady_clock::now();

    // Throttle the whole refresh. The render thread reads an immutable shared
    // snapshot, so nothing can be published on the throttle path (the previous
    // "publish just the view matrix" trick would mutate a snapshot the render
    // thread may still be reading). The view matrix now updates with the full
    // refresh — a +1ms worst-case staleness that is negligible next to the
    // ~7ms render frame interval.
    {
        std::lock_guard<std::mutex> lock(mtx);

#ifdef _DEBUG
        const auto interval = cfg::dev::cache_refresh_rate * 1ms;
#else
        // Adaptive refresh: target half the frame time (2x frame rate)
        // Use cached interval from previous snapshot
        static float s_cached_interval = 2.0f;
        const auto interval = std::chrono::duration<float, std::milli>(s_cached_interval);
#endif

        if (now - last < interval)
            return true;
    }

    // Heavy reads happen WITHOUT holding the lock, so the render thread never
    // blocks on a long memory-read batch.
    Game game_local;
    if (!game_local.Update())
        return false;
    if (!game_local.UpdateEntityList())
        return false;

    Globals globals_local;
    globals_local.Update();

    Bomb bomb_local;
    bomb_local.Update();

    Player local_local;

    std::vector<Player> scan;
    scan.reserve(globals_local.max_clients);
    for (int i = 0; i < globals_local.max_clients; i++) {
        auto player = Player(i, game_local.entity_list, game_local.list_entry);

        if (!player.Update())
            continue;

        if (player.localplayer)
            local_local = player;

        player.has_c4 = (bomb_local.carrier != 0 && player.pawn_controller_addr == bomb_local.carrier);

        scan.push_back(player);
    }

    // Cover check: a player is visible to us when the local player's slot bit is
    // set in their spotted mask (they are spotted by us) or vice-versa. The slot
    // bit is the 0-based controller index (entity index - 1).
    if (local_local.index >= 0 && local_local.index < 64) {
        const uint64_t local_bit = 1ull << local_local.index;
        const uint64_t local_mask = local_local.spotted_mask;
        for (auto& player : scan) {
            if (player.index < 0 || player.index >= 64)
                continue;
            player.visible =
                (player.spotted_mask & local_bit) != 0 ||
                (local_mask & (1ull << player.index)) != 0;
        }
    }

    // Scan active smoke grenades. Smoke is not part of the map collision
    // geometry, so the cover check must handle it separately. Only done when a
    // cover-checking feature is enabled (otherwise zero cost).
    std::vector<SmokeBox> smokes;
    // Always scan smoke grenades — the smoke-block check in the aimbot runs
    // unconditionally (independent of visible_only and map collision status).
    {
        const uint32_t highest = p->read<uint32_t>(game_local.entity_list + offsets::dwGameEntitySystem_highestEntityIndex);
        if (highest > 0 && highest < 0x10000 && game_local.entity_list) {
            smokes.reserve(4);
            uintptr_t cur_chunk = 0;
            uint32_t cur_chunk_idx = UINT32_MAX;
            for (uint32_t i = 0; i < highest; ++i) {
                const uint32_t ci = i >> offsets::HandleBits;
                if (ci != cur_chunk_idx) {
                    cur_chunk_idx = ci;
                    cur_chunk = p->read<uintptr_t>(game_local.entity_list + offsets::EntityListOffset + offsets::ChunkStride * (uint64_t)ci);
                }
                if (!cur_chunk)
                    continue;

                const uintptr_t identity = cur_chunk + offsets::EntryStride * (uint64_t)(i & offsets::HandleMask);
                const uintptr_t name_ptr = p->read<uintptr_t>(identity + offsets::entity::m_designerName);
                if (!name_ptr)
                    continue;

                // 只读前 8 字节判定 class 名（烟雾 class 以 "smoke" 开头），
                // 避免对每个实体读 64 字节字符串（~200 实体 × 64B ≈ 12.8KB/次）。
                char cls[8];
                if (!p->read_raw(name_ptr, cls, sizeof(cls)))
                    continue;

                bool is_smoke = false;
                for (int q = 0; q + 4 < 8 && !is_smoke; ++q)
                    is_smoke = ((cls[q] | 0x20) == 's' && (cls[q + 1] | 0x20) == 'm' && (cls[q + 2] | 0x20) == 'o' &&
                                (cls[q + 3] | 0x20) == 'k' && (cls[q + 4] | 0x20) == 'e');
                if (!is_smoke)
                    continue;

                const uintptr_t ent = p->read<uintptr_t>(identity);
                if (!ent)
                    continue;

                // Batch 1: smoke state (did_effect + tick + pos) — 28 bytes, 1 syscall.
                constexpr uintptr_t s1 = offsets::smoke::m_nSmokeEffectTickBegin;  // 4728
                constexpr size_t s1_sz = (offsets::smoke::m_vSmokeDetonationPos + 12) - s1; // 4760 - 4728 = 32
                uint8_t sblk[s1_sz];
                if (!p->read_raw(ent + s1, sblk, s1_sz))
                    continue;
                const bool did_smoke = sblk[offsets::smoke::m_bDidSmokeEffect - s1] != 0 ||
                                       *(int32_t*)(sblk) > 0;
                if (!did_smoke)
                    continue;
                Vec3_t origin;
                std::memcpy((void*)&origin, sblk + (offsets::smoke::m_vSmokeDetonationPos - s1), 12);
                if (origin.zero())
                    continue;

                // Batch 2: voxel metadata (ptr + size + update + received) — 33 bytes, 1 syscall.
                constexpr uintptr_t s2 = offsets::smoke::m_VoxelFrameData;  // 4768
                constexpr size_t s2_sz = (offsets::smoke::m_bSmokeVolumeDataReceived + 1) - s2; // 4801 - 4768 = 33
                uint8_t vblk[s2_sz];
                const bool have_vmeta = p->read_raw(ent + s2, vblk, s2_sz);
                const int32_t voxel_update = have_vmeta ? *(int32_t*)(vblk + (offsets::smoke::m_nVoxelUpdate - s2)) : 0;

                SmokeBox box;
                bool cached = false;
                for (const auto& e : smoke_cache) {
                    if (e.entity == ent && e.voxel_update == voxel_update) {
                        box = e.box;
                        cached = true;
                        break;
                    }
                }

                if (!cached) {
                    const uintptr_t voxel_ptr = have_vmeta ? *(uintptr_t*)(vblk) : 0;
                    const int32_t voxel_size = have_vmeta ? *(int32_t*)(vblk + (offsets::smoke::m_nVoxelFrameDataSize - s2)) : 0;
                    const bool vol_ready = have_vmeta && vblk[offsets::smoke::m_bSmokeVolumeDataReceived - s2] != 0;

                    if (voxel_ptr && voxel_size > 0 && voxel_size <= 8192 && vol_ready) {
                        uint8_t vbuf[8192];
                        const int32_t to_read = std::min(voxel_size, (int32_t)sizeof(vbuf));
                        if (p->read_raw(voxel_ptr, vbuf, to_read)) {
                            if (ComputeSmokeBox(vbuf, to_read, origin, box)) {
                                if (smoke_cache.size() < 64)
                                    smoke_cache.push_back({ ent, voxel_update, box });
                                smokes.push_back(box);
                                continue;
                            }
                        }
                    }

                    // voxel 读取失败 → 回退到估算 AABB（烟雾云约 300×300×350 units）。
                    box.min = Vec3_t(origin.x - 150.f, origin.y - 150.f, origin.z);
                    box.max = Vec3_t(origin.x + 150.f, origin.y + 150.f, origin.z + 350.f);
                    if (smoke_cache.size() < 64)
                        smoke_cache.push_back({ ent, voxel_update, box });
                }
                smokes.push_back(box);
            }
        }
    }

    // Build the new snapshot off-lock, then publish it under a short lock.
    auto snap = std::make_shared<Snapshot>();
    snap->game = std::move(game_local);
    snap->bomb = std::move(bomb_local);
    snap->globals = std::move(globals_local);
    snap->local = std::move(local_local);
    snap->players = std::move(scan);
    snap->smokes = std::move(smokes);

    {
        std::lock_guard<std::mutex> lock(mtx);
        current = std::move(snap);
        last = now;
        
        // Update cached frame time for next iteration's adaptive interval
        // Target: refresh at 2x frame rate (half frame time), minimum 1ms, maximum 5ms
        float frame_ms = current->globals.frame_time;
        float target_interval_ms = std::clamp(frame_ms * 0.5f, 1.0f, 5.0f);
        // Store for next iteration (we'll use a static variable in the throttle section)
        static float s_cached_interval = 2.0f;
        s_cached_interval = target_interval_ms;
    }

    return true;
}
