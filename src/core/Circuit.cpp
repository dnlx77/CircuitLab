#include <set>

#include "Core/Circuit.h"

int CircuitLab::Circuit::ComputeNodes()
{
	std::set<int> nodes;
	for (const auto &comp : m_components) {
		const std::vector<Terminal> &terminals = comp->GetTerminals();
		for (const auto &terminal : terminals) {
			if (terminal.GetNodeId() != 0)
				nodes.insert(terminal.GetNodeId());
		}
	}

	int i = 0;
	for (const auto &node : nodes) {
		m_nodesMap[node] = i;
		i++;
	}

	int k = static_cast<int>(nodes.size());
	int numVoltageSource = 0;
	for (const auto &comp : m_components) {
		if (comp->GetExtraVariables()) {
			m_voltageSourceMap[comp->GetId()] = k++;
			numVoltageSource++;
		}
	}

	return static_cast<int>(nodes.size()+numVoltageSource);
}

void CircuitLab::Circuit::ComputeCircuit()
{
	if (!m_isDirty)
		return;

	int num_nodes = ComputeNodes();
	m_circuitMatrix = Eigen::MatrixXd::Zero(num_nodes, num_nodes);
	m_circuitVector = Eigen::VectorXd::Zero(num_nodes);

	for (const auto &comp : m_components) {
		comp->Stamp(m_circuitMatrix, m_circuitVector, m_nodesMap, m_voltageSourceMap);
	}

	m_isDirty = false;
}

CircuitLab::Circuit::Circuit() : m_isDirty(true)
{}

const Eigen::MatrixXd &CircuitLab::Circuit::GetCircuitMatrix()
{
	ComputeCircuit();

	return m_circuitMatrix;
}

const Eigen::VectorXd &CircuitLab::Circuit::GetCircuitVector()
{
	ComputeCircuit();

	return m_circuitVector;
}

int CircuitLab::Circuit::AddComponent(std::unique_ptr<Component> comp)
{
	int id = comp->GetId();
	m_components.emplace_back(std::move(comp));
	return id;
}
