#pragma once
#include <string>
#include <vector>
#include <Eigen/Dense>
#include "Terminal.h"

namespace CircuitLab {

	// Classe base astratta per tutti i componenti elettrici.
	// Ogni componente ha un ID univoco, un certo numero di terminali
	// e deve implementare il metodo Stamp() per contribuire alla matrice MNA.
	class Component {
	protected:
		static int s_nextId;        // Contatore globale per assegnare ID univoci
		int m_id;                   // ID univoco di questo componente
		int m_terminalNumber;       // Numero di terminali del componente
		std::vector<Terminal> m_terminals; // Lista dei terminali

		// Metodi per la serializzazione - da implementare nelle classi derivate
		virtual void SaveSpecificData() = 0;
		virtual void LoadSpecificData() = 0;

	public:
		// Il costruttore alloca automaticamente il numero corretto di terminali
		Component(int terminalNumber) : m_id(s_nextId++), m_terminalNumber(terminalNumber) {
			m_terminals.resize(m_terminalNumber);
		}
		virtual ~Component() = default;

		int GetId() const { return m_id; }
		const std::vector<Terminal> &GetTerminals() const { return m_terminals; }
		Terminal &GetTerminal(int index) { return m_terminals[index]; }

		void Save();
		void Load();

		// Stampa il contributo del componente nella matrice MNA (A) e nel vettore (b).
		// nodeMap mappa nodeId -> indice di riga/colonna nella matrice.
		// voltageSourceMap mappa componentId -> indice della riga extra per le sorgenti di tensione.
		virtual void Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap) = 0;

		// Restituisce il numero di variabili extra introdotte nella matrice MNA.
		// Di default 0; le sorgenti di tensione lo sovrascrivono con 1.
		virtual int GetExtraVariables() const { return 0; }
	};
}