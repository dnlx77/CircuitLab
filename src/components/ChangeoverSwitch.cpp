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
// m_conductanceVia2.
void CircuitLab::ChangeoverSwitch::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	double h)
{
	(void)voltageSourceMap;
	(void)h;

	int n0 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n1 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;
	int n2 = (GetTerminals()[2].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[2].GetNodeId()) : -1;

	if (n0 >= 0) A(n0, n0) += m_conductanceVia1;
	if (n1 >= 0) A(n1, n1) += m_conductanceVia1;
	if (n0 >= 0 && n1 >= 0) {
		A(n0, n1) -= m_conductanceVia1;
		A(n1, n0) -= m_conductanceVia1;
	}

	if (n0 >= 0) A(n0, n0) += m_conductanceVia2;
	if (n2 >= 0) A(n2, n2) += m_conductanceVia2;
	if (n0 >= 0 && n2 >= 0) {
		A(n0, n2) -= m_conductanceVia2;
		A(n2, n0) -= m_conductanceVia2;
	}
}

// Puramente resistivo (nel senso lato: due conduttanze fisse): nessun
// contributo dinamico, come Switch::StampVector.
void CircuitLab::ChangeoverSwitch::StampVector(Eigen::VectorXd &B, const std::map<int, int> &nodeMap, const std::map<int, int> &voltageSourceMap, const StampContext &ctx)
{
	(void)B;
	(void)nodeMap;
	(void)voltageSourceMap;
	(void)ctx;
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

// Nessun valore continuo da esporre nel pannello proprietà: lo stato si
// controlla solo con click/tasto (vedi UI), esattamente come Switch.
std::map<CircuitLab::ComponentValue, double> CircuitLab::ChangeoverSwitch::GetValues() const
{
	std::map<ComponentValue, double> map;
	return map;
}

void CircuitLab::ChangeoverSwitch::SetValues(const std::map<ComponentValue, double> &values)
{
	(void)values;
}
