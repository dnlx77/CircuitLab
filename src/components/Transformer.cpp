#include <algorithm>
#include <cmath>

#include "Components/Transformer.h"
#include "Common/ComponentType.h"

CircuitLab::Transformer::Transformer(double primaryInductance, double secondaryInductance, double coupling) :
	Component(4, ComponentType::transformer)
{
	m_l1 = primaryInductance;
	m_l2 = secondaryInductance;
	m_k = coupling;
}

// Stampa il blocco 2x2 G = h * [L1 M; M L2]^-1 come due "conduttanze proprie"
// (G11 tra 0-1, G22 tra 2-3, esattamente come Inductor::StampMatrix) più le
// due conduttanze mutue G12 che accoppiano un avvolgimento all'altro. Essendo
// [L1 M; M L2] simmetrica, lo è anche il suo inverso: G12 = G21, e lo stamp
// risultante è simmetrico (A(n0,n2) == A(n2,n0), ecc.), come per qualunque
// doppio bipolo reciproco.
void CircuitLab::Transformer::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	double h)
{
	(void)voltageSourceMap;

	const double l1 = std::max(m_l1, MIN_INDUCTANCE);
	const double l2 = std::max(m_l2, MIN_INDUCTANCE);
	const double k = std::clamp(m_k, 0.0, MAX_COUPLING);
	const double m = k * std::sqrt(l1 * l2);

	// det > 0 sempre, perché k < 1 rigorosamente (M^2 = k^2*L1*L2 < L1*L2)
	const double det = l1 * l2 - m * m;

	if (h == DC_ANALYSIS_STEP)
	{
		// DC: i due avvolgimenti sono due cortocircuiti indipendenti (a regime il flusso
		// non cambia e non induce tensione: nessun accoppiamento)
		m_g11 = SHORT_CIRCUIT_CONDUCTANCE;
		m_g22 = SHORT_CIRCUIT_CONDUCTANCE;
		m_g12 = 0.0;
	}
	else
	{
		m_g11 = h * l2 / det;
		m_g22 = h * l1 / det;
		m_g12 = -h * m / det;
	}

	int n0 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n1 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;
	int n2 = (GetTerminals()[2].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[2].GetNodeId()) : -1;
	int n3 = (GetTerminals()[3].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[3].GetNodeId()) : -1;

	// Parte "propria": i1 = G11*v1 (primario), i2 = G22*v2 (secondario) — stesso
	// pattern a 4 termini di Resistor::StampMatrix, applicato due volte.
	if (n0 >= 0) A(n0, n0) += m_g11;
	if (n1 >= 0) A(n1, n1) += m_g11;
	if (n0 >= 0 && n1 >= 0) { A(n0, n1) -= m_g11; A(n1, n0) -= m_g11; }

	if (n2 >= 0) A(n2, n2) += m_g22;
	if (n3 >= 0) A(n3, n3) += m_g22;
	if (n2 >= 0 && n3 >= 0) { A(n2, n3) -= m_g22; A(n3, n2) -= m_g22; }

	// Parte mutua: il contributo di v2 a i1 (righe n0/n1) e, simmetricamente,
	// il contributo di v1 a i2 (righe n2/n3). i1 esce da n0 ed entra in n1,
	// quindi la riga n1 è l'opposto della riga n0 (idem n3 rispetto a n2) —
	// stessa convenzione di segno della parte propria qui sopra.
	if (n0 >= 0 && n2 >= 0) A(n0, n2) += m_g12;
	if (n0 >= 0 && n3 >= 0) A(n0, n3) -= m_g12;
	if (n1 >= 0 && n2 >= 0) A(n1, n2) -= m_g12;
	if (n1 >= 0 && n3 >= 0) A(n1, n3) += m_g12;

	if (n2 >= 0 && n0 >= 0) A(n2, n0) += m_g12;
	if (n2 >= 0 && n1 >= 0) A(n2, n1) -= m_g12;
	if (n3 >= 0 && n0 >= 0) A(n3, n0) -= m_g12;
	if (n3 >= 0 && n1 >= 0) A(n3, n1) += m_g12;
}

// Ieq1/Ieq2 = correnti dei due avvolgimenti allo step precedente: stesso segno
// di Inductor::StampVector (prelevata dal terminale +, iniettata nel -),
// applicato a ciascun avvolgimento per conto proprio — nessun termine incrociato:
// la parte mutua è già interamente nella matrice G (StampMatrix).
void CircuitLab::Transformer::StampVector(Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	const StampContext &ctx)
{
	(void)voltageSourceMap;

	int n0 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n1 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;
	int n2 = (GetTerminals()[2].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[2].GetNodeId()) : -1;
	int n3 = (GetTerminals()[3].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[3].GetNodeId()) : -1;

	// DC: nessuna memoria del passo precedente
	const double i1 = (ctx.h == DC_ANALYSIS_STEP) ? 0.0 : m_i1Prev;
	const double i2 = (ctx.h == DC_ANALYSIS_STEP) ? 0.0 : m_i2Prev;

	if (n0 >= 0) B[n0] -= i1;
	if (n1 >= 0) B[n1] += i1;

	if (n2 >= 0) B[n2] -= i2;
	if (n3 >= 0) B[n3] += i2;
}

std::pair<double, double> CircuitLab::Transformer::BranchCurrents(double vPrimary, double vSecondary) const
{
	const double i1 = m_g11 * vPrimary + m_g12 * vSecondary + m_i1Prev;
	const double i2 = m_g12 * vPrimary + m_g22 * vSecondary + m_i2Prev;
	return { i1, i2 };
}

void CircuitLab::Transformer::UpdateWindingState(double vPrimary, double vSecondary)
{
	// Stesso schema di Inductor::UpdateState, applicato a entrambi gli
	// avvolgimenti: usa i valori VECCHI di m_i1Prev/m_i2Prev (letti qui prima
	// dell'assegnazione) per calcolare quelli nuovi.
	m_i1Prev += m_g11 * vPrimary + m_g12 * vSecondary;
	m_i2Prev += m_g12 * vPrimary + m_g22 * vSecondary;
}

void CircuitLab::Transformer::SetStateFromDc(const std::vector<double> &terminalVoltages)
{
	// In DC ogni avvolgimento è la conduttanza di corto: corrente = G*(v+ - v-)
	m_i1Prev = SHORT_CIRCUIT_CONDUCTANCE * (terminalVoltages[0] - terminalVoltages[1]);
	m_i2Prev = SHORT_CIRCUIT_CONDUCTANCE * (terminalVoltages[2] - terminalVoltages[3]);
}

void CircuitLab::Transformer::SaveSpecificData(nlohmann::json &j) const
{
	j["primaryInductance"] = m_l1;
	j["secondaryInductance"] = m_l2;
	j["coupling"] = m_k;
	// Correnti dei due avvolgimenti allo step precedente — stesso motivo di
	// Inductor::m_previousCurrent: senza salvarle, un salva/ricarica o un
	// annulla/ripeti (che riusa lo stesso formato, vedi Application::PushUndoSnapshot)
	// azzererebbe di colpo il trasformatore.
	j["i1Prev"] = m_i1Prev;
	j["i2Prev"] = m_i2Prev;
}

void CircuitLab::Transformer::LoadSpecificData(const nlohmann::json &j)
{
	m_l1 = j["primaryInductance"];
	m_l2 = j["secondaryInductance"];
	m_k = j["coupling"];
	// Assenti nei file salvati prima di questi campi: si assume a riposo (0A).
	m_i1Prev = j.value("i1Prev", 0.0);
	m_i2Prev = j.value("i2Prev", 0.0);
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::Transformer::GetValues() const
{
	std::map<ComponentValue, double> map;
	map[ComponentValue::primaryInductance] = m_l1;
	map[ComponentValue::secondaryInductance] = m_l2;
	map[ComponentValue::couplingCoefficient] = m_k;
	return map;
}

// L1/L2 > 0 e k in [0, MAX_COUPLING]: vedi i commenti sulle costanti in
// Transformer.h (k=1 esatto renderebbe singolare la matrice [L1 M; M L2]).
void CircuitLab::Transformer::SetValues(const std::map<ComponentValue, double> &values)
{
	m_l1 = std::max(values.at(ComponentValue::primaryInductance), MIN_INDUCTANCE);
	m_l2 = std::max(values.at(ComponentValue::secondaryInductance), MIN_INDUCTANCE);
	m_k = std::clamp(values.at(ComponentValue::couplingCoefficient), 0.0, MAX_COUPLING);
}
