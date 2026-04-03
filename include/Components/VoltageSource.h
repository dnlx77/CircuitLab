#pragma once
#include "Component.h"

namespace CircuitLab {

	class VoltageSource : public Component {
	private:
		double m_voltage;
	public:
		VoltageSource(double value);

		// Getter
		double GetVoltage() const { return m_voltage; }

		// Setter
		void SetVoltage(double value);
		void Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B, const std::map<int, int> &nodeMap, const std::map<int, int> &voltageSourceMap) override;
		int GetExtraVariables() const override { return 1; }
		void SaveSpecificData() override;
		void LoadSpecificData() override;
	};
}