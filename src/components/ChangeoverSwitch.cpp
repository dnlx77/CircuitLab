#include "Components/ChangeoverSwitch.h"
#include "Common/ComponentType.h"

CircuitLab::ChangeoverSwitch::ChangeoverSwitch(bool viaSecond) : Component(3, ComponentType::changeoverSwitch)
{
	SetViaSecond(viaSecond);
}

// Stesso approccio di Switch::SetClosed, applicato ai due rami comune-via:
// quello attivo ha conduttanza altissima (cortocircuito), l'altro quasi nulla
// (circuito aperto, ma non zero per non lasciare righe nulle in matrice).
void CircuitLab::ChangeoverSwitch::SetViaSecond(bool viaSecond)
{
	m_viaSecond = viaSecond;
	m_conductanceVia1 = viaSecond ? 1e-12 : SHORT_CIRCUIT_CONDUCTANCE;
	m_conductanceVia2 = viaSecond ? SHORT_CIRCUIT_CONDUCTANCE : 1e-12;
}

// Due stampe indipendenti in stile Resistor/Switch, entrambe centrate sul nodo
// comune (terminale 0): comune-via1 con m_conductanceVia1, comune-via2 con
// m_conductanceVia2. Puramente resistivo: nessun contributo dinamico.
void CircuitLab::ChangeoverSwitch::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &,
	double)
{
	const int n0 = MatrixIndex(0, nodeMap);
	StampConductance(A, n0, MatrixIndex(1, nodeMap), m_conductanceVia1);
	StampConductance(A, n0, MatrixIndex(2, nodeMap), m_conductanceVia2);
}

void CircuitLab::ChangeoverSwitch::ToggleSwitch()
{
	SetViaSecond(!m_viaSecond);
}

void CircuitLab::ChangeoverSwitch::SaveSpecificData(nlohmann::json &j) const
{
	j["viaSecond"] = m_viaSecond;
}

void CircuitLab::ChangeoverSwitch::LoadSpecificData(const nlohmann::json &j)
{
	SetViaSecond(j["viaSecond"]);
}
