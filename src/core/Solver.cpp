#include "Core/Solver.h"
#include <algorithm>
#include <cmath>

// Equilibratura di Ruiz: ripete alcune volte "dividi ogni riga e ogni colonna per
// la radice della sua norma infinito", fino ad avere righe e colonne di norma ~1.
// Restituisce i fattori cumulati in rowScale/colScale e la matrice scalata in scaled.
// Le righe/colonne interamente nulle (circuito degenere) restano com'erano: una
// matrice singolare resta singolare dopo lo scaling, e la fattorizzazione lo rivelerà.
// Lavora in place su scaled e sui vettori passati (membri del Solver, riusati fra
// una chiamata e l'altra): Factorize è chiamata ad ogni iterazione di Newton e
// ricreare matrici temporanee ad ogni passata ne era la parte più costosa.
// I massimi di righe e colonne si calcolano in un'unica passata sulla matrice (che è
// per colonne: scorrere le righe una alla volta salta continuamente in memoria).
// Con warmStart i fattori già presenti in rowScale/colScale (dell'ultima matrice, se ha la
// stessa dimensione) sono il punto di partenza: le matrici di due iterazioni di Newton, o di
// due step, sono molto simili, e di solito basta una o due passate invece di una decina.
static void Equilibrate(const Eigen::MatrixXd &A, Eigen::VectorXd &rowScale, Eigen::VectorXd &colScale,
	Eigen::VectorXd &dr, Eigen::VectorXd &dc, Eigen::MatrixXd &scaled, bool warmStart)
{
	constexpr int MAX_ITERATIONS = 10;
	// Fattori tutti entro questa distanza da 1: righe e colonne sono già a norma
	// ~1 e altre passate non cambierebbero nulla di rilevante (lo scaling serve
	// solo al condizionamento: la soluzione, riportata alla scala originale in
	// SolveCircuit, è la stessa per qualunque scaling).
	constexpr double CONVERGED = 1e-3;
	const Eigen::Index rows = A.rows();
	const Eigen::Index cols = A.cols();

	warmStart = warmStart && rowScale.size() == rows && colScale.size() == cols;
	if (!warmStart)
	{
		rowScale.setOnes(rows);
		colScale.setOnes(cols);
	}
	dr.resize(rows);
	dc.resize(cols);
	if (warmStart)
	{
		scaled.resize(rows, cols);
		for (Eigen::Index j = 0; j < cols; j++)
			for (Eigen::Index i = 0; i < rows; i++)
				scaled(i, j) = rowScale[i] * A(i, j) * colScale[j];
	}
	else
		scaled = A;

	for (int it = 0; it < MAX_ITERATIONS; it++)
	{
		// dr e dc contengono prima i massimi, poi i fattori
		dr.setZero();
		for (Eigen::Index j = 0; j < cols; j++)
		{
			const double *column = scaled.data() + j * rows;
			double columnMax = 0.0;
			for (Eigen::Index i = 0; i < rows; i++)
			{
				const double magnitude = std::abs(column[i]);
				dr[i] = std::max(dr[i], magnitude);
				columnMax = std::max(columnMax, magnitude);
			}
			dc[j] = columnMax;
		}

		double maxDeviation = 0.0;
		for (Eigen::Index i = 0; i < rows; i++)
		{
			dr[i] = (dr[i] > 0.0) ? 1.0 / std::sqrt(dr[i]) : 1.0;
			maxDeviation = std::max(maxDeviation, std::abs(dr[i] - 1.0));
		}
		for (Eigen::Index j = 0; j < cols; j++)
		{
			dc[j] = (dc[j] > 0.0) ? 1.0 / std::sqrt(dc[j]) : 1.0;
			maxDeviation = std::max(maxDeviation, std::abs(dc[j] - 1.0));
		}
		if (maxDeviation < CONVERGED)
			break;

		for (Eigen::Index j = 0; j < cols; j++)
		{
			double *column = scaled.data() + j * rows;
			const double columnFactor = dc[j];
			for (Eigen::Index i = 0; i < rows; i++)
				column[i] = dr[i] * column[i] * columnFactor;
		}
		rowScale.array() *= dr.array();
		colScale.array() *= dc.array();
	}
}

// Equilibra A e la fattorizza; il risultato resta cachato finché non si richiama
// Factorize di nuovo.
// Dopo l'equilibratura la matrice ha elementi dell'ordine di 1, e la LU con pivot parziale
// (circa 4 volte meno operazioni della QR a pivot di colonna) è affidabile: la si usa se
// il pivot più piccolo non è sospettosamente piccolo rispetto al più grande. Altrimenti
// (matrice vicina alla singolarità, circuiti degeneri) si ricade sulla QR a pivot di
// colonna, più robusta, che decide anche se la matrice è invertibile.
void CircuitLab::Solver::Factorize(const Eigen::MatrixXd &A)
{
	Equilibrate(A, m_rowScale, m_colScale, m_dr, m_dc, m_scaled, true);

	constexpr double MIN_PIVOT_RATIO = 1e-8;

	m_useLu = false;
	if (m_scaled.rows() > 0)
	{
		m_lu.compute(m_scaled);
		const auto pivots = m_lu.matrixLU().diagonal().cwiseAbs();
		const double smallest = pivots.minCoeff();
		const double largest = pivots.maxCoeff();
		m_useLu = largest > 0.0 && smallest >= MIN_PIVOT_RATIO * largest;
	}

	if (m_useLu)
	{
		m_isMatrixInvertible = true;
		return;
	}

	m_matrix.compute(m_scaled);
	m_isMatrixInvertible = m_matrix.isInvertible();
}

CircuitLab::Solver::Solver() : m_useLu(false), m_isMatrixInvertible(false)
{}

// Tenta di risolvere A*x = b sul sistema equilibrato: A' * y = R*b con x = C*y (vedi
// Factorize). Se la matrice non è invertibile (circuito degenere), restituisce
// std::nullopt invece di produrre un risultato silenziosamente errato.
std::optional<Eigen::VectorXd> CircuitLab::Solver::SolveCircuit(const Eigen::VectorXd &b)
{
	if (!m_isMatrixInvertible)
		return std::nullopt;

	const Eigen::VectorXd scaledB = m_rowScale.cwiseProduct(b);
	Eigen::VectorXd y;
	if (m_useLu)
		y = m_lu.solve(scaledB);
	else
		y = m_matrix.solve(scaledB);
	y.array() *= m_colScale.array();
	return y;
}
