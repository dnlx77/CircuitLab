#include "UI/ComponentView.h"

CircuitLab::ComponentView::ComponentView(int componentLink, const Vec2 &position, float rotation, const std::string &name) :
	m_componentLink(componentLink),
	m_position(position),
	m_rotation(rotation),
	m_name(name)
{}
