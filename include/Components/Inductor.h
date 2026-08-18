#pragma once
#include "Component.h"

namespace CircuitLab {

	// Modella un induttore ideale nel sistema MNA tramite un modello companion
	// (equivalente di Norton, integrazione backward-Euler):
	//
	//   v(t) = L * di/dt  =>  i(t) = i(t-h) + (h/L) * v(t) = Geq*v(t) + Ieq
	//
	// dove Geq = h/L e Ieq = i(t-h). Geq si comporta come una conduttanza
	// ordinaria (parte statica, StampMatrix) mentre Ieq è un generatore di
	// corrente pari alla CORRENTE (non tensione) memorizzata allo step
	// precedente (parte dinamica, StampVector) — a differenza del condensatore,
	// qui lo stato interno è una corrente, e il segno del contributo nel
	// vettore è opposto (vedi commenti in Inductor.cpp).
	class Inductor : public Component {
	private:
		double m_inductance;      // Induttanza in Henry
		double m_conductance;     // Geq = h/L, ricalcolata in StampMatrix (unico punto in cui h è noto)
		double m_previousCurrent; // i(t-h): corrente nell'induttore all'ultimo step risolto

	public:
		Inductor(double value);

		// Stampa Geq = h/L con lo stesso pattern di una resistenza.
		// h viene passato da Circuit e ricalcolato solo quando il circuito è dirty
		// (topologia cambiata o timestep cambiato, vedi Circuit::SetTimestep).
		void StampMatrix(Eigen::MatrixXd &A,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			double h) override;

		// Inietta la corrente equivalente Ieq = i(t-h): a differenza del
		// condensatore, qui va prelevata da n1 e iniettata in n2 (vedi .cpp).
		void StampVector(Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			const StampContext &ctx) override;

		// Aggiorna la corrente memorizzata: i(t) = Geq*(v1-v2) + i(t-h),
		// da usare come i(t-h) nel prossimo StampVector.
		void UpdateState(double v1, double v2) override;

		// Conduttanza equivalente e corrente memorizzata al passo precedente
		// (per il calcolo della corrente in Application, prima di UpdateState).
		double GetConductance() const { return m_conductance; }
		double GetPreviousCurrent() const { return m_previousCurrent; }

		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
	};
}
