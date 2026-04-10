#pragma once

namespace CircuitLab {

	// Vettore 2D a componenti float.
	// Usato per le posizioni nel canvas (pixel, coordinate continue).
	struct Vec2 {
		float x, y;
		Vec2() : x(0.0f), y(0.0f) {}
		Vec2(int _x, int _y) : x(static_cast<float>(_x)), y(static_cast<float>(_y)) {}
	};

	// Vettore 2D a componenti interi.
	// Usato per gli offset dei terminali nei ComponentDesign
	// e per le posizioni dei click mouse (coordinate discrete).
	struct Vec2i {
		int x, y;
		Vec2i() : x(0), y(0) {}
		Vec2i(int _x, int _y) : x(_x), y(_y) {}
	};
}