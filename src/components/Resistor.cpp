#include "Components/Resistor.h"

CircuitLab::Resistor::Resistor() : Component(2)
{
	SetResistance(1.0);
}

void CircuitLab::Resistor::SetResistance(double res)
{
	m_resistance = res;

	if (m_resistance > 1e-9) {
		m_conductance = 1.0 / m_resistance;
	}
	else {
		m_conductance = 1e12;
	}
}

void CircuitLab::Resistor::Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &b, const std::map<int,int>& nodeMap)
{
	(void)b;
	int n1, n2;
	
	if (GetTerminals()[0].GetNodeId() != 0)
		n1 = nodeMap.at(GetTerminals()[0].GetNodeId());
	else
		n1 = -1;

	if (GetTerminals()[1].GetNodeId() != 0)
		n2 = nodeMap.at(GetTerminals()[1].GetNodeId());
	else
		n2 = -1;

	if (n1 >= 0)
		A(n1, n1) += GetConductance();

	if (n1 >= 0 && n2 >= 0) {
		A(n1, n2) -= GetConductance();
		A(n2, n1) -= GetConductance();
	}

	if (n2 >= 0 )
		A(n2, n2) += GetConductance();
}

void CircuitLab::Resistor::SaveSpecificData()
{}

void CircuitLab::Resistor::LoadSpecificData()
{}
