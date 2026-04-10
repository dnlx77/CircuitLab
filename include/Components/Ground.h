#pragma once
#include "Components/Component.h"

namespace CircuitLab {

	// Modella il nodo di riferimento (ground) del circuito.
	// Nel sistema MNA il ground corrisponde al nodo 0 e non occupa
	// alcuna riga nella matrice: lo Stamp è quindi vuoto.
	// Il costruttore assegna direttamente nodeId = 0 all'unico terminale.
	class Ground : public Component {
	public:
		Ground();

		// Non contribuisce alla matrice MNA
		void Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap) override;

		void SaveSpecificData() override;
		void LoadSpecificData() override;
	};
}