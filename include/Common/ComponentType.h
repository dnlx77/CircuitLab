#pragma once
#include <nlohmann/json.hpp>

namespace CircuitLab {

	// Enumerazione dei tipi di componenti elettrici supportati dal simulatore.
	// Usata sia dal circuito (per la factory in Application) che dalla UI
	// (per determinare il design grafico in ComponentView).
	enum class ComponentType {
		node,          // Nodo generico (riservato per usi futuri)
		ground,        // Nodo di riferimento (massa)
		resistor,      // Resistenza ideale
		voltageGenerator, // Sorgente di tensione ideale
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(ComponentType, {
		{ ComponentType::resistor, "Resistor" },
		{ ComponentType::voltageGenerator, "VoltageGenerator" },
		{ ComponentType::ground, "Ground" },
	})

	enum class WaveFormType {
		dcWaveForm,
		sineWaveForm,
		squareWaveForm
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(WaveFormType, {
		{ WaveFormType::dcWaveForm, "DC" },
		{ WaveFormType::sineWaveForm, "Sine" },
		{ WaveFormType::squareWaveForm, "Square" },
	})
}