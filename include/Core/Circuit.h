#pragma once
#include <vector>
#include <Eigen/Dense>
#include <memory>
#include "Components/Component.h"

namespace CircuitLab {

	// Rappresenta il circuito elettrico nel suo insieme.
	// Si occupa di gestire la collezione di componenti, costruire
	// la matrice MNA (A) e il vettore (b), e gestire le connessioni tra terminali.
	// Usa lazy evaluation: la matrice viene ricalcolata solo se il circuito
	// è stato modificato (flag m_isDirty).
	class Circuit {
	private:
		std::vector<std::unique_ptr<Component>> m_components; // Componenti del circuito
		Eigen::MatrixXd m_circuitMatrix;  // Matrice A del sistema MNA
		Eigen::VectorXd m_circuitVector;  // Vettore b del sistema MNA
		std::map<int, int> m_nodesMap;          // nodeId -> indice nella matrice
		std::map<int, int> m_voltageSourceMap;   // componentId -> indice riga extra (per sorgenti di tensione)
		int m_nextNodeId;  // Prossimo ID disponibile per i nodi
		bool m_isDirty;    // true se il circuito è stato modificato e va ricalcolato

		// Costruisce le mappe nodi/sorgenti e restituisce la dimensione della matrice MNA
		int ComputeNodes();

		// Ricalcola matrice e vettore MNA se il circuito è dirty
		void ComputeCircuit();

	public:
		Circuit();

		// Getter con lazy evaluation: ricalcolano il circuito se necessario
		const Eigen::MatrixXd &GetCircuitMatrix();
		const Eigen::VectorXd &GetCircuitVector();

		// Aggiunge un componente al circuito e restituisce il suo ID
		int AddComponent(std::unique_ptr<Component> comp);
		void RemoveComponent(int compId);

		// Segnala che il circuito è stato modificato e va ricalcolato
		void InvalidateCircuit() { m_isDirty = true; }

		bool IsCircuitEmpty() const { return m_components.empty(); }

		// Collega due terminali di due componenti, propagando il nodeId a tutti
		// i terminali già connessi allo stesso nodo
		void ConnectTerminals(int comp1Id, int termComp1, int comp2Id, int termComp2);

		void PrintCircuit();

		std::vector<int> GetNodesIdFromComponentId(int compId) const;

		// Dato un indice nella matrice, restituisce il nodeId corrispondente (-1 se non trovato)
		int GetNodesFromIndex(int index) const;

		// Dato un indice nella matrice, restituisce il componentId della sorgente di tensione (-1 se non trovato)
		int GetCurrentFromIndex(int index) const;
	};
}