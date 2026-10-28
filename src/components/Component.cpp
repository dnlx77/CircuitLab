#include "Components/Component.h"

std::vector<int> CircuitLab::Component::GetTerminalNodeIds() const
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

// Non legge "id"/"type" da j: il remapping ID e la creazione dell'istanza
// del tipo corretto avvengono a monte, in IOManager.
void CircuitLab::Component::Load(const nlohmann::json &j)
{
	LoadSpecificData(j);
}
