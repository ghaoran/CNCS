#pragma once
#include <cmath>
#include <nlohmann/json.hpp>

class Vec2_t {
public:
	Vec2_t() = default;
	explicit Vec2_t(float a) : x(a), y(a) {}
	Vec2_t(float a, float b) : x(a), y(b) {}

	// ImGui compatibility
	operator ImVec2() const { return ImVec2(x, y); }
	Vec2_t(const ImVec2& v) : x(v.x), y(v.y) {}

	operator float* () {
		return &x;
	}
	bool operator==(const Vec2_t& o) const {
		return x == o.x && y == o.y;
	}
	bool operator==(const float o) const {
		return x == o && y == o;
	}
	bool operator!=(const Vec2_t& o) const {
		return !(*this == o);
	}
	bool operator!=(const float o) const {
		return !(*this == o);
	}
	bool operator>(const Vec2_t& o) const {
		return x > o.x && y > o.y;
	}
	bool operator>(const float o) const {
		return x > o && y > o;
	}
	bool operator<(const Vec2_t& o) const {
		return o > *this;
	}
	bool operator<(const float o) const {
		return Vec2_t(o, o) > *this;
	}
	bool operator>=(const Vec2_t& o) const {
		return x >= o.x && y >= o.y;
	}
	bool operator>=(const float o) const {
		return x >= o && y >= o;
	}
	bool operator<=(const Vec2_t& o) const {
		return o >= *this;
	}
	bool operator<=(const float o) const {
		return Vec2_t(o, o) >= *this;
	}
	Vec2_t operator+(const Vec2_t& o) const {
		return Vec2_t(x + o.x, y + o.y);
	}
	Vec2_t operator+(const float o) const {
		return Vec2_t(x + o, y + o);
	}
	Vec2_t operator-() const {
		return Vec2_t(-x, -y);
	}
	Vec2_t operator-(const Vec2_t& o) const {
		return *this + (-o);
	}
	Vec2_t operator-(const float o) const {
		return *this + (-o);
	}
	Vec2_t operator*(const Vec2_t& o) const {
		return Vec2_t(x * o.x, y * o.y);
	}
	Vec2_t operator*(const float o) const {
		return Vec2_t(x * o, y * o);
	}
	Vec2_t operator/(const Vec2_t& o) const {
		return Vec2_t(x / o.x, y / o.y);
	}
	Vec2_t operator/(const float o) const {
		return Vec2_t(x / o, y / o);
	}
	void operator+=(const Vec2_t& o) {
		x += o.x;
		y += o.y;
	}
	void operator+=(const float o) {
		x += o;
		y += o;
	}
	void operator-=(const Vec2_t& o) {
		x -= o.x;
		y -= o.y;
	}
	void operator*=(const Vec2_t& o) {
		x *= o.x;
		y *= o.y;
	}
	void operator*=(const float o) {
		x *= o;
		y *= o;
	}
	void operator/=(const Vec2_t& o) {
		x /= o.x;
		y /= o.y;
	}
	void operator/=(const float o) {
		x /= o;
		y /= o;
	}

	Vec2_t floor() const {
		return Vec2_t(floorf(x), floorf(y));
	}
	Vec2_t ceil() const {
		return Vec2_t(ceilf(x), ceilf(y));
	}
	Vec2_t round() const {
		return Vec2_t(roundf(x), roundf(y));
	}
	Vec2_t& floor() {
		x = floorf(x);
		y = floorf(y);
		return *this;
	}
	Vec2_t& ceil() {
		x = ceilf(x);
		y = ceilf(y);
		return *this;
	}
	Vec2_t& round() {
		x = roundf(x);
		y = roundf(y);
		return *this;
	}
	float len_sqr() const {
		return x * x + y * y;
	}
	float len() const {
		return sqrtf(len_sqr());
	}
	bool is_zero() const {
		return *this == 0.f;
	}

	float dist_to(const Vec2_t& pos) const {
		return sqrtf(powf(pos.x - x, 2) + powf(pos.y - y, 2));
	}

	// nlohmann::json 适配
	friend void to_json(nlohmann::json& j, const Vec2_t& v) {
		j = nlohmann::json::array({ v.x, v.y });
	}

	friend void from_json(const nlohmann::json& j, Vec2_t& v) {
		if (j.is_array() && j.size() == 2) {
			v.x = j[0].get<float>();
			v.y = j[1].get<float>();
		}
	}

	float x{}, y{};
};