#include <algorithm>
#include <cmath>

#include "Components/Transistor.h"
#include "Components/Junction.h"
#include "Common/ComponentType.h"

CircuitLab::Transistor::Transistor(double saturationCurrent, double forwardBeta, bool pnp) :
	Component(3, pnp ? ComponentType::transistorPnp : ComponentType::transistor),
	m_saturationCurrent(saturationCurrent),
	m_forwardBeta(forwardBeta),
	m_polarity(pnp ? -1.0 : 1.0),
	m_lastVbe(0.0), m_lastVbc(0.0),
	m_lastGpi(0.0), m_lastGmu(0.0), m_lastGm(0.0), m_lastGo(0.0),
	m_lastIBeq(0.0), m_lastICeq(0.0)
{}

// Linearizzazione di Newton attorno a (VBE, VBC): modello companion standard
// di un BJT (senza effetto Early) — vedi la spiegazione in Transistor.h.
// gpi (base-emettitore) e gmu (base-collettore) si stampano come due
// "resistori"; gm e (go + gmu) come due generatori di corrente pilotati in
// tensione da collettore a emettitore, pilotati rispettivamente da VBE e VBC
// (vedi sotto perché per VBC è go + gmu e non go). Il
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
	const int n0 = MatrixIndex(0, nodeMap); // base
	const int n1 = MatrixIndex(1, nodeMap); // collettore
	const int n2 = MatrixIndex(2, nodeMap); // emettitore

	const double vBase = VoltageAt(x, n0);
	const double vColl = VoltageAt(x, n1);
	const double vEmit = VoltageAt(x, n2);

	const double vt = THERMAL_VOLTAGE;
	const double vCrit = CriticalVoltage(vt, m_saturationCurrent);

	// Tensioni nel riferimento "NPN equivalente" (specchiate per un PNP)
	const double vbeRaw = m_polarity * (vBase - vEmit);
	const double vbcRaw = m_polarity * (vBase - vColl);
	const double vbe = LimitVoltage(vbeRaw, m_lastVbe, vt, vCrit);
	const double vbc = LimitVoltage(vbcRaw, m_lastVbc, vt, vCrit);
	const bool limited = (vbe != vbeRaw) || (vbc != vbcRaw);
	m_lastVbe = vbe;
	m_lastVbc = vbc;

	const double eF = LimitedExp(vbe / vt);
	const double eR = LimitedExp(vbc / vt);
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
	// Correnti equivalenti REALI ai terminali: per un PNP sono l'opposto di quelle
	// dell'NPN specchiato (le conduttanze invece sono uguali, perché
	// derivando rispetto alle tensioni vere i due segni si compensano).
	const double iBeq = m_polarity * (iB0 - gpi * vbe - gmu * vbc);
	const double iCeq = m_polarity * (iC0 - gm * vbe - go * vbc);
	const double iEeq = -(iBeq + iCeq); // le tre correnti equivalenti sommano a zero (KCL)

	m_lastGpi = gpi; m_lastGmu = gmu; m_lastGm = gm; m_lastGo = go;
	m_lastIBeq = iBeq; m_lastICeq = iCeq;

	// GMIN solo sui due rami "resistivi" (base-emettitore, base-collettore):
	// evita che base/collettore/emettitore restino isolati a transistor spento,
	// esattamente come per il Diodo. Non contribuisce a iBeq/iCeq (è già lineare).
	const double gpiFull = gpi + JUNCTION_GMIN;
	const double gmuFull = gmu + JUNCTION_GMIN;

	// gpi: "resistore" base(n0)-emettitore(n2)
	StampConductance(A, n0, n2, gpiFull);

	// gmu: "resistore" base(n0)-collettore(n1)
	StampConductance(A, n0, n1, gmuFull);

	// gm: generatore pilotato collettore(n1)->emettitore(n2), pilotato da VBE (n0,n2)
	if (n1 >= 0 && n0 >= 0) A(n1, n0) += gm;
	if (n1 >= 0 && n2 >= 0) A(n1, n2) -= gm;
	if (n2 >= 0 && n0 >= 0) A(n2, n0) -= gm;
	if (n2 >= 0)            A(n2, n2) += gm;

	// go: generatore pilotato collettore(n1)->emettitore(n2), pilotato da VBC (n0,n1).
	// Attenzione: il coefficiente da stampare NON è la derivata totale dIC/dVBC (go): quella
	// comprende già il contributo della corrente base-collettore (iR/BR), che arriva al
	// collettore dalla resistenza gmu stampata sopra. Il generatore deve portare solo la
	// parte "di trasporto" (iF - iR), la cui dipendenza da VBC è -gR = go + gmu. Stampando
	// go, il termine gmu veniva contato due volte: in regione attiva non si nota (gR è
	// trascurabile), ma in saturazione (giunzione base-collettore in diretta, con BR = 1)
	// il punto fisso di Newton non risolveva più le equazioni del modello e la convergenza
	// non veniva mai raggiunta (verificato su un multivibratore astabile).
	const double goTransport = go + gmu;
	if (n1 >= 0 && n0 >= 0) A(n1, n0) += goTransport;
	if (n1 >= 0)            A(n1, n1) -= goTransport;
	if (n2 >= 0 && n0 >= 0) A(n2, n0) -= goTransport;
	if (n2 >= 0 && n1 >= 0) A(n2, n1) += goTransport;

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
	const double vBase = VoltageAt(x, MatrixIndex(0, nodeMap));
	const double vbe = vBase - VoltageAt(x, MatrixIndex(2, nodeMap));
	const double vbc = vBase - VoltageAt(x, MatrixIndex(1, nodeMap));

	const auto currents = Currents(vbe, vbc);
	const double iBpredicted = m_lastGpi * vbe + m_lastGmu * vbc + m_lastIBeq;
	const double iCpredicted = m_lastGm * vbe + m_lastGo * vbc + m_lastICeq;

	return CurrentsAgree(currents.ib, iBpredicted) && CurrentsAgree(currents.ic, iCpredicted);
}

CircuitLab::Transistor::TerminalCurrents CircuitLab::Transistor::Currents(double vbe, double vbc) const
{
	// Solo le correnti di giunzione: GMIN è un artificio numerico (vedi Diode::Current).
	// Si calcola nel riferimento NPN equivalente e si riporta il segno ai
	// terminali veri.
	const double iF = m_saturationCurrent * (LimitedExp(m_polarity * vbe / THERMAL_VOLTAGE) - 1.0);
	const double iR = m_saturationCurrent * (LimitedExp(m_polarity * vbc / THERMAL_VOLTAGE) - 1.0);

	const double ib = m_polarity * (iF / m_forwardBeta + iR / REVERSE_BETA);
	const double ic = m_polarity * ((iF - iR) - iR / REVERSE_BETA);
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
