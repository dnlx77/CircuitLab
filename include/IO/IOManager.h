#pragma once
#include <string>
#include "Core/Circuit.h"
#include "UI/ComponentView.h"
#include "Common/UICommon.h"

namespace CircuitLab {
	
	using fnComponentLoad = std::function<int(CircuitLab::ComponentType type, double value)>;
	using fnLinkLoad = std::function<bool(int compId1, int termComp1, int compId2, int termComp2)>;
	using fnComponentViewLoad = std::function<void(int compId, const std::string &name, ComponentType type, Vec2 position, float rotation)>;
	using fnLinkViewLoad = std::function<void(int compIdA, int compIdB, int terminalIndexA, int terminalIndexB)>;
	using fnOnNew = std::function<void()>;

	class IOManager {
	private:
		fnComponentLoad m_onComponentLoad;
		fnLinkLoad m_onLinkLoad;
		fnComponentViewLoad m_onComponentViewLoad;
		fnLinkViewLoad m_onLinkViewLoad;
		fnOnNew m_onNew;
	public:
		void SaveToFile(const std::string &filePath, const Circuit &circ, const std::vector<ComponentView> &compsView, const std::vector<LinkView> &linksView);
		void LoadFromFile(const std::string &filePath);

		void SetOnComponentLoad(const fnComponentLoad &fn) { m_onComponentLoad = fn; }
		void SetOnLoadLink(const fnLinkLoad &fn) { m_onLinkLoad = fn; }
		void SetOnComponentViewLoad(const fnComponentViewLoad &fn) { m_onComponentViewLoad = fn; }
		void SetOnLinkViewLoad(const fnLinkViewLoad &fn) { m_onLinkViewLoad = fn; }
		void SetOnNew(const fnOnNew &func) { m_onNew = func; }
	};
}