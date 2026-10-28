#include <algorithm>

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

	// Si limita a SHORT_CIRCUIT_CONDUCTANCE anche per R piccole ma non nulle
	// (es. 1 nΩ darebbe 1e9 S): lo stesso intervallo dinamico eccessivo.
	if (m_resistance > 0.0)
		m_conductance = std::min(1.0 / m_resistance, SHORT_CIRCUIT_CONDUCTANCE);
	else
		m_conductance = SHORT_CIRCUIT_CONDUCTANCE; // Approssima un cortocircuito ideale
}

// Contributo MNA di una resistenza tra i nodi n1 e n2:
//
//        n1    n2
//  n1  [ +G   -G ]
//  n2  [ -G   +G ]
//
// I nodi connessi a ground (nodeId == 0) hanno indice -1
// e non vengono scritti nella matrice.
// Il resistore è puramente resistivo: non ha comportamento dipendente dal tempo,
// quindi non contribuisce al vettore B (StampVector resta quello vuoto di Component).
void CircuitLab::Resistor::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &,
	double)
{
	StampConductance(A, MatrixIndex(0, nodeMap), MatrixIndex(1, nodeMap), m_conductance);
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
