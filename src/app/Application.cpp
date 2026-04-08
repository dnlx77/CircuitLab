#include <memory>

#include <iostream>

#include "App/Application.h"
#include "Core/Solver.h"
#include "Components/Resistor.h"
#include "Components/VoltageSource.h"
#include "Components/Ground.h"

std::unique_ptr<CircuitLab::Component> CircuitLab::Application::MakeComponent(ComponentType type, double value)
{
	switch (type) {
	case ComponentType::resistor:
		return std::make_unique<Resistor>(value);
	case ComponentType::voltageSource:
		return std::make_unique<VoltageSource>(value);
	case ComponentType::ground:
		return std::make_unique<Ground>();
	default:
		return nullptr;
	}
}

CircuitLab::Application::Application()
{
	m_ui = std::make_unique<UI>(800, 600, "CircuitLab main window");
	m_circuit = std::make_unique<Circuit>();

	m_ui->SetOnRunSimulation([this]() -> Eigen::VectorXd { RunSimulation(); return m_simulationResult; });
	m_ui->SetOnCircuitChange([this](CircuitLab::ComponentType type, double res) -> int 
		{ int id = m_circuit->AddComponent(MakeComponent(type, res)); 
	return id;
	});
	m_ui->SetOnCreateLink([this](int compId1, int termIndex1, int compId2, int termIndex2) { m_circuit->ConnectTerminals(compId1, termIndex1, compId2, termIndex2); });
}

CircuitLab::SimulationResult CircuitLab::Application::RunSimulation()
{
	// DA CANCELLARE
	std::cout << "Circuito: " << std::endl;
	m_circuit->PrintCircuit();

	if (m_circuit == nullptr)
		return SimulationResult::no_circuit;
	if (m_circuit->IsCircuitEmpty())
		return SimulationResult::empty_circuit;
	
	auto result = Solver::SolveCircuit(m_circuit->GetCircuitMatrix(), m_circuit->GetCircuitVector());
	
	if (!result.has_value())
		return SimulationResult::solve_error;
	
	m_simulationResult = result.value();

	// DA CANCELLARE
	std::cout << "Soluzione: " << m_simulationResult << std::endl;
	
	
	return SimulationResult::success;
}

void CircuitLab::Application::Run()
{
	m_ui->Run();
}
