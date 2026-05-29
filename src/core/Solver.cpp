#include "Core/Solver.h"

void CircuitLab::Solver::Factorize(const Eigen::MatrixXd &A)
{
	m_matrix = A.colPivHouseholderQr();
}

CircuitLab::Solver::Solver()
{}

// Tenta di risolvere A*x = b con decomposizione QR con pivoting per colonna.
// Il pivoting rende il metodo più robusto numericamente rispetto alla LU classica.
// Se la matrice non è invertibile (circuito degenere), restituisce std::nullopt
// invece di produrre un risultato silenziosamente errato.
std::optional<Eigen::VectorXd> CircuitLab::Solver::SolveCircuit(const Eigen::VectorXd &b)
{
	if (m_matrix.isInvertible())
		return m_matrix.solve(b);

	return std::nullopt;
}