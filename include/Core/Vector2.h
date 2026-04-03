#pragma once

namespace CircuitLab {

	struct Vec2 {
		float x, y;
		Vec2() : x(0.0f), y(0.0f) {}
		Vec2(int _x, int _y) : x(static_cast<float>(_x)), y(static_cast<float>(_y)) {}
	};
}