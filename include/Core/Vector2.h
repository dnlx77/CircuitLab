#pragma once

namespace CircuitLab {

	// Vettore 2D a componenti float.
	// Usato per le posizioni nel canvas (pixel, coordinate continue) e per i click mouse convertiti in float.
	struct Vec2 {
		float x, y;
		Vec2() : x(0.0f), y(0.0f) {}
		Vec2(float _x, float _y) : x(_x), y(_y) {}
	};

	// Vettore 2D a componenti interi.
	// Usato per gli offset dei terminali nei ComponentDesign (coordinate discrete relative al centro del componente).
	struct Vec2i {
		int x, y;
		Vec2i() : x(0), y(0) {}
		Vec2i(int _x, int _y) : x(_x), y(_y) {}
	};
}