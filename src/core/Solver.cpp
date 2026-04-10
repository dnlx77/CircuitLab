#include "Core/Solver.h"

// Tenta di risolvere A*x = b con decomposizione QR con pivoting per colonna.
// Il pivoting rende il metodo più robusto numericamente rispetto alla LU classica.
// Se la matrice non è invertibile (circuito degenere), restituisce std::nullopt
// invece di produrre un risultato silenziosamente errato.
std::optional<Eigen::VectorXd> CircuitLab::Solver::SolveCircuit(const Eigen::MatrixXd &A, const Eigen::VectorXd &b)
{
	auto qr = A.colPivHouseholderQr();
	if (qr.isInvertible())
		return qr.solve(b);

	return std::nullopt;
}