#include <set>
#include <iostream>

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

	for (const auto &n : nodes)

		// DA CANCELLARE
		std::cout << "node in set: " << n << std::endl;

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


	// DA CANCELLARE
	std::cout << m_circuitMatrix << std::endl;
	std::cout << m_circuitVector << std::endl;
}

void CircuitLab::Circuit::PrintCircuit()
{
	for (const auto &comp : m_components) 
	{
		std::cout << "Component id: " << comp->GetId() << std::endl;
		for (int i = 0; i < comp->GetTerminals().size(); i++)
		{
			std::cout << "Terminale " << i << " nodeId: " << comp->GetTerminal(i).GetNodeId() << std::endl;
		}
	}
}

void CircuitLab::Circuit::ConnectTerminals(int comp1Id, int termComp1, int comp2Id, int termComp2)
{
	Component *comp1 = nullptr;
	Component *comp2 = nullptr;
	int nodeId = -1;
	int oldNodeId = -1;
	for (const auto &comp : m_components) {
		if (comp->GetId() == comp1Id) comp1 = comp.get();
		if (comp->GetId() == comp2Id) comp2 = comp.get();
	}

	if (comp1->GetTerminals()[termComp1].GetNodeId() < 0 && comp2->GetTerminals()[termComp2].GetNodeId() < 0)
	{
		nodeId = m_nextNodeId++;
		comp1->GetTerminal(termComp1).SetNodeId(nodeId);
		comp2->GetTerminal(termComp2).SetNodeId(nodeId);


		// DA CANCELLARE
		std::cout << "comp1 id=" << comp1Id << " term=" << termComp1
			<< " nodeId=" << comp1->GetTerminal(termComp1).GetNodeId() << std::endl;
		std::cout << "comp2 id=" << comp2Id << " term=" << termComp2
			<< " nodeId=" << comp2->GetTerminal(termComp2).GetNodeId() << std::endl;

		return;
	}

	if (comp1->GetTerminals()[termComp1].GetNodeId() == 0 || comp2->GetTerminals()[termComp2].GetNodeId() == 0)
	{
		int id1 = comp1->GetTerminals()[termComp1].GetNodeId();
		int id2 = comp2->GetTerminals()[termComp2].GetNodeId();
		if (id1 != 0)
			oldNodeId = id1;
		if (id2 != 0)
			oldNodeId = id2;

		comp1->GetTerminal(termComp1).SetNodeId(0);
		comp2->GetTerminal(termComp2).SetNodeId(0);

		for (auto const &comp : m_components)
		{
			for (int i = 0; i < comp->GetTerminals().size(); i++)
			{
				if (comp->GetTerminal(i).GetNodeId() == oldNodeId)
					comp->GetTerminal(i).SetNodeId(0);
			}
		}


		// DA CANCELLARE
		std::cout << "comp1 id=" << comp1Id << " term=" << termComp1
			<< " nodeId=" << comp1->GetTerminal(termComp1).GetNodeId() << std::endl;
		std::cout << "comp2 id=" << comp2Id << " term=" << termComp2
			<< " nodeId=" << comp2->GetTerminal(termComp2).GetNodeId() << std::endl;

		return;
	}

	if (comp1->GetTerminals()[termComp1].GetNodeId() > 0 || comp2->GetTerminals()[termComp2].GetNodeId() > 0)
	{
		int id1 = comp1->GetTerminals()[termComp1].GetNodeId();
		int id2 = comp2->GetTerminals()[termComp2].GetNodeId();
		if (id1 > 0)
		{
			oldNodeId = id2;
			comp1->GetTerminal(termComp1).SetNodeId(id1);
			comp2->GetTerminal(termComp2).SetNodeId(id1);

			// retropropaga solo se il vecchio nodo era collegato
			if (oldNodeId > 0) 
			{
				for (auto const &comp : m_components)
				{
					for (int i = 0; i < comp->GetTerminals().size(); i++)
					{
						if (comp->GetTerminal(i).GetNodeId() == oldNodeId)
							comp->GetTerminal(i).SetNodeId(id1);
					}
				}
			}
		} 
		else
		{
			oldNodeId = id1;
			comp1->GetTerminal(termComp1).SetNodeId(id2);
			comp2->GetTerminal(termComp2).SetNodeId(id2);

			if (oldNodeId > 0)
			{
				for (auto const &comp : m_components)
				{
					for (int i = 0; i < comp->GetTerminals().size(); i++)
					{
						if (comp->GetTerminal(i).GetNodeId() == oldNodeId)
							comp->GetTerminal(i).SetNodeId(id2);
					}
				}
			}
		}


		// DA CANCELLARE
		std::cout << "comp1 id=" << comp1Id << " term=" << termComp1
			<< " nodeId=" << comp1->GetTerminal(termComp1).GetNodeId() << std::endl;
		std::cout << "comp2 id=" << comp2Id << " term=" << termComp2
			<< " nodeId=" << comp2->GetTerminal(termComp2).GetNodeId() << std::endl;

		return;
		
	}
	/*
	if (comp1->GetTerminals()[termComp1].GetNodeId() >= 0) nodeId = comp1->GetTerminals()[termComp1].GetNodeId();
	else if (comp2->GetTerminals()[termComp2].GetNodeId() >= 0) nodeId = comp2->GetTerminals()[termComp2].GetNodeId();
	else nodeId = m_nextNodeId++;

	comp1->GetTerminal(termComp1).SetNodeId(nodeId);
	comp2->GetTerminal(termComp2).SetNodeId(nodeId);
	*/

}

CircuitLab::Circuit::Circuit() : m_isDirty(true), m_nextNodeId(1)
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
