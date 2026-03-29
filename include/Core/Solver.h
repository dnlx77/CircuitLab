#pragma once
#include <Eigen/Dense>
#include <optional>

namespace CircuitLab {
	class Solver {
	public:
		static std::optional<Eigen::VectorXd> SolveCircuit(const Eigen::MatrixXd &A, const Eigen::VectorXd &b);
	};
}
