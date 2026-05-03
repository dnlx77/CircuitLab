#include <imgui-SFML.h>
#include <imgui.h>
#include <numbers>

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

		float cosAngle = static_cast<float>(std::cos(-1 * comp.GetRotation() * std::numbers::pi / 180.0));
		float sinAngle = static_cast<float>(std::sin(-1 * comp.GetRotation() * std::numbers::pi / 180.0));

		int dx = static_cast<int>(pos.x - comp.GetPosition().x);
		int dy = static_cast<int>(pos.y - comp.GetPosition().y);

		float x1 = dx * cosAngle - dy * sinAngle;
		float y1 = dx * sinAngle + dy * cosAngle;

		int i = 0;
		for (const auto &terminal : des.terminalOffset)
		{
			// Click dentro la zona di tolleranza del terminale?
			if ((x1 >= terminal.x - CLICK_TOLLERANCE) && (x1 <= terminal.x + CLICK_TOLLERANCE) &&
				(y1 >= terminal.y - CLICK_TOLLERANCE) && (y1 <= terminal.y + CLICK_TOLLERANCE))
			{
				selComp.compId = comp.GetComponentLink();
				selComp.terminalIndex = i;
				selComp.state = SelectionState::terminalSelected;
				return;
			}
			i++;
		}

		// Click dentro il rettangolo del corpo del componente?
		if ((x1 >= - des.compWidth / 2) &&
			(x1 <= des.compWidth / 2) &&
			(y1 >= - des.compHeight / 2) &&
			(y1 <= des.compHeight / 2))
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
			sf::Vector2f rotTerm = GetRotatedTermnialPos(comp, term1);
			pA.x = comp.GetPosition().x + rotTerm.x;
			pA.y = comp.GetPosition().y + rotTerm.y;
		}

		if (comp.GetComponentLink() == comp2)
		{
			sf::Vector2f rotTerm = GetRotatedTermnialPos(comp, term2);
			pB.x = comp.GetPosition().x + rotTerm.x;
			pB.y = comp.GetPosition().y + rotTerm.y;
		}
	}

	LinkView link;
	link.pointA = pA;
	link.compIdA = comp1;
	link.pointB = pB;
	link.compIdB = comp2;
	link.termIndexA = term1;
	link.termIndexB = term2;
	return link;
}

// Calcola la posizione ruotata di un terminale rispetto al centro del componente.
// Applica la matrice di rotazione 2D all'offset del terminale,
// tenendo conto del raggio per attaccare il filo al bordo del cerchio.
sf::Vector2f CircuitLab::UI::GetRotatedTermnialPos(const ComponentView &cw, int termIndex)
{
	ComponentDesign des = cw.GetComponetDesign();
	float cosAngle = static_cast<float>(std::cos(cw.GetRotation() * std::numbers::pi / 180.0));
	float sinAngle = static_cast<float>(std::sin(cw.GetRotation() * std::numbers::pi / 180.0));

	float x = static_cast<float>(des.terminalOffset[termIndex].x);
	float y = static_cast<float>(des.terminalOffset[termIndex].y + (des.terminalOffset[termIndex].y >= 0 ? 1 : -1) * des.terminalRadius);
	float x1 = x * cosAngle - y * sinAngle;
	float y1 = x * sinAngle + y * cosAngle;

	return sf::Vector2f({ x1, y1 });
}

// Aggiorna le coordinate dei fili collegati al componente specificato.
// Rimuove i vecchi LinkView e li ricalcola con GetLinkCoords(),
// in modo che i fili seguano il componente dopo una rotazione.
void CircuitLab::UI::UpdateLinksForComponent(int compId)
{
	std::vector<LinkView> toUpdateLinkList;
	for (auto const &lw : m_linkViewList)
		if (lw.compIdA == compId || lw.compIdB == compId)
			toUpdateLinkList.emplace_back(lw);

	// Rimuove i fili collegati al componente eliminato
	m_linkViewList.erase(
		std::remove_if(m_linkViewList.begin(), m_linkViewList.end(),
			[compId](const LinkView &lw) {
				return (lw.compIdA == compId || lw.compIdB == compId);
			}),
		m_linkViewList.end()
	);

	for (auto const &lw : toUpdateLinkList)
		m_linkViewList.emplace_back(GetLinkCoords(lw.compIdA, lw.termIndexA, lw.compIdB, lw.termIndexB));
}

void CircuitLab::UI::HandleEvents()
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

			if (mouseEvent->button == sf::Mouse::Button::Left && m_selectedComponent.state != SelectionState::dragging)
			{
				// Aggiunta componenti con tasto modificatore + click
				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R))
				{
					int id = m_onCircuitChange(ComponentType::resistor, DEFAULT_RESISTANCE);
					AddViewComponent(id, "Resistor", ComponentType::resistor, Vec2(pos.x, pos.y), DEFAULT_ROTATION);
				}
				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V))
				{
					int id = m_onCircuitChange(ComponentType::voltageSource, DEFAULT_VOLTAGE);
					AddViewComponent(id, "Voltage source", ComponentType::voltageSource, Vec2(pos.x, pos.y), DEFAULT_ROTATION);
				}
				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::G))
				{
					int id = m_onCircuitChange(ComponentType::ground, 0);
					AddViewComponent(id, "Ground", ComponentType::ground, Vec2(pos.x, pos.y), DEFAULT_ROTATION);
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
							// Notifica il circuito: aggiunge il filo alla lista visiva solo se il collegamento è valido
							bool isConnect = m_onCreateLink(comp1, term1, comp2, term2);

							if (isConnect)
								AddViewLink(comp1, term1, comp2, term2);
						}
						// Reset selezione
						m_selectedComponent.state = SelectionState::none;
						m_selectedComponent.compId = -1;
						m_selectedComponent.terminalIndex = -1;
					}
				}
			}
			else if (mouseEvent->button == sf::Mouse::Button::Right)
			{
				CheckClick(pos, m_selectedComponent);
				if (m_selectedComponent.state != SelectionState::none)
				{
					for (auto const &comp:m_componentViewList)
						if (comp.GetComponentLink() == m_selectedComponent.compId)
						{
							m_compClickOffset.x = pos.x - comp.GetPosition().x;
							m_compClickOffset.y = pos.y - comp.GetPosition().y;
						}
					m_selectedComponent.state = SelectionState::dragging;
				}
			}
		}
		else if (const auto *mouseMovedEvent = event->getIf<sf::Event::MouseMoved>())
		{
			auto pos = mouseMovedEvent->position;
			Vec2 newPos;
			if (m_selectedComponent.state == SelectionState::dragging)
			{
				newPos.x = pos.x - m_compClickOffset.x;
				newPos.y = pos.y - m_compClickOffset.y;

				for (auto &cw : m_componentViewList)
					if (cw.GetComponentLink() == m_selectedComponent.compId)
						cw.SetPosition(newPos);

				UpdateLinksForComponent(m_selectedComponent.compId);
			}
		}
		else if (const auto *mouseReleasedEvent = event->getIf<sf::Event::MouseButtonReleased>()) 
		{
			if (mouseReleasedEvent->button == sf::Mouse::Button::Right)
				m_selectedComponent.state = SelectionState::none;
		}
		else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete) && m_selectedComponent.state != SelectionState::dragging)
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
		else if (const auto *keyboardEvent = event->getIf<sf::Event::KeyPressed>())
		{
			if (m_selectedComponent.state == SelectionState::componentSelected && keyboardEvent->code == sf::Keyboard::Key::Q)
			{
				for (auto &cw : m_componentViewList)
					if (cw.GetComponentLink() == m_selectedComponent.compId)
						cw.SetRotation(static_cast<float>(static_cast<int>(cw.GetRotation() + 45) % 360));

				UpdateLinksForComponent(m_selectedComponent.compId);
			}
		}
	}
}

void CircuitLab::UI::DrawImageGuiPanel()
{
	// --- Pannello ImGui ---
	ImGui::Begin("CircuitLab - Test");

	if (ImGui::Button("RunSimulation"))
		m_simulationOutput = m_onRunSimulation();

	if (ImGui::Button("New"))
		m_onNew();

	static char pathBuffer[256] = "circuit.json";
	ImGui::InputText("File", pathBuffer, sizeof(pathBuffer));

	if (ImGui::Button("Save"))
		m_onSave(pathBuffer);

	if (ImGui::Button("Load"))
		m_onLoad(pathBuffer);


	// Mostra il risultato della simulazione o un messaggio di errore
	if (m_simulationOutput.simRes == SimulationResult::solve_error)
		ImGui::Text("Circuito non risolvibile!");
	else if (m_simulationOutput.simRes == SimulationResult::empty_circuit)
		ImGui::Text("Il circuito non contiene componenti!");
	else if (m_simulationOutput.simRes == SimulationResult::no_circuit)
		ImGui::Text("Errore interno, puntatore a circuito nullo!");
	else if (m_simulationOutput.simRes == SimulationResult::only_ground_circuit)
		ImGui::Text("Il circuito contiene solo componenti ground!");
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
}

void CircuitLab::UI::DrawComponents()
{
	// Disegna ogni componente: rettangolo colorato per tipo + cerchi per i terminali + etichetta
	for (const auto &comp : m_componentViewList)
	{
		ComponentDesign des = comp.GetComponetDesign();

		sf::RectangleShape rect;
		rect.setSize({ static_cast<float>(des.compWidth), static_cast<float>(des.compHeight) });
		rect.setOrigin({ static_cast<float>(des.compWidth / 2), static_cast<float>(des.compHeight / 2) });
		rect.setPosition({ comp.GetPosition().x, comp.GetPosition().y });
		rect.setRotation(sf::degrees(comp.GetRotation()));

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
		std::vector<int> terminalsId = m_onGetCompTerminalId(comp.GetComponentLink());

		int strPosX = 0, strPosY = 0;

		for (int i = 0; i < des.terminalOffset.size(); i++)
		{
			sf::Text termLabel(m_font);

			sf::Vector2f rotTerm = GetRotatedTermnialPos(comp, i);

			strPosX = (rotTerm.x >= 0) ? 1 : -1;
			strPosY = (rotTerm.y >= 0) ? 1 : -1;

			term.setPosition({ comp.GetPosition().x + rotTerm.x,comp.GetPosition().y + rotTerm.y });

			// Outline giallo se questo terminale è selezionato
			if (comp.GetComponentLink() == m_selectedComponent.compId &&
				m_selectedComponent.terminalIndex == i)
			{
				term.setOutlineColor(sf::Color::Yellow);
				term.setOutlineThickness(OUTLINE_THICKNESS);
			}

			m_window.draw(term);
			term.setOutlineThickness(0); // Reset per il prossimo terminale

			// Aggiunge il prefisso "+" se questo è il terminale positivo del componente
			std::string termString;
			if (des.isPositiveTerminal == i)
				termString = "+ ";
			termString += std::to_string(terminalsId[i]);
			termLabel.setString(termString);
			termLabel.setCharacterSize(12);
			termLabel.setPosition({ comp.GetPosition().x + rotTerm.x + strPosX * TEXT_COMPONENT_OFFSET, comp.GetPosition().y + rotTerm.y + strPosY * TEXT_COMPONENT_OFFSET });
			m_window.draw(termLabel);
		}

		// Costruisce l'etichetta del componente nel formato "R3" / "V2" / "G1"
		// usando il prefisso del tipo seguito dall'ID del componente
		std::string compString;
		if (comp.GetComponentType() == ComponentType::resistor)
			compString += "R";
		else if (comp.GetComponentType() == ComponentType::voltageSource)
			compString += "V";
		else if (comp.GetComponentType() == ComponentType::ground)
			compString += "G";

		compString += std::to_string(comp.GetComponentLink());

		float rot = comp.GetRotation();
		float cosAngle = static_cast<float>(std::cos(rot * std::numbers::pi / 180.0));
		float sinAngle = static_cast<float>(std::sin(rot * std::numbers::pi / 180.0));

		float x = 1.0f;
		float y = 0.0f;
		float x1 = x * cosAngle - y * sinAngle;
		float y1 = x * sinAngle + y * cosAngle;

		sf::Text label(m_font);
		label.setString(compString);
		label.setCharacterSize(12);

		sf::FloatRect bound = label.getLocalBounds();
		float originX, originY;
	
		if (x1 < 0)       originX = bound.size.x;
		else if (x1 == 0) originX = bound.size.x / 2;
		else              originX = 0;

		if (y1 < 0)       originY = bound.size.y;
		else if (y1 == 0) originY = bound.size.y / 2;
		else              originY = 0;
		
		label.setOrigin({ originX, originY });
		label.setPosition({ comp.GetPosition().x + x1 * TEXT_COMPONENT_OFFSET, comp.GetPosition().y + y1 * TEXT_COMPONENT_OFFSET });
		m_window.draw(label);
	}
}

void CircuitLab::UI::DrawWires()
{
	// Disegna i fili come linee bianche tra i punti dei terminali collegati
	for (const auto &wire : m_linkViewList)
	{
		sf::Vertex line[2] = {
			sf::Vertex{wire.pointA, sf::Color::White},
			sf::Vertex{wire.pointB, sf::Color::White}
		};
		m_window.draw(line, 2, sf::PrimitiveType::Lines);
	}
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

void CircuitLab::UI::AddViewComponent(int compId, const std::string &name, ComponentType type, Vec2 position, float rotation)
{
	m_componentViewList.emplace_back(ComponentView(compId, position, rotation, name, type));
}

void CircuitLab::UI::AddViewLink(int comp1, int term1, int comp2, int term2)
{
	m_linkViewList.emplace_back(GetLinkCoords(comp1, term1, comp2, term2));
}

void CircuitLab::UI::Clear()
{
	m_componentViewList.clear();
	m_linkViewList.clear();
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
		HandleEvents();

		// --- Aggiornamento ImGui ---
		ImGui::SFML::Update(m_window, deltaClock.restart());

		DrawImageGuiPanel();

		// --- Rendering canvas ---
		m_window.clear(BACKGROUND_COLOR);

		DrawComponents();

		DrawWires();

		// Render ImGui sopra il canvas
		ImGui::SFML::Render(m_window);
		m_window.display();
	}
}