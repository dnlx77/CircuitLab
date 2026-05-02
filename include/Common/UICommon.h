#pragma once
#include <SFML/Graphics.hpp>

namespace CircuitLab {

	// Rappresentazione visiva di un collegamento tra due terminali:
	// semplicemente i due punti estremi del filo nel canvas,
	// con i riferimenti ai componenti collegati (usati per rimuovere il filo
	// quando un componente viene eliminato).
	struct LinkView {
		sf::Vector2f pointA;
		sf::Vector2f pointB;
		int compIdA;   // ID del componente sul primo estremo del filo
		int compIdB;   // ID del componente sul secondo estremo del filo
		int termIndexA; // Indice del terminale del componente A
		int termIndexB; // Indice del terminale del componente B
	};

	// Stato della selezione corrente nel canvas:
	// niente selezionato, un componente selezionato, o un terminale selezionato, o trascinamento.
	enum class SelectionState {
		none,
		componentSelected,
		terminalSelected,
		dragging
	};

	// Tiene traccia del componente (o terminale) attualmente selezionato.
	// Usato per gestire il doppio click sui terminali per creare un collegamento.
	struct SelecetedComponent {
		int compId;          // ID del componente selezionato (-1 se nessuno)
		int terminalIndex;   // Indice del terminale selezionato (-1 se nessuno o se è il corpo)
		SelectionState state;
	};
}