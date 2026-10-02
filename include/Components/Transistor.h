#pragma once
#include "Component.h"

namespace CircuitLab {

	// Modella un transistor bipolare NPN con il modello di Ebers-Moll
	// semplificato (senza effetto Early, senza resistenze ohmiche parassite —
	// lo stesso livello di dettaglio del Diodo, generalizzato a due giunzioni):
	//
	//   VBE = V(base) - V(emettitore),   VBC = V(base) - V(collettore)
	//   IF = Is*(exp(VBE/Vt) - 1)          corrente "di iniezione" diretta (giunzione B-E)
	//   IR = Is*(exp(VBC/Vt) - 1)          corrente "di iniezione" inversa  (giunzione B-C)
	//
	//   IB (nel terminale base)        = IF/BF + IR/BR
	//   IC (nel terminale collettore)  = (IF - IR) - IR/BR
	//   IE (nel terminale emettitore)  = -(IB + IC)   [KCL: nessun nodo interno]
	//
	// Terminale 0 = base, 1 = collettore, 2 = emettitore. BF (guadagno diretto,
	// il "hFE" da datasheet) è impostabile dall'utente; BR (guadagno inverso,
	// che conta solo in saturazione, dove entrambe le giunzioni sono polarizzate
	// diretto) resta fisso a un valore tipico — esporlo aggiungerebbe un
	// parametro che quasi nessun principiante userebbe.
	//
	// Come il Diodo, è NON LINEARE: Application lo risolve con Newton-Raphson,
	// linearizzando ad ogni iterazione attorno a (VBE_k, VBC_k):
	//
	//   IB ≈ gpi*VBE + gmu*VBC + IBeq,   IC ≈ gm*VBE + go*VBC + ICeq
	//
	// con gpi=dIB/dVBE, gmu=dIB/dVBC, gm=dIC/dVBE, go=dIC/dVBC — il modello
	// companion a piccolo segnale standard di un BJT (senza effetto Early),
	// stampato in MNA come due "resistori" (gpi tra base-emettitore, gmu tra
	// base-collettore) più due generatori di corrente pilotati in tensione
	// (gm e go, entrambi da collettore a emettitore, pilotati rispettivamente
	// da VBE e VBC) — vedi StampNonlinear.
	class Transistor : public Component {
	private:
		// Tensione termica kT/q a ~300 K (stessa costante del Diodo)
		static constexpr double THERMAL_VOLTAGE = 0.02585;
		// Guadagno di corrente inverso: conta solo in saturazione (entrambe le
		// giunzioni polarizzate diretto). Fisso, non esposto in UI — vedi sopra.
		static constexpr double REVERSE_BETA = 1.0;
		// Stesso ruolo di Diode::GMIN: evita nodi isolati (transistor spento) e
		// un rapporto tra il termine più grande e il più piccolo della matrice
		// troppo estremo.
		static constexpr double GMIN = 1e-9;
		// Limite sull'argomento dell'esponenziale, per non andare in overflow
		// quando Newton propone tensioni assurde nelle prime iterazioni
		static constexpr double MAX_EXPONENT = 80.0;

		double m_saturationCurrent; // Is, comune alle due giunzioni (Ampere)
		double m_forwardBeta;       // BF (hFE), adimensionale

		// Punto di linearizzazione dell'ultima chiamata a StampNonlinear: serve
		// sia al limitatore di tensione (come Diode::m_lastVd) sia a
		// HasConverged, che vi confronta la corrente vera con quella predetta.
		double m_lastVbe, m_lastVbc;
		double m_lastGpi, m_lastGmu, m_lastGm, m_lastGo;
		double m_lastIBeq, m_lastICeq;

		// pnjlim (SPICE), identico a Diode::LimitVoltage: impedisce che un
		// singolo passo di Newton faccia salire la tensione di giunzione di
		// molte volte Vt, il che darebbe una corrente/conduttanza esponenzialmente
		// enorme. Applicato separatamente a VBE e VBC.
		static double LimitVoltage(double vNew, double vOld, double vt, double vCrit);

	public:
		// Correnti nei tre terminali (convenzione MNA: positive ENTRANTI nel
		// componente dal terminale, come per gli altri componenti multi-terminale
		// — vedi Transformer::BranchCurrents).
		struct TerminalCurrents { double ib, ic, ie; };

		Transistor(double saturationCurrent = 1e-14, double forwardBeta = 100.0);

		// Nessun contributo statico: tutto il transistor è nella parte non lineare
		void StampMatrix(Eigen::MatrixXd &A,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			double h) override;

		void StampVector(Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			const StampContext &ctx) override;

		bool IsNonlinear() const override { return true; }

		// Linearizza il transistor attorno a (VBE, VBC) letti da x (soluzione
		// dell'iterazione precedente) e stampa il modello companion in A/B.
		// Restituisce true se ha dovuto limitare una delle due tensioni.
		bool StampNonlinear(Eigen::MatrixXd &A,
			Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const Eigen::VectorXd &x) override;

		// Confronta le correnti (base, collettore) predette dall'ultima
		// linearizzazione con quelle vere di Ebers-Moll alla soluzione x.
		bool HasConverged(const std::map<int, int> &nodeMap, const Eigen::VectorXd &x) const override;

		// Correnti reali (non linearizzate) ai tre terminali per una data
		// (VBE, VBC) — usata da Application una volta convergita la soluzione,
		// per fili e oscilloscopio (stesso ruolo di Diode::Current).
		TerminalCurrents Currents(double vbe, double vbc) const;

		void ResetDynamicState() override
		{
			m_lastVbe = 0.0; m_lastVbc = 0.0;
			m_lastGpi = 0.0; m_lastGmu = 0.0; m_lastGm = 0.0; m_lastGo = 0.0;
			m_lastIBeq = 0.0; m_lastICeq = 0.0;
		}

		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
	};
}
