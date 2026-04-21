#pragma once
#include <vector>
#include <Eigen/Dense>
#include <memory>
#include "Components/Component.h"

namespace CircuitLab {

	// Rappresenta un collegamento tra due terminali di due componenti.
	// Usato per ricostruire le connessioni dopo la rimozione di un componente.
	struct Link {
		int compId1, termIndex1;  // Primo componente e indice del suo terminale
		int compId2, termIndex2;  // Secondo componente e indice del suo terminale
	};

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
		std::map<int, int> m_nodesMap;         // nodeId -> indice nella matrice
		std::map<int, int> m_voltageSourceMap; // componentId -> indice riga extra (per sorgenti di tensione)
		std::vector<Link> m_links;             // Lista dei collegamenti tra terminali
		int m_nextNodeId;  // Prossimo ID disponibile per i nodi
		bool m_isDirty;    // true se il circuito è stato modificato e va ricalcolato

		// Costruisce le mappe nodi/sorgenti e restituisce la dimensione della matrice MNA
		int ComputeNodes();

		// Ricalcola matrice e vettore MNA se il circuito è dirty
		void ComputeCircuit();

		// Restituisce l'ID del terminale dato il componente e l'indice del terminale
		int GetTerminalId(int compId, int termIndex) const;

		// Restituisce l'ID del componente che possiede il terminale con l'ID dato
		int GetComponentId(int terminalId) const;

		// Restituisce un puntatore al componente con l'ID dato (nullptr se non trovato)
		const Component *GetComponentById(int compId) const;
		Component *GetComponentById(int compId);

		bool IsDuplicate(Link newLink) const;

	public:
		Circuit();

		// Getter con lazy evaluation: ricalcolano il circuito se necessario
		const Eigen::MatrixXd &GetCircuitMatrix();
		const Eigen::VectorXd &GetCircuitVector();

		// Aggiunge un componente al circuito e restituisce il suo ID
		int AddComponent(std::unique_ptr<Component> comp);

		// Rimuove un componente dal circuito e ricostruisce le connessioni rimaste
		void RemoveComponent(int compId);

		// Segnala che il circuito è stato modificato e va ricalcolato
		void InvalidateCircuit() { m_isDirty = true; }

		bool IsCircuitEmpty() const { return m_components.empty(); }
		
		bool CircuitHasOnlyGround() const;

		// Collega due terminali di due componenti, propagando il nodeId a tutti
		// i terminali già connessi allo stesso nodo
		bool ConnectTerminals(int comp1Id, int termComp1, int comp2Id, int termComp2, bool addLink = true);

		void PrintCircuit();

		// Restituisce la lista dei nodeId dei terminali del componente dato
		std::vector<int> GetNodesIdFromComponentId(int compId) const;

		// Dato un indice nella matrice, restituisce il nodeId corrispondente (-1 se non trovato)
		int GetNodesFromIndex(int index) const;

		// Dato un indice nella matrice, restituisce il componentId della sorgente di tensione (-1 se non trovato)
		int GetCurrentFromIndex(int index) const;
	};
}