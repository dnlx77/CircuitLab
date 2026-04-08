#include <imgui-SFML.h>

#include "UI/Ui.h"
#include "Core/Vector2.h"

void CircuitLab::UI::CheckClick(sf::Vector2i pos, SelecetedComponent &selComp)
{
	Vec2i ter2;
	for (const auto &comp : m_componentViewList)
	{
		Vec2i ter1(comp.GetPosition().x, comp.GetPosition().y - 20);
		Vec2i ter2(comp.GetPosition().x, comp.GetPosition().y + 20);

		if ((pos.x >= ter1.x - 5) && (pos.x <= ter1.x + 5) && (pos.y >= ter1.y - 5) && (pos.y <= ter1.y + 5))
		{
			selComp.compId = comp.GetComponentLink();
			selComp.terminalIndex = 0;
			selComp.state = SelectionState::terminalSelected;
			return;
		}

		if ((pos.x >= ter2.x - 5) && (pos.x <= ter2.x + 5) && (pos.y >= ter2.y - 5) && (pos.y <= ter2.y + 5))
		{
			selComp.compId = comp.GetComponentLink();
			selComp.terminalIndex = 1;
			selComp.state = SelectionState::terminalSelected;
			return;
		}

		if ((pos.x >= comp.GetPosition().x - 10) && (pos.x <= comp.GetPosition().x + 10) && (pos.y >= comp.GetPosition().y - 20) && (pos.y <= comp.GetPosition().y + 20))
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
			if (term1 == 0) 
			{
				pA.x = comp.GetPosition().x;
				pA.y = comp.GetPosition().y - 24;
			}
			else
			{
				pA.x = comp.GetPosition().x;
				pA.y = comp.GetPosition().y + 24;
			}
		}

		if (comp.GetComponentLink() == comp2)
		{
			if (term2 == 0)
			{
				pB.x = comp.GetPosition().x;
				pB.y = comp.GetPosition().y - 24;
			}
			else
			{
				pB.x = comp.GetPosition().x;
				pB.y = comp.GetPosition().y + 24;
			}
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
					int id = m_onCircuitChange(ComponentType::resistor, 1.0);
					m_componentViewList.emplace_back(ComponentView(id, Vec2(pos.x, pos.y), 1.0, "Resistor", ComponentType::resistor));
				}

				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V))
				{
					int id = m_onCircuitChange(ComponentType::voltageSource, 1.0);
					m_componentViewList.emplace_back(ComponentView(id, Vec2(pos.x, pos.y), 1.0, "Voltage source", ComponentType::voltageSource));
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

		m_window.clear(sf::Color(30, 30, 30));

		for (const auto &comp : m_componentViewList) {
			sf::RectangleShape rect({ 20,40 });
			sf::CircleShape term(4);
			if (comp.GetComponentType() == ComponentType::resistor)
				rect.setFillColor(sf::Color::Green);
			if (comp.GetComponentType() == ComponentType::voltageSource)
				rect.setFillColor(sf::Color::Red);
			rect.setPosition({ comp.GetPosition().x,comp.GetPosition().y });
			if (comp.GetComponentLink() == m_selectedComponent.compId && m_selectedComponent.terminalIndex == -1) { rect.setOutlineColor(sf::Color::Yellow); rect.setOutlineThickness(2); }
			rect.setOrigin({ 10,20 });
			m_window.draw(rect);
			term.setFillColor(sf::Color::Blue);
			term.setPosition({ comp.GetPosition().x, comp.GetPosition().y - 24 });
			if (comp.GetComponentLink() == m_selectedComponent.compId && m_selectedComponent.terminalIndex == 0) { term.setOutlineColor(sf::Color::Yellow); term.setOutlineThickness(2); }
			term.setOrigin({ 4, 4 });
			m_window.draw(term);
			term.setOutlineThickness(0);
			term.setPosition({ comp.GetPosition().x, comp.GetPosition().y + 24});
			if (comp.GetComponentLink() == m_selectedComponent.compId && m_selectedComponent.terminalIndex == 1) { term.setOutlineColor(sf::Color::Yellow); term.setOutlineThickness(2); }
			term.setOrigin({ 4, 4 });
			m_window.draw(term);
			term.setOutlineThickness(0);
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
