#include "Core/Solver.h"
#include <cmath>

// Equilibratura di Ruiz: ripete alcune volte "dividi ogni riga e ogni colonna per
// la radice della sua norma infinito", fino ad avere righe e colonne di norma ~1.
// Restituisce i fattori cumulati in rowScale/colScale e la matrice scalata in scaled.
// Le righe/colonne interamente nulle (circuito degenere) restano com'erano: una
// matrice singolare resta singolare dopo lo scaling, e la fattorizzazione lo rivelerà.
static void Equilibrate(const Eigen::MatrixXd &A, Eigen::VectorXd &rowScale, Eigen::VectorXd &colScale, Eigen::MatrixXd &scaled)
{
	constexpr int ITERATIONS = 10;
	const Eigen::Index n = A.rows();

	rowScale = Eigen::VectorXd::Ones(n);
	colScale = Eigen::VectorXd::Ones(A.cols());
	scaled = A;

	for (int it = 0; it < ITERATIONS; it++)
	{
		Eigen::VectorXd dr(n), dc(A.cols());
		for (Eigen::Index i = 0; i < n; i++)
		{
			double m = scaled.row(i).cwiseAbs().maxCoeff();
			dr[i] = (m > 0.0) ? 1.0 / std::sqrt(m) : 1.0;
		}
		for (Eigen::Index j = 0; j < A.cols(); j++)
		{
			double m = scaled.col(j).cwiseAbs().maxCoeff();
			dc[j] = (m > 0.0) ? 1.0 / std::sqrt(m) : 1.0;
		}

		scaled = dr.asDiagonal() * scaled * dc.asDiagonal();
		rowScale = rowScale.cwiseProduct(dr);
		colScale = colScale.cwiseProduct(dc);
	}
}

// Equilibra A, la fattorizza con QR a pivot di colonna e verifica se è invertibile;
// il risultato resta cachato in m_matrix finché non si richiama Factorize di nuovo.
void CircuitLab::Solver::Factorize(const Eigen::MatrixXd &A)
{
	Eigen::MatrixXd scaled;
	Equilibrate(A, m_rowScale, m_colScale, scaled);

	m_matrix = scaled.colPivHouseholderQr();
	m_isMatrixInvertible = m_matrix.isInvertible();
}

CircuitLab::Solver::Solver() : m_isMatrixInvertible(false)
{}

// Tenta di risolvere A*x = b con decomposizione QR con pivoting per colonna.
// Il pivoting rende il metodo più robusto numericamente rispetto alla LU classica.
// Il sistema risolto è quello equilibrato: A' * y = R*b con x = C*y (vedi Factorize).
// Se la matrice non è invertibile (circuito degenere), restituisce std::nullopt
// invece di produrre un risultato silenziosamente errato.
std::optional<Eigen::VectorXd> CircuitLab::Solver::SolveCircuit(const Eigen::VectorXd &b)
{
	if (m_isMatrixInvertible)
	{
		Eigen::VectorXd y = m_matrix.solve(m_rowScale.asDiagonal() * b);
		return m_colScale.asDiagonal() * y;
	}

	return std::nullopt;
}
