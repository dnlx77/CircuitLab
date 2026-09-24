#pragma once
#include "Component.h"

namespace CircuitLab {

	// Modella un trasformatore come una coppia di induttori accoppiati
	// magneticamente, tramite un modello companion lineare (equivalente di
	// Norton a due porte, integrazione backward-Euler) — stessa idea
	// dell'induttore singolo (vedi Inductor.h), estesa a due avvolgimenti.
	//
	// Terminali: 0 = primario+, 1 = primario-, 2 = secondario+, 3 = secondario-.
	// v1 = V(0)-V(1), v2 = V(2)-V(3); i1 = corrente nel primario (0->1),
	// i2 = corrente nel secondario (2->3). Le equazioni dei due avvolgimenti
	// accoppiati sono:
	//
	//   v1 = L1*di1/dt + M*di2/dt
	//   v2 = M*di1/dt  + L2*di2/dt
	//
	// con M = k*sqrt(L1*L2) mutua induttanza (k = coefficiente di accoppiamento,
	// 0 = nessun accoppiamento, il trasformatore si comporta come due induttori
	// indipendenti; k->1 = accoppiamento quasi ideale, il rapporto v2/v1 tende a
	// sqrt(L2/L1)). Con backward-Euler (i(t)-i(t-h))/h al posto di di/dt e
	// invertendo la matrice [L1 M; M L2], si ottiene un sistema lineare:
	//
	//   i1(t) = G11*v1 + G12*v2 + Ieq1,   i2(t) = G12*v1 + G22*v2 + Ieq2
	//
	// con G = h * [L1 M; M L2]^-1 (simmetrica: G12 = G21) e Ieq1/Ieq2 = correnti
	// dei due avvolgimenti allo step precedente. Esattamente come Geq/Ieq
	// dell'induttore singolo, ma un blocco 2x2 invece di uno scalare: nessuna
	// variabile aggiuntiva in matrice, nessuna iterazione non lineare — un solo
	// StampMatrix/StampVector, come tutti gli altri componenti lineari.
	class Transformer : public Component {
	private:
		// Sotto questa soglia un'induttanza è trattata come "quasi nulla" (stesso
		// criterio dell'induttore singolo, vedi Inductor::StampMatrix)
		static constexpr double MIN_INDUCTANCE = 1e-9;
		// k=1 esatto rende [L1 M; M L2] singolare (accoppiamento ideale, nessuna
		// induttanza di dispersione): come in SPICE, k resta sempre < 1.
		static constexpr double MAX_COUPLING = 0.999;

		double m_l1, m_l2; // Induttanza di primario e secondario, in Henry
		double m_k;        // Coefficiente di accoppiamento (0..MAX_COUPLING)

		// G = h * [L1 M; M L2]^-1, ricalcolata in StampMatrix (dipende da h)
		double m_g11 = 0.0, m_g12 = 0.0, m_g22 = 0.0;
		// Correnti dei due avvolgimenti allo step precedente (i(t-h))
		double m_i1Prev = 0.0, m_i2Prev = 0.0;

	public:
		Transformer(double primaryInductance = 1e-3, double secondaryInductance = 1e-3, double coupling = 0.999);

		void StampMatrix(Eigen::MatrixXd &A,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			double h) override;

		void StampVector(Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			const StampContext &ctx) override;

		// Correnti dei due avvolgimenti alla tensione convergente (vPrimary,
		// vSecondary), usando lo stato ANCORA VECCHIO (va chiamata prima di
		// UpdateWindingState, come Inductor::GetConductance/GetPreviousCurrent
		// usati insieme in Application::Simulate).
		std::pair<double, double> BranchCurrents(double vPrimary, double vSecondary) const;

		// Aggiorna le correnti memorizzate dei due avvolgimenti, da usare come
		// Ieq1/Ieq2 nel prossimo StampVector. Non sovrascrive il virtual
		// Component::UpdateState (quello ha un solo v1/v2, pensato per 2
		// terminali): qui vPrimary/vSecondary sono le tensioni dei DUE
		// avvolgimenti, non due nodi qualsiasi.
		void UpdateWindingState(double vPrimary, double vSecondary);

		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
	};
}
