#include "Core/Solver.h"

void CircuitLab::Solver::Factorize(const Eigen::MatrixXd &A)
{
	m_matrix = A.colPivHouseholderQr();
	m_isMatrixInvertible = m_matrix.isInvertible();

}

CircuitLab::Solver::Solver() : m_isMatrixInvertible(false)
{}

// Tenta di risolvere A*x = b con decomposizione QR con pivoting per colonna.
// Il pivoting rende il metodo più robusto numericamente rispetto alla LU classica.
// Se la matrice non è invertibile (circuito degenere), restituisce std::nullopt
// invece di produrre un risultato silenziosamente errato.
std::optional<Eigen::VectorXd> CircuitLab::Solver::SolveCircuit(const Eigen::VectorXd &b)
{
	if (m_isMatrixInvertible)
		return m_matrix.solve(b);

	return std::nullopt;
}