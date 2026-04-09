#include "UI/ComponentView.h"

const std::map<CircuitLab::ComponentType, CircuitLab::ComponentDesign> CircuitLab::ComponentView::s_design = {
	{CircuitLab::ComponentType::resistor, { 20, 40, 4, { {0, -20}, {0, 20} } } },
	{CircuitLab::ComponentType::voltageSource, { 20, 40, 4, { {0, -20}, {0, 20} } } },
	{CircuitLab::ComponentType::ground, {20, 20, 4, { {0, -20} } } },
};

CircuitLab::ComponentView::ComponentView(int componentLink, const Vec2 &position, float rotation, const std::string &name, ComponentType type) :
	m_componentLink(componentLink),
	m_position(position),
	m_rotation(rotation),
	m_name(name),
	m_type(type)
{}
