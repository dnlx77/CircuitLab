#include "Components/Ground.h"

// Il ground ha un solo terminale, il cui nodeId è fissato a 0
// per convenzione MNA (nodo di riferimento, non appare nella matrice).
CircuitLab::Ground::Ground() : Component(1, ComponentType::ground)
{
	m_terminals[0].SetNodeId(0);
}

void CircuitLab::Ground::StampMatrix(Eigen::MatrixXd &A, const std::map<int, int> &nodeMap, const std::map<int, int> &voltageSourceMap)
{
	(void)A;
	(void)nodeMap;
	(void)voltageSourceMap;
}

void CircuitLab::Ground::StampVector(Eigen::VectorXd & B, const std::map<int, int>&nodeMap, const std::map<int, int>&voltageSourceMap, const StampContext & ctx)
{
	(void)B;
	(void)nodeMap;
	(void)voltageSourceMap;
	(void)ctx;
}

void CircuitLab::Ground::SaveSpecificData(nlohmann::json &j) const
{
	(void)j;
}

void CircuitLab::Ground::LoadSpecificData(const nlohmann::json &j)
{
	(void)j;
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::Ground::GetValues() const
{
	std::map<ComponentValue, double> map;
	return map;
}

void CircuitLab::Ground::SetValues(const std::map<ComponentValue, double> &values)
{
	(void)values;
}
