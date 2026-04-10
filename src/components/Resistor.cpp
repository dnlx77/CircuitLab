#include "Components/Resistor.h"

CircuitLab::Resistor::Resistor(double value) : Component(2)
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
void CircuitLab::Resistor::Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap)
{
	(void)B;
	(void)voltageSourceMap;

	// Recupera gli indici nella matrice (-1 se il terminale è a ground)
	int n1 = (GetTerminals()[0].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	if (n1 >= 0) A(n1, n1) += GetConductance();
	if (n2 >= 0) A(n2, n2) += GetConductance();

	if (n1 >= 0 && n2 >= 0) {
		A(n1, n2) -= GetConductance();
		A(n2, n1) -= GetConductance();
	}
}

void CircuitLab::Resistor::SaveSpecificData() {}
void CircuitLab::Resistor::LoadSpecificData() {}