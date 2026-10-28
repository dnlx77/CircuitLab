#pragma once
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CircuitLab {

	// Codice di uscita della simulazione.
	// Permette alla UI di distinguere tra i vari casi di errore
	// senza dover interpretare i dati numerici.
	enum class SimulationResult {
		success,              // Simulazione completata con successo
		empty_circuit,        // Il circuito non contiene componenti
		only_ground_circuit,  // Il circuito contiene solo componenti ground (nessun nodo attivo)
		solve_error,          // La matrice MNA è singolare, sistema non risolvibile
		disconnected_terminal, // Un componente ha un terminale non collegato a nulla
		no_convergence        // Le iterazioni di Newton-Raphson (componenti non lineari, es. diodo) non convergono
	};

	// Struttura di output che Application pubblica per la UI (doppio buffer in Application, passato alla UI con UI::UpdateSimulation).
	// Contiene il codice di risultato e, in caso di successo,
	// la lista delle variabili calcolate con i rispettivi valori.
	// Ogni coppia è nella forma { "V1", 3.3 } o { "I(V1_0)", 0.01 }.
	struct SimulationOutput {
		SimulationResult simRes;                         // Esito della simulazione
		std::vector<std::pair<std::string, double>> res; // Variabili calcolate: { nome, valore }
		std::unordered_map<int, double> currentComp;
		// Corrente di ramo per coppia di nodi: { nodoA, nodoB, idComponente } -> corrente da A a B.
		// Serve all'oscilloscopio (sonda "corrente di ramo"), che sceglie il ramo per nodi.
		std::map<std::tuple<int, int, int>, double> currentBranch;
		// Corrente che ENTRA nel componente da ciascun terminale: { idComponente, indice terminale }
		// -> corrente. Vale per ogni tipo di componente (anche per quelli con piu' di una coppia di
		// terminali o con correnti indipendenti), cosi' la UI non ne deve conoscere i casi speciali.
		std::map<std::pair<int, int>, double> terminalCurrent;
		std::unordered_map<int, double> nodeVoltages; // NodeId -> tensione
	};

	enum class SimulationStatus {
		running,
		stopped,
		paused
	};
}