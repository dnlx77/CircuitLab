#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace CircuitLab {

	// Enumerazione dei tipi di componenti elettrici supportati dal simulatore.
	// Usata sia dal circuito (per la factory in Application) che dalla UI
	// (per determinare il design grafico in ComponentView).
	enum class ComponentType {
		node,          // Nessun componente: restituito da Circuit::GetComponentType per un id inesistente
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

	// Tutto ciò che si sa di un tipo di componente fuori dal componente stesso, in un solo posto:
	// per aggiungere un tipo basta una riga qui (più la sua classe, la fabbrica in Application e
	// la geometria in ComponentView). L'ordine delle righe è quello della palette dei componenti.
	struct ComponentTypeInfo {
		ComponentType type;
		const char *prefix;       // Una lettera per etichette sul canvas e nomi dei canali: "R", "V"...
		const char *jsonName;     // Nome salvato nei file: NON cambiarlo, romperebbe i file esistenti
		const char *displayName;  // Nome interno della vista (ComponentView::GetName)
		const char *category;     // Gruppo nel menu Componenti
		const char *paletteLabel; // Voce nel menu Componenti (include la scorciatoia)
		char shortcut;            // Tasto + click sul canvas per posare il componente
		bool toggleableByClick;   // Stato aperto/chiuso cambiato con un click, non con un valore numerico
	};

	inline constexpr ComponentTypeInfo COMPONENT_TYPES[] = {
		{ ComponentType::resistor,         "R", "Resistor",         "Resistor",          "Passivi",        "Resistore (R)",       'R', false },
		{ ComponentType::capacitor,        "C", "Capacitor",        "Capacitor",         "Passivi",        "Condensatore (C)",    'C', false },
		{ ComponentType::inductor,         "L", "Inductor",         "Inductor",          "Passivi",        "Induttore (L)",       'L', false },
		{ ComponentType::voltageGenerator, "V", "VoltageGenerator", "Voltage source",    "Sorgenti",       "Generatore (V)",      'V', false },
		{ ComponentType::switchComponent,  "S", "Switch",           "Switch",            "Commutazione",   "Interruttore (S)",    'S', true },
		{ ComponentType::changeoverSwitch, "Y", "ChangeoverSwitch", "Changeover switch", "Commutazione",   "Deviatore (Y)",       'Y', true },
		{ ComponentType::diode,            "D", "Diode",            "Diode",             "Semiconduttori", "Diodo (D)",           'D', false },
		{ ComponentType::transistor,       "Q", "Transistor",       "Transistor",        "Semiconduttori", "Transistor NPN (J)",  'J', false },
		{ ComponentType::transistorPnp,    "Q", "TransistorPnp",    "Transistor PNP",    "Semiconduttori", "Transistor PNP (K)",  'K', false },
		{ ComponentType::transformer,      "T", "Transformer",      "Transformer",       "Accoppiati",     "Trasformatore (T)",   'T', false },
		{ ComponentType::ground,           "G", "Ground",           "Ground",            "Altro",          "Massa (G)",           'G', false },
	};

	// nullptr per ComponentType::node (non è un componente)
	inline const ComponentTypeInfo *FindComponentType(ComponentType type)
	{
		for (const ComponentTypeInfo &info : COMPONENT_TYPES)
			if (info.type == type)
				return &info;
		return nullptr;
	}

	inline const char *ComponentPrefix(ComponentType type)
	{
		const ComponentTypeInfo *info = FindComponentType(type);
		return info ? info->prefix : "?";
	}

	// Nel file il tipo è il suo jsonName; un nome sconosciuto si legge come il primo tipo
	// della tabella (stesso comportamento di NLOHMANN_JSON_SERIALIZE_ENUM)
	inline void to_json(nlohmann::json &j, ComponentType type)
	{
		const ComponentTypeInfo *info = FindComponentType(type);
		j = (info ? info : &COMPONENT_TYPES[0])->jsonName;
	}

	inline void from_json(const nlohmann::json &j, ComponentType &type)
	{
		const std::string name = j.get<std::string>();
		for (const ComponentTypeInfo &info : COMPONENT_TYPES)
			if (name == info.jsonName)
			{
				type = info.type;
				return;
			}
		type = COMPONENT_TYPES[0].type;
	}

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