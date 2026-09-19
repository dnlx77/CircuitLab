#pragma once
#include "Component.h"

namespace CircuitLab {

	// Modella un diodo a giunzione (equazione di Shockley):
	//
	//   I(Vd) = Is * (exp(Vd / (n*Vt)) - 1),   Vd = V(anodo) - V(catodo)
	//
	// Terminale 0 = anodo, terminale 1 = catodo; la corrente è positiva da 0 a 1,
	// come per gli altri componenti a due terminali.
	//
	// A differenza di resistenza/condensatore/induttore il diodo è NON LINEARE:
	// la sua conduttanza dipende dalla tensione che si sta cercando, quindi non
	// può essere stampata una volta sola in A (StampMatrix/StampVector, qui
	// vuoti). Application risolve ogni step con Newton-Raphson: ad ogni
	// iterazione il diodo viene linearizzato attorno alla tensione corrente Vk,
	//
	//   I ≈ g*Vd + Ieq,   g = dI/dV|Vk,   Ieq = I(Vk) - g*Vk
	//
	// (stesso schema "conduttanza + generatore di corrente" del modello companion
	// di condensatore/induttore, ma ricalcolato a ogni iterazione) — vedi
	// StampNonlinear.
	class Diode : public Component {
	private:
		// Tensione termica kT/q a ~300 K
		static constexpr double THERMAL_VOLTAGE = 0.02585;
		// Conduttanza minima in parallelo alla giunzione (1 GOhm, come la perdita
		// in inversa di un diodo reale). Serve a due cose quando il diodo è spento
		// (g -> 0): non lasciare nodi completamente isolati (es. l'uscita di un
		// ponte con condensatore) e limitare il rapporto tra il termine più grande
		// e il più piccolo della matrice — con un condensatore grande e passo
		// piccolo, Geq = C/h può valere centinaia di S, e con una GMIN di 1e-12 il
		// solver la dichiarerebbe singolare.
		static constexpr double GMIN = 1e-9;
		// Limite sull'argomento dell'esponenziale, per non andare in overflow
		// quando Newton propone tensioni assurde nelle prime iterazioni
		static constexpr double MAX_EXPONENT = 80.0;

		double m_saturationCurrent;   // Is in Ampere
		double m_emissionCoefficient; // n (fattore di idealità, adimensionale)
		double m_lastVd;              // Vd usata all'ultima linearizzazione (serve al limitatore)
		double m_lastG;               // g = dI/dV dell'ultima linearizzazione (senza GMIN)
		double m_lastIeq;             // Ieq dell'ultima linearizzazione (serve a HasConverged)

		// Tolleranze del test di convergenza sulla corrente (come reltol/abstol di SPICE)
		static constexpr double CURRENT_ABS_TOL = 1e-9;
		static constexpr double CURRENT_REL_TOL = 1e-3;

		// Limitatore di tensione della giunzione (pnjlim di SPICE): impedisce che
		// un singolo passo di Newton faccia salire Vd di molte volte n*Vt, il che
		// darebbe una corrente/conduttanza esponenzialmente enorme.
		static double LimitVoltage(double vNew, double vOld, double nVt, double vCrit);

	public:
		Diode(double saturationCurrent = 1e-14, double emissionCoefficient = 1.0);

		// Nessun contributo statico: tutto il diodo è nella parte non lineare
		void StampMatrix(Eigen::MatrixXd &A,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			double h) override;

		void StampVector(Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			const StampContext &ctx) override;

		bool IsNonlinear() const override { return true; }

		// Linearizza il diodo attorno alla tensione letta da x (soluzione
		// dell'iterazione precedente) e stampa g in A e Ieq in B.
		// Restituisce true se ha dovuto limitare la tensione.
		bool StampNonlinear(Eigen::MatrixXd &A,
			Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const Eigen::VectorXd &x) override;

		// Confronta la corrente predetta dall'ultima linearizzazione con quella
		// reale di Shockley alla Vd della nuova soluzione x.
		bool HasConverged(const std::map<int, int> &nodeMap, const Eigen::VectorXd &x) const override;

		// Corrente anodo->catodo per una data Vd (usata da Application una volta
		// convergita la soluzione, per fili e oscilloscopio)
		double Current(double vd) const;

		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
	};
}
