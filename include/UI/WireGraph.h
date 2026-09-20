#pragma once
#include <set>
#include <utility>
#include <vector>

#include "Common/UICommon.h"

namespace CircuitLab {

	// Terminale di un componente: {compId, termIndex}
	using TerminalRef = std::pair<int, int>;

	// Topologia visiva dei collegamenti: NodeView (punti) e LinkView (fili), senza
	// alcuna dipendenza da finestra, disegno o Circuit. Tutte le operazioni che
	// modificano la struttura stanno qui, così si possono provare da sole.
	//
	// Modello:
	//  - Ogni terminale collegato ha UN NodeView "ancorato" (anchorCompId/
	//    anchorTermIndex), creato sopra il terminale e collegato ad esso da un
	//    "tap" (LinkView terminale->NodeView). Finché il NodeView è "agganciato"
	//    (attached) sta sul terminale e ne segue gli spostamenti, e il tap ha
	//    lunghezza zero; se l'utente lo trascina altrove si stacca e il tap diventa
	//    un filo visibile.
	//  - Un filo tra due terminali è un TRATTO DI BUS tra i loro due NodeView
	//    (LinkView con sourceNodeViewId != -1).
	//  - I NodeView "liberi" (anchorCompId == -1) sono nodi voluti dall'utente:
	//    piazzati a mano, creati da uno split, o inseriti sul mezzo di un filo. Non
	//    hanno tap, solo tratti di bus.
	//  - I tratti di bus tra NodeView di uno stesso nodo elettrico formano sempre un
	//    ALBERO: mai un ciclo (i cicli renderebbero indeterminate le correnti).
	//
	// nodeViews, linkViews e i contatori sono pubblici perché la UI li legge e li
	// disegna direttamente (vedi i riferimenti m_nodeViewList/m_linkViewList).
	class WireGraph {
	public:
		std::vector<NodeView> nodeViews;
		std::vector<LinkView> linkViews;
		unsigned int nodeViewCount = 0;   // ultimo id di NodeView assegnato
		unsigned int linkViewIdCount = 0; // ultimo id di LinkView assegnato

		void Clear();

		// --- Ricerca ---
		NodeView *FindNodeView(int id);
		const NodeView *FindNodeView(int id) const;
		LinkView *FindLinkView(int id);
		const LinkView *FindLinkView(int id) const;

		// Il tap (LinkView terminale->NodeView) di un terminale, nullptr se non collegato
		const LinkView *TapOfTerminal(int compId, int termIndex) const;

		// Id del NodeView ancorato al terminale (-1 se il terminale non ha fili)
		int NodeViewIdOfTerminal(int compId, int termIndex) const;

		// Id dei NodeView all'altro capo dei tratti di bus che toccano nvId
		std::vector<int> BusNeighbors(int nvId) const;

		// Numero di tratti di bus che toccano nvId
		int BusDegree(int nvId) const;

		// Tutti i NodeView raggiungibili da nvId lungo i tratti di bus (compreso nvId):
		// sono la rappresentazione di UN solo nodo elettrico.
		std::set<int> CollectGroup(int nvId) const;
		bool SameGroup(int nvIdA, int nvIdB) const;

		// Tutti i terminali collegati (con un NodeView ancorato) nel gruppo di nvId
		std::vector<TerminalRef> TerminalsInGroup(int nvId) const;

		// Primo terminale collegato (tap reale) nel gruppo di startNvId, escluso
		// (se indicato) quello dato: serve per notificare il Circuit di un
		// collegamento. {-1,-1} se il gruppo non ha ancora nessun terminale.
		TerminalRef FindRealTapInGroup(int startNvId, int excludeCompId = -1, int excludeTermIndex = -1) const;

		// --- Costruzione ---
		int AddNodeView(int nodeId, sf::Vector2f position, bool manual = false,
			int anchorCompId = -1, int anchorTermIndex = -1, bool attached = false);

		// Aggiunge un tratto di bus sourceNvId -> targetNvId e lo registra su entrambi.
		// Non controlla i cicli: chi chiama deve aver verificato !SameGroup.
		int AddBusEdge(int sourceNvId, int targetNvId);

		// NodeView ancorato al terminale: quello esistente, oppure uno nuovo sopra
		// il terminale (terminalPos) con il suo tap.
		int EnsureTerminalNodeView(int compId, int termIndex, sf::Vector2f terminalPos);

		// Spezza il tratto di bus linkId nel punto della sua lunghezza più vicino a
		// clickPos, creando lì un NodeView libero. Restituisce il suo id, oppure -1
		// se linkId non è un tratto di bus. Il primo mezzo tratto conserva l'id di linkId.
		int InsertNodeOnBusEdge(int linkId, sf::Vector2f clickPos);

		// --- Spostamento ---
		// Sposta un NodeView e aggiorna gli estremi di tutti i fili che lo toccano.
		void SetNodeViewPosition(int nvId, sf::Vector2f position);

		// Il terminale si è spostato in terminalPos: aggiorna il suo tap e, se il
		// NodeView ancorato è agganciato, sposta anche lui.
		void MoveTerminal(int compId, int termIndex, sf::Vector2f terminalPos);

		// L'utente sta trascinando nvId: se è ancorato si stacca dal terminale.
		void DetachIfAnchored(int nvId);

		// Se nvId è ancorato e sta entro maxDistance dal terminale, lo riporta sul
		// terminale e lo riaggancia. True se l'ha riagganciato.
		bool TryReattach(int nvId, sf::Vector2f terminalPos, float maxDistance);

		// --- Rimozione (restituiscono i terminali rimasti senza fili) ---

		// Cancellazione di un componente: toglie i NodeView ancorati ai suoi
		// terminali (ricollegando tra loro i loro vicini) e pulisce a cascata.
		std::vector<TerminalRef> RemoveComponent(int compId);

		// Cancellazione di un NodeView libero, ricollegando tra loro i suoi vicini.
		// Non fa nulla (e restituisce vuoto) per un NodeView ancorato.
		std::vector<TerminalRef> RemoveFreeNodeView(int nvId);

		// Se un estremo del tratto di bus è un nodo libero "di passaggio" (esattamente
		// 2 tratti), restituisce il suo id: cancellarlo ricongiunge il filo. Altrimenti -1.
		int PassThroughNodeOfEdge(int linkId) const;

		// --- Conversione dei file salvati col vecchio modello a hub ---
		void ConvertLegacy();

	private:
		int AddTapLink(int compId, int termIndex, sf::Vector2f terminalPos, int nvId);
		void RemoveLinkRaw(int linkId);
		void EraseNodeViewRaw(int nvId);

		// Toglie nvId (con tutti i suoi fili) e collega tra loro i suoi vicini a
		// stella attorno al primo, così il gruppo resta connesso. Restituisce i vicini.
		std::vector<int> RemoveNodeViewReconnecting(int nvId);

		// Pulizia a cascata dopo che nvId ha perso un filo: un NodeView ancorato senza
		// più tratti si dissolve (il terminale resta libero), un nodo libero con un
		// solo tratto (moncone) si ritira. Accumula in freed i terminali liberati.
		void CleanUp(int nvId, std::vector<TerminalRef> &freed);
	};
}
