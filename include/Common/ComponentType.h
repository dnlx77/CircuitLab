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
		capacitor,     // Condensatore ideale
		inductor,      // Induttore ideale
		switchComponent, // Interruttore ideale (aperto/chiuso, non ha un valore continuo)
		diode,         // Diodo a giunzione (non lineare, modello di Shockley)
		transformer,   // Trasformatore (induttori accoppiati, 4 terminali)
		changeoverSwitch, // Deviatore: comune + due vie, sempre chiuso su una delle due (3 terminali)
		transistor,    // Transistor bipolare NPN (non lineare, modello di Ebers-Moll semplificato, 3 terminali)
		transistorPnp, // Transistor bipolare PNP: stessa classe e stesso modello dell'NPN, con polarità invertita
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(ComponentType, {
		{ ComponentType::resistor, "Resistor" },
		{ ComponentType::voltageGenerator, "VoltageGenerator" },
		{ ComponentType::ground, "Ground" },
		{ ComponentType::capacitor, "Capacitor" },
		{ ComponentType::inductor, "Inductor" },
		{ ComponentType::switchComponent, "Switch" },
		{ ComponentType::diode, "Diode" },
		{ ComponentType::transformer, "Transformer" },
		{ ComponentType::changeoverSwitch, "ChangeoverSwitch" },
		{ ComponentType::transistor, "Transistor" },
		{ ComponentType::transistorPnp, "TransistorPnp" },
	})

	enum class WaveFormType {
		none,
		dcWaveForm,
		sineWaveForm,
		squareWaveForm
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(WaveFormType, {
		{ WaveFormType::none, "None" },
		{ WaveFormType::dcWaveForm, "DC" },
		{ WaveFormType::sineWaveForm, "Sine" },
		{ WaveFormType::squareWaveForm, "Square" },
	})
}