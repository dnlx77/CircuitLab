#include "UI/ComponentView.h"
#include <numbers>

// Definizione della mappa statica dei design grafici per ogni tipo di componente.
// Gli offset dei terminali sono relativi al centro del componente.
// Esempio per il resistore: terminale 0 in alto (0, -20), terminale 1 in basso (0, +20).
const std::map<CircuitLab::ComponentType, CircuitLab::ComponentDesign> CircuitLab::ComponentView::s_design = {
	// { tipo, { larghezza, altezza, raggioTerminale, { offsetTerm0, offsetTerm1, ... } } }
	{ CircuitLab::ComponentType::resistor,      { 20, 40, 4, { {0, -20}, {0, 20} }, -1 } },
	{ CircuitLab::ComponentType::voltageGenerator,  { 20, 40, 4, { {0, -20}, {0, 20} }, 0 } },
	{ CircuitLab::ComponentType::ground,         { 20, 20, 4, { {0, -20} }, -1 } },
	{ CircuitLab::ComponentType::capacitor,      { 20, 40, 4, { {0, -20}, {0, 20} }, -1 } },
	{ CircuitLab::ComponentType::inductor,       { 20, 40, 4, { {0, -20}, {0, 20} }, -1 } },
	{ CircuitLab::ComponentType::switchComponent, { 20, 40, 4, { {0, -20}, {0, 20} }, -1 } },
	{ CircuitLab::ComponentType::diode,          { 20, 40, 4, { {0, -20}, {0, 20} }, -1 } },
};

CircuitLab::ComponentView::ComponentView(int componentLink, const Vec2 &position,
	float rotation, const std::string &name, ComponentType type) :
	m_componentLink(componentLink),
	m_position(position),
	m_rotation(rotation),
	m_name(name),
	m_type(type)
{}

void CircuitLab::ComponentView::Save(nlohmann::json & j) const
{
	j["componentLink"] = GetComponentLink();
	j["name"] = GetName();
	j["type"] = GetComponentType();
	j["position"] = { GetPosition().x, GetPosition().y };
	j["rotation"] = GetRotation();
}

namespace {
	// Disegna un componente come un insieme di segmenti in coordinate LOCALI
	// (origine = centro del componente, come gli offset dei terminali in
	// ComponentDesign), trasformati con la stessa posizione/rotazione del
	// componente. "type" è LineStrip per un percorso continuo (es. lo zigzag)
	// o Lines per segmenti indipendenti a coppie di punti (es. i due lead).
	void DrawLocalSegments(sf::RenderWindow &window, const sf::RenderStates &states,
		std::initializer_list<sf::Vector2f> points, sf::PrimitiveType type, sf::Color color)
	{
		sf::VertexArray va(type, points.size());
		std::size_t i = 0;
		for (const auto &p : points)
			va[i++] = sf::Vertex{ p, color };
		window.draw(va, states);
	}
}

void CircuitLab::ComponentView::DrawSymbol(sf::RenderWindow &window, sf::Color color, WaveFormType waveForm, bool switchClosed) const
{
	// Trasforma le coordinate locali (definite come gli offset dei terminali:
	// origine al centro, terminali a y=-20/+20) nella posizione e rotazione
	// correnti del componente sul canvas.
	sf::Transform transform;
	transform.translate({ GetPosition().x, GetPosition().y });
	transform.rotate(sf::degrees(GetRotation()));
	sf::RenderStates states;
	states.transform = transform;

	auto draw = [&](std::initializer_list<sf::Vector2f> points, sf::PrimitiveType type)
	{
		DrawLocalSegments(window, states, points, type, color);
	};

	switch (GetComponentType())
	{
	case ComponentType::resistor:
		// Simbolo IEC: rettangolo vuoto tra i due lead.
		draw({ {0,-20}, {0,-12} }, sf::PrimitiveType::Lines);
		draw({ {-8,-12}, {8,-12}, {8,12}, {-8,12}, {-8,-12} }, sf::PrimitiveType::LineStrip);
		draw({ {0,12}, {0,20} }, sf::PrimitiveType::Lines);
		break;

	case ComponentType::capacitor:
		// Due piastre parallele con un lead perpendicolare su ciascuna.
		draw({ {0,-20}, {0,-4} }, sf::PrimitiveType::Lines);
		draw({ {-10,-4}, {10,-4} }, sf::PrimitiveType::Lines);
		draw({ {-10,4}, {10,4} }, sf::PrimitiveType::Lines);
		draw({ {0,4}, {0,20} }, sf::PrimitiveType::Lines);
		break;

	case ComponentType::ground:
		// Lead seguito da tre barre orizzontali decrescenti.
		draw({ {0,-20}, {0,-10} }, sf::PrimitiveType::Lines);
		draw({ {-10,-10}, {10,-10} }, sf::PrimitiveType::Lines);
		draw({ {-6,-5}, {6,-5} }, sf::PrimitiveType::Lines);
		draw({ {-2,0}, {2,0} }, sf::PrimitiveType::Lines);
		break;

	case ComponentType::inductor:
	{
		// Lead-bobina-lead: gobbe successive tutte dalla stessa parte,
		// approssimate campionando una curva (stesso approccio della sinusoide
		// disegnata dentro il generatore di tensione, vedi sotto).
		draw({ {0,-20}, {0,-10} }, sf::PrimitiveType::Lines);
		draw({ {0,10}, {0,20} }, sf::PrimitiveType::Lines);

		constexpr int HUMPS = 3;
		constexpr int SAMPLES_PER_HUMP = 8;
		constexpr float COIL_Y_START = -10.0f;
		constexpr float COIL_Y_END = 10.0f;
		constexpr float COIL_AMPLITUDE = 8.0f;

		std::vector<sf::Vector2f> coilPoints;
		coilPoints.reserve(HUMPS * SAMPLES_PER_HUMP + 1);
		for (int i = 0; i <= HUMPS * SAMPLES_PER_HUMP; i++)
		{
			float t = static_cast<float>(i) / (HUMPS * SAMPLES_PER_HUMP); // 0..1 lungo tutta la bobina
			float humpPhase = std::fmod(t * HUMPS, 1.0f);
			coilPoints.push_back({
				COIL_AMPLITUDE * std::sin(humpPhase * std::numbers::pi_v<float>),
				COIL_Y_START + t * (COIL_Y_END - COIL_Y_START)
				});
		}
		sf::VertexArray coil(sf::PrimitiveType::LineStrip, coilPoints.size());
		for (std::size_t i = 0; i < coilPoints.size(); i++)
			coil[i] = sf::Vertex{ coilPoints[i], color };
		window.draw(coil, states);
		break;
	}

	case ComponentType::voltageGenerator:
	{
		// Due lead verso un cerchio; dentro il cerchio, +/- per una sorgente DC
		// oppure un'icona della forma d'onda (sinusoide/quadra) per le altre.
		draw({ {0,-20}, {0,-14} }, sf::PrimitiveType::Lines);
		draw({ {0,14}, {0,20} }, sf::PrimitiveType::Lines);

		sf::CircleShape circle(14.0f);
		circle.setOrigin({ 14.0f, 14.0f });
		circle.setPosition({ GetPosition().x, GetPosition().y });
		circle.setFillColor(sf::Color::Transparent);
		circle.setOutlineColor(color);
		circle.setOutlineThickness(1.0f);
		window.draw(circle);

		if (waveForm == WaveFormType::sineWaveForm)
		{
			// Una sinusoide completa disegnata come linea spezzata.
			constexpr int SINE_SEGMENTS = 8;
			std::vector<sf::Vector2f> points;
			points.reserve(SINE_SEGMENTS + 1);
			for (int i = 0; i <= SINE_SEGMENTS; i++)
			{
				float t = static_cast<float>(i) / SINE_SEGMENTS;
				points.push_back({
					(t - 0.5f) * 16.0f,
					-std::sin(t * 2.0f * std::numbers::pi_v<float>) * 5.0f
					});
			}
			sf::VertexArray sine(sf::PrimitiveType::LineStrip, points.size());
			for (std::size_t i = 0; i < points.size(); i++)
				sine[i] = sf::Vertex{ points[i], color };
			window.draw(sine, states);
		}
		else if (waveForm == WaveFormType::squareWaveForm)
		{
			draw({ {-8,4}, {-8,-4}, {0,-4}, {0,4}, {8,4}, {8,-4} }, sf::PrimitiveType::LineStrip);
		}
		else // DC (o none): simbolo "+" in alto, "-" in basso
		{
			draw({ {-4,-8}, {4,-8} }, sf::PrimitiveType::Lines);
			draw({ {0,-11}, {0,-5} }, sf::PrimitiveType::Lines);
			draw({ {-4,8}, {4,8} }, sf::PrimitiveType::Lines);
		}
		break;
	}

	case ComponentType::switchComponent:
		// Due lead verso i punti di contatto; la "lama" è un segmento dritto tra
		// i due contatti se chiuso, oppure inclinato lontano dal contatto
		// inferiore se aperto.
		draw({ {0,-20}, {0,-12} }, sf::PrimitiveType::Lines);
		draw({ {0,12}, {0,20} }, sf::PrimitiveType::Lines);
		draw({ {0,-12}, switchClosed ? sf::Vector2f{0,12} : sf::Vector2f{9,6} }, sf::PrimitiveType::Lines);
		break;

	case ComponentType::diode:
		// Anodo (terminale 0, in alto) -> triangolo con l'apice verso il catodo
		// (terminale 1, in basso), chiuso da una barra sul lato del catodo.
		draw({ {0,-20}, {0,-8} }, sf::PrimitiveType::Lines);
		draw({ {-8,-8}, {8,-8}, {0,8}, {-8,-8} }, sf::PrimitiveType::LineStrip);
		draw({ {-8,8}, {8,8} }, sf::PrimitiveType::Lines);
		draw({ {0,8}, {0,20} }, sf::PrimitiveType::Lines);
		break;

	default:
		break;
	}
}
