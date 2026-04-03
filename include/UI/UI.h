#pragma once
#include <SFML/Graphics.hpp>
#include <functional>

#include "Common/ComponentType.h"
#include "Core/Vector2.h"

namespace CircuitLab {

	class UI {
	public:
		using fnCircuitChange = std::function<int(CircuitLab::ComponentType type, const Vec2 &pos, float rot, const std::string &name, double res)>;

	private:
		unsigned int m_width;
		unsigned int m_heigth;
		std::string m_title;
		sf::RenderWindow m_window;

		std::function<void()> m_onRunSimulation;
		fnCircuitChange m_onCircuitChange;

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