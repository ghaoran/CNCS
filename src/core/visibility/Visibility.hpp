#pragma once
#include <string>
#include <vector>
#include "CollisionMesh.hpp"
#include "core/engine/types/Vec3.hpp"

struct SmokeBox {
    Vec3_t min;
    Vec3_t max;
};

// Per-map collision singleton: loads the current map's .glb collision geometry
// and answers visibility (line-of-sight) queries for the aimbot.
class Visibility {
public:
    static Visibility& Get();

    void UpdateMap(const std::string& map_name);
    void SetSmokes(std::vector<SmokeBox> smokes);

    bool IsLoaded() const { return m_mesh.IsLoaded(); }
    size_t TriangleCount() const { return m_mesh.TriangleCount(); }
    size_t SmokeCount() const { return m_smokes.size(); }

    bool RayBlocked(const Vec3_t& from, const Vec3_t& to) const;
    bool SmokeBlocksRay(const Vec3_t& from, const Vec3_t& to) const;

private:
    CollisionMesh m_mesh;
    std::string m_cur_map;
    std::vector<SmokeBox> m_smokes;
};
