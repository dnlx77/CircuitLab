#include "Components/VoltageSource.h"

CircuitLab::VoltageSource::VoltageSource(double value) : Component(2), m_voltage(value)
{}

// Contributo MNA di una sorgente di tensione tra n1 (+) e n2 (-),
// con variabile extra k (corrente attraverso la sorgente):
//
//        n1    n2    k
//  n1  [  0    0   +1 ]
//  n2  [  0    0   -1 ]
//   k  [ +1   -1    0 ]   b[k] = V
//
void CircuitLab::VoltageSource::Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap)
{
	// Recupera gli indici nella matrice (-1 se il terminale è a ground)
	int n1 = (GetTerminals()[0].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() != 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	// k è l'indice della riga extra per la corrente incognita
	int k = voltageSourceMap.at(GetId());

	if (n1 >= 0) { A(n1, k) += 1; A(k, n1) += 1; }
	if (n2 >= 0) { A(n2, k) -= 1; A(k, n2) -= 1; }

	B[k] = GetVoltage();
}

void CircuitLab::VoltageSource::SetVoltage(double value) { m_voltage = value; }
void CircuitLab::VoltageSource::SaveSpecificData() {}
void CircuitLab::VoltageSource::LoadSpecificData() {}