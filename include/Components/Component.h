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
		Terminal &GetTerminal(int index) { return m_terminals[index]; }

		void Save();
		void Load();

		virtual void Stamp(Eigen::MatrixXd &A, Eigen::VectorXd &B, const std::map<int,int>& nodeMap, const std::map<int, int>& voltageSourceMap) = 0;
		virtual int GetExtraVariables() const { return 0; }
	};
}