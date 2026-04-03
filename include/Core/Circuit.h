#pragma once
#include <vector>
#include <Eigen/Dense>
#include <memory>
#include "Components/Component.h"

namespace CircuitLab {

	class Circuit {
	private:
		std::vector<std::unique_ptr<Component>> m_components;
		Eigen::MatrixXd m_circuitMatrix;
		Eigen::VectorXd m_circuitVector;
		std::map<int, int> m_nodesMap;
		std::map<int, int> m_voltageSourceMap;
		bool m_isDirty;
		int ComputeNodes();
		void ComputeCircuit();
	public:
		Circuit();
		const Eigen::MatrixXd &GetCircuitMatrix();
		const Eigen::VectorXd &GetCircuitVector();
		void AddComponent(std::unique_ptr<Component> comp);
		void RemoveComponent(int compId);
		void InvalidateCircuit() { m_isDirty = true; }
		bool IsCircuitEmpty() const { return m_components.empty(); }
	};
}