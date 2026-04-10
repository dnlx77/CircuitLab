#include <memory>

#include <iostream>

#include "App/Application.h"
#include "Core/Solver.h"
#include "Components/Resistor.h"
#include "Components/VoltageSource.h"
#include "Components/Ground.h"
#include "Common/SimulationOutput.h"
#include "UI/Ui.h"

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

	m_ui->SetOnRunSimulation([this]() { return RunSimulation(); });
	m_ui->SetOnCircuitChange([this](CircuitLab::ComponentType type, double res) -> int 
		{ int id = m_circuit->AddComponent(MakeComponent(type, res)); 
	return id;
	});
	m_ui->SetOnCreateLink([this](int compId1, int termIndex1, int compId2, int termIndex2) { m_circuit->ConnectTerminals(compId1, termIndex1, compId2, termIndex2); });
}

CircuitLab::Application::~Application() = default;

CircuitLab::SimulationOutput CircuitLab::Application::RunSimulation()
{
	// DA CANCELLARE
	std::cout << "Circuito: " << std::endl;
	m_circuit->PrintCircuit();

	SimulationOutput output;

	if (m_circuit == nullptr)
	{
		output.simRes = SimulationResult::no_circuit;
		return output;
	}
	if (m_circuit->IsCircuitEmpty())
	{
		output.simRes = SimulationResult::empty_circuit;
		return output;
	}

	auto result = Solver::SolveCircuit(m_circuit->GetCircuitMatrix(), m_circuit->GetCircuitVector());
	
	if (!result.has_value())
	{
		output.simRes = SimulationResult::solve_error;
		return output;
	}

	m_simulationResult = result.value();
	
	std::vector<std::pair<std::string, double>> outVec;
	for (int i = 0; i < m_simulationResult.size(); i++)
	{
		std::pair<std::string, double> p;
		int vNode = m_circuit->GetNodesFromIndex(i);
		int iNode = m_circuit->GetCurrentFromIndex(i);
		if (vNode != -1)
		{
			std::string str = "V" + std::to_string(vNode);
			p.first = str;
			p.second = m_simulationResult[i];
			outVec.emplace_back(p);
		}
		if (iNode != -1)
		{
			std::string str = "Ig" + std::to_string(iNode);
			p.first = str;
			p.second = m_simulationResult[i];
			outVec.emplace_back(p);
		}
	}

	output.simRes = SimulationResult::success;
	output.res = outVec;
	return output;
}

void CircuitLab::Application::Run()
{
	m_ui->Run();
}
