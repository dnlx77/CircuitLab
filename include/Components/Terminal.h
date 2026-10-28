#pragma once

namespace CircuitLab {

	// Rappresenta un terminale fisico di un componente elettrico.
	// Appartiene a un nodo MNA (m_nodeId), che è il vero meccanismo con cui i
	// terminali risultano "collegati" tra loro (vedi Circuit::ConnectTerminals,
	// che unifica i nodeId).
	class Terminal
	{
	private:
		int m_nodeId = -1;      // ID del nodo MNA (-1 = non assegnato, 0 = ground)

	public:
		int GetNodeId() const { return m_nodeId; }

		void SetNodeId(int id) { m_nodeId = id; }
	};
}