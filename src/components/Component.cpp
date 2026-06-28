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

std::string CircuitLab::Component::ComponentTypeName(ComponentType type)
{
	switch (type)
	{
	case ComponentType::resistor:			return "R";
	case ComponentType::voltageGenerator:	return "V";
	case ComponentType::ground:				return "G";
	default:								return "?";

	}
}
