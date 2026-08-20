#pragma once

#include <nlohmann/json.hpp>

struct color_t {
    float r, g, b, a;

    color_t() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    color_t(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    color_t(const ImColor& col) : r(col.Value.x), g(col.Value.y), b(col.Value.z), a(col.Value.w) {}
    color_t(const ImVec4& vec) : r(vec.x), g(vec.y), b(vec.z), a(vec.w) {}

    operator ImColor() const { return ImColor(r, g, b, a); }
    operator ImVec4() const { return ImVec4(r, g, b, a); }

    float* data() { return &r; }
    const float* data() const { return &r; }

    // nlohmann::json 适配
    friend void to_json(nlohmann::json& j, const color_t& c) {
        j = nlohmann::json::array({ c.r, c.g, c.b, c.a });
    }

    friend void from_json(const nlohmann::json& j, color_t& c) {
        if (j.is_array() && j.size() == 4) {
            c.r = j[0].get<float>();
            c.g = j[1].get<float>();
            c.b = j[2].get<float>();
            c.a = j[3].get<float>();
        }
    }
};