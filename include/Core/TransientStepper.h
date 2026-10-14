#pragma once
#include <Eigen/Dense>

#include "Core/Circuit.h"
#include "Core/Solver.h"

namespace CircuitLab {

	// Un passo di simulazione transitoria su un Circuit: costruisce matrice e vettore MNA,
	// risolve (con Newton se ci sono componenti non lineari) e porta avanti lo stato dei
	// componenti con memoria (condensatori, induttori, trasformatori). È l'unico posto in cui
	// questa sequenza è scritta: la usano sia l'applicazione sia i test, senza copie.
	//
	// Non possiede né il circuito né il solver, e non conosce il tempo: tempo e passo arrivano
	// ad ogni chiamata (l'applicazione li azzera e li cambia dall'esterno). Mantiene l'ultima
	// soluzione, che è anche il punto di partenza di Newton al passo successivo.
	//
	// Il costruttore collega il solver al circuito (Circuit::SetOnFactorize): la matrice dei
	// circuiti lineari si fattorizza solo quando cambia, non ad ogni passo.
	class TransientStepper {
	public:
		enum class Status {
			ok,
			noConvergence,  // Newton non è arrivato a una soluzione
			singular        // matrice singolare (circuito mal formato)
		};

		TransientStepper(Circuit &circuit, Solver &solver);

		// Risolve il passo al tempo time con passo timestep e memorizza la soluzione, senza toccare
		// lo stato dei componenti: chi vuole leggere le correnti che dipendono dallo stato
		// "precedente" (condensatore, induttore...) lo fa ora, poi chiama CommitState().
		// Se non riesce la soluzione precedente resta com'era.
		Status Solve(double time, double timestep);

		// Porta avanti lo stato dei componenti con memoria usando la soluzione corrente
		void CommitState();

		// Solve + CommitState (se il passo è riuscito)
		Status Step(double time, double timestep);

		// Adotta come soluzione corrente il punto di lavoro DC (vedi SolveOperatingPoint), che
		// imposta anche lo stato dei componenti con memoria. false se non converge: in quel
		// caso non cambia nulla.
		bool StartFromDcOperatingPoint();

		const Eigen::VectorXd &Solution() const { return m_solution; }
		void SetSolution(Eigen::VectorXd solution) { m_solution = std::move(solution); }
		// Dimentica la soluzione: il prossimo passo non lineare riparte da zero
		void ClearSolution() { m_solution = Eigen::VectorXd(); }

		// Tensione di un nodo rispetto a massa nella soluzione corrente: 0 per la massa (0), per
		// i terminali liberi (-1), per i nodi senza riga in matrice e se non c'è ancora una soluzione
		double NodeVoltage(int nodeId) const;
		double TerminalVoltage(const Terminal &terminal) const { return NodeVoltage(terminal.GetNodeId()); }

	private:
		Circuit &m_circuit;
		Solver &m_solver;
		Eigen::VectorXd m_solution;
	};
}
