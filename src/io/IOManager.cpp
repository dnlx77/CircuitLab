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

	// Serializza le viste grafiche dei fili (estremi e riferimenti ai componenti)
	for (auto const &lw : linksView)
	{
		nlohmann::json linkViewJson;
		linkViewJson["id"] = lw.id;
		linkViewJson["compIdA"] = lw.compIdA;
		if (lw.compIdB.has_value()) linkViewJson["compIdB"] = lw.compIdB;
		linkViewJson["termIndexA"] = lw.termIndexA;
		if (lw.termIndexB.has_value()) linkViewJson["termIndexB"] = lw.termIndexB;
		if (lw.nodeViewId.has_value()) linkViewJson["nodeViewId"] = lw.nodeViewId;
		j["linksView"].push_back(linkViewJson);
	}

	j["nodeView"] = nlohmann::json::array();

	for (auto const &nv : nodesView)
	{
		nlohmann::json nodeViewJson;
		nodeViewJson["id"] = nv.id;
		nodeViewJson["nodeId"] = nv.nodeId;
		nodeViewJson["position"] = { nv.position.x, nv.position.y };
		nodeViewJson["linksViewId"] = nlohmann::json::array();

		for (const auto &lvi : nv.linkViewIds)
			nodeViewJson["linksViewId"].push_back(lvi);

		j["nodeView"].push_back(nodeViewJson);
	}

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

	// Mappa savedId -> newId: gli ID nel file JSON sono quelli del momento
	// in cui il circuito è stato salvato. Alla ricarica, i componenti ricevono
	// nuovi ID progressivi. Questa mappa traduce i riferimenti nei link e nelle viste.
	std::map<int, int> m_loadVsRealNodeMap;

	std::map<int, int> m_loadVsRealNodeViewMap;

	std::map<int, int> m_loadVsRealLinkViewMap;

	// Prima passata: crea i componenti e registra la corrispondenza tra ID salvato e ID nuovo
	for (auto const compJson : j["components"])
	{
		double value = compJson.contains("value") ? compJson["value"].get<double>() : 0.0;
		m_loadVsRealNodeMap[compJson["id"]] = m_onComponentLoad(compJson["type"].get<ComponentType>(), value);
	}

	// Seconda passata: crea le viste grafiche, traducendo l'ID salvato con quello reale
	for (auto const compViewJosn : j["componentsView"])
	{
		Vec2 vec(compViewJosn["position"][0], compViewJosn["position"][1]);
		m_onComponentViewLoad(m_loadVsRealNodeMap.at(compViewJosn["componentLink"]), compViewJosn["name"].get<std::string>(), compViewJosn["type"].get<ComponentType>(), vec, compViewJosn["rotation"]);
	}

	// Terza passata: ricrea i link tra terminali nel circuito
	for (auto const linkJson : j["links"])
		m_onLinkLoad(m_loadVsRealNodeMap.at(linkJson["compId1"]), linkJson["termIndex1"], m_loadVsRealNodeMap.at(linkJson["compId2"]), linkJson["termIndex2"]);

	for (auto const nodeViewJson : j["nodeView"])
	{
		sf::Vector2f pos = { nodeViewJson["position"][0], nodeViewJson["position"][1] };
		std::vector<int> lvIds;
		for (auto const linkViewIdJson : nodeViewJson["linksViewId"])
			lvIds.emplace_back(linkViewIdJson);
		m_loadVsRealNodeViewMap[nodeViewJson["id"]] = m_onNodeViewLoad(nodeViewJson["nodeId"], pos);
	}

	// Quarta passata: ricrea le viste grafiche dei fili
	for (auto const linkViewJson : j["linksView"])
	{
		std::optional<int> compB = (linkViewJson.contains("compIdB")) ? m_loadVsRealNodeMap.at(linkViewJson["compIdB"].get<int>()) : std::optional<int>{};
		std::optional<int> termIndB = (linkViewJson.contains("termIndexB")) ? linkViewJson["termIndexB"].get<int>() : std::optional<int>{};
		std::optional<int> nodeViewId = (linkViewJson.contains("nodeViewId")) ? m_loadVsRealNodeViewMap.at(linkViewJson["nodeViewId"].get<int>()) : std::optional<int>{};
		m_loadVsRealLinkViewMap[linkViewJson["id"]] = m_onLinkViewLoad(m_loadVsRealNodeMap.at(linkViewJson["compIdA"]), compB, linkViewJson["termIndexA"], termIndB, nodeViewId);
	}

	for (auto const nodeViewJson : j["nodeView"])
	{
		std::vector<int> remappedIds;
		for (auto const linkViewIdJson : nodeViewJson["linksViewId"])
			remappedIds.emplace_back(m_loadVsRealLinkViewMap.at(linkViewIdJson.get<int>()));

		m_onUpdateNodeViewLinkIds(m_loadVsRealNodeViewMap.at(nodeViewJson["id"].get<int>()), remappedIds);
	}
}