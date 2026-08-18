#include "Components/Inductor.h"
#include "Common/ComponentType.h"

CircuitLab::Inductor::Inductor(double value) : Component(2, ComponentType::inductor), m_previousCurrent(0.0)
{
	m_inductance = value;
	m_conductance = 0.0; // Ricalcolata al primo StampMatrix, quando h è noto
}

// Contributo MNA di Geq = h/L tra i nodi n1 e n2: identico a quello di una
// resistenza di conduttanza Geq (vedi Resistor::StampMatrix). Se L è troppo
// piccola (quasi un cortocircuito), usa una conduttanza molto grande invece
// di dividere per zero — stesso approccio di Resistor::SetResistance.
void CircuitLab::Inductor::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	double h)
{
	(void)voltageSourceMap;

	m_conductance = (m_inductance > 1e-9) ? (h / m_inductance) : 1e12;

	int n1 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	if (n1 >= 0) A(n1, n1) += m_conductance;
	if (n2 >= 0) A(n2, n2) += m_conductance;

	if (n1 >= 0 && n2 >= 0) {
		A(n1, n2) -= m_conductance;
		A(n2, n1) -= m_conductance;
	}
}

// Ieq = i(t-h): a differenza del condensatore (dove Ieq = Geq*v_prev viene
// iniettata in n1 e prelevata da n2), qui la corrente del ramo è
// i(t) = Geq*(v1-v2) + i(t-h): il generatore equivalente va quindi PRELEVATO
// da n1 e INIETTATO in n2 (segno opposto rispetto al condensatore) perché
// contribuisce con segno positivo, non negativo, alla corrente n1->n2.
void CircuitLab::Inductor::StampVector(Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	const StampContext &ctx)
{
	(void)voltageSourceMap;
	(void)ctx;

	double iEq = m_previousCurrent;

	int n1 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	if (n1 >= 0) B[n1] -= iEq;
	if (n2 >= 0) B[n2] += iEq;
}

void CircuitLab::Inductor::UpdateState(double v1, double v2)
{
	// i(t) = Geq*(v1-v2) + i(t-h): usa il valore VECCHIO di m_previousCurrent
	// (letto qui prima dell'assegnazione) per calcolare quello nuovo.
	m_previousCurrent += m_conductance * (v1 - v2);
}

void CircuitLab::Inductor::SaveSpecificData(nlohmann::json &j) const
{
	j["value"] = m_inductance;
}

void CircuitLab::Inductor::LoadSpecificData(const nlohmann::json &j)
{
	m_inductance = j["value"];
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::Inductor::GetValues() const
{
	std::map<ComponentValue, double> map;
	map[ComponentValue::inductance] = m_inductance;
	return map;
}

void CircuitLab::Inductor::SetValues(const std::map<ComponentValue, double> &values)
{
	m_inductance = values.at(ComponentValue::inductance);
}
