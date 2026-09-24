#pragma once

namespace CircuitLab {
	enum class ComponentValue {
		resistance,
		voltage,
		amplitude,
		frequency,
		phase,
		capacitance,
		inductance,
		saturationCurrent,   // Is del diodo (Ampere)
		emissionCoefficient, // n del diodo (fattore di idealità)
		primaryInductance,   // L1 del trasformatore (Henry)
		secondaryInductance, // L2 del trasformatore (Henry)
		couplingCoefficient  // k del trasformatore (0..1, adimensionale)
	};
}