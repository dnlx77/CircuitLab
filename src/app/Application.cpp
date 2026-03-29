#include "App/Application.h"
#include "Core/Solver.h"

CircuitLab::SimulationResult CircuitLab::Application::RunSimulation()
{
	if (m_circuit == nullptr)
		return SimulationResult::no_circuit;
	if (m_circuit->IsCircuitEmpty())
		return SimulationResult::empty_circuit;
	
	auto result = Solver::SolveCircuit(m_circuit->GetCircuitMatrix(), m_circuit->GetCircuitVector());
	
	if (!result.has_value())
		return SimulationResult::solve_error;
	
	m_simulationResult = result.value();
	return SimulationResult::success;
}
