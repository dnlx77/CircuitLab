#include "IO/IOManager.h"
#include "Common/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace {
	// Versione del modello dei NodeView scritta nei file e minima accettata in lettura
	constexpr int NODE_MODEL_VERSION = 2;

	// I percorsi circolano in UTF-8 (finestre di scelta file, ImGui): su Windows un
	// std::ifstream(std::string) li leggerebbe invece nella tabella di caratteri
	// locale e non aprirebbe un file con accenti nel nome o nella cartella.
	std::filesystem::path FromUtf8(const std::string &utf8)
	{
		return std::filesystem::path(reinterpret_cast<const char8_t *>(utf8.c_str()));
	}
}

nlohmann::json CircuitLab::IOManager::Serialize(const Circuit &circ, const std::vector<ComponentView> &compsView, const std::vector<LinkView> &linksView, const std::vector<NodeView> &nodesView) const
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
	// (un hub per collegamento), che non si legge più (vedi LoadFromFile).
	j["nodeModel"] = NODE_MODEL_VERSION;

	return j;
}

void CircuitLab::IOManager::SaveToFile(const std::string &filePath, const Circuit &circ, const std::vector<ComponentView> &compsView, const std::vector<LinkView> &linksView, const std::vector<NodeView> &nodesView)
{
	nlohmann::json j = Serialize(circ, compsView, linksView, nodesView);

	std::ofstream o(FromUtf8(filePath));
	if (!o.is_open())
	{
		LOG_ERROR("Errore nell'apertura del file " << filePath);
		return;
	}
	o << j.dump(4) << std::endl;
}

void CircuitLab::IOManager::Deserialize(const nlohmann::json &j)
{
	// Mappe savedId -> newId: gli ID nel JSON sono quelli del momento in cui è
	// stato costruito. Alla ricostruzione, gli oggetti ricevono nuovi ID
	// progressivi. Queste mappe traducono i riferimenti salvati.
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
	for (const auto &compViewJson : j["componentsView"])
	{
		Vec2 vec(compViewJson["position"][0], compViewJson["position"][1]);
		m_onComponentViewLoad(loadVsRealNodeMap.at(compViewJson["componentLink"]), compViewJson["name"].get<std::string>(), compViewJson["type"].get<ComponentType>(), vec, compViewJson["rotation"], compViewJson.value("mirrored", false));
	}

	// 3) Ricrea i link logici tra terminali nel circuito (Core, non UI)
	for (const auto &linkJson : j["links"])
		m_onLinkLoad(loadVsRealNodeMap.at(linkJson["compId1"]), linkJson["termIndex1"], loadVsRealNodeMap.at(linkJson["compId2"]), linkJson["termIndex2"]);

	// 4) Ricrea gli hub NodeView (posizione + nodeId), registrando la corrispondenza ID salvato -> ID nuovo.
	// I linkViewIds salvati non vengono ancora tradotti qui: verranno aggiornati al passo 6,
	// dopo che le LinkView (passo 5) avranno ricevuto i loro nuovi ID.
	for (const auto &nodeViewJson : j["nodeView"])
	{
		Vec2 pos = { nodeViewJson["position"][0], nodeViewJson["position"][1] };

		// NodeView ancorato a un terminale: l'id del componente è quello salvato, va tradotto
		// in quello nuovo come per i link (-1 per un NodeView libero).
		int anchorCompId = nodeViewJson.value("anchorCompId", -1);
		if (anchorCompId != -1)
			anchorCompId = loadVsRealNodeMap.at(anchorCompId);
		int anchorTermIndex = nodeViewJson.value("anchorTermIndex", -1);
		bool attached = nodeViewJson.value("attached", false);

		loadVsRealNodeViewMap[nodeViewJson["id"]] = m_onNodeViewLoad(nodeViewJson["nodeId"], pos, anchorCompId, anchorTermIndex, attached);
	}

	// 5) Ricrea le viste grafiche dei fili (LinkView). sourceNodeViewId assente
	// (file salvati da versioni precedenti) o -1 indica un tap normale, tradotto
	// come prima; se presente e != -1 è un tratto di bus tra due NodeView, senza
	// alcun componente coinvolto — entrambi gli hub sono già stati creati al passo 4.
	for (const auto &linkViewJson : j["linksView"])
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
	for (const auto &nodeViewJson : j["nodeView"])
	{
		std::vector<int> remappedIds;
		for (const auto &linkViewIdJson : nodeViewJson["linksViewId"])
			remappedIds.emplace_back(loadVsRealLinkViewMap.at(linkViewIdJson.get<int>()));

		m_onUpdateNodeViewLinkIds(loadVsRealNodeViewMap.at(nodeViewJson["id"].get<int>()), remappedIds);
	}
}

void CircuitLab::IOManager::LoadFromFile(const std::string &filePath)
{
	std::ifstream i(FromUtf8(filePath));
	if (!i.is_open())
	{
		LOG_ERROR("Errore nell'apertura del file " << filePath);
		return;
	}

	// Con la finestra di scelta file si può aprire un .json qualunque: va verificato
	// PRIMA di azzerare il circuito, altrimenti un file sbagliato lascerebbe
	// l'utente con un canvas vuoto (o farebbe terminare il programma per un'eccezione).
	nlohmann::json j;
	try
	{
		i >> j;
	}
	catch (const nlohmann::json::exception &e)
	{
		LOG_ERROR("Il file " << filePath << " non e' un JSON valido: " << e.what());
		return;
	}
	if (!j.is_object() || !j.contains("components") || !j.contains("links"))
	{
		LOG_ERROR("Il file " << filePath << " non e' un circuito CircuitLab");
		return;
	}

	// I file col vecchio modello a hub (senza "nodeModel") non si leggono più: come per un file
	// sbagliato, si rifiuta prima di azzerare il circuito.
	if (j.value("nodeModel", 0) < NODE_MODEL_VERSION)
	{
		LOG_ERROR("Il file " << filePath << " e' stato salvato con una versione precedente di CircuitLab e non si puo' piu' aprire");
		return;
	}

	// Resetta il circuito e la UI prima di ricaricare
	m_onNew();

	Deserialize(j);
}
