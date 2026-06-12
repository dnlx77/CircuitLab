#pragma once
#include <memory>
#include <mutex>
#include "Core/Circuit.h"
#include "Core/Solver.h"
#include "Common/ComponentType.h"
#include "Common/SimulationOutput.h"
#include "IO/IOManager.h"

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
		double m_simulationTime;
		std::atomic<SimulationStatus> m_simStatus;
		std::atomic<bool> m_newOutputReady = false;
		std::atomic<bool> m_isRunning = true;
		SimulationOutput m_buffers[2];
		int m_backIndex = 0;
		int m_frontIndex = 1;
		std::mutex m_swapMutex;

		// Factory method: crea il componente corretto in base al tipo richiesto dalla UI.
		// Restituisce nullptr per tipi non riconosciuti.
		std::unique_ptr<Component> MakeComponent(ComponentType type, double value);

		static constexpr float SIMULATION_STEP = (1.0f / 60.0f);

		void SimulationLoop();
		void RenderLoop();
	public:
		Application();
		~Application();

		// Esegue la simulazione MNA sull'attuale stato del circuito.
		// Chiamato dalla UI tramite callback m_onRunSimulation.
		// Restituisce un SimulationOutput con il risultato e i valori calcolati.
		void Simulate();

		const Eigen::VectorXd &GetResult() const { return m_simulationResult; }

		void SetSimulationStatus(SimulationStatus status);

		// Resetta il circuito e la UI allo stato iniziale (canvas vuoto)
		void New();

		// Avvia il loop principale delegando alla UI
		void Run();
	};
}