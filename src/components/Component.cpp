#include "Components/Component.h"

int CircuitLab::Component::s_nextId = 1;

std::vector<int> CircuitLab::Component::GetTerminalId() const
{
	std::vector<int> vecTerminal;
	for (auto const &terminal : m_terminals)
		vecTerminal.emplace_back(terminal.GetNodeId());
	
	return vecTerminal;
}
