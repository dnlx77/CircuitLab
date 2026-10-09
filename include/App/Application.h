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
		// Se vero, all'avvio da t=0 (e dopo Load o un cambio di timestep) lo stato di partenza è
		// il PUNTO DI LAVORO DC invece dello stato scarico: i condensatori partono già caricati
		// e gli induttori già attraversati dalla corrente di regime, senza il transitorio di
		// assestamento (vedi ApplyDcOperatingPointLocked). Disattivabile dal pannello per
		// osservare la carica dei condensatori da zero.
		//
		// Spento di default: un circuito oscillante (multivibratore astabile) partendo
		// esattamente dal punto di lavoro DC resta fermo su quell'equilibrio (verificato: nemmeno
		// una perturbazione di 10 mV lo fa partire, mentre dallo stato scarico oscilla), e un
		// circuito RC non mostrerebbe più la carica del condensatore.
		std::atomic<bool> m_startFromDc{ false };
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

		// Factory method: crea il componente corretto in base al tipo richiesto dalla UI,
		// con valori di default (es. resistenza 1kΩ, generatore DC 0V) — non prende un
		// valore esplicito: l'utente lo imposta dopo dalla UI.
		std::unique_ptr<Component> MakeComponent(ComponentType type);

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

		// Calcola il punto di lavoro DC e lo adotta come stato di partenza: imposta lo stato
		// dinamico dei componenti e il punto di partenza di Newton (m_simulationResult). Con
		// m_circuitMutex già acquisito dal chiamante. Restituisce false (senza toccare lo stato)
		// se il circuito è vuoto, ha terminali scollegati o il calcolo non converge.
		bool ApplyDcOperatingPointLocked();

		// Calcola un passo dal circuito attuale e lo pubblica al rendering subito (swap +
		// notifica), azzerando poi il tempo virtuale: mostra i valori veri nel pannello senza
		// aspettare l'avvio della simulazione continua. Chiamare con la simulazione ferma.
		void PrimeOutput();

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

		// Riparte da t=0 dallo stato di regime (punto di lavoro DC), anche a simulazione in
		// corso: azzera il tempo e i campioni dell'oscilloscopio e riprende da dove era.
		void RestartFromDc();

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