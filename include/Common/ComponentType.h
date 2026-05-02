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
		voltageSource, // Sorgente di tensione ideale
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(ComponentType, {
		{ ComponentType::resistor, "Resistor" },
		{ ComponentType::voltageSource, "VoltageSource" },
		{ ComponentType::ground, "Ground" },
	})
}