#pragma once
#include <SFML/Graphics.hpp>

namespace CircuitLab {

	// Punto visivo del disegno a cui arrivano fili. Più fili possono toccare lo
	// stesso NodeView, ed è così che più terminali risultano collegati allo stesso
	// nodo elettrico. Due specie (vedi WireGraph.h per il modello completo):
	//  - ANCORATO (anchorCompId != -1): uno per ogni terminale collegato, creato
	//    sopra il terminale e legato ad esso da un tap. Finché è "agganciato"
	//    (attached) sta sul terminale e lo segue; se l'utente lo trascina altrove si
	//    stacca, e il tap diventa un filo visibile.
	//  - LIBERO (anchorCompId == -1): un nodo voluto dall'utente (tasto N, split,
	//    inserito sul mezzo di un filo). Ha solo tratti di bus.
	//
	// Un singolo nodo elettrico è rappresentato da PIÙ NodeView collegati tra loro
	// da tratti di bus (LinkView con sourceNodeViewId != -1): ogni filo tra due
	// terminali è appunto un tratto di bus tra i loro NodeView.
	// ATTENZIONE: il campo nodeId qui sotto è una cache impostata alla creazione
	// e NON viene mai aggiornato in seguito, mentre Circuit::ConnectTerminals può
	// rinumerare in blocco i nodeId reali (caso massa/merge, vedi Circuit.cpp).
	// Non fare quindi affidamento su questo campo per raggruppare i NodeView di
	// uno stesso nodo elettrico: va risolto "live" tramite un tap del gruppo
	// (vedi CreateLinkViewCurrentList in UI.cpp).
	struct NodeView {
		int id;
		int nodeId;                     // nodeId del circuito (Core) rappresentato da questo hub — vedi ATTENZIONE sopra
		sf::Vector2f position;
		std::vector<int> linkViewIds;   // LinkView (tap o tratti di bus) che toccano questo hub
		int anchorCompId = -1;          // Terminale a cui è ancorato (-1 = NodeView libero)
		int anchorTermIndex = -1;
		bool attached = false;          // Solo se ancorato: true = sta sul terminale e lo segue
		// Solo per leggere i file salvati col vecchio modello a hub (vedi
		// WireGraph::ConvertLegacy): là distingueva un nodo voluto dall'utente da un
		// punto medio automatico. Nel modello attuale non ha altri usi.
		bool manual = false;
	};

	// Rappresentazione visiva di UN SOLO capo di un collegamento: un filo dal
	// terminale (compIdA, termIndexA) fino all'hub NodeView (nodeViewId).
	// Il "vero" collegamento tra due terminali è quindi indiretto: entrambi
	// hanno una propria LinkView che punta allo stesso nodeViewId.
	//
	// Un "tratto di bus" è invece un filo tra due NodeView (nessun componente
	// coinvolto): sourceNodeViewId != -1 lo distingue da un tap normale. Serve
	// a rappresentare visivamente UN SOLO nodo elettrico con più punti di presa
	// (es. una fila di resistori in parallelo su un bus, invece che tutti a
	// stella su un unico punto) — compIdA/termIndexA restano a -1 in questo caso.
	// La corrente su un tratto di bus non è quella di un componente: va calcolata
	// per accumulo KCL lungo l'albero dei NodeView che condividono lo stesso
	// nodeId (vedi CreateLinkViewCurrentList in UI.cpp).
	struct LinkView {
		int id;
		sf::Vector2f startPos;
		sf::Vector2f targetPos;
		int compIdA;    // ID del componente da cui parte il filo (-1 per un tratto di bus)
		int termIndexA; // Indice del terminale del componente A (-1 per un tratto di bus)
		int nodeViewId; // Hub (NodeView) a cui arriva il filo
		int sourceNodeViewId = -1; // Se != -1, hub da cui PARTE il filo: è un tratto di bus, non un tap
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