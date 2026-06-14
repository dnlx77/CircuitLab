#include "Components/Resistor.h"
#include "Common/ComponentType.h"

CircuitLab::Resistor::Resistor(double value) : Component(2, ComponentType::resistor)
{
	SetResistance(value);
}

// Aggiorna resistenza e conduttanza.
// Se R è troppo piccola (quasi cortocircuito), usa un valore di conduttanza
// molto grande invece di dividere per zero.
void CircuitLab::Resistor::SetResistance(double res)
{
	m_resistance = res;

	if (m_resistance > 1e-9)
		m_conductance = 1.0 / m_resistance;
	else
		m_conductance = 1e12; // Approssima un cortocircuito ideale
}

// Contributo MNA di una resistenza tra i nodi n1 e n2:
//
//        n1    n2
//  n1  [ +G   -G ]
//  n2  [ -G   +G ]
//
// I nodi connessi a ground (nodeId == 0) hanno indice -1
// e non vengono scritti nella matrice.
void CircuitLab::Resistor::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap)
{
	(void)voltageSourceMap;

	// Recupera gli indici nella matrice (-1 se il terminale è a ground)
	int n1 = (GetTerminals()[0].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	if (n1 >= 0) A(n1, n1) += m_conductance;
	if (n2 >= 0) A(n2, n2) += m_conductance;

	if (n1 >= 0 && n2 >= 0) {
		A(n1, n2) -= m_conductance;
		A(n2, n1) -= m_conductance;
	}
}

void CircuitLab::Resistor::StampVector(Eigen::VectorXd &B, const std::map<int, int> &nodeMap, const std::map<int, int> &voltageSourceMap, const StampContext &ctx)
{
	(void)B;
	(void)nodeMap;
	(void)voltageSourceMap;
	(void)ctx;
}

void CircuitLab::Resistor::SaveSpecificData(nlohmann::json &j) const
{
	j["value"] = m_resistance;
}

void CircuitLab::Resistor::LoadSpecificData(const nlohmann::json &j)
{
	SetResistance(j["value"]);
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::Resistor::GetValues() const
{

	std::map<ComponentValue, double> map;
	map[ComponentValue::resistance] = m_resistance;
	return map;
}

void CircuitLab::Resistor::SetValues(const std::map<ComponentValue, double> &values)
{
	SetResistance(values.at(ComponentValue::resistance));
}
