#pragma once

#include "WaveForm.h"

namespace CircuitLab {
	// Forma d'onda sinusoidale: amplitude * sin(2*pi*frequency*t + phase).
	class SineWaveForm : public WaveForm {
	private:
		double m_amplitude;
		double m_frequency;  // Hz
		double m_phase;      // radianti
	public:
		SineWaveForm(double amplitude, double frequency, double phase);
		double Evaluate(double t) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
		void SaveSpecificData(nlohmann::json &j) const override;
	};
}