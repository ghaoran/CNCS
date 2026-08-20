#pragma once

struct view_matrix_t {
    float* operator[](int index) {
        return matrix[index];
    }

    const float* operator[](int index) const {
        return matrix[index];
    }

    float matrix[4][4];

	// World To Screen
	bool wts(const Vec3_t& pos, const Vec2_t& screen, Vec2_t& out, bool check_bounds = true) const {
		const RECT bounds = RECT(
			0, 0,
			static_cast<LONG>(screen.x),
			static_cast<LONG>(screen.y)
		);

		constexpr int margin = 0;

		float view = 0.f;
		const float SightX = static_cast<float>(bounds.right) / 2.f;
		const float SightY = static_cast<float>(bounds.bottom) / 2.f;

		view = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];

		if (view <= 0.01)
			return false;

		out.x = SightX + (matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3]) / view * SightX;
		out.y = SightY - (matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3]) / view * SightY;

		if (check_bounds &&
			(
				out.x < bounds.left - margin || out.x > bounds.right + margin ||
				out.y < bounds.top - margin || out.y > bounds.bottom + margin
			))
			return false;

		return true;
	}
};