#include "Components/Ground.h"

// Il ground ha un solo terminale, il cui nodeId è fissato a 0
// per convenzione MNA (nodo di riferimento, non appare nella matrice).
CircuitLab::Ground::Ground() : Component(1, ComponentType::ground)
{
	m_terminals[0].SetNodeId(0);
}

void CircuitLab::Ground::SaveSpecificData(nlohmann::json &) const
{
}

void CircuitLab::Ground::LoadSpecificData(const nlohmann::json &)
{
}

