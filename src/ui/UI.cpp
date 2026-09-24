#include <imgui-SFML.h>
#include <imgui.h>
#include <implot.h>
#include <numbers>
#include <set>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

#include "UI/Ui.h"
#include "Core/Vector2.h"
#include "Common/Logger.h"

namespace {
	// Prefisso di una lettera per il tipo di componente ("R", "V", "G", ...),
	// usato nelle etichette sul canvas e nei nomi dei canali dell'oscilloscopio.
	const char *ComponentPrefix(CircuitLab::ComponentType type)
	{
		switch (type)
		{
		case CircuitLab::ComponentType::resistor:         return "R";
		case CircuitLab::ComponentType::voltageGenerator: return "V";
		case CircuitLab::ComponentType::ground:           return "G";
		case CircuitLab::ComponentType::capacitor:        return "C";
		case CircuitLab::ComponentType::inductor:         return "L";
		case CircuitLab::ComponentType::switchComponent:  return "S";
		case CircuitLab::ComponentType::diode:            return "D";
		case CircuitLab::ComponentType::transformer:      return "T";
		default:                                          return "";
		}
	}

	// Unità di misura di una proprietà di componente ("" se adimensionale o non
	// univoca, es. la fase, per cui non si mostra alcuna forma con prefisso SI).
	const char *ComponentValueUnit(CircuitLab::ComponentValue value)
	{
		switch (value)
		{
		case CircuitLab::ComponentValue::resistance:        return "Ohm";
		case CircuitLab::ComponentValue::voltage:           return "V";
		case CircuitLab::ComponentValue::amplitude:         return "V";
		case CircuitLab::ComponentValue::frequency:         return "Hz";
		case CircuitLab::ComponentValue::capacitance:       return "F";
		case CircuitLab::ComponentValue::inductance:        return "H";
		case CircuitLab::ComponentValue::saturationCurrent: return "A";
		case CircuitLab::ComponentValue::primaryInductance:   return "H";
		case CircuitLab::ComponentValue::secondaryInductance: return "H";
		default:                                            return "";
		}
	}

	// Formatta un valore con prefisso SI ("8.05 V", "250 uV", "4.3 mA"). Il micro
	// è scritto "u": il font predefinito di ImGui non ha il glifo "µ", e il resto
	// dell'interfaccia (es. il combo del timestep) usa già "us".
	std::string FormatEngineering(double value, const char *unit)
	{
		struct Prefix { double scale; const char *name; };
		static const Prefix prefixes[] = {
			{ 1e9, "G" }, { 1e6, "M" }, { 1e3, "k" }, { 1.0, "" },
			{ 1e-3, "m" }, { 1e-6, "u" }, { 1e-9, "n" }, { 1e-12, "p" },
		};

		const double magnitude = std::abs(value);
		// Sotto 0.1 pV/pA è solo rumore di arrotondamento (es. la media di una
		// sinusoide simmetrica): meglio "0" che "0.08 pV".
		if (magnitude < 1e-13)
			return std::string("0 ") + unit;

		const Prefix *chosen = &prefixes[std::size(prefixes) - 1];
		for (const Prefix &p : prefixes)
			if (magnitude >= p.scale)
			{
				chosen = &p;
				break;
			}

		char buffer[48];
		std::snprintf(buffer, sizeof(buffer), "%.4g %s%s", value / chosen->scale, chosen->name, unit);
		return buffer;
	}

	// Notazione scientifica con mantissa in [1,10) e senza zeri inutili: "2.5e-3",
	// "1e1", "0". L'esponente c'è sempre (anche "5e0") per restare uniforme lungo un asse.
	std::string FormatScientific(double value)
	{
		if (std::abs(value) < 1e-300)
			return "0";

		int exponent = static_cast<int>(std::floor(std::log10(std::abs(value))));
		double mantissa = value / std::pow(10.0, exponent);

		char buffer[32];
		std::snprintf(buffer, sizeof(buffer), "%.4g", mantissa);
		// L'arrotondamento a 4 cifre può dare 10 (es. 9.99996): rinormalizza
		if (std::abs(std::atof(buffer)) >= 10.0)
		{
			exponent++;
			std::snprintf(buffer, sizeof(buffer), "%.4g", mantissa / 10.0);
		}

		return std::string(buffer) + "e" + std::to_string(exponent);
	}

	// Contesto per il formattatore degli assi di ImPlot: quale notazione e, per la
	// notazione SI, quale unità ("s" per il tempo, "V"/"A" per l'asse Y, "" se mista).
	struct AxisNotation {
		int notation; // OSC_NOTATION_* di UI (1 = scientifica, 2 = SI)
		const char *unit;
	};

	int FormatAxisTick(double value, char *buffer, int size, void *userData)
	{
		const AxisNotation *axis = static_cast<const AxisNotation *>(userData);

		std::string text = (axis->notation == 1) ? FormatScientific(value) : FormatEngineering(value, axis->unit);
		while (!text.empty() && text.back() == ' ')
			text.pop_back();

		return std::snprintf(buffer, size, "%s", text.c_str());
	}

	// Misure di un canale sulla porzione di campioni visibile nel grafico.
	struct ChannelMeasure {
		std::string label;
		ImVec4 color;
		const char *unit = "V";
		double peakToPeak = 0.0;
		double mean = 0.0;
		double rms = 0.0;
		double frequency = 0.0; // 0 = non stimabile (segnale costante o meno di 2 periodi)
	};

	// Stima la frequenza contando i fronti di salita attraverso il valor medio,
	// con un'isteresi del 5% dell'ampiezza picco-picco per non contare il rumore.
	// L'istante di ogni fronte è interpolato linearmente tra due campioni, così
	// la stima non è limitata dalla risoluzione di un campione.
	double EstimateFrequency(const std::vector<double> &samples, int first, int last, double dt, double mean, double peakToPeak)
	{
		if (peakToPeak < 1e-12 || last - first < 2)
			return 0.0;

		const double hysteresis = 0.05 * peakToPeak;
		const double upper = mean + hysteresis;
		const double lower = mean - hysteresis;

		bool high = samples[first] > mean;
		std::vector<double> risingTimes;
		for (int i = first + 1; i <= last; i++)
		{
			if (!high && samples[i] > upper)
			{
				high = true;
				const double fraction = (upper - samples[i - 1]) / (samples[i] - samples[i - 1]);
				risingTimes.push_back((i - 1 + fraction) * dt);
			}
			else if (high && samples[i] < lower)
				high = false;
		}

		if (risingTimes.size() < 2)
			return 0.0;

		return static_cast<double>(risingTimes.size() - 1) / (risingTimes.back() - risingTimes.front());
	}
}

// Determina quale componente o terminale si trova sotto il punto cliccato.
// Controlla in quest'ordine: i NodeView (compresi quelli ancorati a un terminale,
// che stanno sopra di esso), poi i terminali e il corpo dei componenti, infine i fili.
// Aggiorna selComp con il risultato; se nulla è trovato, imposta state = none.
void CircuitLab::UI::CheckClick(sf::Vector2i pos, SelecetedComponent &selComp)
{
	// pos è in coordinate mondo; la tolleranza è definita in pixel di SCHERMO,
	// quindi va divisa per lo zoom (a zoom 2x, 7 px a schermo sono 3.5 unità mondo).
	const float tol = CLICK_TOLLERANCE / m_zoom;
	sf::Vector2f posF(static_cast<float>(pos.x), static_cast<float>(pos.y));

	// NodeView per primi, sia liberi sia ancorati a un terminale: un NodeView appena
	// creato sta esattamente sopra il suo terminale, ma il pallino deve poter essere
	// preso (e trascinato) subito. Cliccarlo equivale a cliccare il terminale: la
	// connessione riguarda comunque lo stesso gruppo di fili. Il corpo del componente
	// resta raggiungibile per spostarlo.
	for (const auto &nv : m_nodeViewList)
	{
		if ((posF.x >= nv.position.x - tol) && (posF.x <= nv.position.x + tol) &&
			(posF.y >= nv.position.y - tol) && (posF.y <= nv.position.y + tol))
		{
			selComp.compId = -1;
			selComp.terminalIndex = -1;
			selComp.linkId = -1;
			selComp.nodeViewId = nv.id;
			selComp.state = SelectionState::nodeViewSelected;
			selComp.clickPos = posF;
			return;
		}
	}

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
			if ((x1 >= terminal.x - tol) && (x1 <= terminal.x + tol) &&
				(y1 >= terminal.y - tol) && (y1 <= terminal.y + tol))
			{
				selComp.compId = comp.GetComponentLink();
				selComp.terminalIndex = i;
				selComp.linkId = -1;
				selComp.nodeViewId = -1;
				selComp.state = SelectionState::terminalSelected;
				selComp.clickPos = sf::Vector2f({ static_cast<float>(pos.x), static_cast<float>(pos.y) });
				return;
			}
			i++;
		}

		// Click dentro il rettangolo del corpo del componente?
		if ((x1 >= -des.compWidth / 2) &&
			(x1 <= des.compWidth / 2) &&
			(y1 >= -des.compHeight / 2) &&
			(y1 <= des.compHeight / 2))
		{
			selComp.compId = comp.GetComponentLink();
			selComp.terminalIndex = -1;
			selComp.linkId = -1;
			selComp.nodeViewId = -1;
			selComp.state = SelectionState::componentSelected;
			selComp.clickPos = sf::Vector2f({ static_cast<float>(pos.x), static_cast<float>(pos.y) });
			return;
		}
	}

	// click su un filo (un tratto di bus, o il tap di un NodeView staccato dal terminale)
	for (auto const &link : m_linkViewList)
	{
		sf::Vector2f diffVec({ link.targetPos.x - link.startPos.x, link.targetPos.y - link.startPos.y });

		if (diffVec == sf::Vector2f(0.f, 0.f))
			continue;
		sf::Vector2f unitaryVec = diffVec.normalized();
		sf::Vector2f shrunkA({ link.startPos.x + unitaryVec.x * tol, link.startPos.y + unitaryVec.y * tol });
		sf::Vector2f shrunkB({ link.targetPos.x - unitaryVec.x * tol, link.targetPos.y - unitaryVec.y * tol });

		float dist = PointToStraightDistance(shrunkA, shrunkB, posF);

		if (dist <= tol &&
			posF.x >= std::min(shrunkA.x, shrunkB.x) - EPSILON &&
			posF.x <= std::max(shrunkA.x, shrunkB.x) + EPSILON &&
			posF.y >= std::min(shrunkA.y, shrunkB.y) - EPSILON &&
			posF.y <= std::max(shrunkA.y, shrunkB.y) + EPSILON)
		{
			selComp.compId = -1;
			selComp.terminalIndex = -1;
			selComp.linkId = link.id;
			selComp.nodeViewId = -1;
			selComp.state = SelectionState::linkSelected;
			selComp.clickPos = sf::Vector2f({ static_cast<float>(pos.x), static_cast<float>(pos.y) });
			LOG_DEBUG("CheckClick found link id: " << link.id);
			return;
		}

	}

	// Nessun componente trovato sotto il click
	selComp.compId = -1;
	selComp.terminalIndex = -1;
	selComp.linkId = -1;
	selComp.nodeViewId = -1;
	selComp.state = SelectionState::none;
	selComp.clickPos = sf::Vector2f({ posF.x, posF.y });
}

// Calcola le coordinate pixel dei due estremi di un collegamento tra terminali e nodeView.
// L'offset verticale del raggio sposta il punto di attacco sul bordo del cerchio
// del terminale, non sul suo centro.
CircuitLab::LinkView CircuitLab::UI::GetLinkCoords(int comp1, int term1, NodeView nodeView)
{
	sf::Vector2f pA, pB;

	for (const auto &comp : m_componentViewList)
	{
		if (comp.GetComponentLink() == comp1)
		{
			sf::Vector2f rotTerm = GetRotatedTerminalPos(comp, term1);
			pA.x = comp.GetPosition().x + rotTerm.x;
			pA.y = comp.GetPosition().y + rotTerm.y;
		}

	}

	LinkView link;
	link.id = ++m_linkViewIdCount;
	link.startPos = pA;
	link.targetPos = nodeView.position;
	link.compIdA = comp1;
	link.termIndexA = term1;
	link.nodeViewId = nodeView.id;
	return link;
}

// Calcola la posizione ruotata di un terminale rispetto al centro del componente.
// Applica la matrice di rotazione 2D all'offset del terminale,
// tenendo conto del raggio per attaccare il filo al bordo del cerchio.
sf::Vector2f CircuitLab::UI::GetRotatedTerminalPos(const ComponentView &cw, int termIndex) const
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

void CircuitLab::UI::UpdateLinksForComponent(int compId)
{
	for (const auto &cv : m_componentViewList)
	{
		if (cv.GetComponentLink() != compId)
			continue;

		ComponentDesign des = cv.GetComponetDesign();
		for (int i = 0; i < static_cast<int>(des.terminalOffset.size()); i++)
		{
			sf::Vector2f rotTer = GetRotatedTerminalPos(cv, i);
			sf::Vector2f posTer(cv.GetPosition().x + rotTer.x, cv.GetPosition().y + rotTer.y);

			// Il filo del terminale ora parte da posTer; il NodeView ancorato lo segue
			// solo se è ancora agganciato (uno che l'utente ha trascinato altrove resta
			// dove sta, e il filo tra terminale e nodo si allunga).
			m_graph.MoveTerminal(compId, i, posTer);
		}
		return;
	}
}

std::vector<sf::Vector2f> CircuitLab::UI::GetTerminalPositionbyCompId(int compId) const
{
	std::vector<sf::Vector2f> terminalsPos;
	for (auto &cv : m_componentViewList)
	{
		if (cv.GetComponentLink() == compId)
		{
			ComponentDesign des = cv.GetComponetDesign();
			for (int i = 0; i < des.terminalOffset.size(); i++)
			{
				sf::Vector2f rotTer = GetRotatedTerminalPos(cv, i);
				sf::Vector2f terPos;
				terPos.x = cv.GetPosition().x + rotTer.x;
				terPos.y = cv.GetPosition().y + rotTer.y;
				terminalsPos.emplace_back(terPos);
			}
			return terminalsPos;
		}
	}
	return terminalsPos;
}

int CircuitLab::UI::GetNodeViewIdByLinkId(int linkId) const
{
	for (const auto lv : m_linkViewList)
		if (lv.id == linkId)
			return lv.nodeViewId;
	return -1;
}

CircuitLab::NodeView CircuitLab::UI::GetNodeViewById(int nodeViewId) const
{
	for (const auto &nv : m_nodeViewList)
		if (nv.id == nodeViewId)
			return nv;

	throw std::runtime_error("NodeView not found for id: " + std::to_string(nodeViewId));
}

void CircuitLab::UI::UpdateNodeViewLinkIds(int nodeViewId, std::vector<int> linkViewIds)
{
	for (auto &nv : m_nodeViewList)
		if (nv.id == nodeViewId)
		{
			nv.linkViewIds = linkViewIds;
			return;
		}
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
			// Da qui in poi tutto il codice lavora in coordinate MONDO (le stesse in
			// cui sono salvate le posizioni di componenti/nodi/fili): con lo zoom
			// non coincidono più coi pixel della finestra, quindi si converte una
			// volta sola qui. pixelPos resta disponibile per i test "sono sopra il
			// canvas o sopra il pannello?", che sono per loro natura in pixel.
			sf::Vector2i pixelPos = mouseEvent->position;
			auto pos = WorldPos(pixelPos);

			if (mouseEvent->button == sf::Mouse::Button::Left && (m_selectedComponent.state != SelectionState::draggingComponent && m_selectedComponent.state != SelectionState::draggingNodeView))
			{
				// Aggiunta componenti con tasto modificatore + click
				bool placedNode = false;
				if (pixelPos.x < static_cast<int>(m_width - PANEL_WIDTH))
				{
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R))
					{
						int id = m_onCircuitChange(ComponentType::resistor);
						AddViewComponent(id, "Resistor", ComponentType::resistor, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
						SnapComponentToGrid(m_componentViewList.back());
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V))
					{
						int id = m_onCircuitChange(ComponentType::voltageGenerator);
						AddViewComponent(id, "Voltage source", ComponentType::voltageGenerator, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
						SnapComponentToGrid(m_componentViewList.back());
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::G))
					{
						int id = m_onCircuitChange(ComponentType::ground);
						AddViewComponent(id, "Ground", ComponentType::ground, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
						SnapComponentToGrid(m_componentViewList.back());
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::C))
					{
						int id = m_onCircuitChange(ComponentType::capacitor);
						AddViewComponent(id, "Capacitor", ComponentType::capacitor, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
						SnapComponentToGrid(m_componentViewList.back());
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::L))
					{
						int id = m_onCircuitChange(ComponentType::inductor);
						AddViewComponent(id, "Inductor", ComponentType::inductor, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
						SnapComponentToGrid(m_componentViewList.back());
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
					{
						int id = m_onCircuitChange(ComponentType::switchComponent);
						AddViewComponent(id, "Switch", ComponentType::switchComponent, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
						SnapComponentToGrid(m_componentViewList.back());
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
					{
						int id = m_onCircuitChange(ComponentType::diode);
						AddViewComponent(id, "Diode", ComponentType::diode, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
						SnapComponentToGrid(m_componentViewList.back());
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::T))
					{
						int id = m_onCircuitChange(ComponentType::transformer);
						AddViewComponent(id, "Transformer", ComponentType::transformer, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
						SnapComponentToGrid(m_componentViewList.back());
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::N))
					{
						// A differenza degli altri tasti, non passa da m_onCircuitChange/Circuit:
						// un nodo vuoto non è un componente, è puramente un punto di aggancio
						// visivo (nodeId -1, come un terminale mai collegato) a cui collegare
						// dei fili in seguito, esattamente come ai terminali di un componente.
						AddNodeView(-1, SnapToGrid(sf::Vector2f(static_cast<float>(pos.x), static_cast<float>(pos.y))), true);
						// Il nodo appena creato è esattamente sotto al cursore: senza questo
						// flag, la CheckClick qui sotto lo selezionerebbe subito da solo
						// (o, se un altro nodo era già selezionato da un click precedente,
						// li collegherebbe entrambi in automatico) — vogliamo invece che
						// piazzarlo sia un'azione a parte, non anche una selezione.
						placedNode = true;
					}
				}

				// Gestione selezione e collegamento terminali:
				// - 1° click su un terminale (o un link, o un nodeView): lo seleziona
				// - 2° click su un altro terminale/link/nodeView: crea il collegamento
				// nodeViewSelected va escluso qui esattamente come linkSelected — altrimenti
				// il primo click su un nodo (es. per unire due nodi piazzati con "N") veniva
				// subito sovrascritto da una CheckClick fresca invece di arrivare al ramo
				// sotto che gestisce il secondo click.
				if (!placedNode && m_selectedComponent.state != SelectionState::terminalSelected && m_selectedComponent.state != SelectionState::linkSelected && m_selectedComponent.state != SelectionState::nodeViewSelected && !ImGui::GetIO().WantCaptureMouse)
				{
					CheckClick(pos, m_selectedComponent);

					// Un interruttore si apre/chiude cliccandoci sopra (il tasto sinistro
					// qui seleziona soltanto — il drag usa il tasto destro, vedi sotto —
					// quindi il click non fa altro che "selezionare", e possiamo far
					// scattare il toggle sulla stessa azione senza conflitti).
					if (m_selectedComponent.state == SelectionState::componentSelected &&
						m_onGetComponentTypeById(m_selectedComponent.compId) == ComponentType::switchComponent)
					{
						m_onToggleSwitch(m_selectedComponent.compId);
					}
				}
				else if (!placedNode && !ImGui::GetIO().WantCaptureMouse &&
					(m_selectedComponent.state == SelectionState::terminalSelected ||
						m_selectedComponent.state == SelectionState::linkSelected ||
						m_selectedComponent.state == SelectionState::nodeViewSelected))
				{
					// Secondo click: dopo aver selezionato un terminale, un filo o un nodo,
					// un secondo elemento dello stesso tipo li collega con un filo. Se il
					// secondo click cade su altro (spazio vuoto, un corpo di componente) non
					// succede nulla e la prima selezione resta.
					SelecetedComponent second;
					CheckClick(pos, second);

					if (second.state == SelectionState::terminalSelected ||
						second.state == SelectionState::linkSelected ||
						second.state == SelectionState::nodeViewSelected)
					{
						ConnectSelections(m_selectedComponent, second);

						m_selectedComponent.state = SelectionState::none;
						m_selectedComponent.compId = -1;
						m_selectedComponent.terminalIndex = -1;
						m_selectedComponent.linkId = -1;
						m_selectedComponent.nodeViewId = -1;
					}
				}
			}
			else if (mouseEvent->button == sf::Mouse::Button::Middle)
			{
				// Inizia il pan solo se il click è sul canvas (non su pannello/oscilloscopio)
				if (!ImGui::GetIO().WantCaptureMouse && pixelPos.x < static_cast<int>(m_width - PANEL_WIDTH))
				{
					m_panning = true;
					m_panLastPixel = pixelPos;
				}
			}
			else if (mouseEvent->button == sf::Mouse::Button::Right)
			{
				if (!ImGui::GetIO().WantCaptureMouse) CheckClick(pos, m_selectedComponent);

				bool ctrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
					sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);

				if (ctrlHeld && m_selectedComponent.state == SelectionState::linkSelected)
				{
					// Split: Ctrl+drag destro su un filo lo stacca dal suo NodeView
					// in un nuovo hub (stesso nodo elettrico, nessuna chiamata al
					// Circuit), e inizia subito a trascinarlo come un nodo normale.
					int newNodeViewId = m_graph.InsertNodeOnBusEdge(m_selectedComponent.linkId, m_selectedComponent.clickPos);
					if (newNodeViewId != -1)
					{
						m_selectedComponent.compId = -1;
						m_selectedComponent.terminalIndex = -1;
						m_selectedComponent.linkId = -1;
						m_selectedComponent.nodeViewId = newNodeViewId;

						for (auto const &nv : m_nodeViewList)
							if (nv.id == newNodeViewId)
							{
								m_compClickOffset.x = pos.x - nv.position.x;
								m_compClickOffset.y = pos.y - nv.position.y;
							}
						m_selectedComponent.state = SelectionState::draggingNodeView;
					}
				}
				else if (m_selectedComponent.state == SelectionState::componentSelected ||
					m_selectedComponent.state == SelectionState::terminalSelected)
				{
					for (auto const &comp : m_componentViewList)
						if (comp.GetComponentLink() == m_selectedComponent.compId)
						{
							m_compClickOffset.x = pos.x - comp.GetPosition().x;
							m_compClickOffset.y = pos.y - comp.GetPosition().y;
						}
					m_selectedComponent.state = SelectionState::draggingComponent;
				}
				else if (m_selectedComponent.state == SelectionState::nodeViewSelected)
				{
					for (auto const &nv : m_nodeViewList)
						if (nv.id == m_selectedComponent.nodeViewId)
						{
							m_compClickOffset.x = pos.x - nv.position.x;
							m_compClickOffset.y = pos.y - nv.position.y;
						}
					m_selectedComponent.state = SelectionState::draggingNodeView;
				}
			}
		}
		else if (const auto *mouseMovedEvent = event->getIf<sf::Event::MouseMoved>())
		{
			if (m_panning)
			{
				// Lo spostamento in pixel va diviso per lo zoom per diventare unità mondo;
				// il segno è negativo perché è la vista a muoversi in senso opposto al
				// cursore (il circuito segue la mano).
				sf::Vector2i deltaPx = mouseMovedEvent->position - m_panLastPixel;
				m_view.move(-sf::Vector2f(static_cast<float>(deltaPx.x), static_cast<float>(deltaPx.y)) / m_zoom);
				m_panLastPixel = mouseMovedEvent->position;
			}

			auto pos = WorldPos(mouseMovedEvent->position);
			Vec2 newPos;

			// Il trascinamento resta confinato alla porzione di mondo oggi visibile
			// (prima coincideva con [0, larghezza canvas] x [0, altezza]).
			const sf::Vector2f viewHalf = m_view.getSize() / 2.0f;
			const sf::Vector2f viewMin = m_view.getCenter() - viewHalf;
			const sf::Vector2f viewMax = m_view.getCenter() + viewHalf;

			if (m_selectedComponent.state == SelectionState::draggingComponent)
			{
				newPos.x = std::clamp(pos.x - m_compClickOffset.x, viewMin.x, viewMax.x);
				newPos.y = std::clamp(pos.y - m_compClickOffset.y, viewMin.y, viewMax.y);

				for (auto &cw : m_componentViewList)
					if (cw.GetComponentLink() == m_selectedComponent.compId)
					{
						cw.SetPosition(newPos);
						SnapComponentToGrid(cw);
					}

				UpdateLinksForComponent(m_selectedComponent.compId);
			}
			else if (m_selectedComponent.state == SelectionState::draggingNodeView)
			{
				// Sotto la soglia, dalla posizione del click che ha avviato il
				// trascinamento, non si muove ancora nulla: un pallino ancorato resta
				// agganciato (vedi NODE_DRAG_THRESHOLD).
				const sf::Vector2f delta(pos.x - m_selectedComponent.clickPos.x, pos.y - m_selectedComponent.clickPos.y);
				const float dragThreshold = NODE_DRAG_THRESHOLD / m_zoom;
				if (delta.x * delta.x + delta.y * delta.y >= dragThreshold * dragThreshold)
				{
					sf::Vector2f nodePos = SnapToGrid(sf::Vector2f(
						std::clamp(pos.x - m_compClickOffset.x, viewMin.x, viewMax.x),
						std::clamp(pos.y - m_compClickOffset.y, viewMin.y, viewMax.y)));

					// Un NodeView ancorato a un terminale, appena lo si sposta davvero, si
					// stacca: da lì in poi non segue più il componente e il filo tra
					// terminale e nodo si vede.
					m_graph.DetachIfAnchored(m_selectedComponent.nodeViewId);
					UpdateLinksForNodeView(m_selectedComponent.nodeViewId, nodePos);
				}
			}
		}
		else if (const auto *wheelEvent = event->getIf<sf::Event::MouseWheelScrolled>())
		{
			// Zoom con la rotellina, ancorato al cursore: il punto del circuito
			// sotto il mouse resta fermo sullo schermo mentre si zooma. Ignorato
			// sopra il pannello/oscilloscopio (dove la rotellina serve a ImGui/ImPlot).
			if (wheelEvent->wheel == sf::Mouse::Wheel::Vertical &&
				!ImGui::GetIO().WantCaptureMouse &&
				wheelEvent->position.x < static_cast<int>(m_width - PANEL_WIDTH))
			{
				const sf::Vector2f before = m_window.mapPixelToCoords(wheelEvent->position, m_view);

				m_zoom = std::clamp(m_zoom * std::pow(ZOOM_STEP, wheelEvent->delta), ZOOM_MIN, ZOOM_MAX);
				m_view.setSize(sf::Vector2f(static_cast<float>(m_width - PANEL_WIDTH), static_cast<float>(m_heigth)) / m_zoom);

				const sf::Vector2f after = m_window.mapPixelToCoords(wheelEvent->position, m_view);
				m_view.move(before - after);
			}
		}
		else if (const auto *mouseReleasedEvent = event->getIf<sf::Event::MouseButtonReleased>())
		{
			if (mouseReleasedEvent->button == sf::Mouse::Button::Right)
			{
				// Un NodeView ancorato rilasciato vicino al suo terminale si riaggancia:
				// torna sul terminale e riprende a seguirlo.
				if (m_selectedComponent.state == SelectionState::draggingNodeView)
				{
					const NodeView *dragged = m_graph.FindNodeView(m_selectedComponent.nodeViewId);
					if (dragged && dragged->anchorCompId != -1)
					{
						const std::vector<sf::Vector2f> terminals = GetTerminalPositionbyCompId(dragged->anchorCompId);
						if (dragged->anchorTermIndex >= 0 && dragged->anchorTermIndex < static_cast<int>(terminals.size()))
							m_graph.TryReattach(dragged->id, terminals[dragged->anchorTermIndex], (CLICK_TOLLERANCE + NODE_RADIUS) / m_zoom);
					}
				}
				m_selectedComponent.state = SelectionState::none;
			}
			else if (mouseReleasedEvent->button == sf::Mouse::Button::Middle)
				m_panning = false;
		}
		else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete) && (m_selectedComponent.state != SelectionState::draggingComponent && m_selectedComponent.state != SelectionState::draggingNodeView))
		{
			// Eliminazione con tasto Delete
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

				// Se è un Ground, si ricordano i terminali collegati con lui (nel
				// disegno) prima di toglierlo: quelli che restano uniti tra loro senza
				// nessun'altra massa vanno staccati dallo 0 nel Circuit.
				std::vector<TerminalRef> groundGroup;
				if (m_onGetComponentTypeById(id) == ComponentType::ground)
					for (int term = 0; term < 2; term++)
					{
						int nvId = m_graph.NodeViewIdOfTerminal(id, term);
						if (nvId == -1)
							continue;
						for (const auto &terminal : m_graph.TerminalsInGroup(nvId))
							if (terminal.first != id)
								groundGroup.push_back(terminal);
					}

				// Toglie i fili e i NodeView del componente; i terminali degli altri
				// componenti rimasti senza più fili tornano liberi anche nel Circuit.
				FreeTerminals(m_graph.RemoveComponent(id));
				DetachSurvivingGroupsFromGround(groundGroup);

				m_onDeleteComponent(id);

				// Reset selezione
				m_selectedComponent.state = SelectionState::none;
				m_selectedComponent.compId = -1;
				m_selectedComponent.terminalIndex = -1;
				m_selectedComponent.nodeViewId = -1;
			}
			else if (m_selectedComponent.state == SelectionState::linkSelected)
			{
				// Un filo tra due terminali non si cancella da solo: il Circuit non sa
				// scollegare, e il disegno direbbe una cosa diversa dal circuito. Si
				// può però togliere un nodo di passaggio (annulla un nodo inserito sul
				// filo): il filo torna un unico tratto.
				int passThrough = m_graph.PassThroughNodeOfEdge(m_selectedComponent.linkId);
				if (passThrough != -1)
					FreeTerminals(m_graph.RemoveFreeNodeView(passThrough));

				m_selectedComponent.state = SelectionState::none;
				m_selectedComponent.linkId = -1;
			}
			else if (m_selectedComponent.state == SelectionState::nodeViewSelected)
			{
				// Si cancellano solo i nodi liberi (i loro vicini restano collegati tra
				// loro). Il NodeView di un terminale sparisce insieme al suo componente.
				FreeTerminals(m_graph.RemoveFreeNodeView(m_selectedComponent.nodeViewId));

				m_selectedComponent.state = SelectionState::none;
				m_selectedComponent.nodeViewId = -1;
			}
		}
		else if (const auto *keyboardEvent = event->getIf<sf::Event::KeyPressed>())
		{
			if (m_selectedComponent.state == SelectionState::componentSelected && keyboardEvent->code == sf::Keyboard::Key::Q)
			{
				for (auto &cw : m_componentViewList)
					if (cw.GetComponentLink() == m_selectedComponent.compId)
					{
						cw.SetRotation(static_cast<float>(static_cast<int>(cw.GetRotation() + 45) % 360));
						// Ruotando attorno al centro il primo terminale si sposta: lo
						// si riaggancia alla griglia (no-op se l'aggancio è disattivato).
						SnapComponentToGrid(cw);
					}

				UpdateLinksForComponent(m_selectedComponent.compId);
			}

			// Alternativa da tastiera al click per aprire/chiudere un interruttore
			// selezionato, comoda per toggle ripetuti senza dover ricliccare ogni volta.
			if (m_selectedComponent.state == SelectionState::componentSelected &&
				keyboardEvent->code == sf::Keyboard::Key::Space &&
				m_onGetComponentTypeById(m_selectedComponent.compId) == ComponentType::switchComponent)
			{
				m_onToggleSwitch(m_selectedComponent.compId);
			}
		}
	}
}

void CircuitLab::UI::DrawImageGuiPanel()
{
	ImGui::SetNextWindowPos({ static_cast<float>(m_width - PANEL_WIDTH), 0.0f });
	ImGui::SetNextWindowSize({ PANEL_WIDTH, static_cast<float>(m_heigth) });
	// --- Pannello ImGui ---
	ImGui::Begin("CircuitLab - Test", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	if (ImGui::Button("Start Simulation"))
		m_onSetSimulationStatus(SimulationStatus::running);
	if (ImGui::Button("Pause Simulation"))
		m_onSetSimulationStatus(SimulationStatus::paused);
	if (ImGui::Button("Stop Simulation"))
		m_onSetSimulationStatus(SimulationStatus::stopped);

	ImGui::Separator();

	const char *timestepNames[] = {
		"1 s", "100 ms", "10 ms", "1 ms",
		"100 us", "10 us", "1 us", "100 ns", "10 ns", "1 ns", "100 ps", "10 ps", "1 ps"
	};
	constexpr int timestepCount = static_cast<int>(std::size(timestepNames));

	if (ImGui::Combo("Timestep", &m_hSimIndex, timestepNames, timestepCount))
		m_onSetHSim(m_hSimIndex);

	ImGui::Separator();

	if (ImGui::Button("New"))
		m_onNew();

	static char pathBuffer[256] = "circuit.json";
	ImGui::InputText("File", pathBuffer, sizeof(pathBuffer));

	if (ImGui::Button("Save"))
		m_onSave(pathBuffer);

	if (ImGui::Button("Load"))
		m_onLoad(pathBuffer);

	ImGui::Separator();
	if (ImGui::Button(m_showOscilloscope ? "Hide Oscilloscope" : "Show Oscilloscope"))
		m_showOscilloscope = !m_showOscilloscope;
	ImGui::Separator();

	ImGui::Checkbox("Mostra griglia", &m_showGrid);
	ImGui::Checkbox("Aggancia alla griglia", &m_snapToGrid);
	const char *gridSizeNames[] = { "10", "20", "40" };
	const int gridSizes[] = { 10, 20, 40 };
	int gridSizeIndex = 1;
	for (int i = 0; i < 3; i++)
		if (gridSizes[i] == m_gridSize)
			gridSizeIndex = i;
	if (ImGui::Combo("Passo griglia", &gridSizeIndex, gridSizeNames, 3))
		m_gridSize = gridSizes[gridSizeIndex];
	ImGui::Text("Zoom: %d%%", static_cast<int>(std::lround(m_zoom * 100.0f)));
	ImGui::SameLine();
	if (ImGui::Button("Reset zoom"))
		ResetZoom();
	ImGui::Separator();

	// Mostra il risultato della simulazione o un messaggio di errore
	if (m_simulationOutput.simRes == SimulationResult::solve_error)
		ImGui::Text("Circuito non risolvibile!");
	else if (m_simulationOutput.simRes == SimulationResult::empty_circuit)
		ImGui::Text("Il circuito non contiene componenti!");
	else if (m_simulationOutput.simRes == SimulationResult::no_circuit)
		ImGui::Text("Errore interno, puntatore a circuito nullo!");
	else if (m_simulationOutput.simRes == SimulationResult::only_ground_circuit)
		ImGui::Text("Il circuito contiene solo componenti ground!");
	else if (m_simulationOutput.simRes == SimulationResult::disconnected_terminal)
		ImGui::Text("Circuito non valido: c'e' un terminale scollegato o un ramo aperto. Simulazione interrotta.");
	else if (m_simulationOutput.simRes == SimulationResult::no_convergence)
		ImGui::Text("La simulazione non converge (componente non lineare).");
	else
	{
		// Valori con prefisso SI ("18.16 V", "184.2 mA", "4.3 uA"): std::to_string
		// ne stampava sempre 6 decimali, e ogni valore sotto 1e-6 (o qualche uA)
		// perdeva le cifre significative, mostrando "0.000000".
		ImGui::Text("Risultato: [");
		for (const auto &r : m_simulationOutput.res)
		{
			// "V<n>" = tensione del nodo n, "I(...)" = corrente in una sorgente di tensione
			const char *unit = (!r.first.empty() && r.first[0] == 'I') ? "A" : "V";
			ImGui::Text("%s  %s", r.first.c_str(), FormatEngineering(r.second, unit).c_str());
		}
		ImGui::Text("]");

		// Ordinate per id: currentComp è una unordered_map, il cui ordine di
		// iterazione non ha alcun significato e può cambiare da un output all'altro.
		std::vector<std::pair<int, double>> currents(m_simulationOutput.currentComp.begin(), m_simulationOutput.currentComp.end());
		std::sort(currents.begin(), currents.end());

		ImGui::Text("Correnti: [");
		for (const auto &[compId, current] : currents)
		{
			// Stesso nome del canvas ("R2", "C10"); "comp<id>" solo per un id senza tipo noto
			const std::string prefix = ComponentPrefix(m_onGetComponentTypeById(compId));
			const std::string name = (prefix.empty() ? std::string("comp") : prefix) + std::to_string(compId);
			ImGui::Text("%s  %s", name.c_str(), FormatEngineering(current, "A").c_str());
		}
		ImGui::Text("]");
	}

	if (m_selectedComponent.state == SelectionState::componentSelected)
	{
		float posX, posY, rot;
		bool edited = false;
		std::map<ComponentValue, double> values;
		for (auto &cw : m_componentViewList)
			if (cw.GetComponentLink() == m_selectedComponent.compId)
			{
				posX = cw.GetPosition().x;
				posY = cw.GetPosition().y;
				rot = cw.GetRotation();
				ImGui::InputFloat("Posizione X: ", &posX); if (ImGui::IsItemDeactivatedAfterEdit()) edited = true;
				ImGui::InputFloat("Posizione Y: ", &posY); if (ImGui::IsItemDeactivatedAfterEdit()) edited = true;
				ImGui::InputFloat("Rotazione: ", &rot); if (ImGui::IsItemDeactivatedAfterEdit()) edited = true;

				if (edited)
				{
					Vec2 newPos;
					newPos.x = posX;
					newPos.y = posY;
					cw.SetRotation(rot);
					cw.SetPosition(newPos);
					UpdateLinksForComponent(m_selectedComponent.compId);
				}
			}

		values = m_onGetComponentValues(m_selectedComponent.compId);
		for (auto &[key, value] : values)
		{
			std::string label(ComponentValueToString(key));
			// %.6g e non %.6f: con 6 decimali fissi qualunque valore sotto 1e-6
			// (un condensatore da 470 nF, Is = 1e-14 del diodo) si vedeva come
			// "0.000000" pur essendo salvato correttamente nel circuito, e sembrava
			// azzerato. %g passa da solo alla notazione scientifica ("4.7e-07").
			ImGui::InputDouble(label.c_str(), &value, 0.0, 0.0, "%.6g");
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				values.at(key) = value;
				m_onSetComponentValues(m_selectedComponent.compId, values);
			}

			// Per valori piccoli o grandi la notazione scientifica è scomoda da
			// leggere: si affianca la forma con prefisso SI ("= 470 nF", "= 1 kOhm").
			const char *unit = ComponentValueUnit(key);
			if (unit[0] != '\0' && value != 0.0 && (std::abs(value) < 1e-3 || std::abs(value) >= 1e3))
				ImGui::TextDisabled("= %s", FormatEngineering(value, unit).c_str());
		}

		// Combo box waveform — visibile solo per VoltageGenerator
		WaveFormType currentWaveForm = m_onGetWaveFormType(m_selectedComponent.compId);
		if (currentWaveForm != WaveFormType::none)
		{
			const char *waveFormNames[] = { "DC", "Sine", "Square" };
			WaveFormType waveFormValues[] = {
				WaveFormType::dcWaveForm,
				WaveFormType::sineWaveForm,
				WaveFormType::squareWaveForm
			};

			constexpr int waveFormCount = static_cast<int>(std::size(waveFormNames));

			int currentIndex = 0;
			for (int i = 0; i < waveFormCount; i++)
				if (waveFormValues[i] == currentWaveForm)
					currentIndex = i;

			if (ImGui::Combo("Waveform", &currentIndex, waveFormNames, waveFormCount))
				m_onSetWaveFormType(m_selectedComponent.compId, waveFormValues[currentIndex]);
		}

	}

	ImGui::End();

	// Dopo ImGui::End() chiama DrawOscilloscope se visibile
	if (m_showOscilloscope)
		DrawOscilloscope();
}

void CircuitLab::UI::DrawComponents()
{
	// Disegna ogni componente: simbolo schematico per tipo + cerchi per i terminali + etichetta
	for (const auto &comp : m_componentViewList)
	{
		ComponentDesign des = comp.GetComponetDesign();

		// Selezionato = corpo (non terminale) del componente corrente
		bool isSelected = comp.GetComponentLink() == m_selectedComponent.compId &&
			m_selectedComponent.terminalIndex == -1 &&
			m_selectedComponent.state == SelectionState::componentSelected;
		sf::Color symbolColor = isSelected ? sf::Color::Yellow : sf::Color::White;

		WaveFormType waveForm = (comp.GetComponentType() == ComponentType::voltageGenerator && m_onGetWaveFormType)
			? m_onGetWaveFormType(comp.GetComponentLink())
			: WaveFormType::none;

		bool switchClosed = (comp.GetComponentType() == ComponentType::switchComponent && m_onIsSwitchClosed)
			? m_onIsSwitchClosed(comp.GetComponentLink())
			: true;

		comp.DrawSymbol(m_window, symbolColor, waveForm, switchClosed);

		// Disegna i terminali come cerchi blu
		sf::CircleShape term(static_cast<float>(des.terminalRadius));
		term.setFillColor(sf::Color::Blue);
		term.setOrigin({ static_cast<float>(des.terminalRadius), static_cast<float>(des.terminalRadius) });
		std::vector<int> terminalsId = m_onGetCompTerminalId(comp.GetComponentLink());

		int strPosX = 0, strPosY = 0;

		for (int i = 0; i < des.terminalOffset.size(); i++)
		{
			sf::Text termLabel(m_font);

			sf::Vector2f rotTerm = GetRotatedTerminalPos(comp, i);

			strPosX = (rotTerm.x >= 0) ? 1 : -1;
			strPosY = (rotTerm.y >= 0) ? 1 : -1;

			term.setPosition({ comp.GetPosition().x + rotTerm.x,comp.GetPosition().y + rotTerm.y });

			// Outline giallo se questo terminale è selezionato
			if (comp.GetComponentLink() == m_selectedComponent.compId &&
				m_selectedComponent.terminalIndex == i &&
				m_selectedComponent.state == SelectionState::terminalSelected)
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
		std::string compString = ComponentPrefix(comp.GetComponentType());

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
			sf::Vertex{wire.startPos, (m_selectedComponent.state == SelectionState::linkSelected && wire.id == m_selectedComponent.linkId) ? sf::Color::Red : sf::Color::White},
			sf::Vertex{GetNodeviewPositionByNodeViewId(wire.nodeViewId), (m_selectedComponent.state == SelectionState::linkSelected && wire.id == m_selectedComponent.linkId) ? sf::Color::Red : sf::Color::White}
		};
		m_window.draw(line, 2, sf::PrimitiveType::Lines);

		DrawParticles(wire.id);
	}
}

void CircuitLab::UI::DrawNodes()
{
	for (const auto &nv : m_nodeViewList)
	{
		// Disegna i nodi come cerchi verdi
		sf::CircleShape node(NODE_RADIUS);
		node.setFillColor(sf::Color::Green);
		node.setOrigin({ NODE_RADIUS, NODE_RADIUS });
		node.setPosition(nv.position);

		// Outline giallo se questo nodo è selezionato
		if (nv.id == m_selectedComponent.nodeViewId && m_selectedComponent.state == SelectionState::nodeViewSelected)
		{
			node.setOutlineColor(sf::Color::Yellow);
			node.setOutlineThickness(OUTLINE_THICKNESS);
		}

		m_window.draw(node);
		node.setOutlineThickness(0); // Reset per il prossimo nodo

		std::string nodeString;
	}
}

void CircuitLab::UI::DrawParticles(int linkId)
{
	for (const auto &lv : m_linkViewList)
	{
		if (lv.id == linkId)
		{
			sf::Vector2f nodeViewPos = GetNodeviewPositionByNodeViewId(lv.nodeViewId);

			// Colore in base al segno della corrente su questo filo: distingue a
			// colpo d'occhio il verso, oltre alla direzione di movimento dei pallini
			// (che da sola può essere poco evidente, specialmente con correnti piccole).
			double current = m_linkViewCurrentList.count(linkId) ? m_linkViewCurrentList.at(linkId) : 0.0;
			sf::Color particleColor = (current > PARTICLE_CURRENT_SIGN_EPSILON) ? PARTICLE_COLOR_POSITIVE
				: (current < -PARTICLE_CURRENT_SIGN_EPSILON) ? PARTICLE_COLOR_NEGATIVE
				: PARTICLE_COLOR_NEUTRAL;

			for (const auto &lp : m_linkParticlesList)
			{
				if (lp.linkViewId == linkId)
				{
					for (int i = 0; i < lp.count; i++)
					{
						float t_i = std::fmod(lp.offset + static_cast<float>(i) / lp.count, 1.0f);
						sf::Vector2f pPos;
						pPos.x = lv.startPos.x + (nodeViewPos.x - lv.startPos.x) * t_i;
						pPos.y = lv.startPos.y + (nodeViewPos.y - lv.startPos.y) * t_i;
						sf::CircleShape particle(NODE_RADIUS);
						particle.setFillColor(particleColor);
						particle.setOrigin({ NODE_RADIUS, NODE_RADIUS });
						particle.setPosition({ pPos.x,pPos.y });
						m_window.draw(particle);
					}
				}
			}
		}
	}
}

void CircuitLab::UI::DrawOscilloscope()
{
	// Finestra compatta e ridimensionabile: il grafico riempie lo spazio che resta
	// invece di avere un'altezza fissa. Posizione/dimensione iniziali valgono solo
	// la prima volta (poi le ricorda imgui.ini); l'ID "##scope2" è nuovo apposta,
	// così non si eredita la vecchia dimensione, molto più grande, già salvata lì.
	ImGui::SetNextWindowSize(ImVec2(440.0f, 300.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(12.0f, static_cast<float>(m_heigth) - 312.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, 180.0f), ImVec2(FLT_MAX, FLT_MAX));
	if (!ImGui::Begin("Oscilloscope##scope2", &m_showOscilloscope))
	{
		ImGui::End();
		return;
	}

	// Snapshot dei canali: aggiornato ogni frame finché non si è congelati, poi
	// resta fermo sull'ultimo dato ricevuto (vedi commento su m_frozenChannels in UI.h).
	// Usato SOLO per il plot: la lista di gestione canali (checkbox/rimozione) sotto
	// continua a leggere i dati live, così restano utilizzabili anche da congelato.
	if (!m_oscFrozen)
		m_frozenChannels = m_onGetOscilloscopeChannels();

	// Nodi e componenti disponibili come sorgente di un canale
	std::vector<int> nodeIds;
	for (auto &[id, v] : m_simulationOutput.nodeVoltages)
		nodeIds.push_back(id);
	std::sort(nodeIds.begin(), nodeIds.end());

	std::vector<int> compIds;
	for (auto &[id, v] : m_simulationOutput.currentComp)
		compIds.push_back(id);
	std::sort(compIds.begin(), compIds.end());

	// Clamp degli indici, che restano validi anche se il circuito cambia
	if (!nodeIds.empty())
	{
		m_oscIdA = std::min(m_oscIdA, static_cast<int>(nodeIds.size()) - 1);
		m_oscIdB = std::min(m_oscIdB, static_cast<int>(nodeIds.size()) - 1);
	}
	if (!compIds.empty())
		m_oscCompId = std::min(m_oscCompId, static_cast<int>(compIds.size()) - 1);

	// --- Barra 1: nuovo canale, finestra temporale, sincronizzazione ---
	if (ImGui::Button("+ Canale"))
		ImGui::OpenPopup("##addChannel");

	if (ImGui::BeginPopup("##addChannel"))
	{
		std::vector<std::string> nodeLabels;
		for (int id : nodeIds)
			nodeLabels.push_back("Node " + std::to_string(id));

		// I componenti si mostrano col nome che hanno sul canvas ("R6", "C10", ...)
		std::vector<std::string> compLabels;
		for (int id : compIds)
			compLabels.push_back(std::string(ComponentPrefix(m_onGetComponentTypeById(id))) + std::to_string(id));

		auto labelCombo = [](const char *label, int &index, const std::vector<std::string> &labels)
		{
			if (labels.empty())
				return;
			if (ImGui::BeginCombo(label, labels[index].c_str()))
			{
				for (int i = 0; i < static_cast<int>(labels.size()); i++)
				{
					const bool selected = (i == index);
					if (ImGui::Selectable(labels[i].c_str(), selected))
						index = i;
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		};

		ImGui::TextUnformatted("Nuovo canale");
		ImGui::Separator();
		ImGui::SetNextItemWidth(170.0f);

		const char *probeTypeNames[] = {
			"Tensione di nodo", "Tensione differenziale",
			"Corrente di componente", "Corrente di ramo"
		};
		ImGui::Combo("Sonda", &m_oscProbeType, probeTypeNames, static_cast<int>(std::size(probeTypeNames)));
		const ProbeType selectedType = static_cast<ProbeType>(m_oscProbeType);

		ImGui::SetNextItemWidth(170.0f);
		if (selectedType == ProbeType::componentCurrent)
			labelCombo("Componente", m_oscCompId, compLabels);
		else
			labelCombo(selectedType == ProbeType::nodeVoltage ? "Nodo" : "Nodo A", m_oscIdA, nodeLabels);

		if (selectedType == ProbeType::differentialVoltage || selectedType == ProbeType::branchCurrent)
		{
			ImGui::SetNextItemWidth(170.0f);
			labelCombo("Nodo B", m_oscIdB, nodeLabels);
		}

		if (selectedType == ProbeType::branchCurrent)
		{
			ImGui::SetNextItemWidth(170.0f);
			labelCombo("Componente", m_oscCompId, compLabels);
		}

		if (ImGui::Button("Aggiungi"))
		{
			int idA = -1, idB = -1, compId = -1;

			if (selectedType == ProbeType::componentCurrent)
				compId = compIds.empty() ? -1 : compIds[m_oscCompId];
			else
				idA = nodeIds.empty() ? -1 : nodeIds[m_oscIdA];

			if (selectedType == ProbeType::differentialVoltage ||
				selectedType == ProbeType::branchCurrent)
				idB = nodeIds.empty() ? -1 : nodeIds[m_oscIdB];

			if (selectedType == ProbeType::branchCurrent)
				compId = compIds.empty() ? -1 : compIds[m_oscCompId];

			if (idA != -1 || compId != -1)
				m_onAddChannel(selectedType, idA, idB, compId);

			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Finestra");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::InputDouble("##window", &m_windowTime, 0.0, 0.0, "%.4g"))
	{
		if (m_windowTime < 0.000001) m_windowTime = 0.000001;
		m_onSetWindowTime(m_windowTime);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Larghezza della finestra temporale visibile, in secondi");
	ImGui::SameLine();
	ImGui::TextUnformatted("s");

	ImGui::SameLine();
	if (ImGui::Button("Auto Sync"))
		m_onAutoSync();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Imposta la finestra a 4 periodi della frequenza piu' alta del circuito");

	// --- Barra 2: opzioni di visualizzazione ---
	// I controlli vanno a capo da soli quando la finestra è stretta: dopo un elemento
	// si resta sulla stessa riga solo se il successivo ci sta ancora. (Non si può
	// guardare GetContentRegionAvail dopo l'elemento: il cursore è già sulla riga
	// dopo e riporta sempre la larghezza intera.)
	const ImGuiStyle &style = ImGui::GetStyle();
	const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
	auto sameLineIfFits = [&](float width)
	{
		if (ImGui::GetItemRectMax().x + style.ItemSpacing.x + width <= rowRight)
			ImGui::SameLine();
	};
	auto checkboxWidth = [&](const char *label)
	{
		return ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
	};
	auto buttonWidth = [&](const char *label)
	{
		return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
	};

	const char *modeNames[] = { "Scorrimento", "Sweep" };
	ImGui::SetNextItemWidth(105.0f);
	ImGui::Combo("##mode", &m_oscMode, modeNames, static_cast<int>(std::size(modeNames)));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Scorrimento: la traccia cammina nel tempo.\n"
			"Sweep: finestra fissa, la traccia riparte da sinistra e si ridisegna sopra la precedente.");

	sameLineIfFits(checkboxWidth("Freeze"));
	ImGui::Checkbox("Freeze", &m_oscFrozen);

	sameLineIfFits(checkboxWidth("Auto Y"));
	ImGui::Checkbox("Auto Y", &m_oscAutoY);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Adatta l'asse Y ai dati visibili");

	sameLineIfFits(buttonWidth("Fit"));
	if (ImGui::Button("Fit"))
		ImPlot::SetNextAxesToFit();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Adatta una volta gli assi ai dati");

	sameLineIfFits(checkboxWidth("Misure"));
	ImGui::Checkbox("Misure", &m_oscShowMeasures);

	const char *notationNames[] = { "Assi: Auto", "Assi: Scientifica", "Assi: SI" };
	sameLineIfFits(125.0f);
	ImGui::SetNextItemWidth(125.0f);
	ImGui::Combo("##notation", &m_oscNotation, notationNames, static_cast<int>(std::size(notationNames)));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Formato dei numeri sugli assi:\n"
			"Auto: quello predefinito di ImPlot.\n"
			"Scientifica: 2.5e-3\n"
			"SI: 2.5 m (con unita': 2.5 ms, 2.5 mV)");

	sameLineIfFits(ImGui::CalcTextSize("(?)").x);
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Trascina per spostare, rotellina per zoom.\nL'asse X si puo' spostare/zoomare solo con Freeze attivo.");

	// --- Legenda dei canali: una "pillola" per canale (attiva, nome, rimuovi) ---
	// Le pillole vanno a capo da sole quando non c'è più spazio in riga.
	auto channels = m_onGetOscilloscopeChannels();
	bool removedChannel = false;
	int activeCount = 0;
	for (int i = 0; i < static_cast<int>(channels.size()); i++)
	{
		const auto &channel = channels[i];
		if (channel.active)
			activeCount++;

		const float chipWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
			ImGui::CalcTextSize(channel.label.c_str()).x + style.ItemInnerSpacing.x +
			ImGui::CalcTextSize("x").x + style.FramePadding.x * 2.0f;
		if (i > 0)
			sameLineIfFits(chipWidth);

		const ImVec4 color(channel.channelColor.r, channel.channelColor.g, channel.channelColor.b, 1.0f);

		ImGui::PushID(i);
		ImGui::PushStyleColor(ImGuiCol_CheckMark, color);
		bool active = channel.active;
		if (ImGui::Checkbox("##active", &active))
			m_onSetChannelActive(i, active);
		ImGui::PopStyleColor();

		ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
		ImGui::TextColored(channel.active ? color : ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", channel.label.c_str());

		ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
		if (ImGui::SmallButton("x"))
		{
			m_onRemoveChannel(i);
			removedChannel = true;
		}
		ImGui::PopID();

		if (removedChannel)
			break;
	}

	// Ricava l'asse X (scorrevole o congelato)
	// Bug 1 fix — xscale corretto: distanza reale tra campioni
	// Ogni campione dista h_sim * decimationFactor secondi simulati
	const double hSim = m_onGetHSim();
	const int decimationFactor = m_onGetDecimationFactor();
	const double xscale = hSim * static_cast<double>(decimationFactor);

	double tMax;
	if (m_oscFrozen)
	{
		// Finestra congelata: riusa l'ultimo tMax calcolato prima del freeze,
		// così sia i limiti dell'asse sia il posizionamento dei campioni restano fermi.
		tMax = m_frozenTMax;
	}
	else
	{
		// Bug 2 fix — asse X scorrevole aggiornato ogni frame
		tMax = m_onGetSimulationTime();
		double fMax = m_onGetMaxFrequency();

		// Trigger matematico — allinea tMin al multiplo del periodo
		if (fMax > 0.0)
		{
			double T = 1.0 / fMax;
			// Trova il multiplo intero di T più vicino a tMax - windowTime
			double tMinRaw = tMax - m_windowTime;
			double tMin = std::floor(tMinRaw / T) * T;
			tMax = tMin + m_windowTime;
		}

		m_frozenTMax = tMax;
	}

	const double tMin = tMax - m_windowTime;

	// Altezza riservata alla tabella delle misure (se attiva): il grafico prende
	// tutto il resto della finestra, con un minimo per restare leggibile.
	float measuresHeight = 0.0f;
	if (m_oscShowMeasures)
	{
		const int rows = std::max(1, activeCount) + 1; // + riga di intestazione
		measuresHeight = rows * (ImGui::GetTextLineHeight() + style.CellPadding.y * 2.0f) + style.ItemSpacing.y;
	}
	const float plotHeight = std::max(80.0f, ImGui::GetContentRegionAvail().y - measuresHeight);

	std::vector<ChannelMeasure> measures;

	// Sweep: la passata dura sweepLength >= finestra, sempre multiplo del periodo
	// della frequenza massima del circuito (senza sorgenti AC, semplicemente la
	// finestra). Ogni passata parte a un multiplo di sweepLength: così un segnale
	// periodico riparte sempre alla stessa fase, come con un trigger, e i tratti
	// tra "finestra" e "sweepLength" restano vuoti (il tempo di ritorno del raggio).
	const bool sweepMode = (m_oscMode == OSC_MODE_SWEEP);
	double sweepLength = m_windowTime;
	if (sweepMode)
	{
		const double fMaxSweep = m_onGetMaxFrequency();
		if (fMaxSweep > 0.0)
		{
			const double period = 1.0 / fMaxSweep;
			sweepLength = std::max(period, std::ceil(m_windowTime / period - 1e-9) * period);
		}
	}

	// Legenda già mostrata sopra: quella interna di ImPlot sarebbe un doppione.
	if (!removedChannel && ImPlot::BeginPlot("##oscilloscope", ImVec2(-1, plotHeight), ImPlotFlags_NoLegend))
	{
		// Auto Y: l'asse segue i dati visibili (RangeFit = solo quelli nella finestra X)
		const ImPlotAxisFlags yFlags = m_oscAutoY ? (ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit) : ImPlotAxisFlags_None;
		ImPlot::SetupAxes("t (s)", nullptr, ImPlotAxisFlags_None, yFlags);

		// Formato dei numeri sugli assi. Il formattatore vale anche per le etichette
		// che compaiono passando il mouse sul grafico. AxisNotation vive fino a
		// EndPlot, che è quando ImPlot lo usa. Con "Auto" non si imposta nulla:
		// ImPlot riparte dal proprio formato ad ogni frame.
		AxisNotation xNotation{ 0, "s" }, yNotation{ 0, "" };
		if (m_oscNotation != OSC_NOTATION_AUTO)
		{
			// L'unità dell'asse Y è nota solo se tutti i canali attivi sono dello
			// stesso tipo (tensioni o correnti); altrimenti si mostra il solo prefisso.
			bool anyVoltage = false, anyCurrent = false;
			for (const auto &ch : channels)
			{
				if (!ch.active)
					continue;
				if (ch.type == ProbeType::nodeVoltage || ch.type == ProbeType::differentialVoltage)
					anyVoltage = true;
				else
					anyCurrent = true;
			}
			yNotation.unit = (anyVoltage && !anyCurrent) ? "V" : (anyCurrent && !anyVoltage) ? "A" : "";

			xNotation.notation = yNotation.notation = (m_oscNotation == OSC_NOTATION_SCIENTIFIC) ? 1 : 2;
			ImPlot::SetupAxisFormat(ImAxis_X1, FormatAxisTick, &xNotation);
			ImPlot::SetupAxisFormat(ImAxis_Y1, FormatAxisTick, &yNotation);
		}

		// Bug 2 fix — ImPlotCond_Always per aggiornare ogni frame.
		// Quando l'oscilloscopio è congelato (m_oscFrozen), NON forziamo più i limiti
		// dell'asse X ad ogni frame: così ImPlot mantiene l'ultimo stato (compreso
		// pan/zoom manuale dell'utente), esattamente come già fa per l'asse Y sotto.
		// In sweep l'asse X è fisso: sempre [0, finestra], la traccia si sposta dentro.
		if (!m_oscFrozen)
		{
			if (sweepMode)
				ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, m_windowTime, ImPlotCond_Always);
			else
				ImPlot::SetupAxisLimits(ImAxis_X1, tMin, tMax, ImPlotCond_Always);
		}
		ImPlot::SetupAxisLimits(ImAxis_Y1, -15, 15, ImPlotCond_Once);

		std::vector<std::pair<int, std::vector<double>>> plotted; // (indice canale, campioni) per le misure
		for (int i = 0; i < static_cast<int>(m_frozenChannels.size()); i++)
		{
			const auto &channel = m_frozenChannels[i];

			// L'active flag va letto dalla lista live (channels), non dallo snapshot:
			// così accendere/spegnere un canale ha effetto immediato anche da congelato.
			bool active = (i < static_cast<int>(channels.size())) ? channels[i].active : channel.active;
			if (!active || channel.samples.empty())
				continue;

			ImPlotSpec spec;
			spec.LineColor = ImVec4(
				channel.channelColor.r,
				channel.channelColor.g,
				channel.channelColor.b,
				1.0f);

			// Bug 3 fix — passa direttamente il deque convertito in vector<double>
			// senza conversione a float
			std::vector<double> samples(
				channel.samples.begin(),
				channel.samples.end());

			int count = static_cast<int>(samples.size());

			// L'indice nell'ID tiene distinti due canali con lo stesso nome
			const std::string plotLabel = channel.label + "##" + std::to_string(i);

			if (!sweepMode)
			{
				// xstart: il primo campione si trova a tMax - count*xscale
				double xstart = tMax - (count * xscale);
				ImPlot::PlotLine(plotLabel.c_str(),
					samples.data(),
					count,
					xscale,
					xstart,
					spec);
			}
			else
			{
				// Asse dei tempi ricavato dai campioni stessi: il campione i è a
				// t0 + i*xscale, con t0 ricavato dal tempo dell'ultimo campione
				// (copiato insieme ai dati, quindi coerente con essi).
				const double tLast = channel.lastSampleTime;
				const double t0 = tLast - (count - 1) * xscale;

				// La passata corrente parte all'ultimo multiplo di sweepLength;
				// cursorX è la posizione del "raggio" dentro la finestra.
				const double curStart = std::floor(tLast / sweepLength) * sweepLength;
				const double cursorX = tLast - curStart;
				const double prevStart = curStart - sweepLength;

				// Primo campione della passata corrente (0 se la storia non arriva
				// fin lì: si disegna solo ciò che c'è)
				const int curFirst = std::clamp(static_cast<int>(std::ceil((curStart - t0) / xscale - 1e-9)), 0, count);

				// Residuo della passata precedente: solo a destra del raggio, e attenuato,
				// così il nuovo tracciato lo "cancella" avanzando. Va disegnato PRIMA
				// della traccia corrente, che così resta sopra.
				if (cursorX < m_windowTime)
				{
					const int prevFirst = std::max(0, static_cast<int>(std::ceil((prevStart + cursorX - t0) / xscale - 1e-9)));
					const int prevLast = std::min(curFirst - 1, static_cast<int>(std::floor((prevStart + m_windowTime - t0) / xscale + 1e-9)));
					if (prevLast >= prevFirst)
					{
						ImPlotSpec dimSpec = spec;
						dimSpec.LineColor.w = 0.35f;
						ImPlot::PlotLine((plotLabel + "prev").c_str(),
							samples.data() + prevFirst,
							prevLast - prevFirst + 1,
							xscale,
							t0 + prevFirst * xscale - prevStart,
							dimSpec);
					}
				}

				// Passata corrente: dal suo inizio fino al raggio (o alla fine della
				// finestra, se il raggio è nel tratto di ritorno)
				if (curFirst < count)
				{
					const double xstart = t0 + curFirst * xscale - curStart;
					const int visible = static_cast<int>(std::floor((m_windowTime - xstart) / xscale + 1e-9)) + 1;
					const int n = std::min(count - curFirst, visible);
					if (n > 0)
						ImPlot::PlotLine(plotLabel.c_str(),
							samples.data() + curFirst,
							n,
							xscale,
							xstart,
							spec);
				}
			}

			if (m_oscShowMeasures)
				plotted.emplace_back(i, std::move(samples));
		}

		// Misure sui campioni oggi visibili (dopo i PlotLine i limiti sono definitivi)
		if (m_oscShowMeasures)
		{
			const ImPlotRect limits = ImPlot::GetPlotLimits();
			for (const auto &[index, samples] : plotted)
			{
				const auto &channel = m_frozenChannels[index];
				const int count = static_cast<int>(samples.size());

				int first, last;
				if (sweepMode)
				{
					// In sweep la parte disegnata cambia a ogni passata e il "raggio" si
					// sposta: si misura sull'ultima finestra di campioni, uguale in
					// ampiezza a quella che si vedrebbe in modalità scorrimento.
					const int windowSamples = std::max(1, static_cast<int>(std::floor(m_windowTime / xscale)) + 1);
					last = count - 1;
					first = std::max(0, count - windowSamples);
				}
				else
				{
					const double xstart = tMax - (count * xscale);
					first = static_cast<int>(std::ceil((limits.X.Min - xstart) / xscale));
					last = static_cast<int>(std::floor((limits.X.Max - xstart) / xscale));
					first = std::clamp(first, 0, count - 1);
					last = std::clamp(last, 0, count - 1);
				}
				if (last < first)
					continue;

				double lo = samples[first], hi = samples[first], sum = 0.0, sumSquares = 0.0;
				for (int k = first; k <= last; k++)
				{
					lo = std::min(lo, samples[k]);
					hi = std::max(hi, samples[k]);
					sum += samples[k];
					sumSquares += samples[k] * samples[k];
				}
				const int n = last - first + 1;

				ChannelMeasure m;
				m.label = channel.label;
				m.color = ImVec4(channel.channelColor.r, channel.channelColor.g, channel.channelColor.b, 1.0f);
				m.unit = (channel.type == ProbeType::nodeVoltage || channel.type == ProbeType::differentialVoltage) ? "V" : "A";
				m.peakToPeak = hi - lo;
				m.mean = sum / n;
				m.rms = std::sqrt(sumSquares / n);
				m.frequency = EstimateFrequency(samples, first, last, xscale, m.mean, m.peakToPeak);
				measures.push_back(std::move(m));
			}
		}

		ImPlot::EndPlot();
	}

	// --- Tabella delle misure ---
	if (m_oscShowMeasures)
	{
		if (ImGui::BeginTable("##measures", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
		{
			ImGui::TableSetupColumn("Canale");
			ImGui::TableSetupColumn("Vpp");
			ImGui::TableSetupColumn("Media");
			ImGui::TableSetupColumn("RMS");
			ImGui::TableSetupColumn("Freq.");
			ImGui::TableHeadersRow();

			for (const ChannelMeasure &m : measures)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextColored(m.color, "%s", m.label.c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(FormatEngineering(m.peakToPeak, m.unit).c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(FormatEngineering(m.mean, m.unit).c_str());
				ImGui::TableSetColumnIndex(3);
				ImGui::TextUnformatted(FormatEngineering(m.rms, m.unit).c_str());
				ImGui::TableSetColumnIndex(4);
				ImGui::TextUnformatted(m.frequency > 0.0 ? FormatEngineering(m.frequency, "Hz").c_str() : "-");
			}
			ImGui::EndTable();
		}
	}

	ImGui::End();
}

std::string_view CircuitLab::UI::ComponentValueToString(CircuitLab::ComponentValue value)
{
	switch (value)
	{
	case ComponentValue::resistance: return "Resistance";
	case ComponentValue::voltage:    return "Voltage";
	case ComponentValue::amplitude:  return "Amplitude";
	case ComponentValue::frequency:  return "Frequency";
	case ComponentValue::phase:      return "Phase";
	case ComponentValue::capacitance: return "Capacitance";
	case ComponentValue::inductance: return "Inductance";
	case ComponentValue::saturationCurrent: return "Sat. current (Is)";
	case ComponentValue::emissionCoefficient: return "Emission coeff. (n)";
	case ComponentValue::primaryInductance: return "Primary inductance (L1)";
	case ComponentValue::secondaryInductance: return "Secondary inductance (L2)";
	case ComponentValue::couplingCoefficient: return "Coupling (k)";
	default:                         return "Unknown";
	}
}

float CircuitLab::UI::PointToStraightDistance(const sf::Vector2f &A, const sf::Vector2f &B, const sf::Vector2f &P)
{
	Vec2 AB(A.x - B.x, A.y - B.y);
	Vec2 AP(A.x - P.x, A.y - P.y);
	float dot = AB.x * AP.y - AP.x * AB.y;
	float distAB = std::sqrt((A.x - B.x) * (A.x - B.x) + (A.y - B.y) * (A.y - B.y));
	return std::abs(dot) / distAB;
}

int CircuitLab::UI::RemoveLinkFromNodeView(int nodeViewId, int linkViewId)
{
	for (auto &nv : m_nodeViewList)
	{
		LOG_DEBUG("linkViewId in list: " << nv.id);
		if (nv.id == nodeViewId)
		{
			nv.linkViewIds.erase(
				std::remove_if(nv.linkViewIds.begin(), nv.linkViewIds.end(),
					[linkViewId](const int id) {
						return id == linkViewId;
					}),
				nv.linkViewIds.end()
			);

			return static_cast<int>(nv.linkViewIds.size());
		}
	}
	return -1;
}

void CircuitLab::UI::CreateLinkViewCurrentList()
{
	m_linkViewCurrentList.clear();

	// 1) Correnti dei tap: lookup diretto per componente, come prima. I tratti
	// di bus (sourceNodeViewId != -1, compIdA == -1) vengono saltati qui: la
	// loro corrente si calcola per accumulo al punto 2.
	for (const auto &lv : m_linkViewList)
	{
		if (lv.sourceNodeViewId != -1)
			continue;

		std::vector<int> termList = m_onGetCompTerminalId(lv.compIdA);
		// I terminali sono raggruppati a coppie (0,1), (2,3), ...: ogni coppia è un
		// "ramo" con corrente propria e indipendente dalle altre (es. primario e
		// secondario di un trasformatore). Il tap di termIndexA usa la sua coppia,
		// non sempre (0,1) — un componente a 2 soli terminali ha una coppia sola,
		// quindi il comportamento per resistori/diodi/ecc. resta invariato.
		int pairBase = (lv.termIndexA / 2) * 2;
		if (pairBase + 1 >= static_cast<int>(termList.size()))
		{
			m_linkViewCurrentList[lv.id] = 0.0;
			continue;
		}
		int nodeId1 = termList[pairBase];
		int nodeId2 = termList[pairBase + 1];
		double current = m_simulationOutput.currentBranch[{nodeId1, nodeId2, lv.compIdA}];
		m_linkViewCurrentList[lv.id] = (lv.termIndexA % 2 == 0) ? -current : current;
	}

	// 2) Correnti dei tratti di bus: ogni filo tra due terminali è un tratto di bus
	// tra i loro NodeView, e un nodo elettrico è rappresentato da più NodeView
	// collegati così. La corrente su ciascun tratto si ottiene per accumulo KCL:
	// quanto entra nel sottoalbero "dall'altra parte" deve uscire tutto da questo
	// filo. Nessuna ambiguità: il grafo dei tratti di bus di uno stesso nodo è
	// sempre un albero (collegare due elementi già nello stesso nodo è rifiutato,
	// vedi ConnectSelections), mai un ciclo di fili ideali paralleli.
	std::set<int> visited;
	for (const auto &startNv : m_nodeViewList)
	{
		if (visited.count(startNv.id)) continue;

		// BFS seguendo solo i tratti di bus: raccoglie tutti i NodeView del
		// gruppo e tutti i tap link al loro interno (i tap restano fuori
		// dall'albero: contribuiscono solo come "iniezione di corrente" nel
		// vertice a cui appartengono).
		std::vector<int> groupNodeViewIds;
		std::vector<const LinkView *> groupTapLinks;
		std::vector<const LinkView *> groupBusEdges;

		std::vector<int> queue{ startNv.id };
		visited.insert(startNv.id);
		while (!queue.empty())
		{
			int currentId = queue.back();
			queue.pop_back();
			groupNodeViewIds.push_back(currentId);

			const NodeView &nv = GetNodeViewById(currentId);
			for (int lvId : nv.linkViewIds)
			{
				for (const auto &lv : m_linkViewList)
				{
					if (lv.id != lvId) continue;

					if (lv.sourceNodeViewId == -1)
					{
						if (lv.nodeViewId == currentId)
							groupTapLinks.push_back(&lv);
					}
					else
					{
						groupBusEdges.push_back(&lv);
						int neighborId = (lv.sourceNodeViewId == currentId) ? lv.nodeViewId : lv.sourceNodeViewId;
						if (!visited.count(neighborId))
						{
							visited.insert(neighborId);
							queue.push_back(neighborId);
						}
					}
					break;
				}
			}
		}

		if (groupNodeViewIds.size() <= 1)
			continue; // Nessun tratto di bus qui: niente da accumulare.

		// Ogni tratto di bus compare due volte (registrato su entrambi gli
		// estremi): deduplica per id.
		std::sort(groupBusEdges.begin(), groupBusEdges.end(),
			[](const LinkView *a, const LinkView *b) { return a->id < b->id; });
		groupBusEdges.erase(std::unique(groupBusEdges.begin(), groupBusEdges.end(),
			[](const LinkView *a, const LinkView *b) { return a->id == b->id; }),
			groupBusEdges.end());

		if (groupTapLinks.empty())
			continue; // Nessun tap nel gruppo: nessuna corrente da propagare.

		// Un Ground ha un solo terminale e non compare tra le correnti di ramo: il
		// suo tap vale 0 (punto 1), ma la corrente che scarica a massa esiste. Senza
		// contarla il bilancio KCL del gruppo non chiude e i fili verso massa
		// mostrerebbero 0. La si ricava come l'opposto della somma degli altri tap
		// (con più masse nello stesso gruppo, che non si può ripartire, va alla prima).
		{
			const LinkView *groundTap = nullptr;
			double othersSum = 0.0;
			for (const auto *tap : groupTapLinks)
			{
				if (m_onGetComponentTypeById(tap->compIdA) == ComponentType::ground)
				{
					if (!groundTap)
						groundTap = tap;
				}
				else
					othersSum += m_linkViewCurrentList[tap->id];
			}
			if (groundTap)
				m_linkViewCurrentList[groundTap->id] = -othersSum;
		}

		// Corrente netta iniettata in ciascun NodeView dai propri tap (già
		// calcolate al punto 1; positiva = dal terminale VERSO il NodeView).
		std::map<int, double> netAtNode;
		for (int id : groupNodeViewIds) netAtNode[id] = 0.0;
		for (const auto *tap : groupTapLinks)
			netAtNode[tap->nodeViewId] += m_linkViewCurrentList[tap->id];

		// Adiacenza per la camminata sull'albero.
		std::map<int, std::vector<std::pair<int, const LinkView *>>> adj;
		for (const auto *edge : groupBusEdges)
		{
			adj[edge->sourceNodeViewId].push_back({ edge->nodeViewId, edge });
			adj[edge->nodeViewId].push_back({ edge->sourceNodeViewId, edge });
		}

		// DFS post-order, radice arbitraria (il primo vertice del gruppo): il
		// risultato per ogni tratto non dipende dalla scelta della radice.
		std::function<double(int, int)> accumulate = [&](int nodeId, int parentId) -> double
		{
			double sum = netAtNode[nodeId];
			for (auto &[neighborId, edge] : adj[nodeId])
			{
				if (neighborId == parentId) continue;
				double childSum = accumulate(neighborId, nodeId);
				// childSum = corrente netta che entra nel sottoalbero di neighborId
				// dai suoi tap: deve uscirne tutta attraverso questo filo, quindi
				// la corrente fisica scorre da neighborId (figlio) verso nodeId
				// (genitore) con intensità childSum. Il segno memorizzato segue la
				// stessa convenzione dei tap: positivo = da sourceNodeViewId verso
				// nodeViewId di QUESTO specifico filo.
				m_linkViewCurrentList[edge->id] =
					(edge->sourceNodeViewId == nodeId) ? -childSum : childSum;
				sum += childSum;
			}
			return sum;
		};
		accumulate(groupNodeViewIds.front(), -1);
	}
}

void CircuitLab::UI::CreateLinkParticlesList()
{
	m_linkParticlesList.clear();
	for (const auto &lv : m_linkViewList)
	{
		sf::Vector2f nodeViewPos = GetNodeviewPositionByNodeViewId(lv.nodeViewId);
		LinkPararticles newLinkParticles;
		float linkLenght = std::sqrt(((nodeViewPos.x - lv.startPos.x) * (nodeViewPos.x - lv.startPos.x)) + ((nodeViewPos.y - lv.startPos.y) * (nodeViewPos.y - lv.startPos.y)));
		newLinkParticles.linkViewId = lv.id;
		newLinkParticles.offset = 0.0f;
		newLinkParticles.count = static_cast<int>(linkLenght / (PARTICLE_SIZE * PARTICLE_SPACING_FACTOR));
		m_linkParticlesList.emplace_back(newLinkParticles);
	}
}

void CircuitLab::UI::UpdateParticles(float dt)
{
	for (auto &lp : m_linkParticlesList)
	{
		// Riporta l'avanzamento in [0,1) con x - floor(x), non fmod(x + 1, 1): fmod
		// mantiene il segno del dividendo, quindi con uno spostamento negativo più
		// grande di 1 (corrente istantanea molto alta, es. un trasformatore con
		// accoppiamento vicino a 1 collegato senza resistenza in serie, che nel primo
		// passo di simulazione può dare una corrente enorme prima che il transitorio
		// si stabilizzi) il "+1" di sicurezza non basta e offset resta negativo. Un
		// singolo offset negativo fa disegnare quel frame di particelle estrapolate
		// all'indietro, ben oltre l'inizio del filo — il pallino "fuori dal circuito"
		// notato ogni tanto vicino a un generatore appena avviata la simulazione.
		// x - floor(x) è invece corretto per qualunque x, positivo o negativo.
		float x = lp.offset + static_cast<float>(m_linkViewCurrentList[lp.linkViewId]) * dt * PARTICLE_SPEED_SCALE;
		lp.offset = x - std::floor(x);
	}
}

CircuitLab::NodeView CircuitLab::UI::GetNodeViewFromLInkId(int linkViewId)
{
	for (auto &nv : m_nodeViewList)
		for (const auto &lv : nv.linkViewIds)
			if (lv == linkViewId)
				return nv;

	// non dovrebbe mai succedere se i dati sono consistenti
	throw std::runtime_error("NodeView not found for linkViewId: " + std::to_string(linkViewId));
}

sf::Vector2f CircuitLab::UI::GetNodeviewPositionByNodeViewId(int nodeViewId)
{
	for (const auto &nv : m_nodeViewList)
		if (nv.id == nodeViewId)
			return nv.position;

	throw std::runtime_error("NodeView not found for id: " + std::to_string(nodeViewId));
}

int CircuitLab::UI::GetNodeViewIdByTerminal(int compId, int termIndex) const
{
	for (const auto &lv : m_linkViewList)
		if (lv.compIdA == compId && lv.termIndexA == termIndex)
			return lv.nodeViewId;
	return -1;
}

void CircuitLab::UI::UpdateLinksForNodeView(int nodeViewId, sf::Vector2f newPos)
{
	m_graph.SetNodeViewPosition(nodeViewId, newPos);
}

// Inizializza la finestra SFML e ImGui-SFML.
// Lancia un'eccezione se ImGui o il font non riescono ad inizializzarsi.
CircuitLab::UI::UI(unsigned int width, unsigned int heigth, const std::string &title) :
	m_width{ width },
	m_heigth{ heigth },
	m_title{ title },
	m_window{ sf::VideoMode({ m_width, m_heigth }), m_title },
	m_showOscilloscope{ false },
	m_hSimIndex{ 3 },
	m_windowTime{ 1.0 }
{
	if (!ImGui::SFML::Init(m_window))
		throw std::runtime_error("Impossibile inizializzare ImGui-SFML");

	ImPlot::CreateContext();

	if (!m_font.openFromFile("JetBrainsMono-Regular.ttf"))
		throw std::runtime_error("Impossibile caricare il font");

	m_view = sf::View(sf::FloatRect({ 0.0f, 0.0f }, { static_cast<float>(m_width - PANEL_WIDTH), static_cast<float>(m_heigth) }));
	// Il viewport serve già prima del primo Render (WorldPos/mapPixelToCoords
	// lo usano per convertire il mouse), quindi si imposta qui e non solo là.
	m_view.setViewport(sf::FloatRect({ 0.f, 0.f }, { static_cast<float>(m_width - PANEL_WIDTH) / m_width, 1.f }));

	m_linkViewIdCount = 0;
	m_nodeViewCount = 0;
}

sf::Vector2i CircuitLab::UI::WorldPos(sf::Vector2i pixelPos) const
{
	sf::Vector2f world = m_window.mapPixelToCoords(pixelPos, m_view);
	return sf::Vector2i(static_cast<int>(std::lround(world.x)), static_cast<int>(std::lround(world.y)));
}

void CircuitLab::UI::ResetZoom()
{
	m_zoom = 1.0f;
	m_view.setSize({ static_cast<float>(m_width - PANEL_WIDTH), static_cast<float>(m_heigth) });
	m_view.setCenter(m_view.getSize() / 2.0f);
}

sf::Vector2f CircuitLab::UI::SnapToGrid(sf::Vector2f p) const
{
	if (!m_snapToGrid)
		return p;

	const float g = static_cast<float>(m_gridSize);
	return { std::round(p.x / g) * g, std::round(p.y / g) * g };
}

// Aggancia il componente in modo che sia il suo PRIMO TERMINALE (non il centro)
// a cadere su un nodo della griglia: sono i terminali a cui si attaccano i fili,
// quindi è lì che l'allineamento si vede. Il centro viene traslato della stessa
// quantità. Con rotazioni multiple di 90° tutti gli altri terminali cadono a loro
// volta sulla griglia (finché il passo divide la distanza tra i terminali).
void CircuitLab::UI::SnapComponentToGrid(ComponentView &cw)
{
	if (!m_snapToGrid || cw.GetComponetDesign().terminalOffset.empty())
		return;

	const sf::Vector2f rot = GetRotatedTerminalPos(cw, 0);
	const sf::Vector2f term = SnapToGrid({ cw.GetPosition().x + rot.x, cw.GetPosition().y + rot.y });
	cw.SetPosition(Vec2(term.x - rot.x, term.y - rot.y));
}

void CircuitLab::UI::DrawGrid()
{
	const sf::Vector2f half = m_view.getSize() / 2.0f;
	const sf::Vector2f minP = m_view.getCenter() - half;
	const sf::Vector2f maxP = m_view.getCenter() + half;

	// Se a schermo le linee sarebbero troppo fitte (zoom molto indietro) si
	// raddoppia il passo finché tornano leggibili, invece di sfumare in un grigio piatto.
	float step = static_cast<float>(m_gridSize);
	while (step * m_zoom < GRID_MIN_SCREEN_SPACING)
		step *= 2.0f;

	const sf::Color minor(255, 255, 255, 22);
	const sf::Color major(255, 255, 255, 48);

	sf::VertexArray lines(sf::PrimitiveType::Lines);

	const int firstX = static_cast<int>(std::floor(minP.x / step));
	const int lastX = static_cast<int>(std::ceil(maxP.x / step));
	for (int i = firstX; i <= lastX; i++)
	{
		const sf::Color &c = (i % GRID_MAJOR_EVERY == 0) ? major : minor;
		lines.append(sf::Vertex{ { i * step, minP.y }, c });
		lines.append(sf::Vertex{ { i * step, maxP.y }, c });
	}

	const int firstY = static_cast<int>(std::floor(minP.y / step));
	const int lastY = static_cast<int>(std::ceil(maxP.y / step));
	for (int i = firstY; i <= lastY; i++)
	{
		const sf::Color &c = (i % GRID_MAJOR_EVERY == 0) ? major : minor;
		lines.append(sf::Vertex{ { minP.x, i * step }, c });
		lines.append(sf::Vertex{ { maxP.x, i * step }, c });
	}

	m_window.draw(lines);
}

// Shutdown di ImGui-SFML alla distruzione della UI
CircuitLab::UI::~UI()
{
	ImPlot::DestroyContext();
	ImGui::SFML::Shutdown();
}

void CircuitLab::UI::AddViewComponent(int compId, const std::string &name, ComponentType type, Vec2 position, float rotation)
{
	m_componentViewList.emplace_back(ComponentView(compId, position, rotation, name, type));
}

int CircuitLab::UI::AddViewLink(int comp1, int term1, int nodeViewId)
{
	NodeView nv = GetNodeViewById(nodeViewId);
	LinkView newLink = GetLinkCoords(comp1, term1, nv);
	m_linkViewList.emplace_back(newLink);
	return newLink.id;
}

// Crea un tratto di bus (NodeView -> NodeView, nessun componente coinvolto).
// Non aggiorna i linkViewIds dei due hub: è responsabilità del chiamante
// (lo split interattivo lo fa esplicitamente su entrambi, come già fanno gli
// scenari di collegamento normale; il caricamento da file sovrascrive comunque
// la lista intera al passo 6 di IOManager::LoadFromFile).
int CircuitLab::UI::AddBusLinkView(int sourceNodeViewId, int targetNodeViewId)
{
	NodeView sourceNv = GetNodeViewById(sourceNodeViewId);
	NodeView targetNv = GetNodeViewById(targetNodeViewId);

	LinkView link;
	link.id = ++m_linkViewIdCount;
	link.startPos = sourceNv.position;
	link.targetPos = targetNv.position;
	link.compIdA = -1;
	link.termIndexA = -1;
	link.nodeViewId = targetNv.id;
	link.sourceNodeViewId = sourceNv.id;

	m_linkViewList.emplace_back(link);
	return link.id;
}

int CircuitLab::UI::AddNodeView(int nodeId, sf::Vector2f position, bool manual, int anchorCompId, int anchorTermIndex, bool attached)
{
	return m_graph.AddNodeView(nodeId, position, manual, anchorCompId, anchorTermIndex, attached);
}

void CircuitLab::UI::ConvertLegacyNodeViews()
{
	m_graph.ConvertLegacy();
}

void CircuitLab::UI::FreeTerminals(const std::vector<TerminalRef> &terminals)
{
	if (!m_onFreeTerminal)
		return;
	for (const auto &[compId, termIndex] : terminals)
		m_onFreeTerminal(compId, termIndex);
}

void CircuitLab::UI::DetachSurvivingGroupsFromGround(const std::vector<TerminalRef> &candidates)
{
	if (!m_onDetachFromGround)
		return;

	std::set<int> doneGroups; // un gruppo va trattato una volta sola (chiave: il suo id più basso)
	for (const auto &[compId, termIndex] : candidates)
	{
		const int nvId = m_graph.NodeViewIdOfTerminal(compId, termIndex);
		if (nvId == -1)
			continue; // rimasto solo: è già stato liberato

		const std::set<int> group = m_graph.CollectGroup(nvId);
		if (!doneGroups.insert(*group.begin()).second)
			continue;

		// Un altro Ground nello stesso gruppo: resta a massa, non c'è nulla da fare
		const std::vector<TerminalRef> terminals = m_graph.TerminalsInGroup(nvId);
		bool stillGrounded = false;
		for (const auto &terminal : terminals)
			if (m_onGetComponentTypeById(terminal.first) == ComponentType::ground)
				stillGrounded = true;

		if (!stillGrounded)
			m_onDetachFromGround(terminals);
	}
}

void CircuitLab::UI::ConnectSelections(const SelecetedComponent &first, const SelecetedComponent &second)
{
	// NodeView che rappresenta il gruppo (il nodo elettrico) di un elemento
	// selezionato, -1 se non ne ha ancora uno (un terminale mai collegato) o
	// se l'elemento non esiste più.
	auto groupNodeView = [this](const SelecetedComponent &sel) -> int
	{
		switch (sel.state)
		{
		case SelectionState::terminalSelected:
			return m_graph.NodeViewIdOfTerminal(sel.compId, sel.terminalIndex);
		case SelectionState::nodeViewSelected:
			return m_graph.FindNodeView(sel.nodeViewId) ? sel.nodeViewId : -1;
		case SelectionState::linkSelected:
		{
			const LinkView *lv = m_graph.FindLinkView(sel.linkId);
			if (!lv)
				return -1;
			// un tratto di bus ha due estremi, ma sono nello stesso gruppo; un tap
			// (filo di un NodeView staccato) porta al suo NodeView
			return lv->sourceNodeViewId != -1 ? lv->sourceNodeViewId : lv->nodeViewId;
		}
		default:
			return -1;
		}
	};

	// Lo stesso elemento cliccato due volte
	if (first.state == second.state &&
		((first.state == SelectionState::terminalSelected && first.compId == second.compId && first.terminalIndex == second.terminalIndex) ||
			(first.state == SelectionState::linkSelected && first.linkId == second.linkId) ||
			(first.state == SelectionState::nodeViewSelected && first.nodeViewId == second.nodeViewId)))
		return;

	const int groupA = groupNodeView(first);
	const int groupB = groupNodeView(second);

	// Un elemento che non esiste più (es. un filo sparito tra i due click)
	if ((first.state != SelectionState::terminalSelected && groupA == -1) ||
		(second.state != SelectionState::terminalSelected && groupB == -1))
		return;

	// Già nello stesso nodo elettrico: un secondo filo tra i due formerebbe un
	// ciclo, e per un ciclo di fili ideali le correnti sono indeterminate.
	if (groupA != -1 && groupB != -1 && m_graph.SameGroup(groupA, groupB))
		return;

	// Un terminale per ciascun lato con cui avvisare il Circuit: il terminale stesso,
	// o un qualunque terminale collegato al gruppo. Un gruppo ancora vuoto (un nodo
	// libero senza nessun componente) non ne ha: il collegamento resta puramente
	// visivo finché non arriva un secondo terminale, come un componente appena
	// piazzato e non ancora collegato.
	auto representativeTap = [this](const SelecetedComponent &sel, int group) -> TerminalRef
	{
		if (sel.state == SelectionState::terminalSelected)
			return { sel.compId, sel.terminalIndex };
		return group == -1 ? TerminalRef{ -1, -1 } : m_graph.FindRealTapInGroup(group);
	};
	const TerminalRef tapA = representativeTap(first, groupA);
	const TerminalRef tapB = representativeTap(second, groupB);

	if (tapA.first != -1 && tapB.first != -1)
	{
		if (tapA.first == tapB.first)
			return; // due terminali dello stesso componente non si collegano

		// Due terminali già sullo stesso nodo elettrico (o a massa entrambi) non si collegano
		if (first.state == SelectionState::terminalSelected && second.state == SelectionState::terminalSelected)
		{
			const int nodeIdA = m_onGetCompTerminalId(tapA.first)[tapA.second];
			const int nodeIdB = m_onGetCompTerminalId(tapB.first)[tapB.second];
			if (nodeIdA == nodeIdB && nodeIdA != -1)
				return;
		}

		// Il Circuit decide per primo: se rifiuta, non si disegna nulla
		if (!m_onCreateLink(tapA.first, tapA.second, tapB.first, tapB.second))
			return;
	}

	// Ogni lato diventa un NodeView: quello del terminale (creato se manca), il nodo
	// scelto, oppure un nodo libero nuovo sul filo nel punto cliccato.
	auto resolve = [this](const SelecetedComponent &sel) -> int
	{
		switch (sel.state)
		{
		case SelectionState::terminalSelected:
		{
			const std::vector<sf::Vector2f> terminals = GetTerminalPositionbyCompId(sel.compId);
			if (sel.terminalIndex < 0 || sel.terminalIndex >= static_cast<int>(terminals.size()))
				return -1;
			return m_graph.EnsureTerminalNodeView(sel.compId, sel.terminalIndex, terminals[sel.terminalIndex]);
		}
		case SelectionState::nodeViewSelected:
			return sel.nodeViewId;
		case SelectionState::linkSelected:
		{
			const LinkView *lv = m_graph.FindLinkView(sel.linkId);
			if (!lv)
				return -1;
			if (lv->sourceNodeViewId == -1)
				return lv->nodeViewId; // filo di un NodeView staccato: ci si collega a quel nodo
			return m_graph.InsertNodeOnBusEdge(sel.linkId, sel.clickPos);
		}
		default:
			return -1;
		}
	};

	const int nodeA = resolve(first);
	const int nodeB = resolve(second);
	if (nodeA == -1 || nodeB == -1 || nodeA == nodeB || m_graph.SameGroup(nodeA, nodeB))
		return;

	m_graph.AddBusEdge(nodeA, nodeB);
}

void CircuitLab::UI::Clear()
{
	m_componentViewList.clear();
	m_linkViewList.clear();
	m_nodeViewList.clear();

	m_selectedComponent.compId = -1;
	m_selectedComponent.terminalIndex = -1;
	m_selectedComponent.linkId = -1;
	m_selectedComponent.nodeViewId = -1;
	m_selectedComponent.state = SelectionState::none;
	m_selectedComponent.clickPos = sf::Vector2f(0.0f, 0.0f);
	m_linkViewIdCount = 0;
	m_nodeViewCount = 0;
}

void CircuitLab::UI::Render()
{
	sf::Time dt = m_deltaClock.restart();
	// --- Aggiornamento ImGui ---
	ImGui::SFML::Update(m_window, dt);

	DrawImageGuiPanel();

	// --- Rendering canvas ---
	m_window.clear(BACKGROUND_COLOR);

	m_view.setViewport(sf::FloatRect({ 0.f, 0.f }, { (static_cast<float>(m_width - PANEL_WIDTH) / m_width), 1.f }));
	m_window.setView(m_view);

	if (m_showGrid)
		DrawGrid();

	DrawComponents();

	DrawNodes();

	// I pallini avanzano solo mentre la simulazione è effettivamente in esecuzione:
	// altrimenti continuavano a muoversi anche a simulazione in pausa/stop, dando
	// l'impressione (falsa) che il circuito stesse ancora facendo qualcosa.
	if (m_onGetSimulationStatus && m_onGetSimulationStatus() == SimulationStatus::running)
		UpdateParticles(dt.asSeconds());

	DrawWires();

	m_window.setView(sf::View(sf::FloatRect({ 0.0f, 0.0f }, { static_cast<float>(m_width), static_cast<float>(m_heigth) })));

	// Render ImGui sopra il canvas
	ImGui::SFML::Render(m_window);
	m_window.display();
}