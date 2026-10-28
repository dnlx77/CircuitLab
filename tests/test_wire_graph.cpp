// WireGraph e' la topologia visiva dei fili (NodeView e LinkView): non dipende da finestra, disegno
// o Circuit, quindi si prova da sola. Non usa SFML.
#include <algorithm>
#include <set>

#include "catch_amalgamated.hpp"
#include "UI/WireGraph.h"

using namespace CircuitLab;

namespace {

	// Ogni NodeView elenca solo fili esistenti che lo toccano, ogni filo e' elencato dai suoi
	// estremi, e i tratti di bus di uno stesso nodo elettrico non formano cicli
	void CheckConsistent(const WireGraph &graph)
	{
		for (const auto &nv : graph.nodeViews)
			for (int linkId : nv.linkViewIds)
			{
				const LinkView *lv = graph.FindLinkView(linkId);
				REQUIRE(lv != nullptr);
				CHECK((lv->nodeViewId == nv.id || lv->sourceNodeViewId == nv.id));
			}

		int busEdges = 0;
		for (const auto &lv : graph.linkViews)
		{
			const NodeView *target = graph.FindNodeView(lv.nodeViewId);
			REQUIRE(target != nullptr);
			CHECK(std::count(target->linkViewIds.begin(), target->linkViewIds.end(), lv.id) == 1);
			if (lv.sourceNodeViewId != -1)
			{
				const NodeView *source = graph.FindNodeView(lv.sourceNodeViewId);
				REQUIRE(source != nullptr);
				CHECK(std::count(source->linkViewIds.begin(), source->linkViewIds.end(), lv.id) == 1);
				busEdges++;
			}
		}

		// Una foresta con g gruppi e n NodeView ha esattamente n - g tratti
		std::set<int> seen;
		int groups = 0;
		for (const auto &nv : graph.nodeViews)
			if (seen.count(nv.id) == 0)
			{
				groups++;
				for (int id : graph.CollectGroup(nv.id))
					seen.insert(id);
			}
		CHECK(busEdges == static_cast<int>(graph.nodeViews.size()) - groups);
	}

	// Filo tra due terminali: un NodeView ancorato per ciascuno, collegati da un tratto di bus
	int Wire(WireGraph &graph, TerminalRef a, Vec2 posA, TerminalRef b, Vec2 posB)
	{
		const int nvA = graph.EnsureTerminalNodeView(a.first, a.second, posA);
		const int nvB = graph.EnsureTerminalNodeView(b.first, b.second, posB);
		return graph.AddBusEdge(nvA, nvB);
	}

	bool Contains(const std::vector<TerminalRef> &terminals, TerminalRef t)
	{
		return std::find(terminals.begin(), terminals.end(), t) != terminals.end();
	}
}

TEST_CASE("WireGraph: un terminale ha un solo NodeView ancorato con il suo tap", "[wiregraph]")
{
	WireGraph graph;
	CHECK(graph.NodeViewIdOfTerminal(1, 0) == -1);
	CHECK(graph.TapOfTerminal(1, 0) == nullptr);

	const int nv = graph.EnsureTerminalNodeView(1, 0, { 10.f, 20.f });
	CHECK(graph.NodeViewIdOfTerminal(1, 0) == nv);
	CHECK(graph.EnsureTerminalNodeView(1, 0, { 99.f, 99.f }) == nv); // non ne crea un secondo
	CHECK(graph.nodeViews.size() == 1);
	CHECK(graph.linkViews.size() == 1);

	const NodeView *view = graph.FindNodeView(nv);
	REQUIRE(view != nullptr);
	CHECK(view->anchorCompId == 1);
	CHECK(view->anchorTermIndex == 0);
	CHECK(view->attached);
	CHECK(view->position == Vec2(10.f, 20.f));

	const LinkView *tap = graph.TapOfTerminal(1, 0);
	REQUIRE(tap != nullptr);
	CHECK(tap->sourceNodeViewId == -1);
	CHECK(tap->nodeViewId == nv);
	CHECK(graph.BusDegree(nv) == 0);
	CheckConsistent(graph);
}

TEST_CASE("WireGraph: i fili uniscono i terminali nello stesso gruppo", "[wiregraph]")
{
	WireGraph graph;
	Wire(graph, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	const int nvC = graph.EnsureTerminalNodeView(3, 1, { 50.f, 50.f });
	const int nvA = graph.NodeViewIdOfTerminal(1, 0);
	const int nvB = graph.NodeViewIdOfTerminal(2, 0);

	CHECK(graph.SameGroup(nvA, nvB));
	CHECK_FALSE(graph.SameGroup(nvA, nvC));
	CHECK(graph.CollectGroup(nvA) == std::set<int>{ nvA, nvB });
	CHECK(graph.CollectGroup(9999).empty());

	const auto terminals = graph.TerminalsInGroup(nvA);
	CHECK(terminals.size() == 2);
	CHECK(Contains(terminals, { 1, 0 }));
	CHECK(Contains(terminals, { 2, 0 }));

	// Il primo terminale del gruppo, escluso quello dato: serve a collegare il Circuit
	CHECK(graph.FindRealTapInGroup(nvA, 1, 0) == TerminalRef{ 2, 0 });
	CHECK(graph.FindRealTapInGroup(nvB, 2, 0) == TerminalRef{ 1, 0 });
	CHECK(graph.FindRealTapInGroup(nvC) == TerminalRef{ 3, 1 });
	CHECK(graph.FindRealTapInGroup(nvC, 3, 1) == TerminalRef{ -1, -1 });
	CheckConsistent(graph);
}

TEST_CASE("WireGraph: inserire un nodo su un filo lo spezza nel punto piu' vicino al click", "[wiregraph]")
{
	WireGraph graph;
	const int edge = Wire(graph, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	const int nvA = graph.NodeViewIdOfTerminal(1, 0);
	const int nvB = graph.NodeViewIdOfTerminal(2, 0);

	const int mid = graph.InsertNodeOnBusEdge(edge, { 30.f, 40.f }); // si proietta sul filo in x = 30
	REQUIRE(mid != -1);
	const NodeView *node = graph.FindNodeView(mid);
	REQUIRE(node != nullptr);
	CHECK(node->anchorCompId == -1); // nodo libero
	CHECK(node->position.x == Catch::Approx(30.f));
	CHECK(node->position.y == Catch::Approx(0.f));
	CHECK(graph.BusDegree(mid) == 2);

	// Il primo mezzo tratto conserva l'id del filo originale
	const LinkView *first = graph.FindLinkView(edge);
	REQUIRE(first != nullptr);
	CHECK(first->sourceNodeViewId == nvA);
	CHECK(first->nodeViewId == mid);

	CHECK(graph.SameGroup(nvA, nvB)); // il nodo elettrico e' sempre uno solo
	CHECK(graph.PassThroughNodeOfEdge(edge) == mid);
	CheckConsistent(graph);

	// Fuori dal filo il punto si limita agli estremi
	WireGraph other;
	const int edge2 = Wire(other, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	const int end = other.InsertNodeOnBusEdge(edge2, { 500.f, 0.f });
	CHECK(other.FindNodeView(end)->position.x == Catch::Approx(100.f));
	CheckConsistent(other);
}

TEST_CASE("WireGraph: un tap non e' un tratto di bus", "[wiregraph]")
{
	WireGraph graph;
	graph.EnsureTerminalNodeView(1, 0, { 0.f, 0.f });
	const int tapId = graph.TapOfTerminal(1, 0)->id;
	CHECK(graph.InsertNodeOnBusEdge(tapId, { 0.f, 0.f }) == -1);
	CHECK(graph.PassThroughNodeOfEdge(tapId) == -1);
	CHECK(graph.InsertNodeOnBusEdge(12345, { 0.f, 0.f }) == -1);
}

TEST_CASE("WireGraph: spostare un terminale trascina il suo NodeView solo se e' agganciato", "[wiregraph]")
{
	WireGraph graph;
	Wire(graph, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	const int nvA = graph.NodeViewIdOfTerminal(1, 0);
	const int busId = graph.linkViews.back().id;

	graph.MoveTerminal(1, 0, { 0.f, 10.f });
	CHECK(graph.FindNodeView(nvA)->position == Vec2(0.f, 10.f));
	CHECK(graph.TapOfTerminal(1, 0)->startPos == Vec2(0.f, 10.f));
	CHECK(graph.FindLinkView(busId)->startPos == Vec2(0.f, 10.f)); // anche il filo lo segue

	// Trascinato altrove il NodeView si stacca: il terminale si muove da solo e il tap diventa un filo
	graph.DetachIfAnchored(nvA);
	graph.SetNodeViewPosition(nvA, { 40.f, 40.f });
	graph.MoveTerminal(1, 0, { 0.f, 30.f });
	CHECK(graph.FindNodeView(nvA)->position == Vec2(40.f, 40.f));
	CHECK(graph.TapOfTerminal(1, 0)->startPos == Vec2(0.f, 30.f));

	// Lontano dal terminale non si riaggancia, vicino si
	CHECK_FALSE(graph.TryReattach(nvA, { 0.f, 30.f }, 5.f));
	CHECK_FALSE(graph.FindNodeView(nvA)->attached);
	CHECK(graph.TryReattach(nvA, { 38.f, 40.f }, 5.f));
	CHECK(graph.FindNodeView(nvA)->attached);
	CHECK(graph.FindNodeView(nvA)->position == Vec2(38.f, 40.f));

	// Un NodeView libero non si aggancia a niente
	const int freeNode = graph.AddNodeView(-1, { 5.f, 5.f });
	CHECK_FALSE(graph.TryReattach(freeNode, { 5.f, 5.f }, 5.f));
	graph.DetachIfAnchored(freeNode);
	CHECK_FALSE(graph.FindNodeView(freeNode)->attached);
	CheckConsistent(graph);
}

TEST_CASE("WireGraph: spostare un NodeView aggiorna gli estremi di tutti i fili che lo toccano", "[wiregraph]")
{
	WireGraph graph;
	Wire(graph, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	const int nvA = graph.NodeViewIdOfTerminal(1, 0);
	const int nvB = graph.NodeViewIdOfTerminal(2, 0);
	const LinkView &bus = graph.linkViews.back();

	graph.SetNodeViewPosition(nvA, { 7.f, 8.f });
	CHECK(bus.startPos == Vec2(7.f, 8.f));
	CHECK(graph.TapOfTerminal(1, 0)->targetPos == Vec2(7.f, 8.f));

	graph.SetNodeViewPosition(nvB, { 70.f, 80.f });
	CHECK(bus.targetPos == Vec2(70.f, 80.f));

	graph.SetNodeViewPosition(9999, { 1.f, 1.f }); // id sconosciuto: nessun effetto
	CheckConsistent(graph);
}

TEST_CASE("WireGraph: cancellare un componente ricollega i vicini e libera i terminali isolati", "[wiregraph]")
{
	// Tre terminali sullo stesso nodo, in catena: A - B - C
	WireGraph graph;
	Wire(graph, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	Wire(graph, { 2, 0 }, { 100.f, 0.f }, { 3, 0 }, { 200.f, 0.f });

	const auto freed = graph.RemoveComponent(2);
	CHECK(freed.empty()); // A e C restano collegati tra loro
	const int nvA = graph.NodeViewIdOfTerminal(1, 0);
	const int nvC = graph.NodeViewIdOfTerminal(3, 0);
	CHECK(graph.NodeViewIdOfTerminal(2, 0) == -1);
	CHECK(graph.SameGroup(nvA, nvC));
	CheckConsistent(graph);

	// Una coppia sola: cancellato un componente l'altro terminale resta senza fili e si libera
	WireGraph pair;
	Wire(pair, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	const auto freedPair = pair.RemoveComponent(2);
	CHECK(freedPair == std::vector<TerminalRef>{ { 1, 0 } });
	CHECK(pair.nodeViews.empty());
	CHECK(pair.linkViews.empty());
}

TEST_CASE("WireGraph: cancellare un componente sul mezzo di un filo con un angolo", "[wiregraph]")
{
	// A --- nodo libero --- B: se sparisce B il moncone si ritira e A resta libero
	WireGraph graph;
	const int edge = Wire(graph, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	graph.InsertNodeOnBusEdge(edge, { 50.f, 0.f });

	const auto freed = graph.RemoveComponent(2);
	CHECK(freed == std::vector<TerminalRef>{ { 1, 0 } });
	CHECK(graph.nodeViews.empty());
	CHECK(graph.linkViews.empty());
}

TEST_CASE("WireGraph: cancellare un nodo libero ricongiunge i suoi vicini", "[wiregraph]")
{
	// Un nodo di derivazione con tre fili: togliendolo i tre terminali restano nello stesso gruppo
	WireGraph graph;
	const int hub = graph.AddNodeView(-1, { 50.f, 50.f });
	const int nvA = graph.EnsureTerminalNodeView(1, 0, { 0.f, 0.f });
	const int nvB = graph.EnsureTerminalNodeView(2, 0, { 100.f, 0.f });
	const int nvC = graph.EnsureTerminalNodeView(3, 0, { 50.f, 100.f });
	graph.AddBusEdge(nvA, hub);
	graph.AddBusEdge(nvB, hub);
	graph.AddBusEdge(nvC, hub);

	const auto freed = graph.RemoveFreeNodeView(hub);
	CHECK(freed.empty());
	CHECK(graph.FindNodeView(hub) == nullptr);
	CHECK(graph.SameGroup(nvA, nvB));
	CHECK(graph.SameGroup(nvA, nvC));
	CheckConsistent(graph);

	// Un NodeView ancorato non si cancella cosi'
	CHECK(graph.RemoveFreeNodeView(nvA).empty());
	CHECK(graph.FindNodeView(nvA) != nullptr);
}

TEST_CASE("WireGraph: cancellare l'angolo di un filo lo ricongiunge", "[wiregraph]")
{
	WireGraph graph;
	const int edge = Wire(graph, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	const int mid = graph.InsertNodeOnBusEdge(edge, { 50.f, 0.f });
	const int nvA = graph.NodeViewIdOfTerminal(1, 0);
	const int nvB = graph.NodeViewIdOfTerminal(2, 0);

	CHECK(graph.RemoveFreeNodeView(mid).empty());
	CHECK(graph.SameGroup(nvA, nvB));
	CHECK(graph.BusDegree(nvA) == 1);
	CheckConsistent(graph);
}

TEST_CASE("WireGraph: Clear azzera anche i contatori degli id", "[wiregraph]")
{
	WireGraph graph;
	Wire(graph, { 1, 0 }, { 0.f, 0.f }, { 2, 0 }, { 100.f, 0.f });
	graph.Clear();
	CHECK(graph.nodeViews.empty());
	CHECK(graph.linkViews.empty());
	CHECK(graph.AddNodeView(-1, { 0.f, 0.f }) == 1);
}
