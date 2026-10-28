#include <algorithm>
#include <cmath>

#include "Components/Diode.h"
#include "Components/Junction.h"
#include "Common/ComponentType.h"

CircuitLab::Diode::Diode(double saturationCurrent, double emissionCoefficient) :
	Component(2, ComponentType::diode),
	m_saturationCurrent(saturationCurrent),
	m_emissionCoefficient(emissionCoefficient),
	m_lastVd(0.0),
	m_lastG(0.0),
	m_lastIeq(0.0)
{}

// Linearizzazione di Newton: attorno a Vd,
//   I(Vd')  ≈  g*Vd' + Ieq,    g = dI/dV,   Ieq = I(Vd) - g*Vd
// La conduttanza g va in A con lo stesso pattern di Resistor::StampMatrix.
// Ieq è una corrente anodo->catodo: la si PRELEVA dall'anodo e la si INIETTA
// nel catodo (stesso segno dell'induttore, vedi Inductor::StampVector).
// GMIN è sommata a g (e la sua corrente GMIN*Vd si cancella in Ieq, perché
// è già lineare), quindi Ieq contiene solo la parte esponenziale.
bool CircuitLab::Diode::StampNonlinear(Eigen::MatrixXd &A,
	Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const Eigen::VectorXd &x)
{
	const int n1 = MatrixIndex(0, nodeMap);
	const int n2 = MatrixIndex(1, nodeMap);

	const double vAnode = VoltageAt(x, n1);
	const double vCathode = VoltageAt(x, n2);

	const double nVt = m_emissionCoefficient * THERMAL_VOLTAGE;
	const double vCrit = CriticalVoltage(nVt, m_saturationCurrent);

	const double vRaw = vAnode - vCathode;
	const double vd = LimitVoltage(vRaw, m_lastVd, nVt, vCrit);
	const bool limited = (vd != vRaw);
	m_lastVd = vd;

	const double e = LimitedExp(vd / nVt);
	const double iDiode = m_saturationCurrent * (e - 1.0);
	const double gDiode = m_saturationCurrent / nVt * e;
	const double iEq = iDiode - gDiode * vd;
	const double g = gDiode + JUNCTION_GMIN;

	m_lastG = gDiode;
	m_lastIeq = iEq;

	StampConductance(A, n1, n2, g);
	StampCurrent(B, n1, n2, -iEq);

	return limited;
}

// Test di convergenza di SPICE: la soluzione x è stata ottenuta col diodo
// sostituito da I ≈ g*Vd + Ieq. Se in quel punto la corrente vera coincide con
// quella predetta, x soddisfa anche le equazioni non lineari, e Newton ha finito.
// Si guarda la corrente e non la variazione delle tensioni tra due iterazioni
// perché quest'ultima, per i nodi quasi isolati (tutti i diodi spenti, con un
// condensatore grande in parallelo al carico), è dominata dal rumore di
// arrotondamento della soluzione lineare e non arriva mai sotto tolleranza.
bool CircuitLab::Diode::HasConverged(const std::map<int, int> &nodeMap, const Eigen::VectorXd &x) const
{
	const double vd = VoltageAt(x, MatrixIndex(0, nodeMap)) - VoltageAt(x, MatrixIndex(1, nodeMap));
	const double nVt = m_emissionCoefficient * THERMAL_VOLTAGE;

	const double iActual = m_saturationCurrent * (LimitedExp(vd / nVt) - 1.0);
	const double iPredicted = m_lastG * vd + m_lastIeq;

	return CurrentsAgree(iActual, iPredicted);
}

double CircuitLab::Diode::Current(double vd) const
{
	// Solo la corrente di giunzione: GMIN è un artificio numerico, non una
	// corrente fisica. Includerla farebbe risultare attraversato da qualche nA
	// un diodo in inversa (a 5 V: 5 nA, sopra la soglia dei pallini di UI).
	const double nVt = m_emissionCoefficient * THERMAL_VOLTAGE;
	return m_saturationCurrent * (LimitedExp(vd / nVt) - 1.0);
}

void CircuitLab::Diode::SaveSpecificData(nlohmann::json &j) const
{
	j["saturationCurrent"] = m_saturationCurrent;
	j["emissionCoefficient"] = m_emissionCoefficient;
	// Tensione dell'ultima linearizzazione: solo il riferimento del limitatore
	// pnjlim (vedi LimitVoltage), non indispensabile alla correttezza (Newton
	// riparte comunque da x=0 dopo un salva/ricarica o un annulla/ripeti), ma
	// senza salvarla si perde il punto di partenza "caldo" della prossima
	// iterazione — stessa idea di Capacitor::m_previousVoltage.
	j["lastVd"] = m_lastVd;
}

void CircuitLab::Diode::LoadSpecificData(const nlohmann::json &j)
{
	m_saturationCurrent = j["saturationCurrent"];
	m_emissionCoefficient = j["emissionCoefficient"];
	m_lastVd = j.value("lastVd", 0.0);
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::Diode::GetValues() const
{
	std::map<ComponentValue, double> map;
	map[ComponentValue::saturationCurrent] = m_saturationCurrent;
	map[ComponentValue::emissionCoefficient] = m_emissionCoefficient;
	return map;
}

// Is e n compaiono a denominatore/in un logaritmo: valori <= 0 darebbero
// NaN/inf, quindi si impone un minimo (come Resistor::SetResistance fa per R).
void CircuitLab::Diode::SetValues(const std::map<ComponentValue, double> &values)
{
	m_saturationCurrent = std::max(values.at(ComponentValue::saturationCurrent), 1e-30);
	m_emissionCoefficient = std::max(values.at(ComponentValue::emissionCoefficient), 0.1);
}

// Corrente anodo->catodo dall'equazione di Shockley con la Vd convergente (stesso
// verso positivo del resistore: terminale 0 -> 1).
bool CircuitLab::Diode::ReportCurrents(const std::vector<double> &v, double, CurrentReport &report) const
{
	report.main = Current(v[0] - v[1]);
	report.AddBranch(0, 1, report.main);
	return true;
}
