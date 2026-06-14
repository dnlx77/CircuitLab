#pragma once

#include "WaveForm.h"

namespace CircuitLab {
	class DCWaveForm : public WaveForm {
	private:
		double m_voltage;
	public:
		DCWaveForm(double voltage);
		double Evaluate(double t) override;
		std::map<ComponentValue, double> GetValues() const override;
		void SetValues(const std::map<ComponentValue, double> &values) override;
		void SaveSpecificData(nlohmann::json &j) const override;
	};
}