#pragma once
#include <SFML/Graphics.hpp>

namespace CircuitLab {

	// "Hub" visivo che rappresenta un nodo elettrico (nodeId) sul canvas.
	// Più LinkView possono puntare allo stesso NodeView (stesso nodeViewId):
	// è così che più terminali risultano collegati allo stesso nodo elettrico.
	// Comportamento attuale: "fantasma" (invisibile, <=2 link) o "giunzione"
	// (pallino verde visibile, >2 link) — vedi TODO: renderla trascinabile.
	struct NodeView {
		int id;
		int nodeId;                     // nodeId del circuito (Core) rappresentato da questo hub
		sf::Vector2f position;
		std::vector<int> linkViewIds;   // LinkView che puntano a questo hub
	};

	// Rappresentazione visiva di UN SOLO capo di un collegamento: un filo dal
	// terminale (compIdA, termIndexA) fino all'hub NodeView (nodeViewId).
	// Il "vero" collegamento tra due terminali è quindi indiretto: entrambi
	// hanno una propria LinkView che punta allo stesso nodeViewId.
	struct LinkView {
		int id;
		sf::Vector2f startPos;
		sf::Vector2f targetPos;
		int compIdA;    // ID del componente da cui parte il filo
		int termIndexA; // Indice del terminale del componente A
		int nodeViewId; // Hub (NodeView) a cui arriva il filo
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
		int nodeViewId = -1;	// ID del nodeView selezionato (-1 se nessuno)
		SelectionState state = SelectionState::none;
		sf::Vector2f clickPos;
	};

	// Usata per l'animazione delle particelle di corrente (stile Falstad) lungo un filo:
	// offset = posizione della particella lungo il filo, count = numero di particelle attive.
	struct LinkPararticles {
		int linkViewId;
		float offset;
		int count;
	};
}