#pragma once
#include "Component.h"

namespace CircuitLab {

	// Modella una resistenza ideale nel sistema MNA.
	// Internamente lavora con la conduttanza (G = 1/R) perché è il valore
	// che si stampa direttamente nella matrice di conduttanza MNA.
	class Resistor : public Component {
	private:
		double m_resistance;   // Resistenza in Ohm
		double m_conductance;  // Conduttanza in Siemens (G = 1/R), usata per lo Stamp

	public:
		Resistor(double value);

		double GetResistance() const { return m_resistance; }
		double GetConductance() const { return m_conductance; }

		// Aggiorna la resistenza e ricalcola la conduttanza.
		// Gestisce il caso R -> 0 con un valore molto grande (cortocircuito approssimato).
		void SetResistance(double res);

		// Stampa il contributo della resistenza nella matrice MNA.
		// Una resistenza tra i nodi n1 e n2 contribuisce con G a (n1,n1), (n2,n2)
		// e con -G a (n1,n2), (n2,n1).
		void Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap) override;

		void SaveSpecificData() override;
		void LoadSpecificData() override;
	};
}