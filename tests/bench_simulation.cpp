// Misure di velocità del passo di simulazione. Nascosti (tag "[.bench]"): non girano con la suite
// normale, si lanciano a mano, meglio in Release:   CircuitLabTests "[bench]"
#include <chrono>
#include <cstdio>

#include "catch_amalgamated.hpp"
#include "TestSupport.h"

using namespace test;

namespace {

	// Esegue steps passi e stampa il costo medio per passo
	template <class Setup>
	void Measure(const char *name, int steps, double h, Setup setup)
	{
		Circuit circuit;
		setup(circuit);
		Simulation sim(circuit, h);
		sim.StartFromDcOperatingPoint();
		for (int i = 0; i < 200; i++)   // riscaldamento: la prima parte include le allocazioni
			sim.Step();

		const auto t0 = std::chrono::steady_clock::now();
		int failures = 0;
		for (int i = 0; i < steps; i++)
			failures += sim.Step() ? 0 : 1;
		const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

		std::printf("  %-28s %8.2f us/passo  (%d passi, %d falliti)\n", name, 1e6 * seconds / steps, steps, failures);
		REQUIRE(failures == 0);
	}

	// Scala RC di n celle (lineare): serve a vedere come cresce il costo con la dimensione
	void BuildRcLadder(Circuit &circuit, int cells)
	{
		const int source = circuit.AddComponent(SineSource(1.0, 100.0));
		const int ground = circuit.AddComponent(std::make_unique<Ground>());
		Connect(circuit, { { { source, 1 }, { ground, 0 } } });
		Pin previous{ source, 0 };
		for (int i = 0; i < cells; i++)
		{
			const int r = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
			const int c = circuit.AddComponent(std::make_unique<Capacitor>(1e-6));
			const int g = circuit.AddComponent(std::make_unique<Ground>());
			Connect(circuit, { { previous, { r, 0 } }, { { r, 1 }, { c, 0 } }, { { c, 1 }, { g, 0 } } });
			previous = { r, 1 };
		}
	}

	// Come sopra ma con un diodo in coda a ogni cella: non lineare e più grande
	void BuildDiodeLadder(Circuit &circuit, int cells)
	{
		const int source = circuit.AddComponent(SineSource(5.0, 100.0));
		const int ground = circuit.AddComponent(std::make_unique<Ground>());
		Connect(circuit, { { { source, 1 }, { ground, 0 } } });
		Pin previous{ source, 0 };
		for (int i = 0; i < cells; i++)
		{
			const int d = circuit.AddComponent(std::make_unique<Diode>());
			const int c = circuit.AddComponent(std::make_unique<Capacitor>(1e-6));
			const int r = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
			const int g = circuit.AddComponent(std::make_unique<Ground>());
			Connect(circuit, { { previous, { d, 0 } }, { { d, 1 }, { c, 0 } }, { { d, 1 }, { r, 0 } }, { { c, 1 }, { g, 0 } }, { { r, 1 }, { g, 0 } } });
			previous = { d, 1 };
		}
	}
}

TEST_CASE("Velocita' del passo di simulazione", "[.bench][bench]")
{
	std::printf("\n");
	Measure("RC lineare 3 celle", 20000, 1e-4, [](Circuit &c) { BuildRcLadder(c, 3); });
	Measure("RC lineare 20 celle", 5000, 1e-4, [](Circuit &c) { BuildRcLadder(c, 20); });
	Measure("amplificatore (1 BJT)", 5000, 1e-5, [](Circuit &c) { BuildCommonEmitterAmplifier(c, 20.0, 0.1); });
	Measure("Schmitt (2 BJT)", 5000, 1e-5, [](Circuit &c) { BuildSchmittTrigger(c, 0.5, 1.0); });
	Measure("astabile (2 BJT, 2 C)", 5000, 1e-4, [](Circuit &c) { BuildAstableMultivibrator(c, 12000.0); });
	Measure("scala di 10 diodi", 2000, 1e-4, [](Circuit &c) { BuildDiodeLadder(c, 10); });
}
