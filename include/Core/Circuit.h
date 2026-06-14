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
	public:
		using fnOnFactorize = std::function<void(const Eigen::MatrixXd &matrix)>;
	private:
		std::vector<std::unique_ptr<Component>> m_components; // Componenti del circuito
		Eigen::MatrixXd m_circuitMatrix;  // Matrice A del sistema MNA
		Eigen::VectorXd m_circuitVector;  // Vettore b del sistema MNA
		std::map<int, int> m_nodesMap;         // nodeId -> indice nella matrice
		std::map<int, int> m_voltageSourceMap; // componentId -> indice riga extra (per sorgenti di tensione)
		std::vector<Link> m_links;             // Lista dei collegamenti tra terminali
		int m_nextNodeId;  // Prossimo ID disponibile per i nodi
		bool m_isDirty;    // true se il circuito è stato modificato e va ricalcolato

		fnOnFactorize m_onFactorize;

		// Costruisce le mappe nodi/sorgenti e restituisce la dimensione della matrice MNA
		int ComputeNodes();

		// Restituisce l'ID del terminale dato il componente e l'indice del terminale
		int GetTerminalId(int compId, int termIndex) const;

		// Restituisce l'ID del componente che possiede il terminale con l'ID dato
		int GetComponentId(int terminalId) const;

		// Controlla se il collegamento esiste già nella lista dei link (in entrambe le direzioni)
		bool IsDuplicate(Link newLink) const;

	public:
		Circuit();

		// Getter con lazy evaluation: ricalcolano il circuito se necessario
		const Eigen::MatrixXd &GetCircuitMatrix();
		const Eigen::VectorXd &GetCircuitVector();

		// Restituisce un puntatore al componente con l'ID dato (nullptr se non trovato)
		Component *GetComponentById(int compId);
		const Component *GetComponentById(int compId) const;

		// Aggiunge un componente al circuito e restituisce il suo ID
		int AddComponent(std::unique_ptr<Component> comp);

		// Rimuove un componente dal circuito e ricostruisce le connessioni rimaste
		void RemoveComponent(int compId);

		// Segnala che il circuito è stato modificato e va ricalcolato
		void InvalidateCircuit() { m_isDirty = true; }

		bool IsCircuitEmpty() const { return m_components.empty(); }

		// Restituisce true se il circuito contiene solo componenti ground
		// (caso degenere: nessun nodo attivo nella matrice MNA)
		bool CircuitHasOnlyGround() const;

		// Collega due terminali di due componenti, propagando il nodeId a tutti
		// i terminali già connessi allo stesso nodo.
		// Il parametro addLink controlla se il collegamento va aggiunto alla lista dei link
		// (false quando si ricostruiscono le connessioni dopo una rimozione).
		// Restituisce false se il collegamento non è valido o è duplicato.
		bool ConnectTerminals(int comp1Id, int termComp1, int comp2Id, int termComp2, bool addLink = true);

		// Ricalcola matrice e vettore MNA se il circuito è dirty
		void ComputeMatrix();
		void ComputeVector(const StampContext &ctx);

		void PrintCircuit();

		void Clear();

		// Restituisce la lista dei nodeId dei terminali del componente dato
		std::vector<int> GetNodesIdFromComponentId(int compId) const;

		// Dato un indice nella matrice, restituisce il nodeId corrispondente (-1 se non trovato)
		int GetNodesFromIndex(int index) const;

		int GetIndexFromNodes(int nodeId) const;

		// Dato un indice nella matrice, restituisce il componentId della sorgente di tensione (-1 se non trovato)
		int GetCurrentFromIndex(int index) const;

		// Restituisce la lista dei componenti (usata da IOManager per la serializzazione)
		const std::vector<std::unique_ptr<Component>> &GetComponentsVector() const &{ return m_components; }

		// Restituisce la lista dei link tra terminali (usata da IOManager per la serializzazione)
		const std::vector<Link> &GetLinksVector() const { return m_links; }

		std::map<ComponentValue, double> GetComponentValues(int compId) const;
		void SetComponentValues(int compId, const std::map<ComponentValue, double> &values);

		void SetOnFactorize(const fnOnFactorize &func) { m_onFactorize = func; }

		std::vector<int> GetComponentsByNodeId(int nodeId) const;

		ComponentType GetComponentType(int compId) const;
	};
}