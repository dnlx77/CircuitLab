#pragma once
#include "Components/Component.h"

namespace CircuitLab {

	class Ground : public Component {
	private:
	public:
		Ground();

		void Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B, const std::map<int, int> &nodeMap, const std::map<int, int> &voltageSourceMap) override;
		void SaveSpecificData() override;
		void LoadSpecificData() override;
	};
}