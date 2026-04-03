#include "Components/VoltageSource.h"

CircuitLab::VoltageSource::VoltageSource(double value) : Component(2), m_voltage(value)
{}

void CircuitLab::VoltageSource::Stamp(Eigen::MatrixXd & A, Eigen::VectorXd & B, const std::map<int, int>&nodeMap, const std::map<int, int> &voltageSourceMap)
{
	int n1, n2, k;

	if (GetTerminals()[0].GetNodeId() != 0)
		n1 = nodeMap.at(GetTerminals()[0].GetNodeId());
	else
		n1 = -1;

	if (GetTerminals()[1].GetNodeId() != 0)
		n2 = nodeMap.at(GetTerminals()[1].GetNodeId());
	else
		n2 = -1;

	k = voltageSourceMap.at(GetId());

	if (n1 >= 0) {
		A(n1, k) += 1;
		A(k, n1) += 1;
	}

	if (n2 >= 0) {
		A(n2, k) -= 1;
		A(k, n2) -= 1;
	}

	B[k] = GetVoltage();
}

void CircuitLab::VoltageSource::SaveSpecificData()
{}

void CircuitLab::VoltageSource::LoadSpecificData()
{}