#pragma once
#include "Component.h"
#include "Junction.h"

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
	// può essere stampata una volta sola in A (StampMatrix/StampVector restano
	// quelli vuoti di Component). Il solver (SolveNonlinearStep) risolve ogni step con Newton-Raphson: ad ogni
	// iterazione il diodo viene linearizzato attorno alla tensione corrente Vk,
	//
	//   I ≈ g*Vd + Ieq,   g = dI/dV|Vk,   Ieq = I(Vk) - g*Vk
	//
	// (stesso schema "conduttanza + generatore di corrente" del modello companion
	// di condensatore/induttore, ma ricalcolato a ogni iterazione) — vedi
	// StampNonlinear.
	class Diode : public Component {
	private:
		double m_saturationCurrent;   // Is in Ampere
		double m_emissionCoefficient; // n (fattore di idealità, adimensionale)
		double m_lastVd;              // Vd usata all'ultima linearizzazione (serve al limitatore)
		double m_lastG;               // g = dI/dV dell'ultima linearizzazione (senza GMIN)
		double m_lastIeq;             // Ieq dell'ultima linearizzazione (serve a HasConverged)

		// Costanti, limitatore di tensione (pnjlim) e test di convergenza: vedi Junction.h

	public:
		Diode(double saturationCurrent = 1e-14, double emissionCoefficient = 1.0);

		// Nessun contributo statico: tutto il diodo è nella parte non lineare
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

		// Corrente anodo->catodo per una data Vd (usata da ReportCurrents una volta
		// convergita la soluzione, per fili e oscilloscopio)
		double Current(double vd) const;

		void ResetDynamicState() override { m_lastVd = 0.0; m_lastG = 0.0; m_lastIeq = 0.0; }

		// Correnti per la UI (vedi Component::ReportCurrents)
		bool ReportCurrents(const std::vector<double> &terminalVoltages, double h, CurrentReport &report) const override;

		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
	};
}
