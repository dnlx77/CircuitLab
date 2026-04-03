#include <memory>
#include "App/Application.h"
#include "Core/Solver.h"
#include "Components/Resistor.h"
#include "Components/VoltageSource.h"

std::unique_ptr<CircuitLab::Component> CircuitLab::Application::MakeComponent(ComponentType type, double value)
{
	switch (type) {
	case ComponentType::resistor:
			return std::make_unique<Resistor>(value);
	default:
		return nullptr;
	}
}

CircuitLab::Application::Application()
{
	m_ui = std::make_unique<UI>(800, 600, "CircuitLab main window");
	m_circuit = std::make_unique<Circuit>();

	m_ui->SetOnRunSimulation([this]() { RunSimulation(); });
	m_ui->SetOnCircuitChange([this](CircuitLab::ComponentType type, const Vec2 &, float, const std::string &, double res) -> int 
		{ int id = m_circuit->AddComponent(MakeComponent(type, res)); 
	return id;
	});
}

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

void CircuitLab::Application::Run()
{
	m_ui->Run();
}
