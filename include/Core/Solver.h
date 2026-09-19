#pragma once
#include <Eigen/Dense>
#include <optional>

namespace CircuitLab {

	// Si occupa di risolvere il sistema lineare MNA A*x = b.
	// Mantiene come stato la fattorizzazione QR corrente (m_matrix), così Factorize()
	// va richiamata solo quando la topologia/matrice A cambia, non ad ogni step
	// di simulazione (vedi Circuit::ComputeMatrix, che la cachea).
	class Solver {
	private:
		// Fattorizzazione QR della matrice EQUILIBRATA: A' = diag(m_rowScale) * A * diag(m_colScale).
		// Un circuito MNA mescola conduttanze di ordini di grandezza diversissimi
		// (un interruttore chiuso, un condensatore con Geq = C/h a passo piccolo, un
		// carico di megaohm): senza normalizzare, il rapporto tra il termine più
		// grande e il più piccolo supera la precisione del double e Eigen dichiara
		// singolare una matrice perfettamente valida (o ne restituisce la soluzione
		// con errori enormi). Scalando righe e colonne a norma ~1 quel rapporto
		// sparisce; il risultato torna alla scala originale in SolveCircuit.
		Eigen::ColPivHouseholderQR<Eigen::MatrixXd> m_matrix;
		Eigen::VectorXd m_rowScale;
		Eigen::VectorXd m_colScale;
		bool m_isMatrixInvertible;

	public:
		Solver();
		// Risolve il sistema A*x = b usando la decomposizione QR con pivot,
		// riutilizzando la fattorizzazione calcolata dall'ultima Factorize().
		// Restituisce std::nullopt se la matrice è singolare (circuito mal formato,
		// ad esempio nodi non connessi o sorgenti in conflitto).
		std::optional<Eigen::VectorXd> SolveCircuit(const Eigen::VectorXd &b);

		// Calcola e memorizza la fattorizzazione QR di A. Da richiamare solo quando
		// la matrice cambia (es. dopo una modifica alla topologia del circuito).
		void Factorize(const Eigen::MatrixXd &A);
	};
}