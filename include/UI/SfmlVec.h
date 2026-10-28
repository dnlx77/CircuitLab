#pragma once

#include <SFML/System/Vector2.hpp>
#include "Core/Vector2.h"

namespace CircuitLab {

	// Conversioni tra il Vec2 del nucleo e il tipo SFML, usate solo dal livello UI
	inline sf::Vector2f ToSf(const Vec2 &v) { return sf::Vector2f(v.x, v.y); }
	inline Vec2 FromSf(const sf::Vector2f &v) { return Vec2(v.x, v.y); }
}
