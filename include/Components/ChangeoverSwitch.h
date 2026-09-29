#pragma once
#include "Component.h"

namespace CircuitLab {

	// Modella un deviatore ideale: 3 terminali, comune (0) + due vie (1, 2).
	// A differenza dello Switch non ha uno stato aperto: il comune è sempre
	// collegato a esattamente una delle due vie (cortocircuito su quella,
	// conduttanza quasi nulla sull'altra, stesso approccio di Switch::SetClosed
	// applicato a due rami che condividono il nodo comune). Lo stato (via 1/via 2)
	// cambia solo su comando esterno (click o tasto in UI, vedi Circuit::ToggleSwitch,
	// riusato qui esattamente come per Switch), non per effetto della simulazione.
	class ChangeoverSwitch : public Component {
	private:
		bool m_viaSecond; // false = comune su via 1, true = comune su via 2
		double m_conductanceVia1;
		double m_conductanceVia2;

		void SetViaSecond(bool viaSecond);

	public:
		ChangeoverSwitch(bool viaSecond = false);

		void StampMatrix(Eigen::MatrixXd &A,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			double h) override;

		void StampVector(Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			const StampContext &ctx) override;

		// Inverte la via collegata al comune. Chi lo chiama (Circuit) deve
		// invalidare il circuito, perché la matrice va ristampata.
		void ToggleSwitch() override;

		// Riusato come "il deviatore è sulla via 2" (vedi UI: stesso campo che
		// per Switch indica aperto/chiuso, qui sceglie quale via disegnare).
		bool IsSwitchClosed() const override { return m_viaSecond; }

		// Conduttanze correnti dei due rami (comune-via1, comune-via2), per il
		// calcolo delle correnti in Application, stesso ruolo di Switch::GetConductance.
		double GetConductanceVia1() const { return m_conductanceVia1; }
		double GetConductanceVia2() const { return m_conductanceVia2; }

		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
	};
}
