#pragma once
#include <string>
#include <vector>
#include <Eigen/Dense>
#include <nlohmann/json.hpp>
#include "Terminal.h"
#include "Common/ComponentType.h"
#include "Common/ComponentValue.h"

namespace CircuitLab {

	struct StampContext {
		double t = 0.0;
		double companionState = 0.0;
		double h = 0.0;
	};

	// Classe base astratta per tutti i componenti elettrici.
	// Ogni componente ha un ID univoco, un certo numero di terminali
	// e deve implementare il metodo Stamp() per contribuire alla matrice MNA.
	class Component {
	protected:
		static int s_nextId;        // Contatore globale per assegnare ID univoci
		int m_id;                   // ID univoco di questo componente
		int m_terminalNumber;       // Numero di terminali del componente
		std::vector<Terminal> m_terminals; // Lista dei terminali
		ComponentType m_componentType;

		// Metodi per la serializzazione - da implementare nelle classi derivate
		virtual void SaveSpecificData(nlohmann::json &j) const = 0;
		virtual void LoadSpecificData(const nlohmann::json &j) = 0;

	public:
		// Il costruttore alloca automaticamente il numero corretto di terminali
		Component(int terminalNumber, ComponentType compType) : m_id(s_nextId++), m_terminalNumber(terminalNumber), m_componentType(compType) {
			m_terminals.resize(m_terminalNumber);
		}
		virtual ~Component() = default;

		int GetId() const { return m_id; }
		static void Reset() { s_nextId = 1; }
		const std::vector<Terminal> &GetTerminals() const { return m_terminals; }
		Terminal &GetTerminal(int index) { return m_terminals[index]; }
		std::vector<int> GetTerminalId() const;
		virtual bool IsGround() const { return false; }
		ComponentType GetType() const { return m_componentType; }

		void Save(nlohmann::json &j) const;
		void Load(const nlohmann::json &j);

		// Stampa il contributo del componente nella matrice MNA (A) e nel vettore (b).
		// nodeMap mappa nodeId -> indice di riga/colonna nella matrice.
		// voltageSourceMap mappa componentId -> indice della riga extra per le sorgenti di tensione.

		virtual void StampMatrix(Eigen::MatrixXd &A,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap) = 0;

		virtual void StampVector(Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			const StampContext& ctx) = 0;

		// Restituisce il numero di variabili extra introdotte nella matrice MNA.
		// Di default 0; le sorgenti di tensione lo sovrascrivono con 1.
		virtual int GetExtraVariables() const { return 0; }

		virtual std::map<ComponentValue, double> GetValues() const = 0;
		virtual void SetValues(const std::map<ComponentValue, double> &values) = 0;
	};
}