#pragma once

#include "WaveForm.h"

namespace CircuitLab {
	class SquareWaveForm : public WaveForm {
	private:
		double m_amplitude;
		double m_frequency;
	public:
		SquareWaveForm(double amplitude, double frequency);
		double Evaluate(double t) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
		void SaveSpecificData(nlohmann::json &j) const override;
	};
}