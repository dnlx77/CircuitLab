#include "Components/Ground.h"

CircuitLab::Ground::Ground() : Component(1)
{
	m_terminals[0].SetNodeId(0);
}

void CircuitLab::Ground::Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B, const std::map<int, int> &nodeMap, const std::map<int, int> &voltageSourceMap)
{
	(void)A;
	(void)B;
	(void)nodeMap;
	(void)voltageSourceMap;
}

void CircuitLab::Ground::SaveSpecificData()
{}

void CircuitLab::Ground::LoadSpecificData()
{}
