#include "Components/Switch.h"
#include "Common/ComponentType.h"

CircuitLab::Switch::Switch(bool closed) : Component(2, ComponentType::switchComponent)
{
	SetClosed(closed);
}

// Stesso approccio del cortocircuito approssimato in Resistor::SetResistance:
// una conduttanza molto grande quando chiuso, molto piccola (non zero, per non
// introdurre righe letteralmente nulle in matrice) quando aperto.
void CircuitLab::Switch::SetClosed(bool closed)
{
	m_closed = closed;
	m_conductance = closed ? SHORT_CIRCUIT_CONDUCTANCE : 1e-12;
}

// Identico a Resistor::StampMatrix: una conduttanza tra i nodi n1 e n2.
// Puramente resistivo (nel senso lato: una conduttanza fissa): nessun contributo dinamico.
void CircuitLab::Switch::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &,
	double)
{
	StampConductance(A, MatrixIndex(0, nodeMap), MatrixIndex(1, nodeMap), m_conductance);
}

void CircuitLab::Switch::ToggleSwitch()
{
	SetClosed(!m_closed);
}

void CircuitLab::Switch::SaveSpecificData(nlohmann::json &j) const
{
	j["closed"] = m_closed;
}

void CircuitLab::Switch::LoadSpecificData(const nlohmann::json &j)
{
	SetClosed(j["closed"]);
}
