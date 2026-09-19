#include <algorithm>
#include <cmath>

#include "Components/Diode.h"
#include "Common/ComponentType.h"

CircuitLab::Diode::Diode(double saturationCurrent, double emissionCoefficient) :
	Component(2, ComponentType::diode),
	m_saturationCurrent(saturationCurrent),
	m_emissionCoefficient(emissionCoefficient),
	m_lastVd(0.0),
	m_lastG(0.0),
	m_lastIeq(0.0)
{}

// Il diodo non ha parte lineare: la sua conduttanza dipende dalla soluzione,
// quindi viene stampata ad ogni iterazione di Newton (StampNonlinear).
void CircuitLab::Diode::StampMatrix(Eigen::MatrixXd &A,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	double h)
{
	(void)A;
	(void)nodeMap;
	(void)voltageSourceMap;
	(void)h;
}

void CircuitLab::Diode::StampVector(Eigen::VectorXd &B,
	const std::map<int, int> &nodeMap,
	const std::map<int, int> &voltageSourceMap,
	const StampContext &ctx)
{
	(void)B;
	(void)nodeMap;
	(void)voltageSourceMap;
	(void)ctx;
}

// pnjlim (SPICE). Se la nuova tensione supera Vcrit (il punto in cui la curva
// esponenziale diventa più ripida di quanto Newton possa seguire) E il salto
// rispetto all'iterazione precedente è più grande di 2*n*Vt, lo comprime:
// in modo logaritmico se la vecchia tensione era già in conduzione, altrimenti
// riportando la tensione a poche n*Vt oltre la soglia.
double CircuitLab::Diode::LimitVoltage(double vNew, double vOld, double nVt, double vCrit)
{
	if (vNew > vCrit && std::fabs(vNew - vOld) > 2.0 * nVt)
	{
		if (vOld > 0.0)
		{
			double arg = 1.0 + (vNew - vOld) / nVt;
			vNew = (arg > 0.0) ? vOld + nVt * std::log(arg) : vCrit;
		}
		else
			vNew = nVt * std::log(vNew / nVt);
	}
	return vNew;
}

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
	int n1 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	double vAnode = (n1 >= 0) ? x[n1] : 0.0;
	double vCathode = (n2 >= 0) ? x[n2] : 0.0;

	const double nVt = m_emissionCoefficient * THERMAL_VOLTAGE;
	const double vCrit = nVt * std::log(nVt / (std::sqrt(2.0) * m_saturationCurrent));

	const double vRaw = vAnode - vCathode;
	const double vd = LimitVoltage(vRaw, m_lastVd, nVt, vCrit);
	const bool limited = (vd != vRaw);
	m_lastVd = vd;

	const double e = std::exp(std::min(vd / nVt, MAX_EXPONENT));
	const double iDiode = m_saturationCurrent * (e - 1.0);
	const double gDiode = m_saturationCurrent / nVt * e;
	const double iEq = iDiode - gDiode * vd;
	const double g = gDiode + GMIN;

	m_lastG = gDiode;
	m_lastIeq = iEq;

	if (n1 >= 0) A(n1, n1) += g;
	if (n2 >= 0) A(n2, n2) += g;

	if (n1 >= 0 && n2 >= 0) {
		A(n1, n2) -= g;
		A(n2, n1) -= g;
	}

	if (n1 >= 0) B[n1] -= iEq;
	if (n2 >= 0) B[n2] += iEq;

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
	int n1 = (GetTerminals()[0].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[0].GetNodeId()) : -1;
	int n2 = (GetTerminals()[1].GetNodeId() > 0) ? nodeMap.at(GetTerminals()[1].GetNodeId()) : -1;

	const double vd = ((n1 >= 0) ? x[n1] : 0.0) - ((n2 >= 0) ? x[n2] : 0.0);
	const double nVt = m_emissionCoefficient * THERMAL_VOLTAGE;

	const double iActual = m_saturationCurrent * (std::exp(std::min(vd / nVt, MAX_EXPONENT)) - 1.0);
	const double iPredicted = m_lastG * vd + m_lastIeq;

	return std::abs(iActual - iPredicted) <= CURRENT_ABS_TOL + CURRENT_REL_TOL * std::max(std::abs(iActual), std::abs(iPredicted));
}

double CircuitLab::Diode::Current(double vd) const
{
	// Solo la corrente di giunzione: GMIN è un artificio numerico, non una
	// corrente fisica. Includerla farebbe risultare attraversato da qualche nA
	// un diodo in inversa (a 5 V: 5 nA, sopra la soglia dei pallini di UI).
	const double nVt = m_emissionCoefficient * THERMAL_VOLTAGE;
	return m_saturationCurrent * (std::exp(std::min(vd / nVt, MAX_EXPONENT)) - 1.0);
}

void CircuitLab::Diode::SaveSpecificData(nlohmann::json &j) const
{
	j["saturationCurrent"] = m_saturationCurrent;
	j["emissionCoefficient"] = m_emissionCoefficient;
}

void CircuitLab::Diode::LoadSpecificData(const nlohmann::json &j)
{
	m_saturationCurrent = j["saturationCurrent"];
	m_emissionCoefficient = j["emissionCoefficient"];
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
