#pragma once
#include "Component.h"

namespace CircuitLab {

	// Modella un condensatore ideale nel sistema MNA tramite un modello companion
	// (equivalente di Norton, integrazione backward-Euler):
	//
	//   i(t) = C * dv/dt ~= (C/h) * (v(t) - v(t-h)) = Geq*v(t) - Ieq
	//
	// dove Geq = C/h e Ieq = Geq * v(t-h). Geq si comporta come una conduttanza
	// ordinaria (parte statica, StampMatrix) mentre Ieq è un generatore di corrente
	// che dipende dalla tensione memorizzata allo step precedente (parte dinamica,
	// StampVector). v(t-h) viene aggiornata dopo ogni step tramite UpdateState(),
	// chiamata da Application una volta risolto il sistema.
	class Capacitor : public Component {
	private:
		double m_capacitance;     // Capacità in Farad
		double m_conductance;     // Geq = C/h, ricalcolata in StampMatrix (unico punto in cui h è noto)
		double m_previousVoltage; // v(t-h): tensione ai capi del condensatore all'ultimo step risolto

	public:
		Capacitor(double value);

		// Stampa Geq = C/h con lo stesso pattern di una resistenza.
		// h viene passato da Circuit e ricalcolato solo quando il circuito è dirty
		// (topologia cambiata o timestep cambiato, vedi Circuit::SetTimestep).
		void StampMatrix(Eigen::MatrixXd &A,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			double h) override;

		// Inietta la corrente equivalente Ieq = Geq * v(t-h) ai nodi n1/n2
		// (stessa polarità dello Stamp di StampMatrix: += su n1, -= su n2).
		void StampVector(Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			const StampContext &ctx) override;

		// Memorizza v1 - v2 come tensione del passo appena risolto, da usare
		// come v(t-h) nel prossimo StampVector.
		void UpdateState(double v1, double v2) override;

		// Tensione memorizzata al passo precedente (per il calcolo della corrente in Application).
		double GetPreviousVoltage() const { return m_previousVoltage; }

		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
	};
}
