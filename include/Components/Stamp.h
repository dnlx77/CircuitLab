#pragma once
#include <Eigen/Dense>

namespace CircuitLab {

	// Piccoli attrezzi per scrivere nella matrice MNA (A) e nel vettore (b). Un indice di
	// matrice negativo (-1) indica un terminale a massa o mai collegato: non ha riga né
	// colonna, quindi non si scrive nulla (vedi Component::MatrixIndex).

	// Conduttanza g tra i nodi n1 e n2:
	//
	//        n1    n2
	//  n1  [ +g   -g ]
	//  n2  [ -g   +g ]
	inline void StampConductance(Eigen::MatrixXd &A, int n1, int n2, double g)
	{
		if (n1 >= 0) A(n1, n1) += g;
		if (n2 >= 0) A(n2, n2) += g;

		if (n1 >= 0 && n2 >= 0) {
			A(n1, n2) -= g;
			A(n2, n1) -= g;
		}
	}

	// Generatore di corrente i che entra nel nodo n1 e esce dal nodo n2
	// (i termini noti del sistema: +i in n1, -i in n2).
	inline void StampCurrent(Eigen::VectorXd &B, int n1, int n2, double i)
	{
		if (n1 >= 0) B[n1] += i;
		if (n2 >= 0) B[n2] -= i;
	}

	// Tensione del nodo con indice di matrice n nella soluzione x (0 per la massa)
	inline double VoltageAt(const Eigen::VectorXd &x, int n)
	{
		return (n >= 0) ? x[n] : 0.0;
	}
}
