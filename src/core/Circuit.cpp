#include <set>
#include <iostream>

#include "Core/Circuit.h"
#include "Common/Logger.h"

// Scandisce tutti i terminali di tutti i componenti per raccogliere i nodi attivi
// (esclude il nodo ground, nodeId == 0, e i terminali mai collegati, nodeId == -1),
// costruisce la mappa nodeId -> indice matrice, e assegna le righe extra per le
// sorgenti di tensione.
// Restituisce la dimensione totale della matrice MNA.
int CircuitLab::Circuit::ComputeNodes()
{
	std::set<int> nodes;
	for (const auto &comp : m_components) {
		const std::vector<Terminal> &terminals = comp->GetTerminals();
		for (const auto &terminal : terminals) {
			// Il nodo 0 è il ground e -1 è "mai collegato": nessuno dei due
			// occupa una riga nella matrice MNA. Con "!= 0" invece di "> 0",
			// tutti i terminali scollegati (nodeId == -1, il valore di default)
			// venivano trattati come se fossero un unico nodo reale condiviso,
			// corrompendo la soluzione.
			if (terminal.GetNodeId() > 0)
				nodes.insert(terminal.GetNodeId());
		}
	}

	// Mappa ogni nodeId a un indice progressivo (0, 1, 2, ...)
	int i = 0;
	for (const auto &node : nodes) {
		m_nodesMap[node] = i;
		i++;
	}

	// LOG
	for (const auto &n : nodes)
		LOG_DEBUG("node in set: " << n);

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

void CircuitLab::Circuit::ComputeMatrix()
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
		comp->StampMatrix(m_circuitMatrix, m_nodesMap, m_voltageSourceMap, m_h);

	if (m_onFactorize)
		m_onFactorize(m_circuitMatrix);
	m_isDirty = false;
}

void CircuitLab::Circuit::ComputeVector(const StampContext &ctx)
{
	assert(!m_isDirty && "ComputerVector called before ComputeMatrix");
	m_circuitVector = Eigen::VectorXd::Zero(m_circuitMatrix.rows());

	for (const auto &comp : m_components)
		comp->StampVector(m_circuitVector, m_nodesMap, m_voltageSourceMap, ctx);
}

bool CircuitLab::Circuit::HasNonlinearComponents() const
{
	for (const auto &comp : m_components)
		if (comp->IsNonlinear())
			return true;
	return false;
}

bool CircuitLab::Circuit::StampNonlinear(Eigen::MatrixXd &A, Eigen::VectorXd &B, const Eigen::VectorXd &x) const
{
	bool anyLimited = false;
	for (const auto &comp : m_components)
		if (comp->IsNonlinear())
			anyLimited |= comp->StampNonlinear(A, B, m_nodesMap, x);
	return anyLimited;
}

bool CircuitLab::Circuit::NonlinearConverged(const Eigen::VectorXd &x) const
{
	for (const auto &comp : m_components)
		if (comp->IsNonlinear() && !comp->HasConverged(m_nodesMap, x))
			return false;
	return true;
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

double CircuitLab::Circuit::GetMinFrequency() const
{
	double minFrequency = 0.0;
	for (auto const &comp : m_components)
	{
		if (comp->GetType() == ComponentType::voltageGenerator && comp->GetWaveFormType() != WaveFormType::none && comp->GetWaveFormType() != WaveFormType::dcWaveForm)
		{
			auto value = comp->GetValues();

			double freq = value.at(ComponentValue::frequency);
			if (freq > 0.0 && (minFrequency == 0.0 || freq < minFrequency))
				minFrequency = freq;
		}
	}

	return minFrequency;
}

double CircuitLab::Circuit::GetMaxFrequency() const
{
	double maxFrequency = 0.0;
	for (auto const &comp : m_components)
	{
		if (comp->GetType() == ComponentType::voltageGenerator &&
			comp->GetWaveFormType() != WaveFormType::none &&
			comp->GetWaveFormType() != WaveFormType::dcWaveForm)
		{
			auto value = comp->GetValues();
			double freq = value.at(ComponentValue::frequency);
			if (freq > maxFrequency)
				maxFrequency = freq;
		}
	}
	return maxFrequency;
}

// Stampa a console lo stato di ogni componente e dei suoi terminali (debug)
void CircuitLab::Circuit::PrintCircuit()
{
	LOG_INFO("Circuito:");
	for (const auto &comp : m_components)
	{
		LOG_INFO("Component id: " << comp->GetId());
		for (int i = 0; i < comp->GetTerminals().size(); i++)
			LOG_INFO("Terminale " << i << " nodeId: " << comp->GetTerminal(i).GetNodeId());
	}
}

void CircuitLab::Circuit::Clear()
{
	m_nextNodeId = 1;
	m_isDirty = true;
	m_components.clear();
	m_links.clear();
	Component::Reset();
}

// Restituisce la lista dei nodeId dei terminali del componente con l'ID dato
std::vector<int> CircuitLab::Circuit::GetNodesIdFromComponentId(int compId) const
{
	for (const auto &comp : m_components)
		if (comp->GetId() == compId)
			return comp->GetTerminalNodeIds();
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

int CircuitLab::Circuit::GetIndexFromNodes(int nodeId) const
{
	return m_nodesMap.at(nodeId);
}

// Ricerca inversa nella mappa sorgenti: dato un indice nella matrice, restituisce il componentId (-1 se non trovato)
int CircuitLab::Circuit::GetCurrentFromIndex(int index) const
{
	for (const auto &[key, value] : m_voltageSourceMap)
		if (value == index)
			return key;
	return -1;
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::Circuit::GetComponentValues(int compId) const
{
	return GetComponentById(compId)->GetValues();
}

void CircuitLab::Circuit::SetComponentValues(int compId, const std::map<CircuitLab::ComponentValue, double> &values)
{
	GetComponentById(compId)->SetValues(values);
	InvalidateCircuit();
}

void CircuitLab::Circuit::ToggleSwitch(int compId)
{
	GetComponentById(compId)->ToggleSwitch();
	InvalidateCircuit();
}

std::vector<int> CircuitLab::Circuit::GetComponentsByNodeId(int nodeId) const
{
	std::vector<int> connectComp;
	for (const auto &comp : m_components)
	{
		std::vector<int> terms = comp->GetTerminalNodeIds();
		for (const auto term : terms)
		{
			if (term == nodeId)
			{
				connectComp.emplace_back(comp->GetId());
				break;
			}
		}
	}
	return connectComp;
}

CircuitLab::ComponentType CircuitLab::Circuit::GetComponentType(int compId) const
{
	for (const auto &comp : m_components)
		if (comp->GetId() == compId)
			return comp->GetType();

	return ComponentType::node;
}

bool CircuitLab::Circuit::CircuitHasOnlyGround() const
{
	for (auto const &comp : m_components)
		if (!comp->IsGround())
			return false;

	return true;
}

bool CircuitLab::Circuit::HasFloatingTerminal() const
{
	// Conta quanti terminali condividono ciascun nodeId "reale" (>= 0).
	std::map<int, int> nodeTerminalCount;

	for (auto const &comp : m_components)
	{
		for (auto const &terminal : comp->GetTerminals())
		{
			int nodeId = terminal.GetNodeId();
			// -1 è il valore condiviso da OGNI terminale mai collegato: non indica
			// un nodo reale, quindi va controllato a parte (non nella mappa sopra,
			// altrimenti terminali scollegati e non correlati sembrerebbero "collegati
			// tra loro" solo perché condividono lo stesso segnaposto).
			if (nodeId == -1)
				return true;
			nodeTerminalCount[nodeId]++;
		}
	}

	// Un nodo con un solo terminale è un ramo aperto: elettricamente equivale
	// a un terminale mai collegato (nessun percorso di ritorno per la corrente),
	// anche se qui ha un nodeId "vero" invece di -1 (es. l'altro capo di un
	// componente rimasto orfano dopo aver cancellato ciò a cui era collegato).
	for (auto const &[nodeId, count] : nodeTerminalCount)
		if (count < 2)
			return true;

	return false;
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


	if (addLink)
		if (!IsDuplicate(Link{ comp1Id, termComp1, comp2Id, termComp2 }))
			m_links.emplace_back(Link{ comp1Id, termComp1, comp2Id, termComp2 });

	// Caso 1: entrambi liberi -> nuovo nodo
	if (comp1->GetTerminals()[termComp1].GetNodeId() < 0 && comp2->GetTerminals()[termComp2].GetNodeId() < 0)
	{
		nodeId = m_nextNodeId++;
		comp1->GetTerminal(termComp1).SetNodeId(nodeId);
		comp2->GetTerminal(termComp2).SetNodeId(nodeId);

		// DEBUG
		LOG_DEBUG("Collegamento di 2 terminali liberi");
		LOG_DEBUG("Link tra comp" << comp1Id << " term" << termComp1 << " -> comp" << comp2Id << " term" << termComp2);
		LOG_DEBUG("Link tra comp" << comp1Id << " term" << termComp1 << " (" << comp1->GetTerminal(termComp1).GetNodeId() <<
			") -> comp" << comp2Id << " term" << termComp2 << " (" << comp2->GetTerminal(termComp2).GetNodeId() << ")");
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

		// DEBUG
		LOG_DEBUG("Collegamento di un terminale a ground");
		LOG_DEBUG("Link tra comp" << comp1Id << " term" << termComp1 << " (" << comp1->GetTerminal(termComp1).GetNodeId() <<
			") -> comp" << comp2Id << " term" << termComp2 << " (" << comp2->GetTerminal(termComp2).GetNodeId() << ")");
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

		// DEBUG
		LOG_DEBUG("Collegamento a terminale esistente");
		LOG_DEBUG("Link tra comp" << comp1Id << " term" << termComp1 << " (" << comp1->GetTerminal(termComp1).GetNodeId() <<
			") -> comp" << comp2Id << " term" << termComp2 << " (" << comp2->GetTerminal(termComp2).GetNodeId() << ")");
		return true;
	}

	return false;
}

CircuitLab::Circuit::Circuit() : m_isDirty(true), m_nextNodeId(1), m_h(0.001)
{}

// I getter usano lazy evaluation: delegano a ComputeCircuit() che agisce solo se dirty
const Eigen::MatrixXd &CircuitLab::Circuit::GetCircuitMatrix()
{
	ComputeMatrix();
	return m_circuitMatrix;
}

const Eigen::VectorXd &CircuitLab::Circuit::GetCircuitVector()
{
	return m_circuitVector;
}

int CircuitLab::Circuit::AddComponent(std::unique_ptr<Component> comp)
{
	int id = comp->GetId();
	m_components.emplace_back(std::move(comp));
	return id;
}

void CircuitLab::Circuit::FreeTerminal(int compId, int termIndex)
{
	Component *comp = GetComponentById(compId);
	if (!comp || termIndex < 0 || termIndex >= static_cast<int>(comp->GetTerminals().size()))
		return;

	comp->GetTerminal(termIndex).SetNodeId(-1);

	m_links.erase(
		std::remove_if(m_links.begin(), m_links.end(),
			[compId, termIndex](const Link &l) {
				return (l.compId1 == compId && l.termIndex1 == termIndex) ||
					(l.compId2 == compId && l.termIndex2 == termIndex);
			}),
		m_links.end()
	);

	InvalidateCircuit();
}

void CircuitLab::Circuit::DetachFromGround(const std::vector<std::pair<int, int>> &terminals)
{
	std::vector<Terminal *> grounded;
	for (const auto &[compId, termIndex] : terminals)
	{
		Component *comp = GetComponentById(compId);
		if (!comp || termIndex < 0 || termIndex >= static_cast<int>(comp->GetTerminals().size()))
			continue;
		if (comp->GetTerminals()[termIndex].GetNodeId() == 0)
			grounded.push_back(&comp->GetTerminal(termIndex));
	}
	if (grounded.empty())
		return;

	// Un terminale solo non ha più nessuno con cui condividere il nodo; due o più
	// restano uniti su un nodo nuovo (che una massa ricollegata poi riporterà a 0).
	const int newNodeId = (grounded.size() == 1) ? -1 : m_nextNodeId++;
	for (Terminal *terminal : grounded)
		terminal->SetNodeId(newNodeId);

	InvalidateCircuit();
}

// Rimuove un componente dal circuito insieme a tutti i link che lo coinvolgono.
// I nodeId dei terminali rimasti NON vengono toccati: nessun componente di questo
// simulatore può collegare i propri due terminali tra loro (ConnectTerminals lo
// impedisce), quindi cancellare un componente non può mai richiedere di "separare"
// un nodo che due terminali di ALTRI componenti condividono — quella condivisione
// esiste indipendentemente da lui. Ricostruire tutto da zero rieseguendo solo i
// link sopravvissuti era sbagliato: se un nodo era formato da più link in serie che
// passavano per il terminale del componente eliminato (es. una giunzione a 3 fili),
// si perdeva anche il collegamento tra gli altri due, anche se elettricamente
// restavano sullo stesso nodo.
void CircuitLab::Circuit::RemoveComponent(int compId)
{
	if (!GetComponentById(compId)) return;

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

	// Un terminale rimasto SOLO sul suo nodo non è più collegato a nulla: lo si
	// riporta a "libero" (-1). Vale sia per un nodo reale (>0) rimasto con un
	// terminale, sia per la massa (0) quando è stato eliminato l'ultimo componente
	// Ground. Senza questo il terminale conserverebbe il vecchio numero di nodo
	// (o resterebbe collegato a massa) senza alcun filo che lo mostri, e
	// l'interfaccia, che ora toglie il filo insieme al componente, direbbe una
	// cosa diversa dal circuito. Gli altri terminali dei nodi condivisi restano.
	std::map<int, int> terminalsPerNode;
	bool groundLeft = false;
	for (const auto &comp : m_components)
	{
		if (comp->IsGround())
			groundLeft = true;
		for (const auto &term : comp->GetTerminals())
			if (term.GetNodeId() > 0)
				terminalsPerNode[term.GetNodeId()]++;
	}
	for (const auto &comp : m_components)
		for (int i = 0; i < static_cast<int>(comp->GetTerminals().size()); i++)
		{
			const int nodeId = comp->GetTerminals()[i].GetNodeId();
			if ((nodeId > 0 && terminalsPerNode[nodeId] == 1) || (nodeId == 0 && !groundLeft))
				comp->GetTerminal(i).SetNodeId(-1);
		}

	InvalidateCircuit();
}