#include "CollisionMesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "zlib.h"

// json 别名避免与 pch_core.hpp（/FI 强制注入）重复定义
#ifndef CNCS_JSON_ALIAS_DEFINED
#define CNCS_JSON_ALIAS_DEFINED
using json = nlohmann::json;
#endif

namespace {
    // --- glTF componentType ids ---
    constexpr int kFloat = 5126;
    constexpr int kUInt = 5125;

    constexpr int kLeafSize = 4;

    // 三角形数量上限（约 360MB）。防止恶意/损坏的缓存文件声明超大 count
    // 触发整数溢出（count * sizeof(Tri)）或内存耗尽。
    constexpr uint64_t kMaxTriangles = 10'000'000;
}

void CollisionMesh::Clear() {
    m_tris.clear();
    m_nodes.clear();
}

bool CollisionMesh::LoadGlb(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    size_t fsz = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> d(fsz);
    f.read((char*)d.data(), (std::streamsize)fsz);
    if (fsz < 20 || d[0] != 'g' || d[1] != 'l' || d[2] != 'T' || d[3] != 'F') return false;

    uint32_t json_len = *(uint32_t*)&d[12];
    if (20 + json_len > fsz) return false;
    std::string json_str((char*)&d[20], json_len);

    // BIN chunk (4-byte aligned after the JSON chunk)
    size_t bin_hdr = (20 + json_len + 3) & ~3ull;
    if (bin_hdr + 8 > fsz) return false;
    uint32_t bin_len = *(uint32_t*)&d[bin_hdr];
    const uint8_t* bin = &d[bin_hdr + 8];
    if (bin_hdr + 8 + bin_len > fsz) return false;

    json j = json::parse(json_str);

    auto accessors = j.value("accessors", json::array());
    auto bufferViews = j.value("bufferViews", json::array());
    auto meshes = j.value("meshes", json::array());

    Clear();
    m_tris.reserve(512 * 1024);

    for (auto& mesh : meshes) {
        for (auto& prim : mesh.value("primitives", json::array())) {
            if (!prim.contains("indices") || !prim["attributes"].contains("POSITION")) continue;

            int ia = prim["indices"].get<int>();
            int pa = prim["attributes"]["POSITION"].get<int>();
            if (ia < 0 || ia >= (int)accessors.size() || pa < 0 || pa >= (int)accessors.size()) continue;

            auto& idx_acc = accessors[ia];
            auto& pos_acc = accessors[pa];
            if (idx_acc.value("componentType", 0) != kUInt) continue;
            if (pos_acc.value("componentType", 0) != kFloat) continue;

            int iv = idx_acc.value("bufferView", -1);
            int pv = pos_acc.value("bufferView", -1);
            if (iv < 0 || pv < 0 || iv >= (int)bufferViews.size() || pv >= (int)bufferViews.size()) continue;

            auto& iview = bufferViews[iv];
            auto& pview = bufferViews[pv];
            size_t ibase = iview.value("byteOffset", 0);
            size_t pbase = pview.value("byteOffset", 0);

            int idx_count = idx_acc.value("count", 0);
            int pos_count = pos_acc.value("count", 0);
            if (idx_count < 3 || pos_count < 3) continue;

            // Bounds-check against the BIN chunk (count * element size).
            if (ibase + (size_t)idx_count * 4 > bin_len) continue;
            if (pbase + (size_t)pos_count * 12 > bin_len) continue;

            size_t pstride = pview.value("byteStride", (size_t)0);
            if (pstride == 0) pstride = 12;
            // 防御非法 stride: 必须 >= 12（一个 float3）且是 4 的倍数（float 对齐），
            // 否则下面的 (pstride/4) 步进和边界计算会出错或越界。
            if (pstride < 12 || (pstride % 4) != 0) continue;

            // 用实际 stride 重新校验位置数据边界。旧检查只按紧凑 12 字节估算，
            // 当 stride > 12（带 padding）时会漏掉真实越界，导致越界读。
            if (pbase + (size_t)(pos_count - 1) * pstride + 12 > bin_len) continue;

            const uint32_t* idx = (const uint32_t*)(bin + ibase);
            const float* pos = (const float*)(bin + pbase);

            for (int i = 0; i + 2 < idx_count; i += 3) {
                uint32_t a = idx[i], b = idx[i + 1], c = idx[i + 2];
                if (a >= (uint32_t)pos_count || b >= (uint32_t)pos_count || c >= (uint32_t)pos_count) continue;
                Tri t;
                const float* pa = pos + (size_t)a * (pstride / 4);
                const float* pb = pos + (size_t)b * (pstride / 4);
                const float* pc = pos + (size_t)c * (pstride / 4);
                t.v0 = Vec3_t(pa[0], pa[1], pa[2]);
                t.v1 = Vec3_t(pb[0], pb[1], pb[2]);
                t.v2 = Vec3_t(pc[0], pc[1], pc[2]);
                m_tris.push_back(t);
            }
        }
    }

    BuildBVH();
    return !m_tris.empty();
}

bool CollisionMesh::LoadCache(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    uint32_t magic = 0, version = 0;
    uint64_t count = 0;
    f.read((char*)&magic, 4);
    f.read((char*)&version, 4);
    f.read((char*)&count, 8);
    if (magic != 0x434F4C4D || count == 0 || count > kMaxTriangles) return false; // "COLM"

    Clear();
    m_tris.resize(count);
    f.read((char*)m_tris.data(), (std::streamsize)(count * sizeof(Tri)));
    if (!f) { Clear(); return false; }

    BuildBVH();
    return true;
}

bool CollisionMesh::LoadCacheFromMemory(const uint8_t* data, size_t size) {
    if (!data || size < 16) return false;
    uint32_t magic = 0, version = 0;
    uint64_t count = 0;
    std::memcpy(&magic, data, 4);
    std::memcpy(&version, data + 4, 4);
    std::memcpy(&count, data + 8, 8);
    if (magic != 0x434F4C4D || count == 0 || count > kMaxTriangles) return false; // "COLM"
    if (16 + count * sizeof(Tri) > size) return false;

    Clear();
    m_tris.resize(count);
    std::memcpy((void*)m_tris.data(), data + 16, count * sizeof(Tri));

    BuildBVH();
    return true;
}

bool CollisionMesh::SaveCache(const std::string& path) const {
    if (m_tris.empty()) return false;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    uint32_t magic = 0x434F4C4D; // "COLM"
    uint32_t version = 1;
    uint64_t count = m_tris.size();
    f.write((char*)&magic, 4);
    f.write((char*)&version, 4);
    f.write((char*)&count, 8);
    for (const Tri& t : m_tris)
        f.write((char*)&t, sizeof(Tri));
    return (bool)f;
}

void CollisionMesh::BuildBVH() {
    m_nodes.clear();
    if (m_tris.empty()) return;

    std::vector<int> tri_idx(m_tris.size());
    for (size_t i = 0; i < tri_idx.size(); i++) tri_idx[i] = (int)i;

    BuildNode(tri_idx, 0, (int)tri_idx.size(), 0);

    // Reorder m_tris into the tree's order so leaf (start, count) ranges index
    // m_tris directly. Without this, leaves referenced the temporary tri_idx
    // permutation and RayBlocked tested the wrong triangles.
    std::vector<Tri> reordered(m_tris.size());
    for (size_t i = 0; i < tri_idx.size(); i++)
        reordered[i] = m_tris[tri_idx[i]];
    m_tris = std::move(reordered);
}

int CollisionMesh::BuildNode(std::vector<int>& tri_idx, int begin, int end, int depth) {
    int node = (int)m_nodes.size();
    m_nodes.emplace_back();

    // Bounds of all triangles in [begin, end). Compute into locals — the
    // reference into m_nodes must not survive the recursive calls below
    // (they emplace_back, which can reallocate the vector).
    Vec3_t bmin(1e30f, 1e30f, 1e30f);
    Vec3_t bmax(-1e30f, -1e30f, -1e30f);
    for (int i = begin; i < end; i++) {
        const Tri& t = m_tris[tri_idx[i]];
        bmin.x = std::min({ bmin.x, t.v0.x, t.v1.x, t.v2.x });
        bmin.y = std::min({ bmin.y, t.v0.y, t.v1.y, t.v2.y });
        bmin.z = std::min({ bmin.z, t.v0.z, t.v1.z, t.v2.z });
        bmax.x = std::max({ bmax.x, t.v0.x, t.v1.x, t.v2.x });
        bmax.y = std::max({ bmax.y, t.v0.y, t.v1.y, t.v2.y });
        bmax.z = std::max({ bmax.z, t.v0.z, t.v1.z, t.v2.z });
    }

    m_nodes[node].bmin = bmin;
    m_nodes[node].bmax = bmax;

    if (end - begin <= kLeafSize || depth > 40) {
        m_nodes[node].start = begin;
        m_nodes[node].count = end - begin;
        return node;
    }

    // Split on the longest axis by centroid.
    int axis = 0;
    Vec3_t ext = bmax - bmin;
    if (ext.y > ext.x) axis = 1;
    if (ext.z > ext.y && ext.z > ext.x) axis = 2;

    int mid = begin + (end - begin) / 2;
    std::nth_element(tri_idx.begin() + begin, tri_idx.begin() + mid, tri_idx.begin() + end,
        [&](int a, int b) {
            const Tri& ta = m_tris[a];
            const Tri& tb = m_tris[b];
            auto cent = [&](const Tri& t) {
                if (axis == 0) return (t.v0.x + t.v1.x + t.v2.x);
                if (axis == 1) return (t.v0.y + t.v1.y + t.v2.y);
                return (t.v0.z + t.v1.z + t.v2.z);
            };
            return cent(ta) < cent(tb);
        });

    int left = BuildNode(tri_idx, begin, mid, depth + 1);
    int right = BuildNode(tri_idx, mid, end, depth + 1);
    m_nodes[node].left = left;
    m_nodes[node].right = right;
    return node;
}

bool CollisionMesh::RayBlocked(const Vec3_t& from, const Vec3_t& to) const {
    if (m_nodes.empty()) return false;

    Vec3_t dir = to - from;
    float max_t = dir.length();
    if (max_t < 1e-6f) return false;
    Vec3_t d = dir * (1.0f / max_t);

    // Iterative BVH traversal.
    std::vector<int> stack;
    stack.reserve(64);
    stack.push_back(0);

    while (!stack.empty()) {
        int ni = stack.back();
        stack.pop_back();
        const Node& n = m_nodes[ni];

        // AABB slab test.
        {
            float tmin = 0.f, tmax = max_t;
            for (int k = 0; k < 3; k++) {
                float o = (k == 0) ? from.x : (k == 1) ? from.y : from.z;
                float dd = (k == 0) ? d.x : (k == 1) ? d.y : d.z;
                float lo = (k == 0) ? n.bmin.x : (k == 1) ? n.bmin.y : n.bmin.z;
                float hi = (k == 0) ? n.bmax.x : (k == 1) ? n.bmax.y : n.bmax.z;
                if (std::fabs(dd) < 1e-8f) {
                    if (o < lo || o > hi) { tmin = tmax + 1.f; break; }
                } else {
                    float inv = 1.0f / dd;
                    float t0 = (lo - o) * inv;
                    float t1 = (hi - o) * inv;
                    if (t0 > t1) std::swap(t0, t1);
                    tmin = std::max(tmin, t0);
                    tmax = std::min(tmax, t1);
                    if (tmin > tmax) break;
                }
            }
            if (tmin > tmax) continue;
        }

        if (n.count > 0) {
            // Leaf: test triangles.
            float t_out;
            for (int i = n.start; i < n.start + n.count; i++) {
                if (RayHitTri(m_tris[i], from, d, max_t, t_out))
                    return true;
            }
        } else {
            stack.push_back(n.left);
            stack.push_back(n.right);
        }
    }

    return false;
}

bool CollisionMesh::RayHitTri(const Tri& tri, const Vec3_t& o, const Vec3_t& d, float max_t, float& t_out) const {
    const float EPS = 1e-6f;
    Vec3_t e1 = tri.v1 - tri.v0;
    Vec3_t e2 = tri.v2 - tri.v0;
    Vec3_t p = d.cross(e2);
    float det = e1.dot(p);
    if (std::fabs(det) < EPS) return false;
    float inv = 1.0f / det;

    Vec3_t s = o - tri.v0;
    float u = s.dot(p) * inv;
    if (u < -EPS || u > 1.0f + EPS) return false;

    Vec3_t q = s.cross(e1);
    float v = d.dot(q) * inv;
    if (v < -EPS || u + v > 1.0f + EPS) return false;

    float dist = e2.dot(q) * inv;
    if (dist > EPS && dist < max_t) {
        t_out = dist;
        return true;
    }
    return false;
}

// ---- v2 quantized cache format ----
// Header: magic/version/counts + AABB + quant scale, followed by the raw
// payload (uint16 quantized vertices + uint32 triangle indices).

namespace {
    struct ColV2Header {
        uint32_t magic = 0x434F4C32; // "COL2"
        uint32_t version = 2;
        uint32_t num_vertices = 0;
        uint32_t num_triangles = 0;
        float bmin[3] = {0, 0, 0};
        float qscale[3] = {0, 0, 0};
        uint32_t payload_compressed = 0; // zlib-compressed size (== raw if stored raw)
        uint32_t payload_raw = 0;        // uncompressed size
    };
}

bool CollisionMesh::SaveCacheV2(const std::string& path) const {
    if (m_tris.empty()) return false;

    // Global AABB.
    Vec3_t bmin(1e30f, 1e30f, 1e30f), bmax(-1e30f, -1e30f, -1e30f);
    for (const Tri& t : m_tris) {
        for (const Vec3_t* v : { &t.v0, &t.v1, &t.v2 }) {
            bmin.x = std::min(bmin.x, v->x);
            bmin.y = std::min(bmin.y, v->y);
            bmin.z = std::min(bmin.z, v->z);
            bmax.x = std::max(bmax.x, v->x);
            bmax.y = std::max(bmax.y, v->y);
            bmax.z = std::max(bmax.z, v->z);
        }
    }

    Vec3_t ext = bmax - bmin;
    float sx = std::max(ext.x / 65535.0f, 1e-6f);
    float sy = std::max(ext.y / 65535.0f, 1e-6f);
    float sz = std::max(ext.z / 65535.0f, 1e-6f);

    // Quantize + dedup vertices.
    std::unordered_map<uint64_t, uint32_t> vmap;
    std::vector<uint16_t> qverts;
    std::vector<uint32_t> indices;
    qverts.reserve(m_tris.size());
    indices.reserve(m_tris.size() * 3);

    auto add_vertex = [&](const Vec3_t& v) -> uint32_t {
        uint16_t qx = (uint16_t)std::clamp((int)std::lround((v.x - bmin.x) / sx), 0, 65535);
        uint16_t qy = (uint16_t)std::clamp((int)std::lround((v.y - bmin.y) / sy), 0, 65535);
        uint16_t qz = (uint16_t)std::clamp((int)std::lround((v.z - bmin.z) / sz), 0, 65535);
        uint64_t key = ((uint64_t)qx << 32) | ((uint64_t)qy << 16) | qz;
        auto it = vmap.find(key);
        if (it != vmap.end()) return it->second;
        uint32_t idx = (uint32_t)(qverts.size() / 3);
        vmap.emplace(key, idx);
        qverts.push_back(qx);
        qverts.push_back(qy);
        qverts.push_back(qz);
        return idx;
    };

    for (const Tri& t : m_tris) {
        indices.push_back(add_vertex(t.v0));
        indices.push_back(add_vertex(t.v1));
        indices.push_back(add_vertex(t.v2));
    }

    // Raw payload.
    size_t vbytes = qverts.size() * sizeof(uint16_t);
    size_t ibytes = indices.size() * sizeof(uint32_t);
    size_t raw_len = vbytes + ibytes;
    std::vector<uint8_t> raw(raw_len);
    std::memcpy(raw.data(), qverts.data(), vbytes);
    std::memcpy(raw.data() + vbytes, indices.data(), ibytes);

    // zlib-compress the payload; fall back to raw when compression doesn't help.
    uLongf comp_len = compressBound((uLong)raw_len);
    std::vector<uint8_t> comp(comp_len);
    const uint8_t* payload = raw.data();
    uint32_t payload_size = (uint32_t)raw_len;
    if (compress2(comp.data(), &comp_len, raw.data(), (uLong)raw_len, Z_BEST_COMPRESSION) == Z_OK && comp_len < raw_len) {
        payload = comp.data();
        payload_size = (uint32_t)comp_len;
    }

    ColV2Header h;
    h.num_vertices = (uint32_t)(qverts.size() / 3);
    h.num_triangles = (uint32_t)m_tris.size();
    h.bmin[0] = bmin.x; h.bmin[1] = bmin.y; h.bmin[2] = bmin.z;
    h.qscale[0] = sx; h.qscale[1] = sy; h.qscale[2] = sz;
    h.payload_compressed = payload_size;
    h.payload_raw = (uint32_t)raw_len;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)&h, sizeof(h));
    f.write((const char*)payload, payload_size);
    return (bool)f;
}

bool CollisionMesh::LoadCacheV2(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    size_t fsz = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> d(fsz);
    f.read((char*)d.data(), fsz);
    if (!f) return false;
    return LoadCacheV2FromMemory(d.data(), d.size());
}

bool CollisionMesh::LoadCacheV2FromMemory(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(ColV2Header)) return false;

    ColV2Header h;
    std::memcpy(&h, data, sizeof(h));
    if (h.magic != 0x434F4C32 || h.num_triangles == 0 || h.num_vertices == 0 || h.num_triangles > kMaxTriangles) return false;
    if (sizeof(h) + h.payload_compressed > size) return false;

    const uint8_t* payload = data + sizeof(h);
    std::vector<uint8_t> raw;
    if (h.payload_compressed == h.payload_raw) {
        raw.assign(payload, payload + h.payload_raw);
    } else {
        raw.resize(h.payload_raw);
        uLongf dest_len = (uLongf)h.payload_raw;
        if (uncompress(raw.data(), &dest_len, payload, h.payload_compressed) != Z_OK || dest_len != h.payload_raw)
            return false;
    }

    size_t vbytes = (size_t)h.num_vertices * 3 * sizeof(uint16_t);
    size_t ibytes = (size_t)h.num_triangles * 3 * sizeof(uint32_t);
    if (vbytes + ibytes != h.payload_raw) return false;

    const uint16_t* qverts = (const uint16_t*)raw.data();
    const uint32_t* indices = (const uint32_t*)(raw.data() + vbytes);

    std::vector<Vec3_t> verts(h.num_vertices);
    for (uint32_t i = 0; i < h.num_vertices; i++) {
        verts[i] = Vec3_t(
            h.bmin[0] + qverts[i * 3 + 0] * h.qscale[0],
            h.bmin[1] + qverts[i * 3 + 1] * h.qscale[1],
            h.bmin[2] + qverts[i * 3 + 2] * h.qscale[2]
        );
    }

    Clear();
    m_tris.resize(h.num_triangles);
    for (uint32_t i = 0; i < h.num_triangles; i++) {
        uint32_t a = indices[i * 3 + 0];
        uint32_t b = indices[i * 3 + 1];
        uint32_t c = indices[i * 3 + 2];
        if (a >= h.num_vertices || b >= h.num_vertices || c >= h.num_vertices) { Clear(); return false; }
        m_tris[i] = { verts[a], verts[b], verts[c] };
    }

    BuildBVH();
    return true;
}
