#include "Components/VoltageSource.h"

CircuitLab::VoltageSource::VoltageSource(double value) : Component(2, ComponentType::voltageSource), m_voltage(value)
{}

void CircuitLab::VoltageSource::StampMatrix(Eigen::MatrixXd & A, const std::map<int, int>&nodeMap, const std::map<int, int>&voltageSourceMap)
{
	// Recupera gli indici nella matrice (-1 se il terminale è a ground)
	int n1 = (GetTerminals()[0].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	// k è l'indice della riga extra per la corrente incognita
	int k = voltageSourceMap.at(GetId());

	if (n1 >= 0) { A(n1, k) += 1; A(k, n1) += 1; }
	if (n2 >= 0) { A(n2, k) -= 1; A(k, n2) -= 1; }
}

void CircuitLab::VoltageSource::StampVector(Eigen::VectorXd & B, const std::map<int, int>&nodeMap, const std::map<int, int>&voltageSourceMap, const StampContext & ctx)
{
	(void)nodeMap;
	(void)ctx;
	// k è l'indice della riga extra per la corrente incognita
	int k = voltageSourceMap.at(GetId());

	B[k] = m_voltage;
}

void CircuitLab::VoltageSource::SaveSpecificData(nlohmann::json &j) const
{
	j["value"] = m_voltage;
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::VoltageSource::GetValues() const
{
	std::map<ComponentValue, double> map;
	map[ComponentValue::voltage] = m_voltage;
	return map;
}

void CircuitLab::VoltageSource::SetValues(const std::map<ComponentValue, double> &values)
{
	m_voltage= values.at(ComponentValue::voltage);
}
