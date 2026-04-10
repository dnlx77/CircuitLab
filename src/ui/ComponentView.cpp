#include "UI/ComponentView.h"

// Definizione della mappa statica dei design grafici per ogni tipo di componente.
// Gli offset dei terminali sono relativi al centro del componente.
// Esempio per il resistore: terminale 0 in alto (0, -20), terminale 1 in basso (0, +20).
const std::map<CircuitLab::ComponentType, CircuitLab::ComponentDesign> CircuitLab::ComponentView::s_design = {
	// { tipo, { larghezza, altezza, raggioTerminale, { offsetTerm0, offsetTerm1, ... } } }
	{ CircuitLab::ComponentType::resistor,      { 20, 40, 4, { {0, -20}, {0, 20} } } },
	{ CircuitLab::ComponentType::voltageSource,  { 20, 40, 4, { {0, -20}, {0, 20} } } },
	{ CircuitLab::ComponentType::ground,         { 20, 20, 4, { {0, -20} } } },
};

CircuitLab::ComponentView::ComponentView(int componentLink, const Vec2 &position,
	float rotation, const std::string &name, ComponentType type) :
	m_componentLink(componentLink),
	m_position(position),
	m_rotation(rotation),
	m_name(name),
	m_type(type)
{}