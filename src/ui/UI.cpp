#include <imgui-SFML.h>

#include "UI/Ui.h"

CircuitLab::UI::UI(unsigned int width, unsigned int heigth, const std::string &title) : m_width(width), m_heigth(heigth), m_title(title), m_window(sf::VideoMode({ m_width,m_heigth }), m_title)
{
	ImGui::SFML::Init(m_window);
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
		}

		ImGui::SFML::Update(m_window, deltaClock.restart());

		// Finestre ImGui qui

		m_window.clear(sf::Color(30, 30, 30));
		ImGui::SFML::Render(m_window);
		m_window.display();
	}
}
