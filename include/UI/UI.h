#pragma once
#include <SFML/Graphics.hpp>
#include <functional>

#include "Common/ComponentType.h"
#include "UI/ComponentView.h"

namespace CircuitLab {

	enum class SelectionState {
		none,
		componentSelected,
		terminalSelected
	};

	struct SelecetedComponent {
		int compId;
		int terminalIndex;
		SelectionState state;
	};

	struct LinkView {
		sf::Vector2f pointA;
		sf::Vector2f pointB;
	};

	class UI {
	public:
		using fnCircuitChange = std::function<int(CircuitLab::ComponentType type, double res)>;
		using fnCreateLink = std::function<void(int compId1, int termIndex1, int compId2, int termIndex2)>;

	private:
		unsigned int m_width;
		unsigned int m_heigth;
		std::string m_title;

		SelecetedComponent m_selectedComponent;

		std::vector<ComponentView> m_componentViewList;
		std::vector<LinkView> m_linkViewList;
		sf::RenderWindow m_window;

		std::function<void()> m_onRunSimulation;
		fnCircuitChange m_onCircuitChange;
		fnCreateLink m_onCreateLink;

		void CheckClick(sf::Vector2i pos, SelecetedComponent &selComp);
		LinkView GetLinkCoords(int comp1, int term1, int comp2, int term2);

	public:
		UI(unsigned int width, unsigned int heigth, const std::string &title);
		~UI();

		void SetOnRunSimulation(const std::function<void()> &func) { m_onRunSimulation = func; }
		void SetOnCircuitChange(const fnCircuitChange &func) { m_onCircuitChange = func; }
		void SetOnCreateLink(const fnCreateLink &func) { m_onCreateLink = func; }

		std::function<void()> GetOnRunSimulation() const { return m_onRunSimulation; }
		fnCircuitChange GetOnCircuitChange() const { return m_onCircuitChange; }
		fnCreateLink GetOnCreateLink() const { return m_onCreateLink; }

		void Run();
	};
}