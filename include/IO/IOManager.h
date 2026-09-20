#pragma once
#include <string>
#include "Core/Circuit.h"
#include "UI/ComponentView.h"
#include "Common/UICommon.h"

namespace CircuitLab {

	// Alias per le callback usate durante il caricamento da file.
	// Permettono a IOManager di delegare la creazione degli oggetti
	// ad Application e UI senza conoscerle direttamente (pattern Mediator).

	// Crea un componente nel circuito dato tipo e valore; restituisce il nuovo ID assegnato
	using fnComponentLoad = std::function<int(CircuitLab::ComponentType type)>;

	using fnComponentLoadData = std::function<void(int compId, const nlohmann::json &j)>;

	// Collega due terminali nel circuito; restituisce true se il collegamento è valido
	using fnLinkLoad = std::function<bool(int compId1, int termComp1, int compId2, int termComp2)>;

	// Aggiunge la vista grafica di un componente alla UI
	using fnComponentViewLoad = std::function<void(int compId, const std::string &name, ComponentType type, Vec2 position, float rotation)>;

	// Aggiunge una LinkView (un filo da un terminale a un NodeView hub) alla UI.
	// Restituisce l'ID assegnato alla nuova LinkView.
	using fnLinkViewLoad = std::function<int(int compIdA, int terminalIndexA, int NodeViewId)>;

	// Aggiunge un tratto di bus (un filo tra due NodeView, nessun componente
	// coinvolto) alla UI. Restituisce l'ID assegnato alla nuova LinkView.
	using fnBusLinkViewLoad = std::function<int(int sourceNodeViewId, int targetNodeViewId)>;

	// Aggiunge un NodeView (hub) alla UI nella posizione data; restituisce l'ID assegnato.
	// anchorCompId/anchorTermIndex/attached descrivono un NodeView ancorato a un terminale
	// (anchorCompId = -1 per uno libero); manual serve solo alla conversione dei file
	// vecchi (vedi NodeView::manual).
	using fnNodeViewLoad = std::function<int(int nodeId, sf::Vector2f position, bool manual, int anchorCompId, int anchorTermIndex, bool attached)>;

	// Converte i NodeView appena letti dal vecchio modello (un hub per collegamento)
	// nel modello attuale (un NodeView per terminale). Chiamata solo per i file senza
	// il campo "nodeModel".
	using fnConvertLegacyNodeViews = std::function<void()>;

	using fnUpdateNodeViewLinkIds = std::function<void(int nodeViewId, std::vector<int> linkViewIds)>;

	// Resetta lo stato del circuito e della UI prima di caricare un nuovo file
	using fnOnNew = std::function<void()>;

	// Gestisce la serializzazione e deserializzazione del circuito su file JSON.
	// Conosce la struttura del formato JSON, ma delega la creazione degli oggetti
	// ad Application e UI tramite le callback registrate con i metodi Set*.
	class IOManager {
	private:
		fnComponentLoad m_onComponentLoad;
		fnLinkLoad m_onLinkLoad;
		fnComponentViewLoad m_onComponentViewLoad;
		fnLinkViewLoad m_onLinkViewLoad;
		fnBusLinkViewLoad m_onBusLinkViewLoad;
		fnNodeViewLoad m_onNodeViewLoad;
		fnConvertLegacyNodeViews m_onConvertLegacyNodeViews;
		fnOnNew m_onNew;
		fnUpdateNodeViewLinkIds m_onUpdateNodeViewLinkIds;
		fnComponentLoadData m_onComponentLoadData;

	public:
		// Serializza l'intero stato del circuito (componenti, link, viste) in un file JSON.
		// Il file viene creato o sovrascritto al percorso indicato da filePath.
		void SaveToFile(const std::string &filePath, const Circuit &circ, const std::vector<ComponentView> &compsView, const std::vector<LinkView> &linksView, const std::vector<NodeView> &nodesView);

		// Deserializza un file JSON e ricostruisce il circuito e la UI tramite le callback.
		// Gestisce il remapping degli ID: gli ID salvati nel file non corrispondono
		// necessariamente a quelli assegnati a runtime, quindi viene mantenuta
		// una mappa savedId -> newId per tradurre i riferimenti nei link.
		void LoadFromFile(const std::string &filePath);

		// Setter per le callback - chiamati da Application nel costruttore
		void SetOnComponentLoad(const fnComponentLoad &fn) { m_onComponentLoad = fn; }
		void SetOnLoadLink(const fnLinkLoad &fn) { m_onLinkLoad = fn; }
		void SetOnComponentViewLoad(const fnComponentViewLoad &fn) { m_onComponentViewLoad = fn; }
		void SetOnLinkViewLoad(const fnLinkViewLoad &fn) { m_onLinkViewLoad = fn; }
		void SetOnBusLinkViewLoad(const fnBusLinkViewLoad &fn) { m_onBusLinkViewLoad = fn; }
		void SetOnNodeViewLoad(const fnNodeViewLoad &fn) { m_onNodeViewLoad = fn; }
		void SetOnConvertLegacyNodeViews(const fnConvertLegacyNodeViews &fn) { m_onConvertLegacyNodeViews = fn; }
		void SetOnNew(const fnOnNew &fn) { m_onNew = fn; }
		void SetOnUpdateNodeViewLinkIds(const fnUpdateNodeViewLinkIds &fn) { m_onUpdateNodeViewLinkIds = fn; }
		void SetOnComponentLoadData(const fnComponentLoadData &fn) { m_onComponentLoadData = fn; }
	};
}