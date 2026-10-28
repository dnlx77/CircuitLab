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
	const std::map<int, int> &,
	double h)
{
	// DC: induttore in corto (a regime la tensione ai suoi capi è nulla)
	m_conductance = (h == DC_ANALYSIS_STEP || m_inductance <= 1e-9) ? SHORT_CIRCUIT_CONDUCTANCE : (h / m_inductance);

	StampConductance(A, MatrixIndex(0, nodeMap), MatrixIndex(1, nodeMap), m_conductance);
}

// Ieq = i(t-h): a differenza del condensatore (dove Ieq = Geq*v_prev viene
// iniettata in n1 e prelevata da n2), qui la corrente del ramo è
// i(t) = Geq*(v1-v2) + i(t-h): il generatore equivalente va quindi PRELEVATO
// da n1 e INIETTATO in n2 (segno opposto rispetto al condensatore) perché
// contribuisce con segno positivo, non negativo, alla corrente n1->n2.
void CircuitLab::Inductor::StampVector(Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &,
	const StampContext &ctx)
{
	// DC: nessuna memoria del passo precedente
	const double iEq = (ctx.h == DC_ANALYSIS_STEP) ? 0.0 : m_previousCurrent;

	StampCurrent(B, MatrixIndex(0, nodeMap), MatrixIndex(1, nodeMap), -iEq);
}

void CircuitLab::Inductor::UpdateState(const std::vector<double> &terminalVoltages)
{
	// i(t) = Geq*(v1-v2) + i(t-h): usa il valore VECCHIO di m_previousCurrent
	// (letto qui prima dell'assegnazione) per calcolare quello nuovo.
	m_previousCurrent += m_conductance * (terminalVoltages[0] - terminalVoltages[1]);
}

void CircuitLab::Inductor::SetStateFromDc(const std::vector<double> &terminalVoltages)
{
	// In DC l'induttore è la conduttanza di corto: la sua corrente è G*(v1-v2)
	m_previousCurrent = SHORT_CIRCUIT_CONDUCTANCE * (terminalVoltages[0] - terminalVoltages[1]);
}

void CircuitLab::Inductor::SaveSpecificData(nlohmann::json &j) const
{
	j["value"] = m_inductance;
	// Corrente memorizzata (i(t-h)) — stesso motivo di Capacitor::m_previousVoltage:
	// senza salvarla, ogni induttore si azzererebbe di colpo a un
	// salva/ricarica o un annulla/ripeti.
	j["previousCurrent"] = m_previousCurrent;
}

void CircuitLab::Inductor::LoadSpecificData(const nlohmann::json &j)
{
	m_inductance = j["value"];
	// Assente nei file salvati prima di questo campo: si assume a riposo (0A).
	m_previousCurrent = j.value("previousCurrent", 0.0);
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

// i(t) = Geq*(v1-v2) + i(t-h), con i(t-h) letta PRIMA di aggiornare lo stato
// dell'induttore per il prossimo step.
bool CircuitLab::Inductor::ReportCurrents(const std::vector<double> &v, double, CurrentReport &report) const
{
	report.main = m_conductance * (v[0] - v[1]) + m_previousCurrent;
	report.AddBranch(0, 1, report.main);
	return true;
}
