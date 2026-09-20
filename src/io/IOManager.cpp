#include "IO/IOManager.h"
#include "Common/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

void CircuitLab::IOManager::SaveToFile(const std::string &filePath, const Circuit &circ, const std::vector<ComponentView> &compsView, const std::vector<LinkView> &linksView, const std::vector<NodeView> &nodesView)
{
	nlohmann::json j;
	j["components"] = nlohmann::json::array();

	// Serializza ogni componente delegando a Component::Save(),
	// che chiama SaveSpecificData() sulla classe derivata (Template Method)
	for (auto const &comp : circ.GetComponentsVector())
	{
		nlohmann::json compJson;
		comp->Save(compJson);
		j["components"].push_back(compJson);
	}

	j["links"] = nlohmann::json::array();

	// Serializza i link tra terminali (topologia del circuito)
	for (auto const &link : circ.GetLinksVector())
	{
		nlohmann::json linkJson;
		linkJson["compId1"] = link.compId1;
		linkJson["compId2"] = link.compId2;
		linkJson["termIndex1"] = link.termIndex1;
		linkJson["termIndex2"] = link.termIndex2;
		j["links"].push_back(linkJson);
	}

	j["componentsView"] = nlohmann::json::array();

	// Serializza le viste grafiche (posizione, rotazione, nome) dei componenti
	for (auto const &cw : compsView)
	{
		nlohmann::json compViewJson;
		cw.Save(compViewJson);
		j["componentsView"].push_back(compViewJson);
	}

	j["linksView"] = nlohmann::json::array();

	// Serializza le viste grafiche dei fili (estremi e riferimenti ai componenti).
	// sourceNodeViewId è -1 per un tap normale (terminale->NodeView); se != -1
	// è un tratto di bus (NodeView->NodeView, nessun componente coinvolto) —
	// vedi il commento su LinkView in UICommon.h.
	for (auto const &lw : linksView)
	{
		nlohmann::json linkViewJson;
		linkViewJson["id"] = lw.id;
		linkViewJson["compIdA"] = lw.compIdA;
		linkViewJson["termIndexA"] = lw.termIndexA;
		linkViewJson["nodeViewId"] = lw.nodeViewId;
		linkViewJson["sourceNodeViewId"] = lw.sourceNodeViewId;
		j["linksView"].push_back(linkViewJson);
	}

	j["nodeView"] = nlohmann::json::array();

	for (auto const &nv : nodesView)
	{
		nlohmann::json nodeViewJson;
		nodeViewJson["id"] = nv.id;
		nodeViewJson["nodeId"] = nv.nodeId;
		nodeViewJson["position"] = { nv.position.x, nv.position.y };
		nodeViewJson["manual"] = nv.manual;
		nodeViewJson["anchorCompId"] = nv.anchorCompId;
		nodeViewJson["anchorTermIndex"] = nv.anchorTermIndex;
		nodeViewJson["attached"] = nv.attached;
		nodeViewJson["linksViewId"] = nlohmann::json::array();

		for (const auto &lvi : nv.linkViewIds)
			nodeViewJson["linksViewId"].push_back(lvi);

		j["nodeView"].push_back(nodeViewJson);
	}

	// Versione del modello dei NodeView: 2 = un NodeView per ogni terminale collegato
	// e fili come tratti di bus. I file senza questo campo usano il vecchio modello
	// (un hub per collegamento) e vengono convertiti al caricamento.
	j["nodeModel"] = 2;

	std::ofstream o(filePath);
	if (!o.is_open())
	{
		LOG_ERROR("Errore nell'apertura del file " << filePath);
		return;
	}
	o << j.dump(4) << std::endl;
}


void CircuitLab::IOManager::LoadFromFile(const std::string &filePath)
{
	std::ifstream i(filePath);
	if (!i.is_open())
	{
		LOG_ERROR("Errore nell'apertura del file " << filePath);
		return;
	}

	nlohmann::json j;
	i >> j;

	// Resetta il circuito e la UI prima di ricaricare
	m_onNew();

	// Mappe savedId -> newId: gli ID nel file JSON sono quelli del momento
	// in cui il circuito è stato salvato. Alla ricarica, gli oggetti ricevono
	// nuovi ID progressivi. Queste mappe traducono i riferimenti salvati.
	// Nota: nonostante il prefisso "m_", sono variabili locali, non membri della classe.
	std::map<int, int> loadVsRealNodeMap;
	std::map<int, int> loadVsRealNodeViewMap;
	std::map<int, int> loadVsRealLinkViewMap;

	// 1) Crea i componenti e registra la corrispondenza tra ID salvato e ID nuovo
	for (auto const &compJson : j["components"])
	{
		int newId = m_onComponentLoad(compJson["type"].get<ComponentType>());
		loadVsRealNodeMap[compJson["id"]] = newId;
		if (m_onComponentLoadData)
			m_onComponentLoadData(newId, compJson);
	}

	// 2) Crea le viste grafiche dei componenti, traducendo l'ID salvato con quello reale
	for (auto const compViewJosn : j["componentsView"])
	{
		Vec2 vec(compViewJosn["position"][0], compViewJosn["position"][1]);
		m_onComponentViewLoad(loadVsRealNodeMap.at(compViewJosn["componentLink"]), compViewJosn["name"].get<std::string>(), compViewJosn["type"].get<ComponentType>(), vec, compViewJosn["rotation"]);
	}

	// 3) Ricrea i link logici tra terminali nel circuito (Core, non UI)
	for (auto const linkJson : j["links"])
		m_onLinkLoad(loadVsRealNodeMap.at(linkJson["compId1"]), linkJson["termIndex1"], loadVsRealNodeMap.at(linkJson["compId2"]), linkJson["termIndex2"]);

	// 4) Ricrea gli hub NodeView (posizione + nodeId), registrando la corrispondenza ID salvato -> ID nuovo.
	// I linkViewIds salvati non vengono ancora tradotti qui: verranno aggiornati al passo 6,
	// dopo che le LinkView (passo 5) avranno ricevuto i loro nuovi ID.
	for (auto const nodeViewJson : j["nodeView"])
	{
		sf::Vector2f pos = { nodeViewJson["position"][0], nodeViewJson["position"][1] };
		std::vector<int> lvIds;
		for (auto const linkViewIdJson : nodeViewJson["linksViewId"])
			lvIds.emplace_back(linkViewIdJson);
		// "manual" non esiste nei file salvati prima che le ancore automatiche fossero
		// nascoste. Allora ogni NodeView con 2 fili era il punto medio automatico di un
		// collegamento (nascosto), mentre uno con un numero diverso di fili era una
		// giunzione o un nodo visibile, spesso già disposto a mano: così quei file si
		// vedono come prima, senza che i nodi sistemati dall'utente spariscano.
		bool manual = nodeViewJson.value("manual", lvIds.size() != 2);

		// NodeView ancorato a un terminale (modello attuale): l'id del componente è
		// quello salvato, va tradotto in quello nuovo come per i link. Assenti nei file vecchi.
		int anchorCompId = nodeViewJson.value("anchorCompId", -1);
		if (anchorCompId != -1)
			anchorCompId = loadVsRealNodeMap.at(anchorCompId);
		int anchorTermIndex = nodeViewJson.value("anchorTermIndex", -1);
		bool attached = nodeViewJson.value("attached", false);

		loadVsRealNodeViewMap[nodeViewJson["id"]] = m_onNodeViewLoad(nodeViewJson["nodeId"], pos, manual, anchorCompId, anchorTermIndex, attached);
	}

	// 5) Ricrea le viste grafiche dei fili (LinkView). sourceNodeViewId assente
	// (file salvati da versioni precedenti) o -1 indica un tap normale, tradotto
	// come prima; se presente e != -1 è un tratto di bus tra due NodeView, senza
	// alcun componente coinvolto — entrambi gli hub sono già stati creati al passo 4.
	for (auto const linkViewJson : j["linksView"])
	{
		int sourceNodeViewId = linkViewJson.value("sourceNodeViewId", -1);
		int newNodeViewId = loadVsRealNodeViewMap.at(linkViewJson["nodeViewId"].get<int>());

		if (sourceNodeViewId != -1)
		{
			int newSourceNodeViewId = loadVsRealNodeViewMap.at(sourceNodeViewId);
			loadVsRealLinkViewMap[linkViewJson["id"].get<int>()] = m_onBusLinkViewLoad(newSourceNodeViewId, newNodeViewId);
		}
		else
		{
			int newCompId = loadVsRealNodeMap.at(linkViewJson["compIdA"].get<int>());
			int termIndexA = linkViewJson["termIndexA"].get<int>();
			loadVsRealLinkViewMap[linkViewJson["id"].get<int>()] = m_onLinkViewLoad(newCompId, termIndexA, newNodeViewId);
		}
	}

	// 6) Ora che le LinkView hanno i loro ID reali, aggiorna ogni NodeView con la lista
	// tradotta dei linkViewIds che gli appartengono (chiudendo il giro rimasto in sospeso al passo 4)
	for (auto const nodeViewJson : j["nodeView"])
	{
		std::vector<int> remappedIds;
		for (auto const linkViewIdJson : nodeViewJson["linksViewId"])
			remappedIds.emplace_back(loadVsRealLinkViewMap.at(linkViewIdJson.get<int>()));

		m_onUpdateNodeViewLinkIds(loadVsRealNodeViewMap.at(nodeViewJson["id"].get<int>()), remappedIds);
	}

	// 7) File del vecchio modello (un hub per collegamento): ora che tutto è caricato,
	// lo si converte in un NodeView per terminale con fili come tratti di bus.
	if (!j.contains("nodeModel") && m_onConvertLegacyNodeViews)
		m_onConvertLegacyNodeViews();
}