#pragma once

#include <cmath>

namespace CircuitLab {

	// Vettore 2D a componenti float.
	// Usato per le posizioni nel canvas (pixel, coordinate continue) e per i click mouse convertiti in float.
	// Indipendente da SFML: il nucleo e la topologia dei fili (WireGraph) non devono dipendere dalla grafica.
	struct Vec2 {
		float x, y;
		Vec2() : x(0.0f), y(0.0f) {}
		Vec2(float _x, float _y) : x(_x), y(_y) {}

		Vec2 &operator+=(const Vec2 &o) { x += o.x; y += o.y; return *this; }
		Vec2 &operator-=(const Vec2 &o) { x -= o.x; y -= o.y; return *this; }
		Vec2 &operator*=(float k) { x *= k; y *= k; return *this; }
		float Length() const { return std::sqrt(x * x + y * y); }
	};

	inline Vec2 operator+(Vec2 a, const Vec2 &b) { return a += b; }
	inline Vec2 operator-(Vec2 a, const Vec2 &b) { return a -= b; }
	inline Vec2 operator-(const Vec2 &a) { return Vec2(-a.x, -a.y); }
	inline Vec2 operator*(Vec2 a, float k) { return a *= k; }
	inline Vec2 operator*(float k, Vec2 a) { return a *= k; }
	inline bool operator==(const Vec2 &a, const Vec2 &b) { return a.x == b.x && a.y == b.y; }
	inline bool operator!=(const Vec2 &a, const Vec2 &b) { return !(a == b); }

	// Vettore 2D a componenti interi.
	// Usato per gli offset dei terminali nei ComponentDesign (coordinate discrete relative al centro del componente).
	struct Vec2i {
		int x, y;
		Vec2i() : x(0), y(0) {}
		Vec2i(int _x, int _y) : x(_x), y(_y) {}
	};
}
