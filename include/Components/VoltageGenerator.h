#pragma once
#include "Component.h"
#include "WaveForm.h"

namespace CircuitLab {

	// Modella una sorgente di tensione ideale nel sistema MNA.
	// Le sorgenti di tensione introducono una variabile extra nella matrice
	// (la corrente che le attraversa), per questo GetExtraVariables() restituisce 1.
	// Lo Stamp usa il metodo della variabile aggiuntiva (MNA standard).
	class VoltageGenerator : public Component {
	private:
		std::unique_ptr<WaveForm> m_waveForm;  // Tensione in Volt

	public:
		VoltageGenerator(std::unique_ptr<WaveForm> waveForm);

		// Stampa il contributo della sorgente nella matrice MNA.
		// Aggiunge le righe/colonne per la corrente incognita k,
		// e imposta b[k] = tensione.
		void StampMatrix(Eigen::MatrixXd &A,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap) override;

		void StampVector(Eigen::VectorXd &B,
			const std::map<int, int> &nodeMap,
			const std::map<int, int> &voltageSourceMap,
			const StampContext &ctx) override;

		// Segnala al Circuit che questa sorgente occupa una riga extra nella matrice
		int GetExtraVariables() const override { return 1; }
		WaveFormType GetWaveFormType() const override;
		void SetWaveFormType(WaveFormType type) override;
		void SetWaveForm(std::unique_ptr<WaveForm> waveForm);


		void SaveSpecificData(nlohmann::json &j) const override;
		void LoadSpecificData(const nlohmann::json &j) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
	};
}
