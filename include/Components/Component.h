#pragma once
#include <string>
#include <vector>
#include <Eigen/Dense>
#include "Terminal.h"

namespace CircuitLab {
	
	class Component {
	protected:
		static int s_nextId;
		int m_id;
		int m_terminalNumber;

		std::vector<Terminal> m_terminals;

		virtual void SaveSpecificData() = 0;
		virtual void LoadSpecificData() = 0;
	public:
		Component (int terminalNumber) : m_id(s_nextId++), m_terminalNumber(terminalNumber) {
			m_terminals.resize(m_terminalNumber);
		}
		virtual ~Component() = default;

		// Getter
		int GetId() const { return m_id; }
		const std::vector<Terminal>& GetTerminals() const { return m_terminals; }

		void Save();
		void Load();

		virtual void Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &b, const std::map<int,int>& nodeMap) = 0;
	};
}