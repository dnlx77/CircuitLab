#pragma once
#include <string>
#include <vector>
#include "Terminal.h"
#include "Core/Vector2.h"

namespace CircuitLab {
	
	class Component {
	protected:
		static int s_nextId;
		int m_id;
		int m_terminalNumber;

		Vec2 m_position;
		float m_rotation;
		std::string m_name;

		std::vector<Terminal> m_terminals;

		virtual void saveSpecificData() = 0;
		virtual void loadSpecificData() = 0;
	public:
		Component (int terminalNumber) : m_id(s_nextId++), m_terminalNumber(terminalNumber) {
			m_terminals.resize(m_terminalNumber);
		}

		// Getter
		const Vec2& GetPosition() const { return m_position; }
		float GetRotation() const { return m_rotation; }
		const std::string& GetName() const { return m_name; }
		int GetId() const { return m_id; }
		const std::vector<Terminal>& GetTerminals() const { return m_terminals; }

		// Setter
		void SetPosition(Vec2 pos) { m_position = pos; }
		void SetRotation(float rot) { m_rotation = rot; }
		void SetName(const std::string &name) { m_name = name; }

		void Save();
		void Load();

		virtual void Stamp() = 0;
	};
}