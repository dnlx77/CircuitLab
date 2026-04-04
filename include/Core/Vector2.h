#pragma once

namespace CircuitLab {

	struct Vec2 {
		float x, y;
		Vec2() : x(0.0f), y(0.0f) {}
		Vec2(int _x, int _y) : x(static_cast<float>(_x)), y(static_cast<float>(_y)) {}
	};

	struct Vec2i {
		int x, y;
		Vec2i() : x(0), y(0) {}
		Vec2i(int _x, int _y) :x(_x), y(_y) {}
	};
}