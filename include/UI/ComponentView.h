#pragma once
#include <string>
#include "Core/Vector2.h"
#include "Common/ComponentType.h"

namespace CircuitLab {
	class ComponentView {
	private:
		Vec2 m_position;
		float m_rotation;
		std::string m_name;
		int m_componentLink;
		ComponentType m_type;

	public:
		ComponentView(int componentLink, const Vec2 &position, float rotation, const std::string &name, ComponentType type);

		//Getter 
		const Vec2 &GetPosition() const { return m_position; }
		float GetRotation() const { return m_rotation; }
		const std::string &GetName() const { return m_name; }
		int GetComponentLink() const { return m_componentLink; }
		ComponentType GetComponentType() const { return m_type; }

		//Setter
		void SetPosition(const Vec2 &position) { m_position = position; }
		void SetRotation(float rotation) { m_rotation = rotation; }
		void SetName(const std::string &name) { m_name = name; }
		void SetComponentLink(int link) { m_componentLink = link; }
		void SetComponentType(ComponentType type) { m_type = type; }
	};
}