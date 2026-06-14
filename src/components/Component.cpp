#include "Components/Component.h"

int CircuitLab::Component::s_nextId = 1;

std::vector<int> CircuitLab::Component::GetTerminalId() const
{
	std::vector<int> vecTerminal;
	for (auto const &terminal : m_terminals)
		vecTerminal.emplace_back(terminal.GetNodeId());
	
	return vecTerminal;
}

void CircuitLab::Component::Save(nlohmann::json &j) const
{
	j["id"] = m_id;
	j["type"] = GetType();

	SaveSpecificData(j);
}

void CircuitLab::Component::Load(const nlohmann::json &j)
{
	LoadSpecificData(j);
}
