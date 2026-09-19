#include <memory>
#include <nlohmann/json.hpp>

#include "App/Application.h"
#include "Core/Solver.h"
#include "Components/Resistor.h"
#include "Components/VoltageGenerator.h"
#include "Components/Ground.h"
#include "Components/Capacitor.h"
#include "Components/Inductor.h"
#include "Components/Switch.h"
#include "Components/Diode.h"
#include "Common/SimulationOutput.h"
#include "UI/Ui.h"
#include "Common/Logger.h"

namespace {
	constexpr double TIMESTEP_VALUES[] = {
		1.0, 0.1, 0.01, 0.001,
		0.0001, 0.00001, 0.000001, 0.0000001, 0.00000001, 0.000000001, 0.0000000001, 0.00000000001, 0.000000000001,
	};
}

// Crea il componente appropriato in base al tipo richiesto.
// Il valore ha significato diverso a seconda del tipo:
//   - resistor:      valore in Ohm
//   - voltageSource: valore in Volt
//   - ground:        valore ignorato
//   - capacitor:     valore in Farad
//   - inductor:      valore in Henry
//   - switch:        chiuso di default
//   - diode:         Is = 1e-14 A, n = 1 (silicio generico)
std::unique_ptr<CircuitLab::Component> CircuitLab::Application::MakeComponent(ComponentType type)
{
	switch (type) {
	case ComponentType::resistor:			return std::make_unique<Resistor>(1000.0);
	case ComponentType::voltageGenerator:	return std::make_unique<VoltageGenerator>(WaveForm::Create(WaveFormType::dcWaveForm));
	case ComponentType::ground:				return std::make_unique<Ground>();
	case ComponentType::capacitor:			return std::make_unique<Capacitor>(0.000001);
	case ComponentType::inductor:			return std::make_unique<Inductor>(0.001);
	case ComponentType::switchComponent:	return std::make_unique<Switch>(true);
	case ComponentType::diode:				return std::make_unique<Diode>();
	default:								return nullptr;
	}
}

// Newton-Raphson per un singolo step temporale. Il punto di partenza è la
// soluzione dello step precedente (le tensioni cambiano poco da uno step
// all'altro, quindi di solito bastano 2-4 iterazioni); se la dimensione non
// coincide (circuito modificato dall'ultimo step) si riparte da zero.
std::optional<Eigen::VectorXd> CircuitLab::Application::SolveNonlinearStep(bool &converged)
{
	converged = false;

	const Eigen::MatrixXd &linearA = m_circuit->GetCircuitMatrix();
	const Eigen::VectorXd &linearB = m_circuit->GetCircuitVector();

	Eigen::VectorXd x = (m_simulationResult.size() == linearB.size())
		? m_simulationResult
		: Eigen::VectorXd::Zero(linearB.size());

	for (int iter = 0; iter < MAX_NEWTON_ITERATIONS; iter++)
	{
		Eigen::MatrixXd A = linearA;
		Eigen::VectorXd b = linearB;
		bool limited = m_circuit->StampNonlinear(A, b, x);

		// A cambia ad ogni iterazione: la fattorizzazione cachata da
		// Circuit::ComputeMatrix (matrice statica) non è più valida qui.
		m_solver->Factorize(A);
		auto next = m_solver->SolveCircuit(b);
		if (!next.has_value())
			return std::nullopt;

		x = *next;

		// Convergenza (come SPICE): nessun componente ha dovuto limitare la
		// tensione in questa iterazione (un valore limitato non è la vera
		// soluzione, solo un passo intermedio) E la corrente predetta dal modello
		// linearizzato coincide con quella reale nel nuovo punto. Non si confronta
		// invece x con l'iterazione precedente: per i nodi quasi isolati (tutti i
		// diodi spenti + un condensatore grande, con Geq = C/h enorme) la
		// soluzione lineare ha rumore di arrotondamento maggiore di qualunque
		// tolleranza ragionevole, e il ciclo non convergerebbe mai.
		if (!limited && m_circuit->NonlinearConverged(x))
		{
			converged = true;
			break;
		}
	}

	return x;
}

void CircuitLab::Application::SimulationLoop()
{
	while (m_isRunning)
	{
		if (m_simStatus == SimulationStatus::running)
		{
			int requiredSteps = std::max(1,
				static_cast<int>(BATCH_TARGET_TIME / m_hSim));
			int actualSteps = std::min(requiredSteps, MAX_STEPS_PER_BATCH);

			auto batchStart = std::chrono::steady_clock::now();

			// Esegui il batch — Simulate() solo calcola e scrive nel back buffer,
			// non swappa né notifica (lo fa questo metodo una sola volta, sotto,
			// a fine batch). Esce subito se Simulate() ferma la simulazione
			// (es. circuito non valido), invece di continuare a ricontrollare
			// per il resto del batch.
			for (int i = 0; i < actualSteps && m_simStatus == SimulationStatus::running; i++)
				Simulate();

			// Swap e notifica UNA SOLA VOLTA alla fine del batch
			{
				std::lock_guard<std::mutex> lock(m_swapMutex);
				std::swap(m_backIndex, m_frontIndex);
			}
			m_newOutputReady = true;

			auto batchEnd = std::chrono::steady_clock::now();
			double elapsed = std::chrono::duration<double>(
				batchEnd - batchStart).count();

			// Sleep basato sul tempo virtuale simulato
			double virtualTimeSimulated = actualSteps * m_hSim;
			double remaining = virtualTimeSimulated - elapsed;
			if (remaining > 0.0)
				std::this_thread::sleep_for(
					std::chrono::duration<double>(remaining));
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
CircuitLab::Application::Application() : m_simulationTime{ 0.0 }, m_hSim{ 0.001 }, m_windowTime{ 1.0 }, m_decimationFactor{ 1 }, m_sampleCounter{ 0 }
{
	Logger::GetInstance().SetMinLogLevel(LogLevel::Debug);
	Logger::GetInstance().SetLogToFile("circuitlab.log");

	m_ui = std::make_unique<UI>(1280, 720, "CircuitLab main window");
	m_circuit = std::make_unique<Circuit>();
	m_ioManager = std::make_unique<IOManager>();
	m_solver = std::make_unique<Solver>();
	m_circuit->SetTimestep(m_hSim);

	m_channelPalette = {
			{1.0f, 0.4f, 0.4f},  // rosso
			{0.4f, 1.0f, 0.4f},  // verde
			{0.4f, 0.6f, 1.0f},  // blu
			{1.0f, 1.0f, 0.4f},  // giallo
			{1.0f, 0.6f, 0.2f},  // arancio
			{0.8f, 0.4f, 1.0f},  // viola
	};

	m_simStatus = SimulationStatus::stopped;

	m_ui->SetOnSetSimulationStatus([this](SimulationStatus status)
		{
			return SetSimulationStatus(status);
		});

	m_ui->SetOnGetSimulationStatus([this]() -> SimulationStatus
		{
			return m_simStatus;
		});

	m_ui->SetOnCircuitChange([this](CircuitLab::ComponentType type) -> int
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->InvalidateCircuit();
			return m_circuit->AddComponent(MakeComponent(type));
		});

	m_ui->SetOnCreateLink([this](int compId1, int termIndex1, int compId2, int termIndex2) -> bool
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->InvalidateCircuit();
			// Propaga il risultato al chiamante: false indica un collegamento non valido o duplicato
			return m_circuit->ConnectTerminals(compId1, termIndex1, compId2, termIndex2);
		});

	m_ui->SetOnGetCompTerminalId([this](int compId)
		{
			// Chiede al circuito i nodeId dei terminali del componente,
			// usati dalla UI per costruire le etichette da visualizzare sul canvas
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			return m_circuit->GetNodesIdFromComponentId(compId);
		});

	m_ui->SetOnDeleteComponent([this](int compId)
		{
			// Rimuove il componente dal circuito e ricostruisce le connessioni rimaste
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->RemoveComponent(compId);
		});

	// Collega IOManager ad Application e UI tramite callback,
	// con la stessa logica usata per i callback della UI:
	// IOManager non conosce né Circuit né UI direttamente.

	// Crea un componente nel circuito durante il caricamento da file
	m_ioManager->SetOnComponentLoad([this](CircuitLab::ComponentType type) -> int
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->InvalidateCircuit();
			return m_circuit->AddComponent(MakeComponent(type));
		});

	m_ioManager->SetOnComponentLoadData([this](int compId, const nlohmann::json &j)
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->GetComponentById(compId)->Load(j);
		});

	// Collega due terminali nel circuito durante il caricamento da file
	m_ioManager->SetOnLoadLink([this](int compId1, int termIndex1, int compId2, int termIndex2) -> bool
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
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

	m_ioManager->SetOnBusLinkViewLoad([this](int sourceNodeViewId, int targetNodeViewId) -> int
		{
			return m_ui->AddBusLinkView(sourceNodeViewId, targetNodeViewId);
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
			std::lock_guard<std::mutex> lock(m_circuitMutex);
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
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			return m_circuit->GetComponentValues(compId);
		});

	m_ui->SetOnSetComponentValues([this](int compId, const std::map<ComponentValue, double> &values)
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->SetComponentValues(compId, values);
		});

	m_ui->SetOnToggleSwitch([this](int compId)
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->ToggleSwitch(compId);
		});

	m_ui->SetOnIsSwitchClosed([this](int compId) -> bool
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			return m_circuit->GetComponentById(compId)->IsSwitchClosed();
		});

	m_ui->SetOnGetComponentTypeById([this](int compId)->ComponentType
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			return m_circuit->GetComponentType(compId);
		});

	m_ui->SetOnGetComponentsByNodeId([this](int nodeId)->std::vector<int>
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			return m_circuit->GetComponentsByNodeId(nodeId);
		});

	m_ui->SetOnGetWaveFormType([this](int compId) -> WaveFormType
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			return m_circuit->GetComponentById(compId)->GetWaveFormType();
		});

	m_ui->SetOnSetWaveFormType([this](int compId, WaveFormType type)
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->InvalidateCircuit();
			m_circuit->GetComponentById(compId)->SetWaveFormType(type);
		});

	m_ui->SetOnGetOscilloscopeChannels([this]() -> std::vector<OscilloscopeChannel>
		{
			std::lock_guard<std::mutex> lock(m_channelsMutex);
			return m_channels;
		});

	m_ui->SetOnAddChannel([this](ProbeType type, int idA, int idB, int compId)
		{
			AddChannel(type, idA, idB, compId);
		});

	m_ui->SetOnSetChannelActive([this](int index, bool active)
		{
			std::lock_guard<std::mutex> lock(m_channelsMutex);
			if (index < static_cast<int>(m_channels.size()))
				m_channels[index].active = active;
		});

	m_ui->SetOnRemoveChannel([this](int index)
		{
			std::lock_guard<std::mutex> lock(m_channelsMutex);
			if (index < static_cast<int>(m_channels.size()))
				m_channels.erase(m_channels.begin() + index);
		});

	m_ui->SetOnGetHSim([this]() -> double
		{
			return m_hSim;
		});

	m_ui->SetOnSetHSim([this](int index)
		{
			m_hSim = TIMESTEP_VALUES[index];
			{
				std::lock_guard<std::mutex> lock(m_circuitMutex);
				m_circuit->SetTimestep(m_hSim);
			}
			UpdateDecimationFactor();
		});

	m_ui->SetOnSetWindowTime([this](double windowTime)
		{
			m_windowTime = windowTime;
			UpdateDecimationFactor();
		});

	m_ui->SetOnAutoSync([this]()
		{
			AutoSync();
		});

	m_ui->SetOnGetSimulationTime([this]() -> double
		{
			return m_simulationTime;
		});

	m_ui->SetOnGetDecimationFactor([this]()->int
		{
			return m_decimationFactor;
		});

	m_ui->SetOnGetMaxFrequency([this]()->double
		{
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			return m_circuit->GetMaxFrequency();
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

	// Protegge l'intero step contro modifiche concorrenti al circuito dal thread
	// di rendering (aggiunta/rimozione componenti, collegamenti, cambio valori...).
	std::lock_guard<std::mutex> lock(m_circuitMutex);

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

	if (m_circuit->HasFloatingTerminal())
	{
		// Circuito non valido (es. un componente è rimasto scollegato dopo
		// un edit): interrompe la simulazione invece di continuare a calcolare
		// risultati privi di senso. Il prossimo "Start" rifarà lo stesso controllo
		// e si fermerà di nuovo finché il circuito non viene ricollegato.
		output.simRes = SimulationResult::disconnected_terminal;
		m_simStatus = SimulationStatus::stopped;
		return;
	}

	StampContext ctx;
	ctx.t = m_simulationTime;
	ctx.h = m_hSim;

	// Risolve il sistema MNA; restituisce nullopt se la matrice è singolare
	m_circuit->ComputeMatrix();
	m_circuit->ComputeVector(ctx);
	std::optional<Eigen::VectorXd> result;
	if (m_circuit->HasNonlinearComponents())
	{
		bool converged = false;
		result = SolveNonlinearStep(converged);

		if (result.has_value() && !converged)
		{
			output.simRes = SimulationResult::no_convergence;
			return;
		}
	}
	else
		result = m_solver->SolveCircuit(m_circuit->GetCircuitVector());

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
			std::vector<int> termList = comp->GetTerminalNodeIds();
			// <= 0 copre sia il ground (0) sia un terminale mai collegato (-1):
			// in entrambi i casi non ha una riga nella matrice, quindi niente
			// GetIndexFromNodes (che altrimenti lancerebbe std::out_of_range).
			if (termList[0] <= 0)
				v1 = 0.0;
			else
				v1 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[0])];
			if (termList[1] <= 0)
				v2 = 0.0;
			else
				v2 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[1])];
			double current = (v1 - v2) / comp->GetValues().at(ComponentValue::resistance);
			componentCurrent[comp->GetId()] = current;
			branchCurrent[{termList[0], termList[1], comp->GetId()}] = current;
		}
		else if (comp->GetType() == ComponentType::switchComponent)
		{
			// Stesso schema del resistore: nessuna dipendenza dal tempo, la
			// corrente è semplicemente Geq*(v1-v2) (Geq alta se chiuso, quasi
			// zero se aperto). Senza questo blocco, output.currentComp/currentBranch
			// non avevano mai una voce per lo switch: i fili "ancorati" a lui
			// restavano sempre a corrente 0 (pallini fermi e gialli).
			std::vector<int> termList = comp->GetTerminalNodeIds();
			if (termList[0] <= 0)
				v1 = 0.0;
			else
				v1 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[0])];
			if (termList[1] <= 0)
				v2 = 0.0;
			else
				v2 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[1])];

			auto *sw = static_cast<Switch *>(comp.get());
			double current = sw->GetConductance() * (v1 - v2);
			componentCurrent[comp->GetId()] = current;
			branchCurrent[{termList[0], termList[1], comp->GetId()}] = current;
		}
		else if (comp->GetType() == ComponentType::capacitor)
		{
			std::vector<int> termList = comp->GetTerminalNodeIds();
			// Vedi commento nel ramo resistor: <= 0 copre ground (0) e terminale
			// mai collegato (-1), entrambi assenti da m_nodesMap.
			if (termList[0] <= 0)
				v1 = 0.0;
			else
				v1 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[0])];
			if (termList[1] <= 0)
				v2 = 0.0;
			else
				v2 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[1])];

			// i = C * dv/dt = Geq * ((v1-v2) - v(t-h)), con v(t-h) letto PRIMA
			// di aggiornare lo stato del condensatore per il prossimo step.
			auto *cap = static_cast<Capacitor *>(comp.get());
			double current = comp->GetValues().at(ComponentValue::capacitance) / m_hSim * ((v1 - v2) - cap->GetPreviousVoltage());
			componentCurrent[comp->GetId()] = current;
			branchCurrent[{termList[0], termList[1], comp->GetId()}] = current;

			cap->UpdateState(v1, v2);
		}
		else if (comp->GetType() == ComponentType::inductor)
		{
			std::vector<int> termList = comp->GetTerminalNodeIds();
			// Vedi commento nel ramo resistor: <= 0 copre ground (0) e terminale
			// mai collegato (-1), entrambi assenti da m_nodesMap.
			if (termList[0] <= 0)
				v1 = 0.0;
			else
				v1 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[0])];
			if (termList[1] <= 0)
				v2 = 0.0;
			else
				v2 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[1])];

			// i(t) = Geq*(v1-v2) + i(t-h), con i(t-h) letta PRIMA di aggiornare
			// lo stato dell'induttore per il prossimo step.
			auto *ind = static_cast<Inductor *>(comp.get());
			double current = ind->GetConductance() * (v1 - v2) + ind->GetPreviousCurrent();
			componentCurrent[comp->GetId()] = current;
			branchCurrent[{termList[0], termList[1], comp->GetId()}] = current;

			ind->UpdateState(v1, v2);
		}
		else if (comp->GetType() == ComponentType::diode)
		{
			std::vector<int> termList = comp->GetTerminalNodeIds();
			// Vedi commento nel ramo resistor: <= 0 copre ground (0) e terminale
			// mai collegato (-1), entrambi assenti da m_nodesMap.
			if (termList[0] <= 0)
				v1 = 0.0;
			else
				v1 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[0])];
			if (termList[1] <= 0)
				v2 = 0.0;
			else
				v2 = m_simulationResult[m_circuit->GetIndexFromNodes(termList[1])];

			// Corrente anodo->catodo dall'equazione di Shockley con la Vd
			// convergente (stesso verso positivo del resistore: terminale 0 -> 1).
			auto *diode = static_cast<Diode *>(comp.get());
			double current = diode->Current(v1 - v2);
			componentCurrent[comp->GetId()] = current;
			branchCurrent[{termList[0], termList[1], comp->GetId()}] = current;
		}
	}

	output.simRes = SimulationResult::success;
	output.res = outVec;
	output.currentComp = componentCurrent;
	output.currentBranch = branchCurrent;
	output.nodeVoltages = nodeToVoltage;

	m_simulationTime += m_hSim;

	m_sampleCounter++;
	if (m_sampleCounter >= m_decimationFactor)
	{
		m_sampleCounter = 0;
		SampleChannels(output);
	}

	// Nessuno swap qui: Simulate() scrive solo nel back buffer (m_buffers[m_backIndex]).
	// Ad ogni chiamata successiva nello stesso batch, sovrascrive lo stesso back buffer
	// (m_backIndex non cambia), finché SimulationLoop() non fa lo swap una sola volta
	// a fine batch, pubblicando così solo l'ultimo stato calcolato al thread di rendering.
}

void CircuitLab::Application::UpdateDecimationFactor()
{
	m_decimationFactor = std::max(1,
		static_cast<int>(std::ceil(m_windowTime /
			(OscilloscopeChannel::MAX_SAMPLES * m_hSim))));

	m_sampleCounter = 0;

	std::lock_guard<std::mutex> lock(m_channelsMutex);
	for (auto &channel : m_channels)
		channel.samples.clear();

	LOG_DEBUG("windowTime=" << m_windowTime
		<< " h_sim=" << m_hSim
		<< " decimationFactor=" << m_decimationFactor);
}

void CircuitLab::Application::AutoSync()
{
	double fMax;
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		fMax = m_circuit->GetMaxFrequency();
	}
	if (fMax <= 0.0)
		return;  // nessun generatore AC — nulla da sincronizzare

	m_windowTime = 4.0 / fMax;
	m_ui->SetWindowTime(m_windowTime);  // aggiorna il widget in UI
	UpdateDecimationFactor();
}

void CircuitLab::Application::SampleChannels(const SimulationOutput &output)
{
	std::lock_guard<std::mutex> lock(m_channelsMutex);
	for (auto &channel : m_channels)
	{
		if (!channel.active) continue;

		double value = 0.0;
		switch (channel.type)
		{
		case ProbeType::nodeVoltage:
			if (output.nodeVoltages.count(channel.idA))
				value = output.nodeVoltages.at(channel.idA);
			break;
		case ProbeType::differentialVoltage:
			if (output.nodeVoltages.count(channel.idA) &&
				output.nodeVoltages.count(channel.idB))
				value = output.nodeVoltages.at(channel.idA) -
				output.nodeVoltages.at(channel.idB);
			break;
		case ProbeType::componentCurrent:
			// Per questo probe l'id del componente è in channel.compId, non in
			// channel.idA (che AddChannel non valorizza mai per componentCurrent —
			// vedi UI::DrawOscilloscope, dove per questo tipo viene passato solo compId).
			if (output.currentComp.count(channel.compId))
				value = output.currentComp.at(channel.compId);
			break;
		case ProbeType::branchCurrent:
		{
			auto key = std::make_tuple(channel.idA, channel.idB, channel.compId);
			if (output.currentBranch.count(key))
				value = output.currentBranch.at(key);
			break;
		}
		}

		channel.samples.push_back(value);
		if (static_cast<int>(channel.samples.size()) > OscilloscopeChannel::MAX_SAMPLES)
			channel.samples.pop_front();
	}
}

void CircuitLab::Application::SetSimulationStatus(SimulationStatus status)
{
	m_simStatus = status;
	if (status == SimulationStatus::running)
		m_ui->CreateLinkParticlesList();
}

void CircuitLab::Application::AddChannel(ProbeType type, int idA, int idB, int compId)
{
	std::string label;
	OscilloscopeChannel channel;

	switch (type)
	{
	case ProbeType::nodeVoltage:          label = "V(" + std::to_string(idA) + ")"; break;
	case ProbeType::differentialVoltage:  label = "V(" + std::to_string(idA) + "," + std::to_string(idB) + ")"; break;
	case ProbeType::componentCurrent:
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		label = "I(" + Component::ComponentTypeName(m_circuit->GetComponentType(compId)) + std::to_string(compId) + ")";
		break;
	}
	case ProbeType::branchCurrent:        label = "I(" + std::to_string(idA) + "," + std::to_string(idB) + ")"; break;
	}

	channel.type = type;
	channel.idA = idA;
	channel.idB = idB;
	channel.compId = compId;
	channel.label = label;
	channel.channelColor = m_channelPalette[(m_nextChannelColorIndex) % m_channelPalette.size()];
	m_nextChannelColorIndex++;

	std::lock_guard<std::mutex> lock(m_channelsMutex);
	m_channels.push_back(std::move(channel));
}

void CircuitLab::Application::New()
{
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		m_circuit->Clear();
	}
	m_ui->Clear();
}

// Delega il loop principale alla UI
void CircuitLab::Application::Run()
{
	std::thread simThread(&Application::SimulationLoop, this);
	RenderLoop();
	simThread.join();
}