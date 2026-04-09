#include <imgui-SFML.h>
#include <imgui.h>

#include <iostream>

#include "UI/Ui.h"
#include "Core/Vector2.h"

void CircuitLab::UI::CheckClick(sf::Vector2i pos, SelecetedComponent &selComp)
{
	for (const auto &comp : m_componentViewList)
	{
		ComponentDesign des = comp.GetComponetDesign();

		int i = 0;
		for (const auto &terminal : des.terminalOffset)
		{
			Vec2i ter(static_cast<int>(comp.GetPosition().x + terminal.x), static_cast<int>(comp.GetPosition().y + terminal.y));
			if ((pos.x >= ter.x - CLICK_TOLLERANCE) && (pos.x <= ter.x + CLICK_TOLLERANCE) && (pos.y >= ter.y - CLICK_TOLLERANCE) && (pos.y <= ter.y + CLICK_TOLLERANCE))
			{
				selComp.compId = comp.GetComponentLink();
				selComp.terminalIndex = i;
				selComp.state = SelectionState::terminalSelected;
				return;
			}

			i++;
		}

		if ((pos.x >= comp.GetPosition().x - des.compWidth/2) && (pos.x <= comp.GetPosition().x + des.compWidth/2) && (pos.y >= comp.GetPosition().y - des.compHeight/2) && (pos.y <= comp.GetPosition().y + des.compHeight/2))
		{
			selComp.compId = comp.GetComponentLink();
			selComp.terminalIndex = -1;
			selComp.state = SelectionState::componentSelected;
			return;
		}

	}
	
	selComp.compId = -1;
	selComp.terminalIndex = -1;
	selComp.state = SelectionState::none;
	return;
}

CircuitLab::LinkView CircuitLab::UI::GetLinkCoords(int comp1, int term1, int comp2, int term2)
{
	sf::Vector2f pA, pB;
	for (const auto &comp : m_componentViewList)
	{
		if (comp.GetComponentLink() == comp1)
		{
			ComponentDesign des = comp.GetComponetDesign();

			pA.x = comp.GetPosition().x + des.terminalOffset[term1].x;
			pA.y = comp.GetPosition().y + des.terminalOffset[term1].y + (des.terminalOffset[term1].y >= 0 ? 1 : -1) * des.terminalRadius;
		}

		if (comp.GetComponentLink() == comp2)
		{
			ComponentDesign des = comp.GetComponetDesign();

			pB.x = comp.GetPosition().x + des.terminalOffset[term2].x;
			pB.y = comp.GetPosition().y + des.terminalOffset[term2].y + (des.terminalOffset[term2].y >= 0 ? 1 : -1) * des.terminalRadius;
		}
	}

	LinkView link;
	link.pointA = pA;
	link.pointB = pB;
	return link;
}

CircuitLab::UI::UI(unsigned int width, unsigned int heigth, const std::string &title) :
	m_width(width),
	m_heigth(heigth),
	m_title(title),
	m_window(sf::VideoMode({ m_width,m_heigth }), m_title)
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
					int id = m_onCircuitChange(ComponentType::resistor, DEFAULT_RESISTANCE);
					m_componentViewList.emplace_back(ComponentView(id, Vec2(pos.x, pos.y), DEFAULT_ROTATION, "Resistor", ComponentType::resistor));
				}

				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V))
				{
					int id = m_onCircuitChange(ComponentType::voltageSource, DEFAULT_VOLTAGE);
					m_componentViewList.emplace_back(ComponentView(id, Vec2(pos.x, pos.y), DEFAULT_ROTATION, "Voltage source", ComponentType::voltageSource));
				}

				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::G))
				{
					int id = m_onCircuitChange(ComponentType::ground, 0);
					m_componentViewList.emplace_back(ComponentView(id, Vec2(pos.x, pos.y), DEFAULT_ROTATION, "Ground", ComponentType::ground));
				}

				if (m_selectedComponent.state != SelectionState::terminalSelected)
					CheckClick(pos, m_selectedComponent); // 1° click
				else if (m_selectedComponent.state == SelectionState::terminalSelected)
				{
					// mi memorizzo i dati del componente in vista di un possibile secondo input
					int comp1 = m_selectedComponent.compId;
					int term1 = m_selectedComponent.terminalIndex;
					int comp2, term2;
					SelecetedComponent temp;
					CheckClick(pos, temp);
					if (temp.state == SelectionState::terminalSelected)
					{
						comp2 = temp.compId;
						term2 = temp.terminalIndex;
						m_onCreateLink(comp1, term1, comp2, term2);

						// DA CANCELLARE
						std::cout << "Link: comp" << comp1 << " term" << term1 << " -> comp" << comp2 << " term" << term2 << std::endl;

						m_linkViewList.emplace_back(GetLinkCoords(comp1, term1, comp2, term2));

						m_selectedComponent.state = SelectionState::none;
						m_selectedComponent.compId = -1;
						m_selectedComponent.terminalIndex = -1;
					}
				}

			}
		}

		ImGui::SFML::Update(m_window, deltaClock.restart());

		// Finestre ImGui qui
		ImGui::Begin("CircuitLab - Test");
	
		if (ImGui::Button("RunSimulation")) {
			m_result = m_onRunSimulation();
		}

		std::string res = "Risultato: [";
		for (int i = 0; i < m_result.size(); i++)
		{
			res += std::to_string(m_result(i));
			res += " ";
		}
		res += "]";
		
		ImGui::Text(res.c_str());
		ImGui::End();

		m_window.clear(BACKGROUND_COLOR);

		for (const auto &comp : m_componentViewList) {
			ComponentDesign des = comp.GetComponetDesign();
			sf::RectangleShape rect;
			sf::CircleShape term(static_cast<float>(des.terminalRadius));
			rect.setSize({ static_cast<float>(des.compWidth) , static_cast<float>(des.compHeight) });
			if (comp.GetComponentType() == ComponentType::resistor)				
				rect.setFillColor(sf::Color::Green);
			else if (comp.GetComponentType() == ComponentType::voltageSource)
				rect.setFillColor(sf::Color::Red);
			else if (comp.GetComponentType() == ComponentType::ground) 
				rect.setFillColor(sf::Color::White);

			rect.setPosition({ comp.GetPosition().x,comp.GetPosition().y });
			if (comp.GetComponentLink() == m_selectedComponent.compId && m_selectedComponent.terminalIndex == -1) { rect.setOutlineColor(sf::Color::Yellow); rect.setOutlineThickness(OUTLINE_THICKNESS); }
			rect.setOrigin({ static_cast<float>(des.compWidth / 2), static_cast<float>(des.compHeight / 2) });
			m_window.draw(rect);

			for (int i =0; i< des.terminalOffset.size(); i++)
			{
				
				term.setFillColor(sf::Color::Blue);
				
				term.setPosition({ comp.GetPosition().x + des.terminalOffset[i].x, comp.GetPosition().y + des.terminalOffset[i].y + (des.terminalOffset[i].y >= 0 ? 1 : -1) * des.terminalRadius });
				if (comp.GetComponentLink() == m_selectedComponent.compId && m_selectedComponent.terminalIndex == i) { term.setOutlineColor(sf::Color::Yellow); term.setOutlineThickness(OUTLINE_THICKNESS); }
				term.setOrigin({ static_cast<float>(des.terminalRadius), static_cast<float>(des.terminalRadius) });
				m_window.draw(term);
				term.setOutlineThickness(0);
			}
		}

	for (const auto &wire : m_linkViewList) {
		sf::Vertex line[2] = {
			sf::Vertex{wire.pointA, sf::Color::White},
			sf::Vertex{wire.pointB, sf::Color::White}
		};

		m_window.draw(line, 2, sf::PrimitiveType::Lines);
	}

	ImGui::SFML::Render(m_window);
	m_window.display();
	}

}
