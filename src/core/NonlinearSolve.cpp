#include <algorithm>
#include <random>

#include "Core/NonlinearSolve.h"

namespace {

	using namespace CircuitLab;

	// Una passata di Newton-Raphson da start, con le sorgenti lineari scalate di
	// sourceScale (1 = valori veri). Il criterio di convergenza è come in SPICE:
	// nessun componente ha dovuto limitare la tensione in questa iterazione (un
	// valore limitato non è la vera soluzione, solo un passo intermedio) E la
	// corrente predetta dal modello linearizzato coincide con quella reale nel
	// nuovo punto. Non si confronta invece x con l'iterazione precedente: per i nodi
	// quasi isolati (tutti i diodi spenti + un condensatore grande, con Geq = C/h
	// enorme) la soluzione lineare ha rumore di arrotondamento maggiore di qualunque
	// tolleranza ragionevole, e il ciclo non convergerebbe mai.
	struct Pass { bool converged; bool singular; Eigen::VectorXd x; };

	Pass Newton(Circuit &circuit, Solver &solver, Eigen::VectorXd x, bool adaptive,
		int maxIterations, double sourceScale)
	{
		const Eigen::MatrixXd &linearA = circuit.GetCircuitMatrix();
		const Eigen::VectorXd &linearB = circuit.GetCircuitVector();

		// prevDelta è l'ultimo spostamento EFFETTIVAMENTE applicato (già scalato per lo
		// smorzamento corrente), per confrontarne la direzione con quello proposto
		// alla prossima iterazione.
		Eigen::VectorXd prevDelta;
		bool havePrevDelta = false;
		double damping = 1.0;

		for (int iter = 0; iter < maxIterations; iter++)
		{
			Eigen::MatrixXd A = linearA;
			Eigen::VectorXd b = (sourceScale == 1.0) ? linearB : Eigen::VectorXd(sourceScale * linearB);
			bool limited = circuit.StampNonlinear(A, b, x);

			// A cambia ad ogni iterazione: la fattorizzazione cachata da
			// Circuit::ComputeMatrix (matrice statica) non è più valida qui.
			solver.Factorize(A);
			auto next = solver.SolveCircuit(b);
			if (!next.has_value())
				return { false, true, x };

			Eigen::VectorXd delta = *next - x;
			if (adaptive)
			{
				if (havePrevDelta)
				{
					if (delta.dot(prevDelta) < 0.0)
						damping = std::max(NonlinearSolve::DAMPING_MIN, damping * NonlinearSolve::DAMPING_SHRINK);
					else
						damping = std::min(1.0, damping * NonlinearSolve::DAMPING_GROW);
				}
				delta *= damping;
				prevDelta = delta;
				havePrevDelta = true;
			}
			x += delta;

			if (!limited && circuit.NonlinearConverged(x))
				return { true, false, x };
		}

		return { false, false, x };
	}

}

std::optional<Eigen::VectorXd> CircuitLab::SolveOperatingPoint(Circuit &circuit, Solver &solver, bool &converged)
{
	converged = false;

	circuit.SetDcAnalysis(true);
	circuit.ComputeMatrix();
	circuit.ComputeVector(StampContext{});

	std::optional<Eigen::VectorXd> result;
	if (circuit.HasNonlinearComponents())
		result = SolveNonlinearStep(circuit, solver, Eigen::VectorXd(), converged);
	else
	{
		// Lineare: una sola soluzione. Si fattorizza qui (il callback di Circuit, se c'è, ha
		// già fattorizzato la stessa matrice, ma non va dato per scontato).
		solver.Factorize(circuit.GetCircuitMatrix());
		result = solver.SolveCircuit(circuit.GetCircuitVector());
		converged = result.has_value();
	}

	if (result.has_value() && converged)
		circuit.ApplyDcState(*result);

	circuit.SetDcAnalysis(false);
	return result;
}

std::optional<Eigen::VectorXd> CircuitLab::SolveNonlinearStep(Circuit &circuit, Solver &solver,
	const Eigen::VectorXd &warmStart, bool &converged)
{
	using namespace NonlinearSolve;

	converged = false;
	const Eigen::Index n = circuit.GetCircuitVector().size();
	const Eigen::VectorXd start = (warmStart.size() == n) ? warmStart : Eigen::VectorXd::Zero(n);

	// Una matrice singolare in un tentativo non è definitiva (può esserlo solo per
	// quella traiettoria): si passa al successivo, e si segnala nullopt solo se
	// nessuno dei tentativi è riuscito e l'ultimo si è fermato per questo.
	bool singular = false;
	auto attempt = [&](const Pass &pass) -> std::optional<Eigen::VectorXd>
	{
		singular = pass.singular;
		if (pass.converged)
		{
			converged = true;
			return pass.x;
		}
		return std::nullopt;
	};

	Pass last = Newton(circuit, solver, start, false, FAST_ITERATIONS, 1.0);
	if (auto r = attempt(last))
		return r;

	last = Newton(circuit, solver, start, true, DAMPED_ITERATIONS, 1.0);
	if (auto r = attempt(last))
		return r;

	const Eigen::VectorXd zero = Eigen::VectorXd::Zero(n);
	last = Newton(circuit, solver, zero, false, COLD_ITERATIONS, 1.0);
	if (auto r = attempt(last))
		return r;

	// Partenze casuali (generatore con seme fisso: il risultato è riproducibile).
	// Dove la soluzione precedente non esiste più, la nuova è spesso raggiungibile
	// da molte partenze ma non dalle due già provate; nel trigger di Schmitt
	// verificato il 70% delle partenze casuali converge, mentre né zero né il
	// source stepping ci arrivavano. L'intervallo copre le tensioni plausibili
	// del circuito: da poco sotto zero a poco sopra la tensione più grande vista
	// nella soluzione precedente o fra le sorgenti.
	const double span = std::max({ 1.0, start.lpNorm<Eigen::Infinity>(), circuit.GetCircuitVector().lpNorm<Eigen::Infinity>() });
	std::mt19937 rng(12345);
	std::uniform_real_distribution<double> pick(-0.2 * span, 1.2 * span);
	for (int k = 0; k < RANDOM_RESTARTS; k++)
	{
		Eigen::VectorXd random(n);
		for (Eigen::Index i = 0; i < n; i++)
			random[i] = pick(rng);

		last = Newton(circuit, solver, random, false, COLD_ITERATIONS, 1.0);
		if (auto r = attempt(last))
			return r;
	}

	Eigen::VectorXd x = zero;
	for (int k = 1; k <= SOURCE_STEPS; k++)
	{
		last = Newton(circuit, solver, x, true, DAMPED_ITERATIONS, static_cast<double>(k) / SOURCE_STEPS);
		singular = last.singular;
		x = last.x;
		if (!last.converged)
			break;
		if (k == SOURCE_STEPS)
		{
			converged = true;
			return x;
		}
	}

	if (singular)
		return std::nullopt;
	return x;
}
