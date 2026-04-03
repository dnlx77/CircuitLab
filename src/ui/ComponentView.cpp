#include "UI/ComponentView.h"

CircuitLab::ComponentView::ComponentView(int componentLink, const Vec2 &position, float rotation, const std::string &name, ComponentType type) :
	m_componentLink(componentLink),
	m_position(position),
	m_rotation(rotation),
	m_name(name),
	m_type(type)
{}
