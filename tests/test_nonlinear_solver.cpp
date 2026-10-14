#include <algorithm>
#include <cmath>

#include "catch_amalgamated.hpp"
#include "TestSupport.h"

using Catch::Approx;
using namespace test;

namespace {
	// Esegue steps passi e restituisce quanti non convergono. Come l'applicazione, un passo che
	// non converge non fa avanzare il tempo: il simulatore si bloccherebbe, ed è proprio ciò che
	// questi test devono escludere.
	int CountFailures(Simulation &simulation, int steps)
	{
		int failures = 0;
		for (int i = 0; i < steps; i++)
			if (!simulation.Step())
				failures++;
		return failures;
	}
}

TEST_CASE("Trigger di Schmitt: soglie di commutazione con isteresi", "[solver][schmitt]")
{
	// Solo il segnale lento (40 Hz, 2 V): le soglie sono statiche (nessun condensatore)
	Circuit circuit;
	const SchmittTrigger schmitt = BuildSchmittTrigger(circuit, 0.0, 2.0);
	Simulation simulation(circuit, 1e-5);

	double upSum = 0.0, downSum = 0.0;
	int upCount = 0, downCount = 0, failures = 0;
	bool havePrevious = false, previousHigh = false;
	for (int i = 0; i < 40000; i++) // 0.4 s = 16 periodi
	{
		if (!simulation.Step())
		{
			failures++;
			continue;
		}
		if (simulation.time() < 0.02) // salta l'assestamento iniziale
			continue;

		const bool high = simulation.Voltage(schmitt.q6, 1) > 3.0;
		const double input = simulation.Voltage(schmitt.q3, 0);
		if (havePrevious && high != previousHigh)
		{
			// Q6 si spegne quando l'ingresso supera la soglia superiore (Q3 conduce, il collettore
			// di Q6 sale); si riaccende quando l'ingresso scende sotto quella inferiore
			if (high) { upSum += input; upCount++; }      // uscita sale: ingresso sulla soglia superiore
			else      { downSum += input; downCount++; }  // uscita scende: ingresso sulla soglia inferiore
		}
		previousHigh = high;
		havePrevious = true;
	}

	REQUIRE(failures == 0);
	REQUIRE(upCount >= 10);
	REQUIRE(downCount >= 10);
	const double upperThreshold = upSum / upCount, lowerThreshold = downSum / downCount;
	CHECK(upperThreshold == Approx(1.496).margin(0.02));
	CHECK(lowerThreshold == Approx(1.097).margin(0.02));
	CHECK(upperThreshold - lowerThreshold > 0.3); // c'è isteresi
}

TEST_CASE("Trigger di Schmitt: ogni passo converge a timestep diversi", "[solver][schmitt]")
{
	// Regressione: nei passaggi di stato Newton non convergeva e il tempo simulato si fermava
	for (const double h : { 1e-4, 1e-5, 1e-6 })
	{
		DYNAMIC_SECTION("h = " << h)
		{
			Circuit circuit;
			BuildSchmittTrigger(circuit, 0.2, 2.0); // anche col rumore da 2 kHz sovrapposto
			Simulation simulation(circuit, h);
			const int steps = static_cast<int>(0.1 / h); // 100 ms: quattro periodi del segnale lento
			CHECK(CountFailures(simulation, steps) == 0);
		}
	}
}

TEST_CASE("Trigger di Schmitt: il rumore dentro la banda di isteresi non causa commutazioni in più", "[solver][schmitt]")
{
	auto transitions = [](double noiseAmplitude) {
		Circuit circuit;
		const SchmittTrigger schmitt = BuildSchmittTrigger(circuit, noiseAmplitude, 2.0);
		Simulation simulation(circuit, 1e-5);
		int count = 0;
		bool havePrevious = false, previousHigh = false;
		for (int i = 0; i < 40000; i++)
		{
			REQUIRE(simulation.Step());
			if (simulation.time() < 0.02)
				continue;
			const bool high = simulation.Voltage(schmitt.q6, 1) > 3.0;
			if (havePrevious && high != previousHigh)
				count++;
			previousHigh = high;
			havePrevious = true;
		}
		return count;
	};

	const int clean = transitions(0.0);
	CHECK(clean >= 28);
	CHECK(clean <= 32);                    // due per periodo del segnale lento
	CHECK(transitions(0.1) == clean);      // rumore da 0.1 V (0.2 V picco-picco) < banda di 0.4 V
	CHECK(transitions(0.6) > clean);       // rumore molto più grande della banda: commuta a raffica
}

TEST_CASE("Amplificatore: portare l'alimentazione a 0 V non blocca Newton (cicli limite)", "[solver][amplifier]")
{
	// Regressione: nella zona di transizione accensione/interdizione del transistor Newton restava in
	// un ciclo limite e il simulatore si fermava
	Circuit circuit;
	const CommonEmitterAmplifier amplifier = BuildCommonEmitterAmplifier(circuit, 20.0, 0.5, 110000.0); // R2 = 110 k: la polarizzazione del caso che bloccava Newton
	Simulation simulation(circuit, 1e-5);
	REQUIRE(CountFailures(simulation, 2000) == 0); // a regime a 20 V

	static_cast<VoltageGenerator *>(circuit.GetComponentById(amplifier.supply))->SetValues({ { ComponentValue::voltage, 0.0 } });
	circuit.InvalidateCircuit();
	CHECK(CountFailures(simulation, 30000) == 0);
}

TEST_CASE("Multivibratore astabile: oscilla senza passi non convergenti", "[solver][astable]")
{
	// Regressione: con lo stamp sbagliato in saturazione Newton non convergeva e il circuito
	// restava bloccato in uno stato falso
	Circuit circuit;
	const AstableMultivibrator multivibrator = BuildAstableMultivibrator(circuit, 1000.0);
	Simulation simulation(circuit, 1e-4);

	int failures = 0, transitions = 0;
	bool havePrevious = false, previousHigh = false;
	for (int i = 0; i < 10000; i++) // 1 s
	{
		if (!simulation.Step())
		{
			failures++;
			continue;
		}
		const bool high = simulation.Voltage(multivibrator.q1, 1) > 2.5;
		if (havePrevious && high != previousHigh)
			transitions++;
		previousHigh = high;
		havePrevious = true;
	}

	CHECK(failures == 0);
	// Periodo ~ 1.4 * R * C = 25 ms: circa 80 commutazioni in 1 s
	CHECK(transitions >= 60);
	CHECK(transitions <= 100);
}

TEST_CASE("Solver: una matrice singolare viene segnalata, non risolta", "[solver]")
{
	// Due generatori ideali in parallelo con tensioni diverse: sistema senza soluzione
	Circuit circuit;
	const int a = circuit.AddComponent(DcSource(5.0));
	const int b = circuit.AddComponent(DcSource(3.0));
	const int resistor = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, {
		{ { a, 0 }, { b, 0 } }, { { a, 0 }, { resistor, 0 } },
		{ { resistor, 1 }, { ground, 0 } }, { { a, 1 }, { ground, 0 } }, { { b, 1 }, { ground, 0 } },
	});

	Simulation simulation(circuit, 1e-4);
	CHECK_FALSE(simulation.Step());
}
