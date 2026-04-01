#pragma once
#include "Component.h"

namespace CircuitLab {
	
	class Resistor : public Component {
	private:
		double m_resistance;
		double m_conductance;
	public:
		Resistor(double value);
		
		// Getter
		double GetResistance() const { return m_resistance; }
		double GetConductance() const { return m_conductance; }

		// Setter
		void SetResistance(double res);
		void Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &b, const std::map<int, int> &nodeMap) override;
		void SaveSpecificData() override;
		void LoadSpecificData() override;
	};
}