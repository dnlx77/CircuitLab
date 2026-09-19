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
		emissionCoefficient  // n del diodo (fattore di idealità)
	};
}