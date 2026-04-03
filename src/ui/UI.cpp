#include <imgui-SFML.h>

#include "UI/Ui.h"

CircuitLab::UI::UI(unsigned int width, unsigned int heigth, const std::string &title) : m_width(width), m_heigth(heigth), m_title(title), m_window(sf::VideoMode({ m_width,m_heigth }), m_title)
{
	if (!ImGui::SFML::Init(m_window))
		throw std::runtime_error("Impossibile inizializzare SFML");
}

CircuitLab::UI::~UI()
{
	ImGui::SFML::Shutdown();
}

void CircuitLab::UI::Run()
{
	sf::Clock deltaClock;

	while (m_window.isOpen())
	{
		while (const std::optional event = m_window.pollEvent())
		{
			ImGui::SFML::ProcessEvent(m_window, *event);
			if (event->is<sf::Event::Closed>())
				m_window.close();
			else if (const auto *mouseEvent = event->getIf<sf::Event::MouseButtonPressed>())
			{
				auto pos = mouseEvent->position;
				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R)) 
				{	
					int id = m_onCircuitChange(ComponentType::resistor, 1.0);
					m_componentViewList.emplace_back(ComponentView(id, Vec2(pos.x, pos.y), 1.0, "Resistor"));
				}
			}
		}

		ImGui::SFML::Update(m_window, deltaClock.restart());

		// Finestre ImGui qui

		m_window.clear(sf::Color(30, 30, 30));

		for (const auto &comp : m_componentViewList) {
			sf::RectangleShape rect({ 20,40 });
			rect.setPosition({ comp.GetPosition().x,comp.GetPosition().y });
			m_window.draw(rect);
		}

		ImGui::SFML::Render(m_window);

		m_window.display();
	}
}
