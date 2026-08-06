#pragma once

#include "WaveForm.h"

namespace CircuitLab {
	// Forma d'onda quadra: alterna +amplitude/-amplitude al ritmo di frequency Hz.
	class SquareWaveForm : public WaveForm {
	private:
		double m_amplitude;
		double m_frequency;  // Hz
	public:
		SquareWaveForm(double amplitude, double frequency);
		double Evaluate(double t) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
		void SaveSpecificData(nlohmann::json &j) const override;
	};
}