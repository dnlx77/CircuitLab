#pragma once
#include <SFML/Graphics.hpp>

namespace CircuitLab {

	struct NodeView {
		int id;
		int nodeId;
		sf::Vector2f position;
		std::vector<int> linkViewIds;
	};

	// Rappresentazione visiva di un collegamento tra due terminali:
	// semplicemente i due punti estremi del filo nel canvas,
	// con i riferimenti ai componenti collegati (usati per rimuovere il filo
	// quando un componente viene eliminato).
	struct LinkView {
		int id;
		sf::Vector2f pointA;
		sf::Vector2f pointB;
		int compIdA;   // ID del componente sul primo estremo del filo
		int termIndexA; // Indice del terminale del componente A
		std::optional<int> compIdB;   // ID del componente sul secondo estremo del filo
		std::optional<int> termIndexB; // Indice del terminale del componente B
		std::optional<int> nodeViewId;
	};

	// Stato della selezione corrente nel canvas:
	// niente selezionato, un componente selezionato, o un terminale selezionato, o trascinamento.
	enum class SelectionState {
		none,
		componentSelected,
		terminalSelected,
		draggingComponent,
		draggingNodeView,
		linkSelected,
		nodeViewSelected
	};

	// Tiene traccia del componente (o terminale) attualmente selezionato.
	// Usato per gestire il doppio click sui terminali per creare un collegamento.
	struct SelecetedComponent {
		int compId = -1;        // ID del componente selezionato (-1 se nessuno)
		int terminalIndex = -1; // Indice del terminale selezionato (-1 se nessuno o se è il corpo)
		int linkId = -1;		// ID del link selezionato (-1 se nessuno) 
		SelectionState state = SelectionState::none;
		sf::Vector2f clickPos;
	};
}