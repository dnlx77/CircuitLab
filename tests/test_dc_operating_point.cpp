#include <algorithm>
#include <cmath>

#include "catch_amalgamated.hpp"
#include "TestSupport.h"

using Catch::Approx;
using namespace test;

TEST_CASE("Punto DC: RC, il condensatore parte carico e il transitorio resta fermo", "[dc]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int capacitor = circuit.AddComponent(std::make_unique<Capacitor>(1e-6));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { resistor, 1 }, { capacitor, 0 } },
		{ { capacitor, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-4);
	REQUIRE(simulation.StartFromDcOperatingPoint());

	const auto *cap = static_cast<Capacitor *>(circuit.GetComponentById(capacitor));
	CHECK(cap->GetPreviousVoltage() == Approx(5.0).epsilon(1e-6));

	double largestDeviation = 0.0;
	for (int i = 0; i < 200; i++)
	{
		REQUIRE(simulation.Step());
		largestDeviation = std::max(largestDeviation, std::abs(simulation.Voltage(capacitor, 0) - 5.0));
	}
	CHECK(largestDeviation < 1e-6);
}

TEST_CASE("Punto DC: RL, la corrente dell'induttore è V/R", "[dc]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int inductor = circuit.AddComponent(std::make_unique<Inductor>(1e-3));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { resistor, 1 }, { inductor, 0 } },
		{ { inductor, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-4);
	REQUIRE(simulation.StartFromDcOperatingPoint());

	const auto *ind = static_cast<Inductor *>(circuit.GetComponentById(inductor));
	CHECK(ind->GetPreviousCurrent() == Approx(5e-3).epsilon(1e-6));
}

TEST_CASE("Punto DC: i due avvolgimenti di un trasformatore sono cortocircuiti indipendenti", "[dc]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int primaryResistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int transformer = circuit.AddComponent(std::make_unique<Transformer>(1e-3, 1e-3, 0.9));
	const int load = circuit.AddComponent(std::make_unique<Resistor>(100.0));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, {
		{ { source, 0 }, { primaryResistor, 0 } }, { { primaryResistor, 1 }, { transformer, 0 } },
		{ { transformer, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } },
		{ { transformer, 2 }, { load, 0 } }, { { transformer, 3 }, { ground, 0 } }, { { load, 1 }, { ground, 0 } },
	});

	Simulation simulation(circuit, 1e-4);
	REQUIRE(simulation.StartFromDcOperatingPoint());

	// La corrente primaria da DC (5 mA) deve restare tale nel transitorio: nessun assestamento
	auto *transformerPtr = static_cast<Transformer *>(circuit.GetComponentById(transformer));
	double largestDeviation = 0.0;
	for (int i = 0; i < 100; i++)
	{
		REQUIRE(simulation.Step());
		const double primaryVoltage = simulation.Voltage(transformer, 0) - simulation.Voltage(transformer, 1);
		const double secondaryVoltage = simulation.Voltage(transformer, 2) - simulation.Voltage(transformer, 3);
		const auto currents = transformerPtr->BranchCurrents(primaryVoltage, secondaryVoltage);
		largestDeviation = std::max(largestDeviation, std::abs(currents.first - 5e-3));
	}
	CHECK(largestDeviation < 1e-4);
}

TEST_CASE("Punto DC: diodo in conduzione", "[dc]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int diode = circuit.AddComponent(std::make_unique<Diode>());
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { resistor, 1 }, { diode, 0 } },
		{ { diode, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-4);
	REQUIRE(simulation.StartFromDcOperatingPoint());

	const double diodeVoltage = simulation.Voltage(diode, 0);
	CHECK(diodeVoltage > 0.6);
	CHECK(diodeVoltage < 0.8);
}

TEST_CASE("Punto DC: un nodo collegato solo a condensatori non rende il sistema singolare", "[dc]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int first = circuit.AddComponent(std::make_unique<Capacitor>(1e-6));
	const int second = circuit.AddComponent(std::make_unique<Capacitor>(1e-6));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { first, 0 } }, { { first, 1 }, { second, 0 } },
		{ { second, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-4);
	CHECK(simulation.StartFromDcOperatingPoint());
}

TEST_CASE("Punto DC: lo stato dell'amplificatore coincide col regime di un transitorio lungo", "[dc][amplifier]")
{
	// Riferimento: transitorio con passo grande per 60 s, molto più dei 5 s di C7*R8
	Circuit referenceCircuit;
	const CommonEmitterAmplifier reference = BuildCommonEmitterAmplifier(referenceCircuit, 20.0, 0.01);
	Simulation longRun(referenceCircuit, 1e-3);
	for (int i = 0; i < 60000; i++)
		REQUIRE(longRun.Step());

	Circuit circuit;
	const CommonEmitterAmplifier amplifier = BuildCommonEmitterAmplifier(circuit, 20.0, 0.01);
	Simulation simulation(circuit, 1e-3);
	REQUIRE(simulation.StartFromDcOperatingPoint());

	const auto *c6 = static_cast<Capacitor *>(circuit.GetComponentById(amplifier.c6));
	const auto *c7 = static_cast<Capacitor *>(circuit.GetComponentById(amplifier.c7));
	const auto *referenceC6 = static_cast<Capacitor *>(referenceCircuit.GetComponentById(reference.c6));
	const auto *referenceC7 = static_cast<Capacitor *>(referenceCircuit.GetComponentById(reference.c7));
	CHECK(c6->GetPreviousVoltage() == Approx(referenceC6->GetPreviousVoltage()).margin(0.01));
	CHECK(c7->GetPreviousVoltage() == Approx(referenceC7->GetPreviousVoltage()).margin(0.01));

	// Partendo dal DC non c'è assestamento: il collettore si muove solo per il piccolo segnale
	const double dcCollector = simulation.Voltage(amplifier.q1, 1);
	double lowest = dcCollector, highest = dcCollector;
	for (int i = 0; i < 2000; i++) // 2 s
	{
		REQUIRE(simulation.Step());
		const double collector = simulation.Voltage(amplifier.q1, 1);
		lowest = std::min(lowest, collector);
		highest = std::max(highest, collector);
	}
	CHECK(highest - lowest < 0.2);
}

TEST_CASE("Punto DC: un oscillatore resta sull'equilibrio, dallo stato scarico invece oscilla", "[dc][astable]")
{
	// Documenta perché "Parti dal punto di lavoro DC" è spento di default: il multivibratore
	// partendo dal regime non si avvia
	auto transitions = [](bool fromDc) {
		Circuit circuit;
		const AstableMultivibrator multivibrator = BuildAstableMultivibrator(circuit, 1000.0);
		Simulation simulation(circuit, 1e-4);
		if (fromDc)
			REQUIRE(simulation.StartFromDcOperatingPoint());

		int count = 0;
		bool havePrevious = false, previousHigh = false;
		for (int i = 0; i < 20000; i++)
		{
			REQUIRE(simulation.Step());
			const bool high = simulation.Voltage(multivibrator.q1, 1) > 2.5;
			if (havePrevious && high != previousHigh)
				count++;
			previousHigh = high;
			havePrevious = true;
		}
		return count;
	};

	CHECK(transitions(false) > 100);
	CHECK(transitions(true) == 0);
}
