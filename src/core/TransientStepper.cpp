#include <optional>

#include "Core/TransientStepper.h"
#include "Core/NonlinearSolve.h"
#include "Components/Transformer.h"

CircuitLab::TransientStepper::TransientStepper(Circuit &circuit, Solver &solver) : m_circuit(circuit), m_solver(solver)
{
	m_circuit.SetOnFactorize([&solver](const Eigen::MatrixXd &matrix)
		{
			solver.Factorize(matrix);
		});
}

CircuitLab::TransientStepper::Status CircuitLab::TransientStepper::Solve(double time, double timestep)
{
	StampContext context;
	context.t = time;
	context.h = timestep;

	// Se il circuito è cambiato ricostruisce e fattorizza la matrice (vedi il costruttore)
	m_circuit.ComputeMatrix();
	m_circuit.ComputeVector(context);

	std::optional<Eigen::VectorXd> result;
	if (m_circuit.HasNonlinearComponents())
	{
		bool converged = false;
		result = SolveNonlinearStep(m_circuit, m_solver, m_solution, converged);

		if (result.has_value() && !converged)
			return Status::noConvergence;
	}
	else
		result = m_solver.SolveCircuit(m_circuit.GetCircuitVector());

	if (!result.has_value())
		return Status::singular;

	m_solution = std::move(*result);
	return Status::ok;
}

void CircuitLab::TransientStepper::CommitState()
{
	for (const auto &component : m_circuit.GetComponentsVector())
	{
		const std::vector<Terminal> &terminals = component->GetTerminals();
		switch (component->GetType())
		{
		case ComponentType::capacitor:
		case ComponentType::inductor:
			component->UpdateState(TerminalVoltage(terminals[0]), TerminalVoltage(terminals[1]));
			break;
		case ComponentType::transformer:
			// 4 terminali: 0/1 primario, 2/3 secondario
			static_cast<Transformer *>(component.get())->UpdateWindingState(
				TerminalVoltage(terminals[0]) - TerminalVoltage(terminals[1]),
				TerminalVoltage(terminals[2]) - TerminalVoltage(terminals[3]));
			break;
		default:
			break;
		}
	}
}

CircuitLab::TransientStepper::Status CircuitLab::TransientStepper::Step(double time, double timestep)
{
	const Status status = Solve(time, timestep);
	if (status == Status::ok)
		CommitState();
	return status;
}

bool CircuitLab::TransientStepper::StartFromDcOperatingPoint()
{
	bool converged = false;
	const auto result = SolveOperatingPoint(m_circuit, m_solver, converged);
	if (!result.has_value() || !converged)
		return false;

	m_solution = *result;
	return true;
}

double CircuitLab::TransientStepper::NodeVoltage(int nodeId) const
{
	int index;
	if (nodeId <= 0 || !m_circuit.TryGetIndexFromNodes(nodeId, index) || index >= m_solution.size())
		return 0.0;
	return m_solution[index];
}
