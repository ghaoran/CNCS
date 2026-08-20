#pragma once
#include <string>
#include <vector>
#include "core/engine/types/Vec3.hpp"

// Loads a CS2 map's collision geometry from a Source 2 Viewer .glb export and
// answers "is the segment from A to B blocked by world geometry?" via a
// Bounding Volume Hierarchy + Möller–Trumbore ray-triangle test.
//
// The .glb vertices are in the same coordinate space as the game (z = up),
// so no axis remap is needed.
class CollisionMesh {
public:
    bool LoadGlb(const std::string& path);
    void Clear();
    bool IsLoaded() const { return !m_nodes.empty(); }
    size_t TriangleCount() const { return m_tris.size(); }

    // Compact cache: raw triangles only (~36 bytes/tri), much smaller and faster
    // to load than the .glb (no JSON/BIN parsing). Generated on first load.
    bool LoadCache(const std::string& path);
    bool LoadCacheFromMemory(const uint8_t* data, size_t size);
    bool SaveCache(const std::string& path) const;

    // v2: quantized + vertex-deduplicated cache (much smaller). Quantizes each
    // vertex to uint16 against the map AABB, dedups shared vertices, stores a
    // uint32 index buffer, then zlib-compresses the whole payload.
    bool LoadCacheV2(const std::string& path);
    bool LoadCacheV2FromMemory(const uint8_t* data, size_t size);
    bool SaveCacheV2(const std::string& path) const;

    // Returns true if the segment [from, to] intersects any triangle.
    bool RayBlocked(const Vec3_t& from, const Vec3_t& to) const;

private:
    struct Tri {
        Vec3_t v0, v1, v2;
    };

    struct Node {
        Vec3_t bmin, bmax;
        int left = -1;
        int right = -1;
        int start = 0; // leaf: first triangle index
        int count = 0; // leaf: triangle count
    };

    std::vector<Tri> m_tris;
    std::vector<Node> m_nodes;

    void BuildBVH();
    int BuildNode(std::vector<int>& tri_idx, int begin, int end, int depth);
    bool RayHitTri(const Tri& t, const Vec3_t& o, const Vec3_t& d, float max_t, float& t_out) const;
};
