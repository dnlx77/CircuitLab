#pragma once
#include <nlohmann/json.hpp>
#include <map>

#include "Common/ComponentType.h"
#include "Common/ComponentValue.h"

namespace CircuitLab {
	// Interfaccia polimorfica per le forme d'onda (DC, sinusoidale, quadra, ...)
	// usate da VoltageGenerator. Ogni derivata implementa la propria legge Evaluate(t)
	// e la propria serializzazione dei parametri specifici.
	class WaveForm
	{
	protected:
		WaveFormType m_waveFormType;
		// Serializza i parametri specifici della forma d'onda (es. ampiezza, frequenza) in j["value"].
		// Implementata da ogni derivata, chiamata da Save().
		virtual void SaveSpecificData(nlohmann::json &j) const = 0;
	public:
		virtual ~WaveForm() = default;
		// Restituisce il valore istantaneo della forma d'onda al tempo t.
		virtual double Evaluate(double t) = 0;
		virtual std::map<ComponentValue, double> GetValues() const = 0;
		virtual void SetValues(const std::map<ComponentValue, double> &values) = 0;
		WaveFormType GetType() const { return m_waveFormType; }
		// Serializza la forma d'onda sotto la chiave "waveform" di j, per evitare
		// collisioni con le chiavi JSON del componente che la possiede (es. VoltageGenerator).
		void Save(nlohmann::json &j) const;
		// Factory: crea una forma d'onda del tipo richiesto con valori di default.
		static std::unique_ptr<WaveForm> Create(WaveFormType type);
		// Deserializza una forma d'onda da JSON (crea l'istanza corretta e ne carica i valori).
		static std::unique_ptr<WaveForm> Load(const nlohmann::json &j);
	};
}