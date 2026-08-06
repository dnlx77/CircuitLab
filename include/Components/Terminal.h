#pragma once

namespace CircuitLab {

	// Rappresenta un terminale fisico di un componente elettrico.
	// Ogni terminale ha un ID univoco e appartiene a un nodo MNA (m_nodeId),
	// che è il vero meccanismo con cui i terminali risultano "collegati" tra loro
	// (vedi Circuit::ConnectTerminals, che unifica i nodeId).
	class Terminal
	{
	private:
		static int s_nextId;    // Contatore globale per assegnare ID univoci
		int m_id;               // ID univoco di questo terminale
		int m_nodeId;           // ID del nodo MNA (-1 = non assegnato, 0 = ground)

	public:
		Terminal() : m_id(s_nextId++), m_nodeId(-1) {}

		int GetId() const { return m_id; }
		int GetNodeId() const { return m_nodeId; }
		
		void SetNodeId(int id) { m_nodeId = id; }
	};
}