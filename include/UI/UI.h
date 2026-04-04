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

	class UI {
	public:
		using fnCircuitChange = std::function<int(CircuitLab::ComponentType type, double res)>;

	private:
		unsigned int m_width;
		unsigned int m_heigth;
		std::string m_title;

		SelecetedComponent m_selectedComponent;

		std::vector<ComponentView> m_componentViewList;
		sf::RenderWindow m_window;

		std::function<void()> m_onRunSimulation;
		fnCircuitChange m_onCircuitChange;

		void CheckClick(sf::Vector2i pos, SelecetedComponent &selComp);

	public:
		UI(unsigned int width, unsigned int heigth, const std::string &title);
		~UI();

		void SetOnRunSimulation(const std::function<void()> &func) { m_onRunSimulation = func; }
		void SetOnCircuitChange(const fnCircuitChange &func) { m_onCircuitChange = func; }

		std::function<void()> GetOnRunSimulation() const { return m_onRunSimulation; }
		fnCircuitChange GetOnCircuitChange() const { return m_onCircuitChange; }

		void Run();
	};
}