#include <memory>
#include <iostream>

#include "App/Application.h"
#include "Core/Solver.h"
#include "Components/Resistor.h"
#include "Components/VoltageSource.h"
#include "Components/Ground.h"
#include "Common/SimulationOutput.h"
#include "UI/Ui.h"

// Crea il componente appropriato in base al tipo richiesto.
// Il valore ha significato diverso a seconda del tipo:
//   - resistor:      valore in Ohm
//   - voltageSource: valore in Volt
//   - ground:        valore ignorato
std::unique_ptr<CircuitLab::Component> CircuitLab::Application::MakeComponent(ComponentType type, double value)
{
	switch (type) {
	case ComponentType::resistor:      return std::make_unique<Resistor>(value);
	case ComponentType::voltageSource: return std::make_unique<VoltageSource>(value);
	case ComponentType::ground:        return std::make_unique<Ground>();
	default:                           return nullptr;
	}
}

// Costruisce UI e Circuit, poi collega i cinque callback:
//   - onRunSimulation:     la UI chiama RunSimulation() su Application
//   - onCircuitChange:     la UI chiede ad Application di aggiungere un componente al circuito
//   - onCreateLink:        la UI chiede ad Application di collegare due terminali
//   - onGetCompTerminalId: la UI chiede i nodeId dei terminali di un componente
//   - onDeleteComponent:   la UI chiede ad Application di rimuovere un componente
CircuitLab::Application::Application()
{
	m_ui = std::make_unique<UI>(800, 600, "CircuitLab main window");
	m_circuit = std::make_unique<Circuit>();

	m_ui->SetOnRunSimulation([this]()
		{
			return RunSimulation();
		});

	m_ui->SetOnCircuitChange([this](CircuitLab::ComponentType type, double value) -> int
		{
			m_circuit->InvalidateCircuit();
			return m_circuit->AddComponent(MakeComponent(type, value));
		});

	m_ui->SetOnCreateLink([this](int compId1, int termIndex1, int compId2, int termIndex2) -> bool
		{
			m_circuit->InvalidateCircuit();
			// Propaga il risultato al chiamante: false indica un collegamento non valido o duplicato
			return m_circuit->ConnectTerminals(compId1, termIndex1, compId2, termIndex2);
		});

	m_ui->SetOnGetCompTerminalId([this](int compId)
		{
			// Chiede al circuito i nodeId dei terminali del componente,
			// usati dalla UI per costruire le etichette da visualizzare sul canvas
			return m_circuit->GetNodesIdFromComponentId(compId);
		});

	m_ui->SetOnDeleteComponent([this](int compId)
		{
			// Rimuove il componente dal circuito e ricostruisce le connessioni rimaste
			m_circuit->RemoveComponent(compId);
		});
}

CircuitLab::Application::~Application() = default;

// Esegue la simulazione MNA sull'attuale stato del circuito.
// Controlla prima i casi degeneri (circuito nullo o vuoto),
// poi risolve il sistema A*x = b e costruisce il vettore di output
// con i nomi delle variabili (tensioni Vn e correnti nei rami).
CircuitLab::SimulationOutput CircuitLab::Application::RunSimulation()
{
	// DA CANCELLARE
	std::cout << "Circuito: " << std::endl;
	m_circuit->PrintCircuit();

	SimulationOutput output;

	// Questi controlli sono difensivi: m_circuit non dovrebbe mai essere nullptr
	// dato che viene creato nel costruttore, ma è buona pratica verificarlo
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

	if (m_circuit->CircuitHasOnlyGround())
	{
		output.simRes = SimulationResult::only_ground_circuit;
		return output;
	}

	// Risolve il sistema MNA; restituisce nullopt se la matrice è singolare
	auto result = Solver::SolveCircuit(m_circuit->GetCircuitMatrix(), m_circuit->GetCircuitVector());

	if (!result.has_value())
	{
		output.simRes = SimulationResult::solve_error;
		return output;
	}

	m_simulationResult = result.value();

	// Costruisce il vettore di output associando ogni indice della soluzione
	// al nome della variabile corrispondente:
	//   - se l'indice corrisponde a un nodo -> "Vn" (tensione al nodo n)
	//   - se l'indice corrisponde a una sorgente -> "I(Vn_m)" (corrente nella sorgente tra nodi n e m)
	std::vector<std::pair<std::string, double>> outVec;
	for (int i = 0; i < m_simulationResult.size(); i++)
	{
		int vNode = m_circuit->GetNodesFromIndex(i);
		int iNode = m_circuit->GetCurrentFromIndex(i);

		if (vNode != -1)
			outVec.emplace_back("V" + std::to_string(vNode), m_simulationResult[i]);

		if (iNode != -1)
		{
			// Costruisce la stringa "I(Vn_m)" usando i nodeId dei terminali della sorgente
			std::vector<int> terminalsId = m_circuit->GetNodesIdFromComponentId(iNode);
			std::string compString;
			for (int j = 0; j < terminalsId.size(); j++)
			{
				compString += std::to_string(terminalsId[j]);
				if (j < terminalsId.size() - 1)
					compString += "_";
			}
			outVec.emplace_back("I(V" + compString + ")", m_simulationResult[i]);
		}
	}

	output.simRes = SimulationResult::success;
	output.res = outVec;
	return output;
}

// Delega il loop principale alla UI
void CircuitLab::Application::Run()
{
	m_ui->Run();
}