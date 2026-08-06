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
		Terminal *m_connected;  // Non utilizzato: residuo di una precedente logica di collegamento diretto tra terminali. Da rimuovere.
		int m_nodeId;           // ID del nodo MNA (-1 = non assegnato, 0 = ground)

	public:
		Terminal() : m_id(s_nextId++), m_connected(nullptr), m_nodeId(-1) {}

		int GetId() const { return m_id; }
		int GetNodeId() const { return m_nodeId; }
		Terminal *GetTerminalConnected() const { return m_connected; }  // Non utilizzato altrove nel codice

		void SetNodeId(int id) { m_nodeId = id; }
		void SetTerminalConnected(Terminal *terminal) { m_connected = terminal; }  // Non utilizzato altrove nel codice
	};
}