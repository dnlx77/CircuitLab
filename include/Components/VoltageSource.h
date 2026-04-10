#pragma once
#include "Component.h"

namespace CircuitLab {

	// Modella una sorgente di tensione ideale nel sistema MNA.
	// Le sorgenti di tensione introducono una variabile extra nella matrice
	// (la corrente che le attraversa), per questo GetExtraVariables() restituisce 1.
	// Lo Stamp usa il metodo della variabile aggiuntiva (MNA standard).
	class VoltageSource : public Component {
	private:
		double m_voltage;  // Tensione in Volt

	public:
		VoltageSource(double value);

		double GetVoltage() const { return m_voltage; }
		void SetVoltage(double value);

		// Stampa il contributo della sorgente nella matrice MNA.
		// Aggiunge le righe/colonne per la corrente incognita k,
		// e imposta b[k] = tensione.
		void Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap) override;

		// Segnala al Circuit che questa sorgente occupa una riga extra nella matrice
		int GetExtraVariables() const override { return 1; }

		void SaveSpecificData() override;
		void LoadSpecificData() override;
	};
}