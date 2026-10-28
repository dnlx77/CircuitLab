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

		// Segnala che questo componente è il nodo di riferimento del circuito
		bool IsGround() const override { return true; }

		// Non contribuisce alla matrice MNA né ha parametri elettrici propri
		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
	};
}