#pragma once
#include <memory>
#include <mutex>
#include <deque>
#include <nlohmann/json.hpp>
#include "Core/Circuit.h"
#include "Core/Solver.h"
#include "Common/ComponentType.h"
#include "Common/SimulationOutput.h"
#include "IO/IOManager.h"
#include "Common/OscilloscopeChannel.h"

namespace CircuitLab {

	// Forward declaration per evitare inclusione circolare tra Application e UI
	class UI;

	// Classe Mediator principale dell'applicazione.
	// Coordina la comunicazione tra il circuito (logica di simulazione)
	// e l'interfaccia grafica (UI), senza che i due si conoscano direttamente.
	// Possiede in esclusiva sia il circuito che la UI tramite unique_ptr.
	class Application {
	private:
		std::unique_ptr<Circuit> m_circuit;         // Il circuito elettrico
		std::unique_ptr<UI> m_ui;                   // L'interfaccia grafica
		std::unique_ptr<Solver> m_solver;
		Eigen::VectorXd m_simulationResult;         // Ultimo vettore soluzione MNA
		std::unique_ptr<IOManager> m_ioManager;		// Gestisce salvataggio e caricamento su file JSON
		sf::Clock m_deltaClock;
		std::atomic<double> m_simulationTime;
		double m_hSim;
		// Moltiplicatore della velocità del tempo simulato (vedi SPEED_VALUES
		// in Application.cpp e SimulationLoop). Atomico: scritto dal thread di
		// rendering (SetOnSetSimSpeed), letto dal thread di simulazione.
		std::atomic<double> m_simSpeed{ 1.0 };
		double m_windowTime;
		int m_decimationFactor;
		int m_sampleCounter;
		std::atomic<SimulationStatus> m_simStatus;
		std::atomic<bool> m_newOutputReady = false;
		std::atomic<bool> m_isRunning = true;
		SimulationOutput m_buffers[2];
		int m_backIndex = 0;
		int m_frontIndex = 1;
		std::mutex m_swapMutex, m_channelsMutex;
		// Protegge m_circuit: è letto/modificato sia dal thread di simulazione
		// (SimulationLoop -> Simulate) sia dal thread di rendering (callback della UI
		// che aggiungono/rimuovono componenti, collegano terminali, cambiano valori...).
		// Senza questo mutex, un'edit del circuito durante una simulazione in corso
		// può correre in parallelo con ComputeMatrix/ComputeVector sullo stesso Circuit,
		// causando iteratori invalidati o un m_isDirty "perso" (la modifica sembra
		// non essere mai stata rilevata).
		std::mutex m_circuitMutex;

		std::vector<OscilloscopeChannel> m_channels;
		std::vector<Color> m_channelPalette;
		int m_nextChannelColorIndex = 0;

		// Undo/redo a snapshot: ogni voce è l'intero stato (componenti, link,
		// viste) serializzato da IOManager::Serialize, lo stesso formato usato
		// per salvare su file. Più semplice e robusto di un comando per ogni tipo
		// di modifica (aggiungi, sposta, ruota, collega, cancella, modifica
		// valore...): un solo punto di cattura/ripristino per tutti, a costo di
		// più memoria per singolo passo — trascurabile per circuiti di queste
		// dimensioni. UI chiama PushUndoSnapshot() PRIMA di ogni gesto utente che
		// muta lo stato (vedi UI::m_onPushUndoSnapshot), mai dopo: è lì che serve
		// lo stato PRECEDENTE alla modifica.
		std::deque<nlohmann::json> m_undoStack;
		std::deque<nlohmann::json> m_redoStack;
		static constexpr size_t MAX_UNDO_STEPS = 100;

		static constexpr int MAX_STEPS_PER_BATCH = 5000;     // anti-spirale della morte
		static constexpr double MAX_BATCH_WALL_TIME = 0.008; // s reali max per batch (il rendering aspetta al più questo)
		static constexpr double MAX_PACING_LAG = 0.25;       // s reali di ritardo oltre cui il debito si abbandona

		// Newton-Raphson per i circuiti con componenti non lineari (es. diodo)
		// (il criterio di convergenza è del singolo componente: Component::HasConverged)
		static constexpr int MAX_NEWTON_ITERATIONS = 50;

		// Smorzamento ADATTIVO del passo di Newton (vedi SolveNonlinearStep):
		// parte a passo pieno e si riduce solo quando rileva un'oscillazione,
		// per poi rilassarsi di nuovo. Un componente a più giunzioni accoppiate
		// (il transistor: base-emettitore e base-collettore si influenzano a
		// vicenda tramite gm/go, vedi Transistor::StampNonlinear) può restare in
		// un ciclo limite — il collettore oscilla fra due valori vicini senza
		// mai stabilizzarsi — proprio nella stretta zona di transizione
		// accensione/interdizione, dove pnjlim (LimitVoltage) non interviene
		// perché nessuna delle due tensioni supera mai la sua soglia.
		// Uno smorzamento FISSO non basta: abbastanza forte da rompere
		// quell'oscillazione (serviva <= 0.2, verificato sperimentalmente) è
		// anche troppo lento per i casi ben comportati (un caso che convergeva
		// in 7 iterazioni a passo pieno ne richiedeva più di 150 a passo fisso
		// 0.2, oltre MAX_NEWTON_ITERATIONS). Lo smorzamento adattivo invece
		// resta a passo pieno finché la direzione dello spostamento è coerente
		// con l'iterazione precedente, e lo riduce SOLO quando la direzione si
		// inverte (il segno dell'oscillazione) — verificato su 60000 step
		// dell'esatto caso che oscillava: zero fallimenti, nessun rallentamento
		// per i casi semplici.
		static constexpr double NEWTON_DAMPING_MIN = 0.1;
		static constexpr double NEWTON_DAMPING_SHRINK = 0.5; // fattore di riduzione quando la direzione si inverte
		static constexpr double NEWTON_DAMPING_GROW = 1.5;   // fattore di recupero verso il passo pieno

		// Factory method: crea il componente corretto in base al tipo richiesto dalla UI,
		// con valori di default (es. resistenza 1kΩ, generatore DC 0V) — non prende un
		// valore esplicito: l'utente lo imposta dopo dalla UI.
		std::unique_ptr<Component> MakeComponent(ComponentType type);

		// Risolve lo step corrente quando il circuito contiene componenti non lineari:
		// itera Newton-Raphson ripartendo dalla soluzione dello step precedente. Ad
		// ogni iterazione ristampa il modello companion dei componenti non lineari
		// sopra la parte lineare (già calcolata da Circuit::ComputeMatrix/ComputeVector),
		// rifattorizza e risolve. Restituisce nullopt se una matrice è singolare;
		// converged dice se le iterazioni sono arrivate a convergenza.
		// Va chiamato con m_circuitMutex già acquisito (da Simulate).
		std::optional<Eigen::VectorXd> SolveNonlinearStep(bool &converged);

		// Come Simulate(), ma con m_circuitMutex già acquisito dal chiamante
		// (SimulationLoop lo tiene per tutto un batch di step). Con publish=false
		// non costruisce l'output per il rendering (solo quanto serve
		// all'oscilloscopio negli step campionati). Restituisce true se il back
		// buffer contiene un output completo da pubblicare (publish, o un errore).
		bool SimulateLocked(bool publish);

		// Esito dei controlli di topologia (vuoto, solo massa, terminale
		// scollegato), ricalcolato da SimulateLocked solo quando il circuito è dirty
		SimulationResult m_topologyCheck = SimulationResult::success;
		bool m_topologyChecked = false;

		void SimulationLoop();
		void RenderLoop();

		// Resetta circuito e UI (stesso lavoro di New()) SENZA toccare le pile di
		// undo/redo: usato da New() stesso e, separatamente, da Undo/Redo, che
		// devono poter ricostruire lo stato senza svuotare le pile che stanno
		// proprio maneggiando.
		void ClearState();

	public:
		Application();
		~Application();

		// Esegue la simulazione MNA sull'attuale stato del circuito.
		// Chiamato dalla UI tramite callback m_onRunSimulation.
		// Restituisce un SimulationOutput con il risultato e i valori calcolati.
		void Simulate();

		void UpdateDecimationFactor();

		void AutoSync();

		void SampleChannels(const SimulationOutput &output);

		const Eigen::VectorXd &GetResult() const { return m_simulationResult; }

		void SetSimulationStatus(SimulationStatus status);

		void AddChannel(ProbeType type, int idA, int idB = -1, int compId = -1);

		// Resetta il circuito e la UI allo stato iniziale (canvas vuoto)
		void New();

		// Cattura lo stato attuale in cima a m_undoStack e svuota m_redoStack
		// (un nuovo gesto invalida i "ripeti" precedenti, come in qualunque
		// editor). Chiamata dalla UI, tramite callback, PRIMA di ogni gesto che
		// muta lo stato — mai dopo, altrimenti si catturerebbe già la modifica.
		void PushUndoSnapshot();

		// Annulla l'ultima modifica: sposta lo stato attuale su m_redoStack e
		// ricostruisce quello in cima a m_undoStack. No-op se m_undoStack è vuoto.
		void Undo();

		// Ripete l'ultima modifica annullata: simmetrico di Undo, tra m_redoStack
		// e m_undoStack. No-op se m_redoStack è vuoto.
		void Redo();

		bool CanUndo() const { return !m_undoStack.empty(); }
		bool CanRedo() const { return !m_redoStack.empty(); }

		// Avvia il loop principale dell'applicazione: lancia il thread di simulazione
		// e gestisce il thread di rendering, che a sua volta delega a UI la gestione
		// eventi e il disegno di ogni frame.
		void Run();
	};
}