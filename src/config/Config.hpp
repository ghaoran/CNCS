#pragma once

#include "Current.hpp"
#include <nlohmann/json.hpp>
#include <string>

// json 别名。pch_core.hpp 通过 /FI 强制注入时也会引入 nlohmann/json，
// 为避免同一 TU 内 using json 重复定义，用宏做一次性保护。
#ifndef CNCS_JSON_ALIAS_DEFINED
#define CNCS_JSON_ALIAS_DEFINED
using json = nlohmann::json;
#endif

class Config {
public:
    ~Config() = default;
    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&) = delete;

    static bool Read();
    static bool Write();
private:
    Config() {};

    static Config& GetInstance()
    {
        static Config i{};
        return i;
    }

    bool ReadImpl();
    bool WriteImpl();

    static color_t JsonToColor(const json& parent, const std::string& key, const color_t& def);
    static void ColorToJson(json& parent, const std::string& key, const color_t& color);
    static void Vec2ToJson(json& parent, const std::string& key, const Vec2_t& vec);
    static Vec2_t JsonToVec2(const json& parent, const std::string& key, const Vec2_t& def);
};