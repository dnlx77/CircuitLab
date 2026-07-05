#pragma once
#include <memory>
#include <mutex>
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

		std::vector<OscilloscopeChannel> m_channels;
		std::vector<Color> m_channelPalette;
		int m_nextChannelColorIndex = 0;

		static constexpr double BATCH_TARGET_TIME = 0.010; // 10ms virtuali per batch
		static constexpr int MAX_STEPS_PER_BATCH = 5000;   // anti-spirale della morte

		// Factory method: crea il componente corretto in base al tipo richiesto dalla UI.
		// Restituisce nullptr per tipi non riconosciuti.
		std::unique_ptr<Component> MakeComponent(ComponentType type);

		void SimulationLoop();
		void RenderLoop();

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

		// Avvia il loop principale delegando alla UI
		void Run();
	};
}