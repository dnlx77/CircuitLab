#include <memory>
#include <nlohmann/json.hpp>
#include <SFML/System/Sleep.hpp>

#include "App/Application.h"
#include "Core/Solver.h"
#include "Components/Resistor.h"
#include "Components/VoltageGenerator.h"
#include "Components/Ground.h"
#include "Components/Capacitor.h"
#include "Components/Inductor.h"
#include "Components/Switch.h"
#include "Components/Diode.h"
#include "Components/Transformer.h"
#include "Components/ChangeoverSwitch.h"
#include "Components/Transistor.h"
#include "Common/SimulationOutput.h"
#include "UI/Ui.h"
#include "Common/Logger.h"

namespace {
	constexpr double TIMESTEP_VALUES[] = {
		1.0, 0.1, 0.01, 0.001,
		0.0001, 0.00001, 0.000001, 0.0000001, 0.00000001, 0.000000001, 0.0000000001, 0.00000000001, 0.000000000001,
	};

	// Moltiplicatore della velocità con cui il tempo simulato avanza rispetto
	// al tempo reale (vedi SimulationLoop): 1.0 = tempo reale (il ritmo di
	// default, utile per leggere l'oscilloscopio come uno strumento vero).
	// L'ultimo valore (<= 0) è un segnale di "velocità massima": nessuna
	// pausa tra un batch e l'altro, il tempo simulato avanza alla velocità
	// con cui la CPU riesce a calcolare — utile per raggiungere in fretta un
	// transitorio lungo (es. l'assestamento di un filtro passa-alto RC con
	// costante di tempo di alcuni secondi) senza aspettare in tempo reale.
	constexpr double SPEED_VALUES[] = { 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, -1.0 };
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
//   - transformer:   L1 = L2 = 1 mH, k = 0.999 (accoppiamento 1:1 quasi ideale)
//   - changeoverSwitch: comune sulla via 1 di default
//   - transistor:    Is = 1e-14 A, BF = 100 (NPN generico)
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
	case ComponentType::transformer:		return std::make_unique<Transformer>();
	case ComponentType::changeoverSwitch:	return std::make_unique<ChangeoverSwitch>(false);
	case ComponentType::transistor:		return std::make_unique<Transistor>();
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

	// Stato dello smorzamento adattivo (vedi NEWTON_DAMPING_MIN/SHRINK/GROW):
	// prevDelta è l'ultimo spostamento EFFETTIVAMENTE applicato (già scalato
	// per lo smorzamento corrente), per confrontarne la direzione con quello
	// proposto alla prossima iterazione.
	Eigen::VectorXd prevDelta;
	bool havePrevDelta = false;
	double damping = 1.0;

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

		// Smorzamento adattivo: se lo spostamento proposto va nella direzione
		// OPPOSTA al precedente (prodotto scalare negativo — il segno di
		// un'oscillazione), lo si riduce; se è coerente, ci si rilassa verso il
		// passo pieno. Parte a passo pieno, quindi non rallenta i casi già ben
		// comportati (vedi il commento su NEWTON_DAMPING_MIN in Application.h).
		Eigen::VectorXd delta = *next - x;
		if (havePrevDelta)
		{
			if (delta.dot(prevDelta) < 0.0)
				damping = std::max(NEWTON_DAMPING_MIN, damping * NEWTON_DAMPING_SHRINK);
			else
				damping = std::min(1.0, damping * NEWTON_DAMPING_GROW);
		}
		x += damping * delta;
		prevDelta = damping * delta;
		havePrevDelta = true;

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
	using Clock = std::chrono::steady_clock;

	// Pacing ad "accumulatore": invece di dormire dopo ogni batch il tempo che
	// quel batch "dovrebbe" durare (su Windows sleep ha una granularità di
	// ~15 ms, quindi le pause brevi duravano molto più del previsto e a 1x si
	// arrivava solo a ~0.6x), si fissa un'ancora (istante reale, tempo
	// simulato) e ad ogni giro si calcola quanto tempo simulato è "dovuto"
	// rispetto all'ancora: un ritardo accumulato viene recuperato al giro
	// dopo, invece di perdersi.
	bool anchored = false;
	Clock::time_point anchorWall;
	double anchorSim = 0.0;
	double anchorSpeed = 0.0;

	while (m_isRunning)
	{
		if (m_simStatus != SimulationStatus::running)
		{
			anchored = false;
			sf::sleep(sf::milliseconds(10));
			continue;
		}

		const double speed = m_simSpeed;
		const double h = m_hSim;
		const auto now = Clock::now();

		// Si riancora alla partenza, quando cambia la velocità e quando il tempo
		// simulato viene azzerato da fuori (Load, New, cambio timestep...).
		if (!anchored || speed != anchorSpeed || m_simulationTime < anchorSim)
		{
			anchorWall = now;
			anchorSim = m_simulationTime;
			anchorSpeed = speed;
			anchored = true;
		}

		int steps = MAX_STEPS_PER_BATCH;
		if (speed > 0.0)
		{
			double wall = std::chrono::duration<double>(now - anchorWall).count();
			double owed = anchorSim + wall * speed - m_simulationTime;

			// Se il calcolo non riesce a stare al passo con la velocità chiesta, il
			// debito crescerebbe senza limite e, appena il circuito diventa più
			// leggero, la simulazione "correrebbe" per recuperarlo. Oltre
			// MAX_PACING_LAG secondi reali di ritardo lo si lascia perdere.
			if (owed > speed * MAX_PACING_LAG)
			{
				anchorWall = now;
				anchorSim = m_simulationTime;
				owed = 0.0;
			}
			steps = std::min(MAX_STEPS_PER_BATCH, static_cast<int>(owed / h));
		}

		if (steps > 0)
		{
			// Il mutex del circuito è tenuto per tutto il batch (non per singolo
			// step): meno lock/unlock, e soprattutto il thread di rendering non
			// deve "rubarlo" fra uno step e l'altro — col lock per step, su un mutex
			// non equo, a velocità massima il rendering restava quasi sempre a
			// bocca asciutta (interfaccia a ~2 FPS). Il batch è limitato in tempo
			// reale (MAX_BATCH_WALL_TIME) e seguito sempre da una pausa, in cui
			// il rendering trova il mutex libero.
			// Solo l'ultimo step del batch costruisce l'output completo per il
			// rendering (vedi SimulateLocked). Se il batch si interrompe prima (es.
			// pausa dall'interfaccia) il back buffer resta incompleto e non si
			// pubblica: il rendering continua a mostrare l'ultimo output completo.
			const auto batchStart = Clock::now();
			bool outputComplete = false;
			{
				std::lock_guard<std::mutex> lock(m_circuitMutex);
				for (int i = 0; i < steps; i++)
				{
					const bool last = i == steps - 1 || ((i & 63) == 63 &&
						std::chrono::duration<double>(Clock::now() - batchStart).count() > MAX_BATCH_WALL_TIME);
					outputComplete = SimulateLocked(last);
					if (last || m_simStatus != SimulationStatus::running)
						break;
				}
			}

			// Swap e notifica UNA SOLA VOLTA alla fine del batch
			if (outputComplete)
			{
				{
					std::lock_guard<std::mutex> lock(m_swapMutex);
					std::swap(m_backIndex, m_frontIndex);
				}
				m_newOutputReady = true;
			}
		}

		// sf::sleep (non std::this_thread::sleep_for): su Windows alza la
		// risoluzione del timer di sistema per la durata della pausa, così 1 ms
		// dura davvero ~1 ms e non ~15.
		sf::sleep(sf::milliseconds(1));
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

	// Colori saturi e ben distinguibili tra loro (anche per chi ha difficoltà con
	// rosso/verde) su sfondo scuro; ordinati in modo che i primi canali, i più
	// usati, siano i più diversi tra loro.
	m_channelPalette = {
			{1.00f, 0.87f, 0.20f},  // giallo
			{0.25f, 0.85f, 1.00f},  // ciano
			{1.00f, 0.40f, 0.85f},  // magenta
			{0.45f, 1.00f, 0.45f},  // verde
			{1.00f, 0.60f, 0.20f},  // arancio
			{0.70f, 0.60f, 1.00f},  // lavanda
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

	m_ui->SetOnFreeTerminal([this](int compId, int termIndex)
		{
			// La UI ha tolto il filo di questo terminale: lo lascia libero anche nel circuito
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->FreeTerminal(compId, termIndex);
		});

	m_ui->SetOnDetachFromGround([this](const std::vector<std::pair<int, int>> &terminals)
		{
			// La UI ha cancellato un Ground: questi terminali restano uniti tra loro
			// ma non sono più a massa
			std::lock_guard<std::mutex> lock(m_circuitMutex);
			m_circuit->DetachFromGround(terminals);
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

	m_ioManager->SetOnNodeViewLoad([this](int nodeId, sf::Vector2f position, bool manual, int anchorCompId, int anchorTermIndex, bool attached) -> int
		{
			return m_ui->AddNodeView(nodeId, position, manual, anchorCompId, anchorTermIndex, attached);
		});

	m_ioManager->SetOnConvertLegacyNodeViews([this]()
		{
			m_ui->ConvertLegacyNodeViews();
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
			// Il caricamento non è un'unica operazione atomica sul circuito:
			// IOManager chiama New() e poi ricostruisce componenti e collegamenti
			// uno alla volta, ognuno con il proprio lock su m_circuitMutex (non
			// uno solo per tutto il caricamento — vedi IOManager::Deserialize).
			// Se la simulazione è in corso, il thread di simulazione può
			// intrufolarsi tra una chiamata e l'altra e vedere il circuito a
			// metà ricostruzione (es. terminali ancora scollegati subito dopo
			// Clear()): Simulate() lo rileva come HasFloatingTerminal() e ferma
			// silenziosamente la simulazione (m_simStatus = stopped), lasciando
			// il pannello dei risultati bloccato sull'ultimo valore calcolato
			// prima del blocco. Per evitarlo si ferma la simulazione PRIMA di
			// iniziare il caricamento — aspettando, riprendendo brevemente
			// m_circuitMutex, che un passo eventualmente già in corso finisca —
			// e si ripristina lo stato precedente solo a caricamento completato.
			SimulationStatus previousStatus = m_simStatus;
			m_simStatus = SimulationStatus::stopped;
			{
				std::lock_guard<std::mutex> lock(m_circuitMutex);
			}

			m_ioManager->LoadFromFile(path);

			// A differenza di Undo/Redo (che devono ripristinare lo stato ESATTO
			// di un istante fa, dinamica compresa — vedi Undo/Redo più sotto, che
			// non passano da qui ma chiamano Deserialize direttamente), caricare
			// un file da disco è concettualmente iniziare un esperimento pulito
			// con quella topologia: si azzera lo stato dinamico appena caricato
			// (tensione dei condensatori, punto di linearizzazione di
			// diodo/transistor...), anche se il file lo conteneva (es. perché
			// salvato mentre la simulazione era in corso, o da un'altra sessione).
			{
				std::lock_guard<std::mutex> lock(m_circuitMutex);
				m_circuit->ResetDynamicState();
			}

			// Calcola subito un punto di funzionamento del circuito appena
			// caricato, invece di lasciare il pannello Risultato/Correnti con
			// l'output di PRIMA del caricamento — o, alla primissima apertura
			// dell'app (nessuna simulazione mai partita), con un
			// SimulationOutput mai scritto (che UI::DrawImageGuiPanel legge
			// comunque, mostrando un risultato "vuoto" invece che i valori
			// veri). Un solo passo, pubblicato come farebbe normalmente un
			// batch (swap + notifica): RenderLoop lo raccoglie al giro
			// successivo, quasi istantaneo. Il tempo virtuale avanzato da
			// questo passo viene poi azzerato per non sfalsare l'inizio di
			// una simulazione continua successiva.
			Simulate();
			{
				std::lock_guard<std::mutex> lock(m_swapMutex);
				std::swap(m_backIndex, m_frontIndex);
			}
			m_newOutputReady = true;
			m_simulationTime = 0.0;

			m_simStatus = previousStatus;
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

	m_ui->SetOnPushUndoSnapshot([this]() { PushUndoSnapshot(); });
	m_ui->SetOnUndo([this]() { Undo(); });
	m_ui->SetOnRedo([this]() { Redo(); });
	m_ui->SetOnCanUndo([this]() -> bool { return CanUndo(); });
	m_ui->SetOnCanRedo([this]() -> bool { return CanRedo(); });

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

				// Un timestep molto diverso da quello con cui lo stato dinamico
				// attuale (tensione dei condensatori, punto di linearizzazione del
				// transistor/diodo...) è stato calcolato può produrre, per un solo
				// step, un risultato numericamente estremo ma "legittimo" secondo
				// le equazioni di QUEL passo — che poi resta lì per molti step
				// anche tornando al timestep originale (il modello companion parte
				// sempre da dove l'ultimo step lo ha lasciato). Si azzera qui,
				// come un mini "New" che non tocca però topologia o valori.
				m_circuit->ResetDynamicState();
				m_simulationResult = Eigen::VectorXd();

				// Vedi il commento su m_simulationTime in ClearState(): con un
				// timestep enorme anche solo qualche step fa crescere parecchio il
				// tempo simulato, e sin(2*pi*f*t) perde precisione per t grande —
				// tornare a un timestep piccolo non basta se il "tempo" stesso è
				// rimasto quello. Si riparte da t=0 ad ogni cambio di timestep.
				m_simulationTime = 0.0;
			}
			UpdateDecimationFactor();
		});

	m_ui->SetOnSetSimSpeed([this](int index)
		{
			m_simSpeed = SPEED_VALUES[index];
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
	// Protegge l'intero step contro modifiche concorrenti al circuito dal thread
	// di rendering (aggiunta/rimozione componenti, collegamenti, cambio valori...).
	std::lock_guard<std::mutex> lock(m_circuitMutex);
	SimulateLocked(true);
}

bool CircuitLab::Application::SimulateLocked(bool publish)
{
	SimulationOutput &output = m_buffers[m_backIndex];
	output = SimulationOutput{};

	// Questi controlli sono difensivi: m_circuit non dovrebbe mai essere nullptr
	// dato che viene creato nel costruttore, ma è buona pratica verificarlo
	if (m_circuit == nullptr)
	{
		output.simRes = SimulationResult::no_circuit;
		return true;
	}

	// I controlli di validità dipendono solo dalla topologia: si rifanno solo
	// quando il circuito è cambiato (dirty), non ad ogni step.
	if (m_circuit->IsDirty() || !m_topologyChecked)
	{
		if (m_circuit->IsCircuitEmpty())
			m_topologyCheck = SimulationResult::empty_circuit;
		else if (m_circuit->CircuitHasOnlyGround())
			m_topologyCheck = SimulationResult::only_ground_circuit;
		else if (m_circuit->HasFloatingTerminal())
			m_topologyCheck = SimulationResult::disconnected_terminal;
		else
			m_topologyCheck = SimulationResult::success;
		m_topologyChecked = true;
	}

	if (m_topologyCheck != SimulationResult::success)
	{
		output.simRes = m_topologyCheck;
		// Circuito non valido (es. un componente è rimasto scollegato dopo
		// un edit): interrompe la simulazione invece di continuare a calcolare
		// risultati privi di senso. Il prossimo "Start" rifarà lo stesso controllo
		// e si fermerà di nuovo finché il circuito non viene ricollegato.
		if (m_topologyCheck == SimulationResult::disconnected_terminal)
			m_simStatus = SimulationStatus::stopped;
		return true;
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
			return true;
		}
	}
	else
		result = m_solver->SolveCircuit(m_circuit->GetCircuitVector());

	if (!result.has_value())
	{
		output.simRes = SimulationResult::solve_error;
		return true;
	}

	m_simulationResult = result.value();

	// L'output completo (nomi, mappe delle correnti) serve solo allo step che
	// verrà pubblicato al rendering e a quelli campionati dall'oscilloscopio:
	// costruirlo ad ogni step era una parte rilevante del costo per step. Lo
	// stato dei componenti (condensatori, induttori...) si aggiorna invece sempre.
	m_sampleCounter++;
	const bool sampleNow = m_sampleCounter >= m_decimationFactor;
	const bool needOutput = publish || sampleNow;

	// Costruisce il vettore di output associando ogni indice della soluzione
	// al nome della variabile corrispondente:
	//   - se l'indice corrisponde a un nodo -> "Vn" (tensione al nodo n)
	//   - se l'indice corrisponde a una sorgente -> "I(Vn_m)" (corrente nella sorgente tra nodi n e m)
	std::vector<std::pair<std::string, double>> outVec;
	std::unordered_map<int, double> componentCurrent;
	std::map<std::tuple<int, int, int>, double> branchCurrent;
	std::unordered_map<int, double> nodeToVoltage;
	nodeToVoltage[0] = 0.0;
	for (int i = 0; needOutput && i < m_simulationResult.size(); i++)
	{
		int vNode = m_circuit->GetNodesFromIndex(i);
		int iNode = m_circuit->GetCurrentFromIndex(i);

		if (vNode != -1)
		{
			if (publish)
				outVec.emplace_back("V" + std::to_string(vNode), m_simulationResult[i]);
			nodeToVoltage[vNode] = m_simulationResult[i];
		}

		if (iNode != -1)
		{
			// Costruisce la stringa "I(Vn_m)" usando i nodeId dei terminali della sorgente
			std::vector<int> terminalsId = m_circuit->GetNodesIdFromComponentId(iNode);
			if (publish)
			{
				std::string compString;
				for (int j = 0; j < terminalsId.size(); j++)
				{
					compString += std::to_string(terminalsId[j]);
					if (j < terminalsId.size() - 1)
						compString += "_";
				}
				outVec.emplace_back("I(V" + compString + ")", m_simulationResult[i]);
			}
			componentCurrent[iNode] = m_simulationResult[i];
			branchCurrent[{terminalsId[0], terminalsId[1], iNode}] = m_simulationResult[i];
		}
	}

	const std::vector<std::unique_ptr<Component>> &compList = m_circuit->GetComponentsVector();
	double v1, v2;
	for (auto &comp : compList)
	{
		// Senza output da costruire servono solo i componenti con stato da
		// aggiornare a fine step (condensatore, induttore, trasformatore)
		const ComponentType type = comp->GetType();
		if (!needOutput && type != ComponentType::capacitor
			&& type != ComponentType::inductor && type != ComponentType::transformer)
			continue;

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
		else if (comp->GetType() == ComponentType::transformer)
		{
			// 4 terminali: 0/1 = primario, 2/3 = secondario, ciascuna coppia con
			// la propria corrente indipendente (vedi Transformer.h). componentCurrent
			// tiene solo la corrente di primario (per il probe "corrente di
			// componente" dell'oscilloscopio, che ha un solo valore per id);
			// branchCurrent ha invece una voce per ciascun avvolgimento.
			std::vector<int> termList = comp->GetTerminalNodeIds();
			auto voltageAt = [&](int idx) -> double
			{
				if (termList[idx] <= 0)
					return 0.0;
				return m_simulationResult[m_circuit->GetIndexFromNodes(termList[idx])];
			};
			double vPrimary = voltageAt(0) - voltageAt(1);
			double vSecondary = voltageAt(2) - voltageAt(3);

			// Correnti alla soluzione convergente, lette PRIMA di aggiornare lo
			// stato (stesso schema di condensatore/induttore).
			auto *transformer = static_cast<Transformer *>(comp.get());
			auto [primaryCurrent, secondaryCurrent] = transformer->BranchCurrents(vPrimary, vSecondary);
			componentCurrent[comp->GetId()] = primaryCurrent;
			branchCurrent[{termList[0], termList[1], comp->GetId()}] = primaryCurrent;
			branchCurrent[{termList[2], termList[3], comp->GetId()}] = secondaryCurrent;

			transformer->UpdateWindingState(vPrimary, vSecondary);
		}
		else if (comp->GetType() == ComponentType::changeoverSwitch)
		{
			// 3 terminali: 0 = comune, 1/2 = vie. Stesso schema dello switch (Geq*(v1-v2),
			// nessuna dipendenza dal tempo) applicato a due rami che condividono il
			// comune. componentCurrent tiene la corrente totale al comune (somma dei
			// due rami: quello inattivo vale ~0, quindi equivale alla corrente della
			// via attiva); branchCurrent ha una voce per ciascuna via.
			std::vector<int> termList = comp->GetTerminalNodeIds();
			auto voltageAt = [&](int idx) -> double
			{
				if (termList[idx] <= 0)
					return 0.0;
				return m_simulationResult[m_circuit->GetIndexFromNodes(termList[idx])];
			};
			double vCommon = voltageAt(0);
			double vVia1 = voltageAt(1);
			double vVia2 = voltageAt(2);

			auto *devSwitch = static_cast<ChangeoverSwitch *>(comp.get());
			double currentVia1 = devSwitch->GetConductanceVia1() * (vCommon - vVia1);
			double currentVia2 = devSwitch->GetConductanceVia2() * (vCommon - vVia2);
			componentCurrent[comp->GetId()] = currentVia1 + currentVia2;
			branchCurrent[{termList[0], termList[1], comp->GetId()}] = currentVia1;
			branchCurrent[{termList[0], termList[2], comp->GetId()}] = currentVia2;
		}
		else if (comp->GetType() == ComponentType::transistor)
		{
			// 3 terminali: 0 = base, 1 = collettore, 2 = emettitore, ciascuno con
			// una corrente PROPRIA (Transistor::Currents) — a differenza del
			// deviatore, qui non c'è un terminale "comune" che accumula le altre:
			// le tre correnti sono indipendenti, quindi non si prestano alla
			// convenzione a coppie (nodoA, nodoB) usata per i rami degli altri
			// componenti. Si usa perciò un secondo campo sentinella (-1000-indice,
			// mai un nodeId valido) solo per rendere unica la chiave per terminale;
			// il ramo dedicato in UI::CreateLinkViewCurrentList lo sa e la
			// interroga così, invece di passare dal percorso generico a coppie.
			// componentCurrent tiene la corrente di collettore (il probe più
			// comune per un transistor nell'oscilloscopio).
			std::vector<int> termList = comp->GetTerminalNodeIds();
			auto voltageAt = [&](int idx) -> double
			{
				if (termList[idx] <= 0)
					return 0.0;
				return m_simulationResult[m_circuit->GetIndexFromNodes(termList[idx])];
			};
			double vBase = voltageAt(0);
			double vColl = voltageAt(1);
			double vEmit = voltageAt(2);

			auto *transistor = static_cast<Transistor *>(comp.get());
			auto currents = transistor->Currents(vBase - vEmit, vBase - vColl);
			componentCurrent[comp->GetId()] = currents.ic;
			branchCurrent[{termList[0], -1000, comp->GetId()}] = currents.ib;
			branchCurrent[{termList[1], -1001, comp->GetId()}] = currents.ic;
			branchCurrent[{termList[2], -1002, comp->GetId()}] = currents.ie;
		}
	}

	output.simRes = SimulationResult::success;
	output.res = std::move(outVec);
	output.currentComp = std::move(componentCurrent);
	output.currentBranch = std::move(branchCurrent);
	output.nodeVoltages = std::move(nodeToVoltage);

	m_simulationTime += m_hSim;

	if (sampleNow)
	{
		m_sampleCounter = 0;
		SampleChannels(output);
	}

	// Nessuno swap qui: Simulate() scrive solo nel back buffer (m_buffers[m_backIndex]).
	// Ad ogni chiamata successiva nello stesso batch, sovrascrive lo stesso back buffer
	// (m_backIndex non cambia), finché SimulationLoop() non fa lo swap una sola volta
	// a fine batch, pubblicando così solo l'ultimo stato calcolato al thread di rendering.
	return publish;
}

void CircuitLab::Application::UpdateDecimationFactor()
{
	// Il trigger dell'oscilloscopio (UI::DrawOscilloscope) allinea la finestra al
	// periodo, quindi può finire fino a un periodo prima dell'ultimo campione: i
	// campioni tenuti devono coprire la finestra più un periodo, o il bordo
	// sinistro resterebbe vuoto.
	double fMax;
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		fMax = m_circuit->GetMaxFrequency();
	}
	const double span = m_windowTime + (fMax > 0.0 ? 1.0 / fMax : 0.0);
	m_decimationFactor = std::max(1,
		static_cast<int>(std::ceil(span /
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
		// Simulate() ha già avanzato m_simulationTime di h: il campione appena
		// aggiunto appartiene allo step risolto a (m_simulationTime - h).
		channel.lastSampleTime = m_simulationTime - m_hSim;
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
	// Nomi nello stesso stile del canvas: nodi "N2", componenti "R6" (prefisso di
	// tipo + id), così il canale si riconosce a colpo d'occhio nel circuito.
	case ProbeType::nodeVoltage:          label = "V(N" + std::to_string(idA) + ")"; break;
	case ProbeType::differentialVoltage:  label = "V(N" + std::to_string(idA) + "-N" + std::to_string(idB) + ")"; break;
	case ProbeType::componentCurrent:
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		label = "I(" + Component::ComponentTypeName(m_circuit->GetComponentType(compId)) + std::to_string(compId) + ")";
		break;
	}
	case ProbeType::branchCurrent:
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		label = "I(" + Component::ComponentTypeName(m_circuit->GetComponentType(compId)) + std::to_string(compId) +
			": N" + std::to_string(idA) + "-N" + std::to_string(idB) + ")";
		break;
	}
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

void CircuitLab::Application::ClearState()
{
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		m_circuit->Clear();

		// SolveNonlinearStep riusa m_simulationResult come punto di partenza di
		// Newton quando la sua dimensione combacia con quella del nuovo sistema
		// (vedi lì) — un caso frequente qui, perché un annulla/ripeti ricostruisce
		// tipicamente la STESSA topologia (stesso numero di nodi/sorgenti) di
		// prima. Senza azzerarlo, Newton ripartirebbe da tensioni di un istante
		// prima dell'annulla mentre ogni componente con memoria (condensatori,
		// induttori, il transistor) è già stato ricostruito da zero: un punto di
		// partenza incoerente che può far fallire la convergenza, specialmente
		// se il "vecchio" x è molto lontano dal nuovo punto di lavoro (es. si è
		// nel frattempo azzerato un generatore). Azzerarlo forza il prossimo
		// step a ripartire da x=0, sempre coerente con lo stato appena ricostruito.
		m_simulationResult = Eigen::VectorXd();

		// m_simulationTime non veniva MAI azzerato qui (solo nel costruttore):
		// cresceva per tutta la durata della sessione dell'app, anche attraverso
		// New/Load/annulla/ripeti. Con un timestep enorme (es. 1 s) bastano
		// pochi step per farlo salire di parecchio; tornare a un timestep
		// piccolo non lo riporta indietro, e l'argomento sempre più grande
		// passato a sin() (vedi VoltageGenerator, per le forme d'onda AC) perde
		// precisione fino a produrre valori praticamente casuali da uno step
		// all'altro — esattamente il "si rompe e non si aggiusta più, serve
		// riavviare l'app" segnalato. Si azzera qui, così anche New/Load lo
		// riportano a un punto noto.
		m_simulationTime = 0.0;
	}
	m_ui->Clear();

	// I canali dell'oscilloscopio puntano a id di componenti/nodi del circuito
	// appena azzerato (vedi SampleChannels: idA/idB/compId). Un nuovo circuito
	// (o lo stesso ricostruito da un annulla/ripeti) riparte con id da 1, quasi
	// certamente diversi: senza svuotare i canali restano in lista puntando ad
	// "altro" — non a un crash (SampleChannels controlla .count() prima di
	// leggere, quindi mostrano solo una traccia piatta a zero), ma a voci
	// del circuito precedente che continuano a comparire nell'oscilloscopio.
	{
		std::lock_guard<std::mutex> lock(m_channelsMutex);
		m_channels.clear();
		m_nextChannelColorIndex = 0;
	}
}

void CircuitLab::Application::New()
{
	ClearState();

	// Un documento nuovo (o un altro file caricato, che passa da qui via
	// m_onNew) non ha più senso rispetto alla cronologia di annulla/ripeti del
	// documento precedente.
	m_undoStack.clear();
	m_redoStack.clear();
}

void CircuitLab::Application::PushUndoSnapshot()
{
	nlohmann::json snapshot;
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		snapshot = m_ioManager->Serialize(*m_circuit, m_ui->GetComponentsViewList(), m_ui->GetLinkVIewList(), m_ui->GetNodeViewList());
	}

	m_undoStack.push_back(std::move(snapshot));
	if (m_undoStack.size() > MAX_UNDO_STEPS)
		m_undoStack.pop_front();

	// Un nuovo gesto rende non ripetibili le modifiche annullate finora, come
	// in qualunque editor: si annulla, si cambia idea e si fa qualcos'altro, i
	// vecchi "ripeti" non hanno più senso (porterebbero a uno stato che non
	// segue più da quello attuale).
	m_redoStack.clear();
}

void CircuitLab::Application::Undo()
{
	if (m_undoStack.empty())
		return;

	nlohmann::json current;
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		current = m_ioManager->Serialize(*m_circuit, m_ui->GetComponentsViewList(), m_ui->GetLinkVIewList(), m_ui->GetNodeViewList());
	}
	m_redoStack.push_back(std::move(current));
	if (m_redoStack.size() > MAX_UNDO_STEPS)
		m_redoStack.pop_front();

	nlohmann::json snapshot = std::move(m_undoStack.back());
	m_undoStack.pop_back();

	// ClearState(), non New(): quest'ultimo svuoterebbe anche le due pile che
	// si sta proprio maneggiando.
	ClearState();
	m_ioManager->Deserialize(snapshot);
}

void CircuitLab::Application::Redo()
{
	if (m_redoStack.empty())
		return;

	nlohmann::json current;
	{
		std::lock_guard<std::mutex> lock(m_circuitMutex);
		current = m_ioManager->Serialize(*m_circuit, m_ui->GetComponentsViewList(), m_ui->GetLinkVIewList(), m_ui->GetNodeViewList());
	}
	m_undoStack.push_back(std::move(current));
	if (m_undoStack.size() > MAX_UNDO_STEPS)
		m_undoStack.pop_front();

	nlohmann::json snapshot = std::move(m_redoStack.back());
	m_redoStack.pop_back();

	ClearState();
	m_ioManager->Deserialize(snapshot);
}

// Delega il loop principale alla UI
void CircuitLab::Application::Run()
{
	std::thread simThread(&Application::SimulationLoop, this);
	RenderLoop();
	simThread.join();
}