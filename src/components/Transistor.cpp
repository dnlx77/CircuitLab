#include <algorithm>
#include <cmath>

#include "Components/Transistor.h"
#include "Common/ComponentType.h"

CircuitLab::Transistor::Transistor(double saturationCurrent, double forwardBeta) :
	Component(3, ComponentType::transistor),
	m_saturationCurrent(saturationCurrent),
	m_forwardBeta(forwardBeta),
	m_lastVbe(0.0), m_lastVbc(0.0),
	m_lastGpi(0.0), m_lastGmu(0.0), m_lastGm(0.0), m_lastGo(0.0),
	m_lastIBeq(0.0), m_lastICeq(0.0)
{}

// Il transistor non ha parte lineare: tutto è nella parte non lineare
// (StampNonlinear), ristampata ad ogni iterazione di Newton.
void CircuitLab::Transistor::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	double h)
{
	(void)A;
	(void)nodeMap;
	(void)voltageSourceMap;
	(void)h;
}

void CircuitLab::Transistor::StampVector(Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	const StampContext &ctx)
{
	(void)B;
	(void)nodeMap;
	(void)voltageSourceMap;
	(void)ctx;
}

// pnjlim (SPICE), identico a Diode::LimitVoltage — vedi lì per la spiegazione.
double CircuitLab::Transistor::LimitVoltage(double vNew, double vOld, double vt, double vCrit)
{
	if (vNew > vCrit && std::fabs(vNew - vOld) > 2.0 * vt)
	{
		if (vOld > 0.0)
		{
			double arg = 1.0 + (vNew - vOld) / vt;
			vNew = (arg > 0.0) ? vOld + vt * std::log(arg) : vCrit;
		}
		else
			vNew = vt * std::log(vNew / vt);
	}
	return vNew;
}

// Linearizzazione di Newton attorno a (VBE, VBC): modello companion standard
// di un BJT (senza effetto Early) — vedi la spiegazione in Transistor.h.
// gpi (base-emettitore) e gmu (base-collettore) si stampano come due
// "resistori"; gm e go come due generatori di corrente pilotati in tensione
// da collettore a emettitore, pilotati rispettivamente da VBE e VBC. Il
// pattern di stampa di un generatore pilotato I=g*(Vcp-Vcn) da nodo p a nodo n
// è: A(p,cp)+=g; A(p,cn)-=g; A(n,cp)-=g; A(n,cn)+=g — la stessa idea del
// resistore (che è il caso degenere p=cp, n=cn), generalizzata a controllo e
// uscita su coppie di nodi diverse. Verificato che la somma di ogni colonna
// dei quattro contributi (gpi, gmu, gm, go) sia zero: la corrente totale nel
// componente non dipende da un riferimento assoluto di tensione, come deve
// essere per un componente a 3 terminali senza nodi interni.
bool CircuitLab::Transistor::StampNonlinear(Eigen::MatrixXd &A,
	Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const Eigen::VectorXd &x)
{
	int n0 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1; // base
	int n1 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1; // collettore
	int n2 = (GetTerminals()[2].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[2].GetNodeId()) : -1; // emettitore

	double vBase = (n0 >= 0) ? x[n0] : 0.0;
	double vColl = (n1 >= 0) ? x[n1] : 0.0;
	double vEmit = (n2 >= 0) ? x[n2] : 0.0;

	const double vt = THERMAL_VOLTAGE;
	const double vCrit = vt * std::log(vt / (std::sqrt(2.0) * m_saturationCurrent));

	const double vbeRaw = vBase - vEmit;
	const double vbcRaw = vBase - vColl;
	const double vbe = LimitVoltage(vbeRaw, m_lastVbe, vt, vCrit);
	const double vbc = LimitVoltage(vbcRaw, m_lastVbc, vt, vCrit);
	const bool limited = (vbe != vbeRaw) || (vbc != vbcRaw);
	m_lastVbe = vbe;
	m_lastVbc = vbc;

	const double eF = std::exp(std::min(vbe / vt, MAX_EXPONENT));
	const double eR = std::exp(std::min(vbc / vt, MAX_EXPONENT));
	const double iF = m_saturationCurrent * (eF - 1.0);
	const double iR = m_saturationCurrent * (eR - 1.0);
	const double gF = m_saturationCurrent / vt * eF; // dIF/dVBE
	const double gR = m_saturationCurrent / vt * eR; // dIR/dVBC

	const double gpi = gF / m_forwardBeta;               // dIB/dVBE
	const double gmu = gR / REVERSE_BETA;                // dIB/dVBC
	const double gm = gF;                                // dIC/dVBE
	const double go = -gR * (1.0 + 1.0 / REVERSE_BETA);  // dIC/dVBC

	const double iB0 = iF / m_forwardBeta + iR / REVERSE_BETA;
	const double iC0 = (iF - iR) - iR / REVERSE_BETA;
	const double iBeq = iB0 - gpi * vbe - gmu * vbc;
	const double iCeq = iC0 - gm * vbe - go * vbc;
	const double iEeq = -(iBeq + iCeq); // le tre correnti equivalenti sommano a zero (KCL)

	m_lastGpi = gpi; m_lastGmu = gmu; m_lastGm = gm; m_lastGo = go;
	m_lastIBeq = iBeq; m_lastICeq = iCeq;

	// GMIN solo sui due rami "resistivi" (base-emettitore, base-collettore):
	// evita che base/collettore/emettitore restino isolati a transistor spento,
	// esattamente come per il Diodo. Non contribuisce a iBeq/iCeq (è già lineare).
	const double gpiFull = gpi + GMIN;
	const double gmuFull = gmu + GMIN;

	// gpi: "resistore" base(n0)-emettitore(n2)
	if (n0 >= 0) A(n0, n0) += gpiFull;
	if (n2 >= 0) A(n2, n2) += gpiFull;
	if (n0 >= 0 && n2 >= 0) { A(n0, n2) -= gpiFull; A(n2, n0) -= gpiFull; }

	// gmu: "resistore" base(n0)-collettore(n1)
	if (n0 >= 0) A(n0, n0) += gmuFull;
	if (n1 >= 0) A(n1, n1) += gmuFull;
	if (n0 >= 0 && n1 >= 0) { A(n0, n1) -= gmuFull; A(n1, n0) -= gmuFull; }

	// gm: generatore pilotato collettore(n1)->emettitore(n2), pilotato da VBE (n0,n2)
	if (n1 >= 0 && n0 >= 0) A(n1, n0) += gm;
	if (n1 >= 0 && n2 >= 0) A(n1, n2) -= gm;
	if (n2 >= 0 && n0 >= 0) A(n2, n0) -= gm;
	if (n2 >= 0)            A(n2, n2) += gm;

	// go: generatore pilotato collettore(n1)->emettitore(n2), pilotato da VBC (n0,n1)
	if (n1 >= 0 && n0 >= 0) A(n1, n0) += go;
	if (n1 >= 0)            A(n1, n1) -= go;
	if (n2 >= 0 && n0 >= 0) A(n2, n0) -= go;
	if (n2 >= 0 && n1 >= 0) A(n2, n1) += go;

	if (n0 >= 0) B[n0] -= iBeq;
	if (n1 >= 0) B[n1] -= iCeq;
	if (n2 >= 0) B[n2] -= iEeq;

	return limited;
}

// Test di convergenza di SPICE (come Diode::HasConverged, per due correnti
// indipendenti invece di una): confronta IB e IC predette dall'ultima
// linearizzazione con quelle vere di Ebers-Moll alla nuova soluzione x.
bool CircuitLab::Transistor::HasConverged(const std::map<int, int> &nodeMap, const Eigen::VectorXd &x) const
{
	int n0 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n1 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;
	int n2 = (GetTerminals()[2].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[2].GetNodeId()) : -1;

	const double vbe = ((n0 >= 0) ? x[n0] : 0.0) - ((n2 >= 0) ? x[n2] : 0.0);
	const double vbc = ((n0 >= 0) ? x[n0] : 0.0) - ((n1 >= 0) ? x[n1] : 0.0);

	const auto currents = Currents(vbe, vbc);
	const double iBpredicted = m_lastGpi * vbe + m_lastGmu * vbc + m_lastIBeq;
	const double iCpredicted = m_lastGm * vbe + m_lastGo * vbc + m_lastICeq;

	constexpr double CURRENT_ABS_TOL = 1e-9;
	constexpr double CURRENT_REL_TOL = 1e-3;

	bool baseOk = std::abs(currents.ib - iBpredicted) <= CURRENT_ABS_TOL + CURRENT_REL_TOL * std::max(std::abs(currents.ib), std::abs(iBpredicted));
	bool collOk = std::abs(currents.ic - iCpredicted) <= CURRENT_ABS_TOL + CURRENT_REL_TOL * std::max(std::abs(currents.ic), std::abs(iCpredicted));
	return baseOk && collOk;
}

CircuitLab::Transistor::TerminalCurrents CircuitLab::Transistor::Currents(double vbe, double vbc) const
{
	// Solo le correnti di giunzione: GMIN è un artificio numerico (vedi Diode::Current).
	const double iF = m_saturationCurrent * (std::exp(std::min(vbe / THERMAL_VOLTAGE, MAX_EXPONENT)) - 1.0);
	const double iR = m_saturationCurrent * (std::exp(std::min(vbc / THERMAL_VOLTAGE, MAX_EXPONENT)) - 1.0);

	const double ib = iF / m_forwardBeta + iR / REVERSE_BETA;
	const double ic = (iF - iR) - iR / REVERSE_BETA;
	const double ie = -(ib + ic);
	return { ib, ic, ie };
}

void CircuitLab::Transistor::SaveSpecificData(nlohmann::json &j) const
{
	j["saturationCurrent"] = m_saturationCurrent;
	j["forwardBeta"] = m_forwardBeta;
	// Tensioni dell'ultima linearizzazione: solo il riferimento del limitatore
	// pnjlim (vedi LimitVoltage), come Diode::m_lastVd — non indispensabili
	// alla correttezza, ma senza salvarle si perde il punto di partenza
	// "caldo" della prossima iterazione dopo un salva/ricarica o un
	// annulla/ripeti (che riusa lo stesso formato, vedi Application::PushUndoSnapshot).
	j["lastVbe"] = m_lastVbe;
	j["lastVbc"] = m_lastVbc;
}

void CircuitLab::Transistor::LoadSpecificData(const nlohmann::json &j)
{
	m_saturationCurrent = j["saturationCurrent"];
	m_forwardBeta = j["forwardBeta"];
	m_lastVbe = j.value("lastVbe", 0.0);
	m_lastVbc = j.value("lastVbc", 0.0);
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::Transistor::GetValues() const
{
	std::map<ComponentValue, double> map;
	map[ComponentValue::saturationCurrent] = m_saturationCurrent;
	map[ComponentValue::forwardCurrentGain] = m_forwardBeta;
	return map;
}

// Is e BF compaiono a denominatore/in un logaritmo: valori <= 0 darebbero
// NaN/inf, quindi si impone un minimo (come Diode::SetValues).
void CircuitLab::Transistor::SetValues(const std::map<ComponentValue, double> &values)
{
	m_saturationCurrent = std::max(values.at(ComponentValue::saturationCurrent), 1e-30);
	m_forwardBeta = std::max(values.at(ComponentValue::forwardCurrentGain), 1.0);
}
