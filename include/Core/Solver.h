#pragma once
#include <Eigen/Dense>
#include <optional>

namespace CircuitLab {

	// Classe statica che si occupa di risolvere il sistema lineare MNA.
	// Non mantiene stato interno: tutti i metodi sono statici.
	class Solver {
	public:
		// Risolve il sistema A*x = b usando la decomposizione QR con pivot.
		// Restituisce std::nullopt se la matrice è singolare (circuito mal formato,
		// ad esempio nodi non connessi o sorgenti in conflitto).
		static std::optional<Eigen::VectorXd> SolveCircuit(const Eigen::MatrixXd &A, const Eigen::VectorXd &b);
	};
}