#include "Visibility.hpp"

#include <cmath>
#include <filesystem>
#include <vector>
#include <windows.h>

#include "core/logger/LogHelper.hpp"
#include "resource.h"

namespace {
    std::filesystem::path ExeDir() {
        char buf[MAX_PATH];
        DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
        if (n == 0) return std::filesystem::current_path();
        return std::filesystem::path(buf).parent_path();
    }

    // FNV-1a hash so the CS2 map names never appear as plaintext strings in the
    // binary (they are an obvious signature). The lookup hashes are evaluated
    // at compile time; the runtime side hashes the game's map name.
    constexpr uint32_t Fnv1a(const char* s) {
        uint32_t h = 0x811c9dc5u;
        for (; *s; ++s) {
            h ^= static_cast<uint8_t>(*s);
            h *= 0x01000193u;
        }
        return h;
    }

    uint32_t HashStr(const std::string& s) {
        uint32_t h = 0x811c9dc5u;
        for (unsigned char c : s) {
            h ^= c;
            h *= 0x01000193u;
        }
        return h;
    }

    // The game reports the map as "maps/de_dust2.vpk" (or a bare "de_dust2").
    // Strip the directory and extension so lookups and file paths use the plain
    // map id.
    std::string NormalizeMapName(const std::string& s) {
        size_t start = s.find_last_of("/\\");
        start = (start == std::string::npos) ? 0 : start + 1;
        size_t end = s.find('.', start);
        if (end == std::string::npos)
            end = s.size();
        return s.substr(start, end - start);
    }

    struct MapRes { uint32_t hash; int id; };
    const MapRes kMaps[] = {
        { Fnv1a("de_ancient"), IDR_COLMESH_ANCIENT },
        { Fnv1a("de_anubis"),  IDR_COLMESH_ANUBIS },
        { Fnv1a("de_cache"),   IDR_COLMESH_CACHE },
        { Fnv1a("de_dust2"),   IDR_COLMESH_DUST2 },
        { Fnv1a("de_inferno"), IDR_COLMESH_INFERNO },
        { Fnv1a("de_mirage"),  IDR_COLMESH_MIRAGE },
        { Fnv1a("de_nuke"),    IDR_COLMESH_NUKE },
    };

    int MapResourceId(const std::string& map_name) {
        const uint32_t h = HashStr(map_name);
        for (const MapRes& m : kMaps)
            if (m.hash == h)
                return m.id;
        return -1;
    }

    bool LoadEmbeddedMap(CollisionMesh& mesh, const std::string& map_name) {
        int id = MapResourceId(map_name);
        if (id < 0) return false;
        HMODULE hm = GetModuleHandle(nullptr);
        HRSRC res = FindResource(hm, MAKEINTRESOURCE(id), RT_RCDATA);
        if (!res) return false;
        HGLOBAL hg = LoadResource(hm, res);
        if (!hg) return false;
        const uint8_t* data = (const uint8_t*)LockResource(hg);
        DWORD size = SizeofResource(hm, res);
        if (!data || size == 0) return false;
        return mesh.LoadCacheV2FromMemory(data, size);
    }
}

Visibility& Visibility::Get() {
    static Visibility inst;
    return inst;
}

void Visibility::UpdateMap(const std::string& raw_name) {
    if (raw_name.empty()) return;
    const std::string map_name = NormalizeMapName(raw_name);
    if (map_name == m_cur_map && m_mesh.IsLoaded()) return;

    m_cur_map = map_name;
    m_mesh.Clear();

    namespace fs = std::filesystem;
    std::vector<fs::path> bases = { ExeDir() / "maps", fs::current_path() / "maps" };

    auto log_loaded = [&](const char* src) {
        LOGF(INFO, "掩体判断：已加载地图碰撞数据（{}，{}三角形），当前地图 '{}'",
            src, m_mesh.TriangleCount(), map_name);
    };

    // 1) external v2 cache, 2) external v1 cache, 3) external .glb (auto-cache v2).
    for (auto& base : bases) {
        fs::path p = base / (map_name + ".m2");
        if (fs::exists(p) && m_mesh.LoadCacheV2(p.string())) { log_loaded(p.filename().string().c_str()); return; }
    }
    for (auto& base : bases) {
        fs::path p = base / (map_name + ".m1");
        if (fs::exists(p) && m_mesh.LoadCache(p.string())) { log_loaded(p.filename().string().c_str()); return; }
    }
    for (auto& base : bases) {
        fs::path p = base / (map_name + ".glb");
        if (fs::exists(p) && m_mesh.LoadGlb(p.string())) {
            m_mesh.SaveCacheV2((base / (map_name + ".m2")).string());
            log_loaded(p.filename().string().c_str());
            return;
        }
    }

    // 4) embedded resource for this map (no external files needed).
    if (LoadEmbeddedMap(m_mesh, map_name)) { log_loaded("内置"); return; }

    // 5) last resort: any external .m2/.m1/.glb in the maps dir.
    for (auto& base : bases) {
        if (!fs::exists(base)) continue;
        for (auto& e : fs::directory_iterator(base)) {
            if (e.path().extension() == ".m2" && m_mesh.LoadCacheV2(e.path().string())) { log_loaded(e.path().filename().string().c_str()); return; }
        }
        for (auto& e : fs::directory_iterator(base)) {
            if (e.path().extension() == ".m1" && m_mesh.LoadCache(e.path().string())) { log_loaded(e.path().filename().string().c_str()); return; }
        }
        for (auto& e : fs::directory_iterator(base)) {
            if (e.path().extension() == ".glb" && m_mesh.LoadGlb(e.path().string())) { log_loaded(e.path().filename().string().c_str()); return; }
        }
    }

    static std::string last_warned;
    if (last_warned != map_name) {
        last_warned = map_name;
        LOGF(WARNING, "掩体判断：未找到地图 '{}' 的碰撞数据，自瞄将不启用掩体过滤", map_name);
    }
}

void Visibility::SetSmokes(std::vector<SmokeBox> smokes) {
    m_smokes = std::move(smokes);
}

bool Visibility::SmokeBlocksRay(const Vec3_t& from, const Vec3_t& to) const {
    if (m_smokes.empty())
        return false;

    const Vec3_t d = to - from;
    const float len = d.length();
    if (len < 1e-6f)
        return false;

    // Ray-AABB intersection using the slab method (O(1) per smoke box).
    for (const auto& box : m_smokes) {
        float tmin = 0.f, tmax = len;
        bool hit = true;
        for (int ax = 0; ax < 3 && hit; ++ax) {
            const float s = from[ax], dir = d[ax];
            const float bmin = box.min[ax], bmax = box.max[ax];
            if (std::abs(dir) < 1e-8f) {
                if (s < bmin || s > bmax)
                    hit = false;
            } else {
                float t1 = (bmin - s) / dir;
                float t2 = (bmax - s) / dir;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax)
                    hit = false;
            }
        }
        if (hit && tmax >= 0.f)
            return true;
    }
    return false;
}

bool Visibility::RayBlocked(const Vec3_t& from, const Vec3_t& to) const {
    if (m_mesh.RayBlocked(from, to))
        return true;
    return SmokeBlocksRay(from, to);
}
