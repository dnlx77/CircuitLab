#include "Core/Solver.h"
#include <cmath>

// Equilibratura di Ruiz: ripete alcune volte "dividi ogni riga e ogni colonna per
// la radice della sua norma infinito", fino ad avere righe e colonne di norma ~1.
// Restituisce i fattori cumulati in rowScale/colScale e la matrice scalata in scaled.
// Le righe/colonne interamente nulle (circuito degenere) restano com'erano: una
// matrice singolare resta singolare dopo lo scaling, e la fattorizzazione lo rivelerà.
// Lavora in place su scaled e sui vettori passati (membri del Solver, riusati fra
// una chiamata e l'altra): Factorize è chiamata ad ogni iterazione di Newton e
// ricreare matrici temporanee ad ogni passata ne era la parte più costosa.
static void Equilibrate(const Eigen::MatrixXd &A, Eigen::VectorXd &rowScale, Eigen::VectorXd &colScale,
	Eigen::VectorXd &dr, Eigen::VectorXd &dc, Eigen::MatrixXd &scaled)
{
	constexpr int MAX_ITERATIONS = 10;
	// Fattori tutti entro questa distanza da 1: righe e colonne sono già a norma
	// ~1 e altre passate non cambierebbero nulla di rilevante (lo scaling serve
	// solo al condizionamento: la soluzione, riportata alla scala originale in
	// SolveCircuit, è la stessa per qualunque scaling).
	constexpr double CONVERGED = 1e-3;
	const Eigen::Index rows = A.rows();
	const Eigen::Index cols = A.cols();

	rowScale.setOnes(rows);
	colScale.setOnes(cols);
	dr.resize(rows);
	dc.resize(cols);
	scaled = A;

	for (int it = 0; it < MAX_ITERATIONS; it++)
	{
		double maxDeviation = 0.0;
		for (Eigen::Index i = 0; i < rows; i++)
		{
			double m = scaled.row(i).cwiseAbs().maxCoeff();
			dr[i] = (m > 0.0) ? 1.0 / std::sqrt(m) : 1.0;
			maxDeviation = std::max(maxDeviation, std::abs(dr[i] - 1.0));
		}
		for (Eigen::Index j = 0; j < cols; j++)
		{
			double m = scaled.col(j).cwiseAbs().maxCoeff();
			dc[j] = (m > 0.0) ? 1.0 / std::sqrt(m) : 1.0;
			maxDeviation = std::max(maxDeviation, std::abs(dc[j] - 1.0));
		}
		if (maxDeviation < CONVERGED)
			break;

		for (Eigen::Index j = 0; j < cols; j++)
			for (Eigen::Index i = 0; i < rows; i++)
				scaled(i, j) = dr[i] * scaled(i, j) * dc[j];
		rowScale.array() *= dr.array();
		colScale.array() *= dc.array();
	}
}

// Equilibra A, la fattorizza con QR a pivot di colonna e verifica se è invertibile;
// il risultato resta cachato in m_matrix finché non si richiama Factorize di nuovo.
void CircuitLab::Solver::Factorize(const Eigen::MatrixXd &A)
{
	Equilibrate(A, m_rowScale, m_colScale, m_dr, m_dc, m_scaled);

	m_matrix.compute(m_scaled);
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
