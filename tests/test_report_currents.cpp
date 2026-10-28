#include <cmath>

#include "catch_amalgamated.hpp"
#include "TestSupport.h"
#include "Components/ChangeoverSwitch.h"
#include "Components/Switch.h"

using Catch::Approx;
using namespace test;

TEST_CASE("Correnti: resistore e generatore in serie danno V/R sul ramo 0 -> 1", "[currents]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(10.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(2000.0));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { resistor, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-4);
	CurrentReport report;
	REQUIRE(simulation.StepAndReport(resistor, report));
	CHECK(report.main == Approx(5e-3).epsilon(1e-9));
	REQUIRE(report.branchCount == 1);
	CHECK(report.branches[0].from == 0);
	CHECK(report.branches[0].to == 1);
	CHECK(report.branches[0].current == report.main);
}

TEST_CASE("Correnti: in un anello RC condensatore e resistore portano la stessa corrente", "[currents]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int capacitor = circuit.AddComponent(std::make_unique<Capacitor>(1e-6));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { resistor, 1 }, { capacitor, 0 } },
		{ { capacitor, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-5);
	for (int i = 0; i < 50; i++)
	{
		CurrentReport capacitorReport;
		REQUIRE(simulation.StepAndReport(capacitor, capacitorReport));
		// La corrente di resistore alla stessa soluzione: (Vs - Vc) / R
		const double resistorCurrent = (5.0 - simulation.Voltage(capacitor, 0)) / 1000.0;
		CHECK(capacitorReport.main == Approx(resistorCurrent).margin(1e-9));
	}
}

TEST_CASE("Correnti: l'induttore in serie a un resistore tende a V/R", "[currents]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int inductor = circuit.AddComponent(std::make_unique<Inductor>(1e-3));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { resistor, 1 }, { inductor, 0 } },
		{ { inductor, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-7);
	CurrentReport report;
	for (int i = 0; i < 100; i++)
		REQUIRE(simulation.StepAndReport(inductor, report));
	CHECK(report.main == Approx(5e-3).epsilon(1e-3));
}

TEST_CASE("Correnti: il diodo in diretta porta la corrente del resistore in serie", "[currents]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int diode = circuit.AddComponent(std::make_unique<Diode>());
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { resistor, 0 } }, { { resistor, 1 }, { diode, 0 } },
		{ { diode, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-4);
	CurrentReport report;
	REQUIRE(simulation.StepAndReport(diode, report));
	const double resistorCurrent = (5.0 - simulation.Voltage(diode, 0)) / 1000.0;
	// Newton si ferma quando le correnti concordano entro CURRENT_REL_TOL (1e-3)
	CHECK(report.main == Approx(resistorCurrent).epsilon(1e-3));
	CHECK(report.main > 3e-3);
}

TEST_CASE("Correnti: lo switch chiuso conduce, aperto no", "[currents]")
{
	Circuit circuit;
	const int source = circuit.AddComponent(DcSource(5.0));
	const int sw = circuit.AddComponent(std::make_unique<Switch>());
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, { { { source, 0 }, { sw, 0 } }, { { sw, 1 }, { resistor, 0 } },
		{ { resistor, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Simulation simulation(circuit, 1e-4);
	CurrentReport report;
	REQUIRE(simulation.StepAndReport(sw, report));
	CHECK(report.main == Approx(5e-3).epsilon(1e-6));

	circuit.GetComponentById(sw)->ToggleSwitch();
	circuit.InvalidateCircuit();
	REQUIRE(simulation.StepAndReport(sw, report));
	CHECK(std::abs(report.main) < 1e-6);
}

TEST_CASE("Correnti: il transistor riporta tre correnti indipendenti che sommano a zero", "[currents]")
{
	Circuit circuit;
	const auto amplifier = BuildCommonEmitterAmplifier(circuit, 12.0, 0.0);
	Simulation simulation(circuit, 1e-5);
	CurrentReport report;
	for (int i = 0; i < 20; i++)
		REQUIRE(simulation.StepAndReport(amplifier.q1, report));

	REQUIRE(report.branchCount == 3);
	double sum = 0.0;
	for (int i = 0; i < 3; i++)
	{
		CHECK(report.branches[i].from == i);
		CHECK(report.branches[i].to == CurrentReport::SINGLE_TERMINAL);
		sum += report.branches[i].current;
	}
	CHECK(std::abs(sum) < 1e-12);
	// main e' la corrente di collettore, ed e' quella che entra dal collettore
	CHECK(report.main == report.branches[1].current);
	CHECK(report.main > 0.0);
}

TEST_CASE("Correnti: massa e generatore non riportano nulla", "[currents]")
{
	Ground ground;
	const auto generator = DcSource(1.0);
	CurrentReport report;
	CHECK_FALSE(ground.ReportCurrents({ 0.0 }, 1e-3, report));
	CHECK_FALSE(generator->ReportCurrents({ 1.0, 0.0 }, 1e-3, report));
}
