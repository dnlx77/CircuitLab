#include "UI/WireGraph.h"

#include <algorithm>
#include <cmath>

void CircuitLab::WireGraph::Clear()
{
	nodeViews.clear();
	linkViews.clear();
	nodeViewCount = 0;
	linkViewIdCount = 0;
}

// --- Ricerca ---

CircuitLab::NodeView *CircuitLab::WireGraph::FindNodeView(int id)
{
	for (auto &nv : nodeViews)
		if (nv.id == id)
			return &nv;
	return nullptr;
}

const CircuitLab::NodeView *CircuitLab::WireGraph::FindNodeView(int id) const
{
	for (const auto &nv : nodeViews)
		if (nv.id == id)
			return &nv;
	return nullptr;
}

CircuitLab::LinkView *CircuitLab::WireGraph::FindLinkView(int id)
{
	for (auto &lv : linkViews)
		if (lv.id == id)
			return &lv;
	return nullptr;
}

const CircuitLab::LinkView *CircuitLab::WireGraph::FindLinkView(int id) const
{
	for (const auto &lv : linkViews)
		if (lv.id == id)
			return &lv;
	return nullptr;
}

const CircuitLab::LinkView *CircuitLab::WireGraph::TapOfTerminal(int compId, int termIndex) const
{
	for (const auto &lv : linkViews)
		if (lv.sourceNodeViewId == -1 && lv.compIdA == compId && lv.termIndexA == termIndex)
			return &lv;
	return nullptr;
}

int CircuitLab::WireGraph::NodeViewIdOfTerminal(int compId, int termIndex) const
{
	const LinkView *tap = TapOfTerminal(compId, termIndex);
	return tap ? tap->nodeViewId : -1;
}

std::vector<int> CircuitLab::WireGraph::BusNeighbors(int nvId) const
{
	std::vector<int> neighbors;
	const NodeView *nv = FindNodeView(nvId);
	if (!nv)
		return neighbors;

	for (int linkId : nv->linkViewIds)
	{
		const LinkView *lv = FindLinkView(linkId);
		if (!lv || lv->sourceNodeViewId == -1)
			continue; // un tap, non un tratto di bus
		neighbors.push_back(lv->sourceNodeViewId == nvId ? lv->nodeViewId : lv->sourceNodeViewId);
	}
	return neighbors;
}

int CircuitLab::WireGraph::BusDegree(int nvId) const
{
	return static_cast<int>(BusNeighbors(nvId).size());
}

std::set<int> CircuitLab::WireGraph::CollectGroup(int nvId) const
{
	std::set<int> group;
	if (!FindNodeView(nvId))
		return group;

	std::vector<int> queue{ nvId };
	group.insert(nvId);
	while (!queue.empty())
	{
		int current = queue.back();
		queue.pop_back();
		for (int neighbor : BusNeighbors(current))
			if (group.insert(neighbor).second)
				queue.push_back(neighbor);
	}
	return group;
}

bool CircuitLab::WireGraph::SameGroup(int nvIdA, int nvIdB) const
{
	return CollectGroup(nvIdA).count(nvIdB) > 0;
}

std::vector<CircuitLab::TerminalRef> CircuitLab::WireGraph::TerminalsInGroup(int nvId) const
{
	std::vector<TerminalRef> terminals;
	for (int id : CollectGroup(nvId))
	{
		const NodeView *nv = FindNodeView(id);
		if (nv && nv->anchorCompId != -1)
			terminals.push_back({ nv->anchorCompId, nv->anchorTermIndex });
	}
	return terminals;
}

CircuitLab::TerminalRef CircuitLab::WireGraph::FindRealTapInGroup(int startNvId, int excludeCompId, int excludeTermIndex) const
{
	for (int nvId : CollectGroup(startNvId))
	{
		const NodeView *nv = FindNodeView(nvId);
		for (int linkId : nv->linkViewIds)
		{
			const LinkView *lv = FindLinkView(linkId);
			if (lv && lv->sourceNodeViewId == -1 && lv->nodeViewId == nvId &&
				!(lv->compIdA == excludeCompId && lv->termIndexA == excludeTermIndex))
				return { lv->compIdA, lv->termIndexA };
		}
	}
	return { -1, -1 };
}

// --- Costruzione ---

int CircuitLab::WireGraph::AddNodeView(int nodeId, Vec2 position,
	int anchorCompId, int anchorTermIndex, bool attached)
{
	NodeView nv;
	nv.id = static_cast<int>(++nodeViewCount);
	nv.nodeId = nodeId;
	nv.position = position;
	nv.anchorCompId = anchorCompId;
	nv.anchorTermIndex = anchorTermIndex;
	nv.attached = attached;
	nodeViews.push_back(nv);
	return nv.id;
}

int CircuitLab::WireGraph::AddTapLink(int compId, int termIndex, Vec2 terminalPos, int nvId)
{
	NodeView *nv = FindNodeView(nvId);
	if (!nv)
		return -1;

	LinkView lv;
	lv.id = static_cast<int>(++linkViewIdCount);
	lv.startPos = terminalPos;
	lv.targetPos = nv->position;
	lv.compIdA = compId;
	lv.termIndexA = termIndex;
	lv.nodeViewId = nvId;
	lv.sourceNodeViewId = -1;
	linkViews.push_back(lv);

	nv->linkViewIds.push_back(lv.id);
	return lv.id;
}

int CircuitLab::WireGraph::AddBusEdge(int sourceNvId, int targetNvId)
{
	const NodeView *source = FindNodeView(sourceNvId);
	const NodeView *target = FindNodeView(targetNvId);
	if (!source || !target)
		return -1;

	LinkView lv;
	lv.id = static_cast<int>(++linkViewIdCount);
	lv.startPos = source->position;
	lv.targetPos = target->position;
	lv.compIdA = -1;
	lv.termIndexA = -1;
	lv.nodeViewId = targetNvId;
	lv.sourceNodeViewId = sourceNvId;
	linkViews.push_back(lv);

	FindNodeView(sourceNvId)->linkViewIds.push_back(lv.id);
	FindNodeView(targetNvId)->linkViewIds.push_back(lv.id);
	return lv.id;
}

int CircuitLab::WireGraph::EnsureTerminalNodeView(int compId, int termIndex, Vec2 terminalPos)
{
	int existing = NodeViewIdOfTerminal(compId, termIndex);
	if (existing != -1)
		return existing;

	int nvId = AddNodeView(-1, terminalPos, compId, termIndex, true);
	AddTapLink(compId, termIndex, terminalPos, nvId);
	return nvId;
}

int CircuitLab::WireGraph::InsertNodeOnBusEdge(int linkId, Vec2 clickPos)
{
	const LinkView *edge = FindLinkView(linkId);
	if (!edge || edge->sourceNodeViewId == -1)
		return -1;

	const int sourceId = edge->sourceNodeViewId;
	const int targetId = edge->nodeViewId;
	const Vec2 a = FindNodeView(sourceId)->position;
	const Vec2 b = FindNodeView(targetId)->position;

	// Punto del filo più vicino al click (proiezione sul segmento a-b)
	const Vec2 ab = b - a;
	const float lengthSquared = ab.x * ab.x + ab.y * ab.y;
	float t = 0.5f;
	if (lengthSquared > 1e-9f)
		t = std::clamp(((clickPos.x - a.x) * ab.x + (clickPos.y - a.y) * ab.y) / lengthSquared, 0.0f, 1.0f);
	const Vec2 point = a + ab * t;

	// Da qui i puntatori a nodeViews/linkViews non sono più validi dopo ogni
	// inserimento (i vettori possono riallocare): si rileggono per id.
	const int newId = AddNodeView(-1, point);

	// Il filo originale diventa il primo mezzo tratto: source -> nuovo nodo...
	LinkView *first = FindLinkView(linkId);
	first->nodeViewId = newId;
	first->targetPos = point;

	NodeView *target = FindNodeView(targetId);
	target->linkViewIds.erase(std::remove(target->linkViewIds.begin(), target->linkViewIds.end(), linkId), target->linkViewIds.end());
	FindNodeView(newId)->linkViewIds.push_back(linkId);

	// ...e un nuovo tratto porta dal nuovo nodo al vecchio destinatario.
	AddBusEdge(newId, targetId);
	return newId;
}

// --- Spostamento ---

void CircuitLab::WireGraph::SetNodeViewPosition(int nvId, Vec2 position)
{
	NodeView *nv = FindNodeView(nvId);
	if (!nv)
		return;

	nv->position = position;
	for (int linkId : nv->linkViewIds)
	{
		LinkView *lv = FindLinkView(linkId);
		if (!lv)
			continue;
		// Per un dato filo i due casi si escludono (nodeViewId e sourceNodeViewId
		// non coincidono mai): come destinazione si aggiorna targetPos, come
		// sorgente di un tratto di bus startPos.
		if (lv->nodeViewId == nvId)
			lv->targetPos = position;
		if (lv->sourceNodeViewId == nvId)
			lv->startPos = position;
	}
}

void CircuitLab::WireGraph::MoveTerminal(int compId, int termIndex, Vec2 terminalPos)
{
	LinkView *tap = nullptr;
	for (auto &lv : linkViews)
		if (lv.sourceNodeViewId == -1 && lv.compIdA == compId && lv.termIndexA == termIndex)
		{
			tap = &lv;
			break;
		}
	if (!tap)
		return;

	tap->startPos = terminalPos;

	const NodeView *nv = FindNodeView(tap->nodeViewId);
	if (nv && nv->attached && nv->anchorCompId == compId && nv->anchorTermIndex == termIndex)
		SetNodeViewPosition(nv->id, terminalPos);
}

void CircuitLab::WireGraph::DetachIfAnchored(int nvId)
{
	NodeView *nv = FindNodeView(nvId);
	if (nv && nv->anchorCompId != -1)
		nv->attached = false;
}

bool CircuitLab::WireGraph::TryReattach(int nvId, Vec2 terminalPos, float maxDistance)
{
	NodeView *nv = FindNodeView(nvId);
	if (!nv || nv->anchorCompId == -1)
		return false;

	const Vec2 delta = nv->position - terminalPos;
	if (std::sqrt(delta.x * delta.x + delta.y * delta.y) > maxDistance)
		return false;

	SetNodeViewPosition(nvId, terminalPos);
	FindNodeView(nvId)->attached = true;
	return true;
}

// --- Rimozione ---

void CircuitLab::WireGraph::RemoveLinkRaw(int linkId)
{
	const LinkView *lv = FindLinkView(linkId);
	if (!lv)
		return;

	const int ends[2] = { lv->nodeViewId, lv->sourceNodeViewId };
	for (int nvId : ends)
	{
		NodeView *nv = (nvId != -1) ? FindNodeView(nvId) : nullptr;
		if (nv)
			nv->linkViewIds.erase(std::remove(nv->linkViewIds.begin(), nv->linkViewIds.end(), linkId), nv->linkViewIds.end());
	}

	linkViews.erase(
		std::remove_if(linkViews.begin(), linkViews.end(), [linkId](const LinkView &l) { return l.id == linkId; }),
		linkViews.end());
}

void CircuitLab::WireGraph::EraseNodeViewRaw(int nvId)
{
	nodeViews.erase(
		std::remove_if(nodeViews.begin(), nodeViews.end(), [nvId](const NodeView &n) { return n.id == nvId; }),
		nodeViews.end());
}

std::vector<int> CircuitLab::WireGraph::RemoveNodeViewReconnecting(int nvId)
{
	const NodeView *nv = FindNodeView(nvId);
	if (!nv)
		return {};

	const std::vector<int> neighbors = BusNeighbors(nvId);
	const std::vector<int> linkIds = nv->linkViewIds; // copia: RemoveLinkRaw la modifica
	for (int linkId : linkIds)
		RemoveLinkRaw(linkId);
	EraseNodeViewRaw(nvId);

	// I vicini erano nello stesso nodo elettrico solo grazie a questo NodeView:
	// senza un filo tra loro il disegno direbbe il contrario del Circuit, che li
	// tiene uniti. A stella attorno al primo: resta un albero, mai un ciclo.
	for (size_t i = 1; i < neighbors.size(); i++)
		AddBusEdge(neighbors[0], neighbors[i]);

	return neighbors;
}

void CircuitLab::WireGraph::CleanUp(int nvId, std::vector<TerminalRef> &freed)
{
	const NodeView *nv = FindNodeView(nvId);
	if (!nv)
		return; // già rimosso da una pulizia precedente

	const int degree = BusDegree(nvId);

	if (nv->anchorCompId != -1)
	{
		// Un terminale senza più nessun filo: il pallino non ha più motivo di
		// esistere e il terminale torna libero (anche per il Circuit).
		if (degree == 0)
		{
			freed.push_back({ nv->anchorCompId, nv->anchorTermIndex });
			const std::vector<int> linkIds = nv->linkViewIds; // i tap
			for (int linkId : linkIds)
				RemoveLinkRaw(linkId);
			EraseNodeViewRaw(nvId);
		}
		return;
	}

	// Nodo libero con un solo tratto: un moncone, il filo non porta da nessuna
	// parte. Si ritira e si controlla l'altro capo. Con 0 tratti (un nodo piazzato
	// a mano mai collegato) o con 2+ (un angolo o una derivazione) resta com'è.
	if (degree == 1)
	{
		const int other = BusNeighbors(nvId)[0];
		const std::vector<int> linkIds = nv->linkViewIds;
		for (int linkId : linkIds)
			RemoveLinkRaw(linkId);
		EraseNodeViewRaw(nvId);
		CleanUp(other, freed);
	}
}

std::vector<CircuitLab::TerminalRef> CircuitLab::WireGraph::RemoveComponent(int compId)
{
	std::vector<TerminalRef> freed;
	std::vector<int> touched;

	std::vector<int> anchoredIds;
	for (const auto &nv : nodeViews)
		if (nv.anchorCompId == compId)
			anchoredIds.push_back(nv.id);

	for (int nvId : anchoredIds)
		for (int neighbor : RemoveNodeViewReconnecting(nvId))
			touched.push_back(neighbor);

	// Tap rimasti del componente verso altri NodeView (un file vecchio non
	// ancora convertito): si tolgono comunque.
	std::vector<std::pair<int, int>> strays; // (linkId, nodeViewId)
	for (const auto &lv : linkViews)
		if (lv.sourceNodeViewId == -1 && lv.compIdA == compId)
			strays.emplace_back(lv.id, lv.nodeViewId);
	for (const auto &[linkId, nvId] : strays)
	{
		RemoveLinkRaw(linkId);
		touched.push_back(nvId);
	}

	for (int nvId : touched)
		CleanUp(nvId, freed);

	return freed;
}

std::vector<CircuitLab::TerminalRef> CircuitLab::WireGraph::RemoveFreeNodeView(int nvId)
{
	std::vector<TerminalRef> freed;
	const NodeView *nv = FindNodeView(nvId);
	if (!nv || nv->anchorCompId != -1)
		return freed;

	for (int neighbor : RemoveNodeViewReconnecting(nvId))
		CleanUp(neighbor, freed);

	return freed;
}

int CircuitLab::WireGraph::PassThroughNodeOfEdge(int linkId) const
{
	const LinkView *lv = FindLinkView(linkId);
	if (!lv || lv->sourceNodeViewId == -1)
		return -1;

	const int ends[2] = { lv->sourceNodeViewId, lv->nodeViewId };
	for (int nvId : ends)
	{
		const NodeView *nv = FindNodeView(nvId);
		if (nv && nv->anchorCompId == -1 && BusDegree(nvId) == 2)
			return nvId;
	}
	return -1;
}
