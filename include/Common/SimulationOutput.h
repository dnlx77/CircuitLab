#pragma once
#include <vector>
#include <string>

namespace CircuitLab {

	// Codice di uscita della simulazione.
	// Permette alla UI di distinguere tra i vari casi di errore
	// senza dover interpretare i dati numerici.
	enum class SimulationResult {
		success,              // Simulazione completata con successo
		empty_circuit,        // Il circuito non contiene componenti
		only_ground_circuit,  // Il circuito contiene solo componenti ground (nessun nodo attivo)
		no_circuit,           // Il puntatore al circuito è nullo (errore interno)
		solve_error           // La matrice MNA è singolare, sistema non risolvibile
	};

	// Struttura di output restituita da Application::RunSimulation() alla UI.
	// Contiene il codice di risultato e, in caso di successo,
	// la lista delle variabili calcolate con i rispettivi valori.
	// Ogni coppia è nella forma { "V1", 3.3 } o { "I(V1_0)", 0.01 }.
	struct SimulationOutput {
		SimulationResult simRes;                         // Esito della simulazione
		std::vector<std::pair<std::string, double>> res; // Variabili calcolate: { nome, valore }
	};
}