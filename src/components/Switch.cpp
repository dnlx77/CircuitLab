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
	m_conductance = closed ? 1e12 : 1e-12;
}

// Identico a Resistor::StampMatrix: una conduttanza tra i nodi n1 e n2.
void CircuitLab::Switch::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	double h)
{
	(void)voltageSourceMap;
	(void)h;

	int n1 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	if (n1 >= 0) A(n1, n1) += m_conductance;
	if (n2 >= 0) A(n2, n2) += m_conductance;

	if (n1 >= 0 && n2 >= 0) {
		A(n1, n2) -= m_conductance;
		A(n2, n1) -= m_conductance;
	}
}

// Puramente resistivo (nel senso lato: una conduttanza fissa): nessun
// contributo dinamico, come Resistor::StampVector.
void CircuitLab::Switch::StampVector(Eigen::VectorXd &B, const std::map<int, int> &nodeMap, const std::map<int, int> &voltageSourceMap, const StampContext &ctx)
{
	(void)B;
	(void)nodeMap;
	(void)voltageSourceMap;
	(void)ctx;
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

// Nessun valore continuo da esporre nel pannello proprietà: lo stato si
// controlla solo con click/tasto (vedi UI), non editando un numero.
std::map<CircuitLab::ComponentValue, double> CircuitLab::Switch::GetValues() const
{
	std::map<ComponentValue, double> map;
	return map;
}

void CircuitLab::Switch::SetValues(const std::map<ComponentValue, double> &values)
{
	(void)values;
}
