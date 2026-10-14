#include <cmath>
#include <map>

#include "catch_amalgamated.hpp"
#include "TestSupport.h"

using Catch::Approx;
using namespace test;

TEST_CASE("Circuito: partitore resistivo", "[circuit][linear]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(10.0));
	const int top = circuit.AddComponent(std::make_unique<Resistor>(3000.0));
	const int bottom = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { top, 0 } }, { { top, 1 }, { bottom, 0 } },
		{ { bottom, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-3);
	REQUIRE(simulation.Step());
	CHECK(simulation.Voltage(bottom, 0) == Approx(2.5).epsilon(1e-9));
}

TEST_CASE("Circuito: la carica di un condensatore segue l'esponenziale", "[circuit][linear]")
{
	// tau = R*C = 1 ms; il metodo di Eulero all'indietro con h = tau/200 sbaglia meno dell'1%
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int capacitor = circuit.AddComponent(std::make_unique<Capacitor>(1e-6));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { resistor, 1 }, { capacitor, 0 } },
		{ { capacitor, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	const double tau = 1e-3, h = tau / 200.0;
	Simulation simulation(circuit, h);
	for (int i = 1; i <= 1000; i++) // 5 tau
	{
		REQUIRE(simulation.Step());
		if (i % 200 == 0)
		{
			const double t = i * h;
			CHECK(simulation.Voltage(capacitor, 0) == Approx(5.0 * (1.0 - std::exp(-t / tau))).epsilon(0.01));
		}
	}
}

TEST_CASE("Circuito: la risposta di un RL segue l'esponenziale", "[circuit][linear]")
{
	// tau = L/R = 1 ms: la tensione sull'induttore cala come exp(-t/tau)
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int inductor = circuit.AddComponent(std::make_unique<Inductor>(1.0));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { resistor, 1 }, { inductor, 0 } },
		{ { inductor, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	const double tau = 1e-3, h = tau / 200.0;
	Simulation simulation(circuit, h);
	for (int i = 1; i <= 600; i++)
	{
		REQUIRE(simulation.Step());
		if (i % 200 == 0)
			CHECK(simulation.Voltage(inductor, 0) == Approx(5.0 * std::exp(-(i * h) / tau)).epsilon(0.02).margin(1e-3));
	}
}

TEST_CASE("Circuito: cancellare un componente mantiene uniti, nei link salvati, i terminali che lo erano solo tramite lui", "[circuit][links]")
{
	// A.1 - X.0, X.0 - C.1 e X.0 - D.0: tutti sullo stesso nodo tramite X. Tolto X, nel circuito
	// vivo restano sullo stesso nodo, ma salvare e ricaricare dai soli link li scollegava
	Circuit circuit;
	const int a = circuit.AddComponent(std::make_unique<Resistor>(100.0));
	const int x = circuit.AddComponent(std::make_unique<Resistor>(100.0));
	const int c = circuit.AddComponent(std::make_unique<Resistor>(100.0));
	const int d = circuit.AddComponent(std::make_unique<Resistor>(100.0));
	Connect(circuit, { { { a, 1 }, { x, 0 } }, { { x, 0 }, { c, 1 } }, { { x, 0 }, { d, 0 } }, { { a, 0 }, { c, 0 } } });
	circuit.RemoveComponent(x);

	REQUIRE(NodeOf(circuit, a, 1) > 0);
	CHECK(NodeOf(circuit, a, 1) == NodeOf(circuit, c, 1));
	CHECK(NodeOf(circuit, c, 1) == NodeOf(circuit, d, 0));

	// "Salva e ricarica": ricostruisce un circuito nuovo solo dalla lista dei link
	Circuit reloaded;
	std::map<int, int> ids;
	for (const auto &component : circuit.GetComponentsVector())
		ids[component->GetId()] = reloaded.AddComponent(std::make_unique<Resistor>(100.0));
	for (const Link &link : circuit.GetLinksVector())
		reloaded.ConnectTerminals(ids.at(link.compId1), link.termIndex1, ids.at(link.compId2), link.termIndex2);

	REQUIRE(NodeOf(reloaded, ids.at(a), 1) > 0);
	CHECK(NodeOf(reloaded, ids.at(a), 1) == NodeOf(reloaded, ids.at(c), 1));
	CHECK(NodeOf(reloaded, ids.at(c), 1) == NodeOf(reloaded, ids.at(d), 0));
}

TEST_CASE("Circuito: terminali scollegati e rami aperti vengono rilevati", "[circuit]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { source, 1 }, { ground, 0 } } });

	CHECK(circuit.HasFloatingTerminal()); // resistor.1 non è collegato a nulla

	Connect(circuit, { { { resistor, 1 }, { ground, 0 } } });
	CHECK_FALSE(circuit.HasFloatingTerminal());
}
