#include "IO/IOManager.h"
#include "Common/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

void CircuitLab::IOManager::SaveToFile(const std::string &filePath, const Circuit &circ, const std::vector<ComponentView> &compsView, const std::vector<LinkView> &linksView)
{
	nlohmann::json j;
	j["components"] = nlohmann::json::array();

	for (auto const &comp : circ.GetComponentsVector())
	{
		nlohmann::json compJson;
		comp->Save(compJson);
		j["components"].push_back(compJson);
	}

	j["links"] = nlohmann::json::array();
	
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

	for (auto const &cw : compsView)
	{
		nlohmann::json compViewJson;
		cw.Save(compViewJson);
		j["componentsView"].push_back(compViewJson);
	}

	j["linksView"] = nlohmann::json::array();

	for (auto const &lw : linksView)
	{
		nlohmann::json linkViewJson;
		linkViewJson["compIdA"] = lw.compIdA;
		linkViewJson["compIdB"] = lw.compIdB;
		linkViewJson["termIndexA"] = lw.termIndexA;
		linkViewJson["termIndexB"] = lw.termIndexB;
		j["linksView"].push_back(linkViewJson);
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

	m_onNew();
	std::map<int, int> m_loadVsRealNodeMap;

	for (auto const compJson : j["components"])
	{
		double value = compJson.contains("value") ? compJson["value"].get<double>() : 0.0;
		m_loadVsRealNodeMap[compJson["id"]] = m_onComponentLoad(compJson["type"].get<ComponentType>(), value);
	}

	for (auto const compViewJosn : j["componentsView"])
	{
		Vec2 vec(compViewJosn["position"][0], compViewJosn["position"][1]);
		m_onComponentViewLoad(m_loadVsRealNodeMap.at(compViewJosn["componentLink"]), compViewJosn["name"].get<std::string>(), compViewJosn["type"].get<ComponentType>(), vec, compViewJosn["rotation"]);
	}

	for (auto const linkJson : j["links"])
		m_onLinkLoad(m_loadVsRealNodeMap.at(linkJson["compId1"]), linkJson["termIndex1"], m_loadVsRealNodeMap.at(linkJson["compId2"]), linkJson["termIndex2"]);

	for (auto const linkViewJson : j["linksView"])
		m_onLinkViewLoad(m_loadVsRealNodeMap.at(linkViewJson["compIdA"]), linkViewJson["termIndexA"], m_loadVsRealNodeMap.at(linkViewJson["compIdB"]), linkViewJson["termIndexB"]);
}
