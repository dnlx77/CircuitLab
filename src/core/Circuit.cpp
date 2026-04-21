#include <set>
#include <iostream>

#include "Core/Circuit.h"

// Scandisce tutti i terminali di tutti i componenti per raccogliere i nodi attivi
// (esclude il nodo ground, nodeId == 0), costruisce la mappa nodeId -> indice matrice,
// e assegna le righe extra per le sorgenti di tensione.
// Restituisce la dimensione totale della matrice MNA.
int CircuitLab::Circuit::ComputeNodes()
{
	std::set<int> nodes;
	for (const auto &comp : m_components) {
		const std::vector<Terminal> &terminals = comp->GetTerminals();
		for (const auto &terminal : terminals) {
			// Il nodo 0 è il ground: non occupa una riga nella matrice MNA
			if (terminal.GetNodeId() != 0)
				nodes.insert(terminal.GetNodeId());
		}
	}

	// Mappa ogni nodeId a un indice progressivo (0, 1, 2, ...)
	int i = 0;
	for (const auto &node : nodes) {
		m_nodesMap[node] = i;
		i++;
	}

	// DA CANCELLARE
	for (const auto &n : nodes)
		std::cout << "node in set: " << n << std::endl;

	// Le sorgenti di tensione introducono una variabile extra (la corrente)
	// che occupa una riga/colonna aggiuntiva in fondo alla matrice
	int k = static_cast<int>(nodes.size());
	for (const auto &comp : m_components) {
		if (comp->GetExtraVariables()) {
			m_voltageSourceMap[comp->GetId()] = k++;
		}
	}

	return static_cast<int>(nodes.size() + m_voltageSourceMap.size());
}

// Ricalcola la matrice MNA e il vettore b solo se il circuito è stato modificato.
// Azzera le mappe, le ricostruisce, poi chiede a ogni componente di "stamparsi".
void CircuitLab::Circuit::ComputeCircuit()
{
	if (!m_isDirty)
		return;

	// Le mappe vanno azzerate prima di ogni ricalcolo per evitare dati obsoleti
	m_nodesMap.clear();
	m_voltageSourceMap.clear();

	int num_nodes = ComputeNodes();
	m_circuitMatrix = Eigen::MatrixXd::Zero(num_nodes, num_nodes);
	m_circuitVector = Eigen::VectorXd::Zero(num_nodes);

	for (const auto &comp : m_components)
		comp->Stamp(m_circuitMatrix, m_circuitVector, m_nodesMap, m_voltageSourceMap);

	m_isDirty = false;

	// DA CANCELLARE
	std::cout << m_circuitMatrix << std::endl;
	std::cout << m_circuitVector << std::endl;
}

// Restituisce l'ID del terminale dato il componente e l'indice del terminale (-1 se non trovato)
int CircuitLab::Circuit::GetTerminalId(int compId, int termIndex) const
{
	for (const auto &comp : m_components)
		if (comp->GetId() == compId)
			return comp->GetTerminal(termIndex).GetId();
	return -1;
}

// Restituisce l'ID del componente che possiede il terminale con l'ID dato (-1 se non trovato)
int CircuitLab::Circuit::GetComponentId(int terminalId) const
{
	for (const auto &comp : m_components)
		for (const auto &term : comp->GetTerminals())
			if (term.GetId() == terminalId)
				return comp->GetId();
	return -1;
}

// Restituisce un puntatore costante al componente con l'ID dato (nullptr se non trovato)
const CircuitLab::Component *CircuitLab::Circuit::GetComponentById(int compId) const
{
	for (auto const &c : m_components)
		if (c->GetId() == compId)
			return c.get();
	return nullptr;
}

// Versione non-const: usata quando è necessario modificare il componente trovato
CircuitLab::Component *CircuitLab::Circuit::GetComponentById(int compId)
{
	for (auto const &c : m_components)
		if (c->GetId() == compId)
			return c.get();
	return nullptr;
}

bool CircuitLab::Circuit::IsDuplicate(Link newLink) const
{
	bool duplicate = std::any_of(m_links.begin(), m_links.end(), [&](const Link &l) {
		return (l.compId1 == newLink.compId1 && l.termIndex1 == newLink.termIndex1 && l.compId2 == newLink.compId2 && l.termIndex2 == newLink.termIndex2) ||
			(l.compId1 == newLink.compId2 && l.termIndex1 == newLink.termIndex2 && l.compId2 == newLink.compId1 && l.termIndex2 == newLink.termIndex1);
		});
	return duplicate;
}

// Stampa a console lo stato di ogni componente e dei suoi terminali (debug)
void CircuitLab::Circuit::PrintCircuit()
{
	for (const auto &comp : m_components)
	{
		std::cout << "Component id: " << comp->GetId() << std::endl;
		for (int i = 0; i < comp->GetTerminals().size(); i++)
			std::cout << "Terminale " << i << " nodeId: " << comp->GetTerminal(i).GetNodeId() << std::endl;
	}
}

// Restituisce la lista dei nodeId dei terminali del componente con l'ID dato
std::vector<int> CircuitLab::Circuit::GetNodesIdFromComponentId(int compId) const
{
	for (const auto &comp : m_components)
		if (comp->GetId() == compId)
			return comp->GetTerminalId();
	return std::vector<int>();
}

// Ricerca inversa nella mappa nodi: dato un indice nella matrice, restituisce il nodeId (-1 se non trovato)
int CircuitLab::Circuit::GetNodesFromIndex(int index) const
{
	for (const auto &[key, value] : m_nodesMap)
		if (value == index)
			return key;
	return -1;
}

// Ricerca inversa nella mappa sorgenti: dato un indice nella matrice, restituisce il componentId (-1 se non trovato)
int CircuitLab::Circuit::GetCurrentFromIndex(int index) const
{
	for (const auto &[key, value] : m_voltageSourceMap)
		if (value == index)
			return key;
	return -1;
}

bool CircuitLab::Circuit::CircuitHasOnlyGround() const
{
	for (auto const &comp : m_components)
		if (!comp->IsGround())
			return false;

	return true;
}

// Collega due terminali tra loro, unificando i nodeId.
// Gestisce tre casi:
//   1. Entrambi i terminali sono liberi (-1): assegna un nuovo nodeId
//   2. Uno dei due è ground (0): propaga lo 0 a tutti i terminali del vecchio nodo
//   3. Uno dei due ha già un nodeId > 0: propaga quel nodeId all'altro e a tutti i collegati
bool CircuitLab::Circuit::ConnectTerminals(int comp1Id, int termComp1, int comp2Id, int termComp2, bool addLink)
{
	if (comp1Id == comp2Id) return false;

	Component *comp1 = nullptr;
	Component *comp2 = nullptr;
	int nodeId = -1;
	int oldNodeId = -1;

	for (const auto &comp : m_components) {
		if (comp->GetId() == comp1Id) comp1 = comp.get();
		if (comp->GetId() == comp2Id) comp2 = comp.get();
	}

	if (!comp1 || !comp2) return false;
	
	// Evita di ricollegare terminali già sullo stesso nodo
	if (comp1->GetTerminal(termComp1).GetId() == comp2->GetTerminal(termComp2).GetId()) return false;


	if(addLink)
		if (!IsDuplicate(Link{ comp1Id, termComp1, comp2Id, termComp2 }))
			m_links.emplace_back(Link{ comp1Id, termComp1, comp2Id, termComp2 });

	// Caso 1: entrambi liberi -> nuovo nodo
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
		std::cout << "Link: comp" << comp1 << " term" << termComp1
			<< " -> comp" << comp2 << " term" << termComp2 << std::endl;
		return true;
	}

	// Caso 2: uno dei due è ground -> propaga 0 a tutti i terminali del vecchio nodo
	if (comp1->GetTerminals()[termComp1].GetNodeId() == 0 || comp2->GetTerminals()[termComp2].GetNodeId() == 0)
	{
		int id1 = comp1->GetTerminals()[termComp1].GetNodeId();
		int id2 = comp2->GetTerminals()[termComp2].GetNodeId();
		if (id1 != 0) oldNodeId = id1;
		if (id2 != 0) oldNodeId = id2;

		comp1->GetTerminal(termComp1).SetNodeId(0);
		comp2->GetTerminal(termComp2).SetNodeId(0);

		if (oldNodeId > 0)
			for (auto const &comp : m_components)
				for (int i = 0; i < comp->GetTerminals().size(); i++)
					if (comp->GetTerminal(i).GetNodeId() == oldNodeId)
						comp->GetTerminal(i).SetNodeId(0);

		// DA CANCELLARE
		std::cout << "comp1 id=" << comp1Id << " term=" << termComp1
			<< " nodeId=" << comp1->GetTerminal(termComp1).GetNodeId() << std::endl;
		std::cout << "comp2 id=" << comp2Id << " term=" << termComp2
			<< " nodeId=" << comp2->GetTerminal(termComp2).GetNodeId() << std::endl;
		std::cout << "Link: comp" << comp1 << " term" << termComp1
			<< " -> comp" << comp2 << " term" << termComp2 << std::endl;
		return true;
	}

	// Caso 3: almeno uno ha un nodeId > 0 -> propaga quel nodeId all'altro e ai collegati
	if (comp1->GetTerminals()[termComp1].GetNodeId() > 0 || comp2->GetTerminals()[termComp2].GetNodeId() > 0)
	{
		int id1 = comp1->GetTerminals()[termComp1].GetNodeId();
		int id2 = comp2->GetTerminals()[termComp2].GetNodeId();
		if (id1 > 0)
		{
			oldNodeId = id2;
			comp1->GetTerminal(termComp1).SetNodeId(id1);
			comp2->GetTerminal(termComp2).SetNodeId(id1);

			if (oldNodeId > 0)
				for (auto const &comp : m_components)
					for (int i = 0; i < comp->GetTerminals().size(); i++)
						if (comp->GetTerminal(i).GetNodeId() == oldNodeId)
							comp->GetTerminal(i).SetNodeId(id1);
		}
		else
		{
			oldNodeId = id1;
			comp1->GetTerminal(termComp1).SetNodeId(id2);
			comp2->GetTerminal(termComp2).SetNodeId(id2);

			if (oldNodeId > 0)
				for (auto const &comp : m_components)
					for (int i = 0; i < comp->GetTerminals().size(); i++)
						if (comp->GetTerminal(i).GetNodeId() == oldNodeId)
							comp->GetTerminal(i).SetNodeId(id2);
		}

		// DA CANCELLARE
		std::cout << "comp1 id=" << comp1Id << " term=" << termComp1
			<< " nodeId=" << comp1->GetTerminal(termComp1).GetNodeId() << std::endl;
		std::cout << "comp2 id=" << comp2Id << " term=" << termComp2
			<< " nodeId=" << comp2->GetTerminal(termComp2).GetNodeId() << std::endl;
		std::cout << "Link: comp" << comp1 << " term" << termComp1
			<< " -> comp" << comp2 << " term" << termComp2 << std::endl;
		return true;
	}

	return false;
}

CircuitLab::Circuit::Circuit() : m_isDirty(true), m_nextNodeId(1)
{}

// I getter usano lazy evaluation: delegano a ComputeCircuit() che agisce solo se dirty
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

// Rimuove un componente dal circuito insieme a tutti i link che lo coinvolgono.
// Dopo la rimozione, azzera i nodeId di tutti i terminali rimasti e
// ricostruisce le connessioni rieseguendo i link sopravvissuti,
// in modo da mantenere la topologia del circuito consistente.
void CircuitLab::Circuit::RemoveComponent(int compId)
{
	if (!GetComponentById(compId)) return;

	//DA CANCELLARE
	std::cout << "Link iniziali: " << m_links.size() << std::endl;

	// Rimuove il componente dalla lista
	m_components.erase(
		std::remove_if(m_components.begin(), m_components.end(),
			[compId](const std::unique_ptr<Component> &c) {
				return c->GetId() == compId;
			}),
		m_components.end()
	);

	// Rimuove tutti i link che coinvolgevano il componente eliminato
	m_links.erase(
		std::remove_if(m_links.begin(), m_links.end(),
			[compId](const Link &l) {
				return (l.compId1 == compId || l.compId2 == compId);
			}),
		m_links.end()
	);

	// DA CANCELLARE
	std::cout << "Link finali: " << m_links.size() << std::endl;

	// Azzera i nodeId di tutti i terminali non-ground per ripartire da zero
	for (auto const &comp : m_components)
	{
		if (comp->IsGround()) continue;
		for (int i = 0; i < comp->GetTerminals().size(); i++)
			comp->GetTerminal(i).SetNodeId(-1);
	}

	// Ricostruisce le connessioni rieseguendo i link rimasti
	for (auto const &link : m_links)
		ConnectTerminals(link.compId1, link.termIndex1, link.compId2, link.termIndex2, false);

	// DA CANCELLARE
	std::cout << "Link finali2: " << m_links.size() << std::endl;

	InvalidateCircuit();
}