#pragma once
#include "Component.h"

namespace CircuitLab {

	// Modella un interruttore ideale: chiuso si comporta come un cortocircuito
	// (conduttanza altissima), aperto come un circuito aperto (conduttanza
	// pressoché nulla). A differenza di condensatore/induttore non ha memoria
	// né dipendenza dal tempo: StampVector non fa nulla, esattamente come Resistor.
	// Lo stato (aperto/chiuso) cambia solo su comando esterno (click o tasto in UI,
	// vedi Circuit::ToggleSwitch), non per effetto della simulazione.
	class Switch : public Component {
	private:
		bool m_closed;
		double m_conductance; // Alta se chiuso, quasi zero se aperto (vedi SetClosed)

		void SetClosed(bool closed);

	public:
		Switch(bool closed = true);

		void StampMatrix(Eigen::MatrixXd &A,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			double h) override;

		void StampVector(Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			const StampContext &ctx) override;

		// Inverte lo stato e ricalcola la conduttanza. Chi lo chiama (Circuit)
		// deve invalidare il circuito, perché la matrice va ristampata.
		void ToggleSwitch() override;

		bool IsSwitchClosed() const override { return m_closed; }

		// Conduttanza corrente (per il calcolo della corrente in Application,
		// stesso ruolo di Capacitor::GetConductance / Inductor::GetConductance).
		double GetConductance() const { return m_conductance; }

		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
	};
}
