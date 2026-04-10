#pragma once
#include <memory>
#include <Eigen/Dense>
#include "Core/Circuit.h"
#include "Common/ComponentType.h"
#include "Common/SimulationOutput.h"

namespace CircuitLab {

	// Forward declaration per evitare inclusione circolare tra Application e UI
	class UI;

	// Classe Mediator principale dell'applicazione.
	// Coordina la comunicazione tra il circuito (logica di simulazione)
	// e l'interfaccia grafica (UI), senza che i due si conoscano direttamente.
	// Possiede in esclusiva sia il circuito che la UI tramite unique_ptr.
	class Application {
	private:
		std::unique_ptr<Circuit> m_circuit;          // Il circuito elettrico
		std::unique_ptr<UI> m_ui;                    // L'interfaccia grafica
		Eigen::VectorXd m_simulationResult;          // Ultimo vettore soluzione MNA

		// Factory method: crea il componente corretto in base al tipo richiesto dalla UI.
		// Restituisce nullptr per tipi non riconosciuti.
		std::unique_ptr<Component> MakeComponent(ComponentType type, double value);

	public:
		Application();
		~Application();

		// Esegue la simulazione MNA sull'attuale stato del circuito.
		// Chiamato dalla UI tramite callback m_onRunSimulation.
		// Restituisce un SimulationOutput con il risultato e i valori calcolati.
		SimulationOutput RunSimulation();

		const Eigen::VectorXd &GetResult() const { return m_simulationResult; }

		// Avvia il loop principale delegando alla UI
		void Run();
	};
}