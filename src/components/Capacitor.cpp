#include "Components/Capacitor.h"
#include "Common/ComponentType.h"

CircuitLab::Capacitor::Capacitor(double value) : Component(2, ComponentType::capacitor), m_previousVoltage(0.0)
{
	m_capacitance = value;
	m_conductance = 0.0; // Ricalcolata al primo StampMatrix, quando h è noto
}

// Contributo MNA di Geq = C/h tra i nodi n1 e n2: identico a quello di una
// resistenza di conduttanza Geq (vedi Resistor::StampMatrix).
void CircuitLab::Capacitor::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	double h)
{
	(void)voltageSourceMap;

	m_conductance = (h > 1e-15) ? (m_capacitance / h) : 1e12;

	int n1 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	if (n1 >= 0) A(n1, n1) += m_conductance;
	if (n2 >= 0) A(n2, n2) += m_conductance;

	if (n1 >= 0 && n2 >= 0) {
		A(n1, n2) -= m_conductance;
		A(n2, n1) -= m_conductance;
	}
}

// Ieq = Geq * v(t-h), iniettata in n1 e prelevata da n2 (stessa polarità
// del termine v1-v2 usato in StampMatrix).
void CircuitLab::Capacitor::StampVector(Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	const StampContext &ctx)
{
	(void)voltageSourceMap;
	(void)ctx;

	double iEq = m_conductance * m_previousVoltage;

	int n1 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	if (n1 >= 0) B[n1] += iEq;
	if (n2 >= 0) B[n2] -= iEq;
}

void CircuitLab::Capacitor::UpdateState(double v1, double v2)
{
	m_previousVoltage = v1 - v2;
}

void CircuitLab::Capacitor::SaveSpecificData(nlohmann::json &j) const
{
	j["value"] = m_capacitance;
}

void CircuitLab::Capacitor::LoadSpecificData(const nlohmann::json &j)
{
	m_capacitance = j["value"];
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::Capacitor::GetValues() const
{
	std::map<ComponentValue, double> map;
	map[ComponentValue::capacitance] = m_capacitance;
	return map;
}

void CircuitLab::Capacitor::SetValues(const std::map<ComponentValue, double> &values)
{
	m_capacitance = values.at(ComponentValue::capacitance);
}
