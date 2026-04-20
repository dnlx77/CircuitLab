#include <imgui-SFML.h>
#include <imgui.h>
#include <iostream>

#include "UI/Ui.h"
#include "Core/Vector2.h"

// Determina quale componente o terminale si trova sotto il punto cliccato.
// Controlla prima i terminali (area più piccola, priorità alta),
// poi il corpo del componente (rettangolo centrale).
// Aggiorna selComp con il risultato; se nulla è trovato, imposta state = none.
void CircuitLab::UI::CheckClick(sf::Vector2i pos, SelecetedComponent &selComp)
{
	for (const auto &comp : m_componentViewList)
	{
		ComponentDesign des = comp.GetComponetDesign();

		int i = 0;
		for (const auto &terminal : des.terminalOffset)
		{
			// Posizione assoluta del terminale nel canvas
			Vec2i ter(
				static_cast<int>(comp.GetPosition().x + terminal.x),
				static_cast<int>(comp.GetPosition().y + terminal.y)
			);

			// Click dentro la zona di tolleranza del terminale?
			if ((pos.x >= ter.x - CLICK_TOLLERANCE) && (pos.x <= ter.x + CLICK_TOLLERANCE) &&
				(pos.y >= ter.y - CLICK_TOLLERANCE) && (pos.y <= ter.y + CLICK_TOLLERANCE))
			{
				selComp.compId = comp.GetComponentLink();
				selComp.terminalIndex = i;
				selComp.state = SelectionState::terminalSelected;
				return;
			}
			i++;
		}

		// Click dentro il rettangolo del corpo del componente?
		if ((pos.x >= comp.GetPosition().x - des.compWidth / 2) &&
			(pos.x <= comp.GetPosition().x + des.compWidth / 2) &&
			(pos.y >= comp.GetPosition().y - des.compHeight / 2) &&
			(pos.y <= comp.GetPosition().y + des.compHeight / 2))
		{
			selComp.compId = comp.GetComponentLink();
			selComp.terminalIndex = -1;
			selComp.state = SelectionState::componentSelected;
			return;
		}
	}

	// Nessun componente trovato sotto il click
	selComp.compId = -1;
	selComp.terminalIndex = -1;
	selComp.state = SelectionState::none;
}

// Calcola le coordinate pixel dei due estremi di un collegamento tra terminali.
// L'offset verticale del raggio sposta il punto di attacco sul bordo del cerchio
// del terminale, non sul suo centro.
CircuitLab::LinkView CircuitLab::UI::GetLinkCoords(int comp1, int term1, int comp2, int term2)
{
	sf::Vector2f pA, pB;

	for (const auto &comp : m_componentViewList)
	{
		if (comp.GetComponentLink() == comp1)
		{
			ComponentDesign des = comp.GetComponetDesign();
			pA.x = comp.GetPosition().x + des.terminalOffset[term1].x;
			pA.y = comp.GetPosition().y + des.terminalOffset[term1].y
				+ (des.terminalOffset[term1].y >= 0 ? 1 : -1) * des.terminalRadius;
		}

		if (comp.GetComponentLink() == comp2)
		{
			ComponentDesign des = comp.GetComponetDesign();
			pB.x = comp.GetPosition().x + des.terminalOffset[term2].x;
			pB.y = comp.GetPosition().y + des.terminalOffset[term2].y
				+ (des.terminalOffset[term2].y >= 0 ? 1 : -1) * des.terminalRadius;
		}
	}

	LinkView link;
	link.pointA = pA;
	link.compIdA = comp1;
	link.pointB = pB;
	link.compIdB = comp2;
	return link;
}

// Inizializza la finestra SFML e ImGui-SFML.
// Lancia un'eccezione se ImGui o il font non riescono ad inizializzarsi.
CircuitLab::UI::UI(unsigned int width, unsigned int heigth, const std::string &title) :
	m_width(width),
	m_heigth(heigth),
	m_title(title),
	m_window(sf::VideoMode({ m_width, m_heigth }), m_title)
{
	if (!ImGui::SFML::Init(m_window))
		throw std::runtime_error("Impossibile inizializzare ImGui-SFML");

	if (!m_font.openFromFile("JetBrainsMono-Regular.ttf"))
		throw std::runtime_error("Impossibile caricare il font");
}

// Shutdown di ImGui-SFML alla distruzione della UI
CircuitLab::UI::~UI()
{
	ImGui::SFML::Shutdown();
}

// Loop principale della UI:
//   1. Gestione eventi SFML (chiusura, click mouse, tasto Delete)
//   2. Aggiornamento ImGui
//   3. Pannello ImGui con pulsante di simulazione e risultati
//   4. Rendering canvas: componenti, terminali, etichette, fili
//   5. Render ImGui sopra il canvas
void CircuitLab::UI::Run()
{
	sf::Clock deltaClock;

	while (m_window.isOpen())
	{
		// --- Gestione eventi ---
		while (const std::optional event = m_window.pollEvent())
		{
			ImGui::SFML::ProcessEvent(m_window, *event);

			if (event->is<sf::Event::Closed>())
				m_window.close();

			else if (const auto *mouseEvent = event->getIf<sf::Event::MouseButtonPressed>())
			{
				auto pos = mouseEvent->position;

				// Aggiunta componenti con tasto modificatore + click
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

				// Gestione selezione e collegamento terminali:
				// - 1° click su un terminale: lo seleziona
				// - 2° click su un altro terminale: crea il collegamento
				if (m_selectedComponent.state != SelectionState::terminalSelected)
				{
					CheckClick(pos, m_selectedComponent);
				}
				else
				{
					// Salva i dati del primo terminale selezionato
					int comp1 = m_selectedComponent.compId;
					int term1 = m_selectedComponent.terminalIndex;

					SelecetedComponent temp;
					CheckClick(pos, temp);

					if (temp.state == SelectionState::terminalSelected)
					{
						int comp2 = temp.compId;
						int term2 = temp.terminalIndex;

						// Notifica il circuito e aggiunge il filo alla lista visiva
						if (comp1 != comp2)
						{
							m_onCreateLink(comp1, term1, comp2, term2);

							// DA CANCELLARE
							std::cout << "Link: comp" << comp1 << " term" << term1
								<< " -> comp" << comp2 << " term" << term2 << std::endl;

							m_linkViewList.emplace_back(GetLinkCoords(comp1, term1, comp2, term2));
						}
						// Reset selezione
						m_selectedComponent.state = SelectionState::none;
						m_selectedComponent.compId = -1;
						m_selectedComponent.terminalIndex = -1;
					}
				}
			}
			else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete))
			{
				// Eliminazione del componente selezionato con tasto Delete:
				// rimuove la vista, i fili collegati e notifica il circuito
				if (m_selectedComponent.state == SelectionState::componentSelected)
				{
					int id = m_selectedComponent.compId;

					// Rimuove la vista del componente
					m_componentViewList.erase(
						std::remove_if(m_componentViewList.begin(), m_componentViewList.end(),
							[id](const ComponentView &cw) {
								return cw.GetComponentLink() == id;
							}),
						m_componentViewList.end()
					);

					// Rimuove i fili collegati al componente eliminato
					m_linkViewList.erase(
						std::remove_if(m_linkViewList.begin(), m_linkViewList.end(),
							[id](const LinkView &lw) {
								return (lw.compIdA == id || lw.compIdB == id);
							}),
						m_linkViewList.end()
					);

					m_onDeleteComponent(m_selectedComponent.compId);

					// Reset selezione
					m_selectedComponent.state = SelectionState::none;
					m_selectedComponent.compId = -1;
					m_selectedComponent.terminalIndex = -1;
				}
			}
		}

		// --- Aggiornamento ImGui ---
		ImGui::SFML::Update(m_window, deltaClock.restart());

		// --- Pannello ImGui ---
		ImGui::Begin("CircuitLab - Test");

		if (ImGui::Button("RunSimulation"))
			m_simulationOutput = m_onRunSimulation();

		// Mostra il risultato della simulazione o un messaggio di errore
		if (m_simulationOutput.simRes == SimulationResult::solve_error)
			ImGui::Text("Circuito non risolvibile!");
		else if (m_simulationOutput.simRes == SimulationResult::empty_circuit)
			ImGui::Text("Il circuito non contiene componenti!");
		else if (m_simulationOutput.simRes == SimulationResult::no_circuit)
			ImGui::Text("Errore interno, puntatore a circuito nullo!");
		else
		{
			ImGui::Text("Risultato: [");
			for (const auto &r : m_simulationOutput.res)
			{
				std::string res = r.first + " " + std::to_string(r.second) + " ";
				ImGui::Text(res.c_str());
			}
			ImGui::Text("]");
		}

		ImGui::End();

		// --- Rendering canvas ---
		m_window.clear(BACKGROUND_COLOR);

		// Disegna ogni componente: rettangolo colorato per tipo + cerchi per i terminali + etichetta
		for (const auto &comp : m_componentViewList)
		{
			ComponentDesign des = comp.GetComponetDesign();

			sf::RectangleShape rect;
			rect.setSize({ static_cast<float>(des.compWidth), static_cast<float>(des.compHeight) });
			rect.setOrigin({ static_cast<float>(des.compWidth / 2), static_cast<float>(des.compHeight / 2) });
			rect.setPosition({ comp.GetPosition().x, comp.GetPosition().y });

			// Colore del corpo in base al tipo
			if (comp.GetComponentType() == ComponentType::resistor)
				rect.setFillColor(sf::Color::Green);
			else if (comp.GetComponentType() == ComponentType::voltageSource)
				rect.setFillColor(sf::Color::Red);
			else if (comp.GetComponentType() == ComponentType::ground)
				rect.setFillColor(sf::Color::White);

			// Outline giallo se il componente è selezionato (corpo, non terminale)
			if (comp.GetComponentLink() == m_selectedComponent.compId &&
				m_selectedComponent.terminalIndex == -1)
			{
				rect.setOutlineColor(sf::Color::Yellow);
				rect.setOutlineThickness(OUTLINE_THICKNESS);
			}

			m_window.draw(rect);

			// Disegna i terminali come cerchi blu
			sf::CircleShape term(static_cast<float>(des.terminalRadius));
			term.setFillColor(sf::Color::Blue);
			term.setOrigin({ static_cast<float>(des.terminalRadius), static_cast<float>(des.terminalRadius) });

			for (int i = 0; i < des.terminalOffset.size(); i++)
			{
				term.setPosition({
					comp.GetPosition().x + des.terminalOffset[i].x,
					comp.GetPosition().y + des.terminalOffset[i].y
						+ (des.terminalOffset[i].y >= 0 ? 1 : -1) * des.terminalRadius
					});

				// Outline giallo se questo terminale è selezionato
				if (comp.GetComponentLink() == m_selectedComponent.compId &&
					m_selectedComponent.terminalIndex == i)
				{
					term.setOutlineColor(sf::Color::Yellow);
					term.setOutlineThickness(OUTLINE_THICKNESS);
				}

				m_window.draw(term);
				term.setOutlineThickness(0); // Reset per il prossimo terminale
			}

			// Costruisce l'etichetta del componente nel formato "R1_2" / "V1_2" / "G0"
			// usando i nodeId dei terminali ottenuti dal circuito tramite callback
			std::vector<int> terminalsId = m_onGetCompTerminalId(comp.GetComponentLink());
			std::string compString;
			if (comp.GetComponentType() == ComponentType::resistor)
				compString += "R";
			else if (comp.GetComponentType() == ComponentType::voltageSource)
				compString += "V";
			else if (comp.GetComponentType() == ComponentType::ground)
				compString += "G";
			for (int i = 0; i < terminalsId.size(); i++)
			{
				compString += std::to_string(terminalsId[i]);
				if (i < terminalsId.size() - 1)
					compString += "_";
			}

			sf::Text label(m_font);
			label.setString(compString);
			label.setCharacterSize(12);
			label.setPosition({ comp.GetPosition().x + TEXT_COMPONENT_OFFSET, comp.GetPosition().y });
			m_window.draw(label);
		}

		// Disegna i fili come linee bianche tra i punti dei terminali collegati
		for (const auto &wire : m_linkViewList)
		{
			sf::Vertex line[2] = {
				sf::Vertex{wire.pointA, sf::Color::White},
				sf::Vertex{wire.pointB, sf::Color::White}
			};
			m_window.draw(line, 2, sf::PrimitiveType::Lines);
		}

		// Render ImGui sopra il canvas
		ImGui::SFML::Render(m_window);
		m_window.display();
	}
}