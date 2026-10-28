#pragma once
#include <algorithm>
#include <cmath>

namespace CircuitLab {

	// Tutto ciò che condividono le giunzioni a semiconduttore (diodo, transistor) nel modello
	// di Shockley e nella risoluzione di Newton-Raphson.

	// Tensione termica kT/q a ~300 K
	inline constexpr double THERMAL_VOLTAGE = 0.02585;

	// Conduttanza minima in parallelo a una giunzione (1 GOhm, come la perdita in inversa di
	// un componente reale). Serve a due cose quando la giunzione è spenta (g -> 0): non
	// lasciare nodi completamente isolati (es. l'uscita di un ponte con condensatore) e
	// limitare il rapporto tra il termine più grande e il più piccolo della matrice — con un
	// condensatore grande e passo piccolo, Geq = C/h può valere centinaia di S, e con una
	// GMIN di 1e-12 il solver la dichiarerebbe singolare.
	inline constexpr double JUNCTION_GMIN = 1e-9;

	// Limite sull'argomento dell'esponenziale, per non andare in overflow quando Newton
	// propone tensioni assurde nelle prime iterazioni
	inline constexpr double MAX_EXPONENT = 80.0;

	// Tolleranze del test di convergenza sulla corrente (come reltol/abstol di SPICE)
	inline constexpr double CURRENT_ABS_TOL = 1e-9;
	inline constexpr double CURRENT_REL_TOL = 1e-3;

	// exp(v/vt) con l'argomento limitato a MAX_EXPONENT
	inline double LimitedExp(double argument)
	{
		return std::exp(std::min(argument, MAX_EXPONENT));
	}

	// Tensione oltre la quale la curva esponenziale diventa più ripida di quanto Newton
	// possa seguire (vt = n*Vt della giunzione, is = corrente di saturazione)
	inline double CriticalVoltage(double vt, double is)
	{
		return vt * std::log(vt / (std::sqrt(2.0) * is));
	}

	// pnjlim (SPICE). Se la nuova tensione supera Vcrit E il salto rispetto all'iterazione
	// precedente è più grande di 2*vt, la comprime: in modo logaritmico se la vecchia
	// tensione era già in conduzione, altrimenti riportando la tensione a poche vt oltre la
	// soglia. Impedisce che un singolo passo di Newton faccia salire la tensione di giunzione
	// di molte volte vt, il che darebbe una corrente/conduttanza esponenzialmente enorme.
	inline double LimitVoltage(double vNew, double vOld, double vt, double vCrit)
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

	// True se la corrente vera e quella predetta dal modello linearizzato coincidono entro
	// tolleranza: è il test di convergenza di SPICE.
	inline bool CurrentsAgree(double actual, double predicted)
	{
		return std::abs(actual - predicted) <= CURRENT_ABS_TOL + CURRENT_REL_TOL * std::max(std::abs(actual), std::abs(predicted));
	}
}
