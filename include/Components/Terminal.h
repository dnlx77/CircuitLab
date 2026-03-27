#pragma once

namespace CircuitLab {

	class Terminal
	{
	private:
		static int s_nextId;
		int m_id;
		Terminal *m_connected;
		int m_nodeId;
	public:

		Terminal() : m_id(s_nextId++), m_connected(nullptr), m_nodeId(0) {}
		int GetId() const { return m_id; }
		int GetNodeId() const { return m_nodeId; }
		Terminal *GetTerminalConnected() const { return m_connected; }

		void SetNodeId(int id) { m_nodeId = id; }
		void SetTerminalConnected(Terminal * terminal) { m_connected = terminal; }
	};
}