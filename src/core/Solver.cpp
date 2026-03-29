#include "Core/Solver.h"

std::optional<Eigen::VectorXd> CircuitLab::Solver::SolveCircuit(const Eigen::MatrixXd &A, const Eigen::VectorXd &b)
{
	auto qr = A.colPivHouseholderQr();
	if (qr.isInvertible())
		return qr.solve(b);
	
	return std::nullopt;
}
