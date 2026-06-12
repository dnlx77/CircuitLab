#include <memory>
#include <iostream>

#include "App/Application.h"
#include "Core/Solver.h"
#include "Components/Resistor.h"
#include "Components/VoltageSource.h"
#include "Components/Ground.h"
#include "Common/SimulationOutput.h"
#include "UI/Ui.h"
#include "Common/Logger.h"

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

void CircuitLab::Application::SimulationLoop()
{
	while (m_ui->IsWindowOpen())
	{
		sf::Time elapsed = m_deltaClock.getElapsedTime();
		if (elapsed >= sf::seconds(SIMULATION_STEP) && m_simStatus == SimulationStatus::running)
		{
			m_deltaClock.restart();
			Simulate();
		}
	}
}

void CircuitLab::Application::RenderLoop()
{
	while (m_ui->IsWindowOpen())
	{
		if (m_newOutputReady)
		{
			SimulationOutput localOutput;
			{
				std::lock_guard<std::mutex> lock(m_swapMutex);
				localOutput = m_buffers[m_frontIndex];
			}

			m_ui->UpdateSimulation(localOutput);
			m_ui->CreateLinkViewCurrentList();
			m_newOutputReady = false;

		}

		m_ui->HandleEvents();

		m_ui->Render();
	}
	m_isRunning = false;
}

// Costruisce UI e Circuit, poi collega i cinque callback:
//   - onRunSimulation:     la UI chiama RunSimulation() su Application
//   - onCircuitChange:     la UI chiede ad Application di aggiungere un componente al circuito
//   - onCreateLink:        la UI chiede ad Application di collegare due terminali
//   - onGetCompTerminalId: la UI chiede i nodeId dei terminali di un componente
//   - onDeleteComponent:   la UI chiede ad Application di rimuovere un componente
CircuitLab::Application::Application()
{
	Logger::GetInstance().SetMinLogLevel(LogLevel::Debug);
	Logger::GetInstance().SetLogToFile("circuitlab.log");

	m_ui = std::make_unique<UI>(1280, 720, "CircuitLab main window");
	m_circuit = std::make_unique<Circuit>();
	m_ioManager = std::make_unique<IOManager>();
	m_solver = std::make_unique<Solver>();

	m_simStatus = SimulationStatus::stopped;

	m_ui->SetOnSetSimulationStatus([this](SimulationStatus status)
		{
			return SetSimulationStatus(status);
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

	// Collega IOManager ad Application e UI tramite callback,
	// con la stessa logica usata per i callback della UI:
	// IOManager non conosce né Circuit né UI direttamente.

	// Crea un componente nel circuito durante il caricamento da file
	m_ioManager->SetOnComponentLoad([this](CircuitLab::ComponentType type, double value) -> int
		{
			m_circuit->InvalidateCircuit();
			return m_circuit->AddComponent(MakeComponent(type, value));
		});

	// Collega due terminali nel circuito durante il caricamento da file
	m_ioManager->SetOnLoadLink([this](int compId1, int termIndex1, int compId2, int termIndex2) -> bool
		{
			m_circuit->InvalidateCircuit();
			// Propaga il risultato al chiamante: false indica un collegamento non valido o duplicato
			return m_circuit->ConnectTerminals(compId1, termIndex1, compId2, termIndex2);
		});

	// Aggiunge la vista grafica di un componente alla UI durante il caricamento
	m_ioManager->SetOnComponentViewLoad([this](int compId, const std::string &name, ComponentType type, Vec2 position, float rotation)
		{
			m_ui->AddViewComponent(compId, name, type, position, rotation);
		});

	// Aggiunge la vista grafica di un filo alla UI durante il caricamento
	m_ioManager->SetOnLinkViewLoad([this](int comp1, int term1, int NodeViewId) -> int
		{
			return m_ui->AddViewLink(comp1, term1, NodeViewId);
		});

	m_ioManager->SetOnNodeViewLoad([this](int nodeId, sf::Vector2f position) -> int 
		{
			return m_ui->AddNodeView(nodeId, position);
		});

	m_ioManager->SetOnUpdateNodeViewLinkIds([this](int nodeViewId, std::vector<int> linkViewIds)
		{
			m_ui->UpdateNodeViewLinkIds(nodeViewId, linkViewIds);
		});

	m_ui->SetOnSave([this](const std::string &path)
		{
			m_ioManager->SaveToFile(path, *m_circuit, m_ui->GetComponentsViewList(), m_ui->GetLinkVIewList(), m_ui->GetNodeViewList());
		});

	m_ui->SetOnLoad([this](const std::string &path)
		{
			m_ioManager->LoadFromFile(path);
		});

	m_ui->SetOnNew([this]() 
		{
			New();
		});
	
	// Resetta circuito e UI prima di caricare un nuovo file
	m_ioManager->SetOnNew([this]()
		{
			New();
		});

	m_ui->SetOnGetComponentValues([this](int compId) -> std::map<ComponentValue, double>
		{
			return m_circuit->GetComponentValues(compId);
		});

	m_ui->SetOnSetComponentValues([this](int compId, const std::map<ComponentValue, double> &values) 
		{
			m_circuit->SetComponentValues(compId, values);
		});

	m_ui->SetOnGetComponentTypeById([this](int compId)->ComponentType 
		{
			return m_circuit->GetComponentType(compId);
		});

	m_ui->SetOnGetComponentsByNodeId([this](int nodeId)->std::vector<int>
		{
			return m_circuit->GetComponentsByNodeId(nodeId);
		});

	m_circuit->SetOnFactorize([this](const Eigen::MatrixXd &matrix) 
		{
			m_solver->Factorize(matrix);
		});
}

CircuitLab::Application::~Application() = default;

// Esegue la simulazione MNA sull'attuale stato del circuito.
// Controlla prima i casi degeneri (circuito nullo o vuoto),
// poi risolve il sistema A*x = b e costruisce il vettore di output
// con i nomi delle variabili (tensioni Vn e correnti nei rami).
void CircuitLab::Application::Simulate()
{
	// LOG
	//m_circuit->PrintCircuit();

	SimulationOutput &output = m_buffers[m_backIndex];
	output = SimulationOutput{};

	// Questi controlli sono difensivi: m_circuit non dovrebbe mai essere nullptr
	// dato che viene creato nel costruttore, ma è buona pratica verificarlo
	if (m_circuit == nullptr)
	{
		output.simRes = SimulationResult::no_circuit;
		return;
	}

	if (m_circuit->IsCircuitEmpty())
	{
		output.simRes = SimulationResult::empty_circuit;
		return;
	}

	if (m_circuit->CircuitHasOnlyGround())
	{
		output.simRes = SimulationResult::only_ground_circuit;
		return;
	}

	// Risolve il sistema MNA; restituisce nullopt se la matrice è singolare
	auto result = m_solver->SolveCircuit(m_circuit->GetCircuitVector());

	if (!result.has_value())
	{
		output.simRes = SimulationResult::solve_error;
		return;
	}

	m_simulationResult = result.value();

	// Costruisce il vettore di output associando ogni indice della soluzione
	// al nome della variabile corrispondente:
	//   - se l'indice corrisponde a un nodo -> "Vn" (tensione al nodo n)
	//   - se l'indice corrisponde a una sorgente -> "I(Vn_m)" (corrente nella sorgente tra nodi n e m)
	std::vector<std::pair<std::string, double>> outVec;
	std::unordered_map<int, double> componentCurrent;
	std::map<std::tuple<int, int, int>, double> branchCurrent;
	std::unordered_map<int, double> nodeToVoltage;
	nodeToVoltage[0] = 0.0;
	for (int i = 0; i < m_simulationResult.size(); i++)
	{
		int vNode = m_circuit->GetNodesFromIndex(i);
		int iNode = m_circuit->GetCurrentFromIndex(i);

		if (vNode != -1)
		{
			outVec.emplace_back("V" + std::to_string(vNode), m_simulationResult[i]);
			nodeToVoltage[vNode] = m_simulationResult[i];
		}

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
			componentCurrent[iNode] = m_simulationResult[i];
			branchCurrent[{terminalsId[0], terminalsId[1], iNode}] = m_simulationResult[i];
		}
	}
		
	const std::vector<std::unique_ptr<Component>> &compList = m_circuit->GetComponentsVector();
	double v1, v2;
	for (auto &comp : compList)
	{
		if (comp->GetType() == ComponentType::resistor)
		{
			std::vector<int> termList = comp->GetTerminalId();
			if (termList[0] == 0)
				v1 = 0.0;
			else 
				v1 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[0])];
			if (termList[1] == 0)
				v2 = 0.0;
			else
				v2 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[1])];
			double current = (v1 - v2) / comp->GetValues().at(ComponentValue::resistance);
			componentCurrent[comp->GetId()] = current;
			branchCurrent[{termList[0], termList[1], comp->GetId()}] = current;
		}
	}

	output.simRes = SimulationResult::success;
	output.res = outVec;
	output.currentComp = componentCurrent;
	output.currentBranch = branchCurrent;
	output.nodeVoltages = nodeToVoltage;

	{
		std::lock_guard<std::mutex> lock(m_swapMutex);
		std::swap(m_backIndex, m_frontIndex);
	}
	m_newOutputReady = true;
}

void CircuitLab::Application::SetSimulationStatus(SimulationStatus status)
{
	m_simStatus = status;
	if (status == SimulationStatus::running)
		m_ui->CreateLinkParticlesList();
}

void CircuitLab::Application::New()
{
	m_circuit->Clear();
	m_ui->Clear();
}

// Delega il loop principale alla UI
void CircuitLab::Application::Run()
{
	std::thread simThread(&Application::SimulationLoop, this);
	RenderLoop();
	simThread.join();
}