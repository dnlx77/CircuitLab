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
	const std::map<int, int> &,
	double h)
{
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

	const int n0 = MatrixIndex(0, nodeMap);
	const int n1 = MatrixIndex(1, nodeMap);
	const int n2 = MatrixIndex(2, nodeMap);
	const int n3 = MatrixIndex(3, nodeMap);

	// Parte "propria": i1 = G11*v1 (primario), i2 = G22*v2 (secondario) — stesso
	// pattern di Resistor::StampMatrix, applicato due volte.
	StampConductance(A, n0, n1, m_g11);
	StampConductance(A, n2, n3, m_g22);

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
	const std::map<int, int> &,
	const StampContext &ctx)
{
	// DC: nessuna memoria del passo precedente
	const double i1 = (ctx.h == DC_ANALYSIS_STEP) ? 0.0 : m_i1Prev;
	const double i2 = (ctx.h == DC_ANALYSIS_STEP) ? 0.0 : m_i2Prev;

	StampCurrent(B, MatrixIndex(0, nodeMap), MatrixIndex(1, nodeMap), -i1);
	StampCurrent(B, MatrixIndex(2, nodeMap), MatrixIndex(3, nodeMap), -i2);
}

std::pair<double, double> CircuitLab::Transformer::BranchCurrents(double vPrimary, double vSecondary) const
{
	const double i1 = m_g11 * vPrimary + m_g12 * vSecondary + m_i1Prev;
	const double i2 = m_g12 * vPrimary + m_g22 * vSecondary + m_i2Prev;
	return { i1, i2 };
}

void CircuitLab::Transformer::UpdateState(const std::vector<double> &terminalVoltages)
{
	const double vPrimary = terminalVoltages[0] - terminalVoltages[1];
	const double vSecondary = terminalVoltages[2] - terminalVoltages[3];

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

// 4 terminali: 0/1 = primario, 2/3 = secondario, ciascuna coppia con la propria corrente
// indipendente (vedi Transformer.h). main tiene solo la corrente di primario (il probe
// "corrente di componente" dell'oscilloscopio ha un solo valore per id); i rami ne hanno
// uno per avvolgimento.
bool CircuitLab::Transformer::ReportCurrents(const std::vector<double> &v, double, CurrentReport &report) const
{
	const auto [primaryCurrent, secondaryCurrent] = BranchCurrents(v[0] - v[1], v[2] - v[3]);
	report.main = primaryCurrent;
	report.AddBranch(0, 1, primaryCurrent);
	report.AddBranch(2, 3, secondaryCurrent);
	return true;
}
