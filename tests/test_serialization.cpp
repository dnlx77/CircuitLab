// Salvataggio e caricamento dei componenti in JSON (Component::Save / Load): e' il formato dei file
// e degli snapshot di annulla/ripeti, quindi un valore perso qui si perde in silenzio.
#include <map>
#include <memory>

#include "catch_amalgamated.hpp"
#include "TestSupport.h"
#include "Components/ChangeoverSwitch.h"
#include "Components/DCWaveForm.h"
#include "Components/SineWaveForm.h"
#include "Components/SquareWaveForm.h"
#include "Components/Switch.h"

using Catch::Approx;
using namespace test;

namespace {

	// Valori diversi da quelli di default, uno per uno: se il caricamento ne ignora uno si vede
	void ChangeEveryValue(Component &component)
	{
		auto values = component.GetValues();
		double factor = 2.0;
		for (auto &[key, value] : values)
		{
			value = value * factor + 0.25;
			factor += 0.5;
		}
		component.SetValues(values);
	}

	void CheckRoundTrip(Component &original, Component &fresh)
	{
		nlohmann::json saved;
		original.Save(saved);
		REQUIRE(saved.contains("id"));
		REQUIRE(saved.contains("type"));
		CHECK(saved["type"].get<ComponentType>() == original.GetType());

		// Prima del caricamento le due istanze devono essere diverse, altrimenti il confronto non prova nulla
		if (!original.GetValues().empty())
			REQUIRE(fresh.GetValues() != original.GetValues());

		fresh.Load(saved);

		const auto expected = original.GetValues();
		const auto loaded = fresh.GetValues();
		REQUIRE(loaded.size() == expected.size());
		for (const auto &[key, value] : expected)
		{
			REQUIRE(loaded.count(key) == 1);
			CHECK(loaded.at(key) == Approx(value).epsilon(1e-12));
		}
		CHECK(fresh.GetWaveFormType() == original.GetWaveFormType());
		CHECK(fresh.IsSwitchClosed() == original.IsSwitchClosed());

		// Salvando di nuovo si ottiene lo stesso contenuto (a parte l'id, assegnato a ogni istanza)
		nlohmann::json again;
		fresh.Save(again);
		saved.erase("id");
		again.erase("id");
		CHECK(again == saved);
	}
}

TEST_CASE("Salvataggio: i valori dei componenti sopravvivono a Save e Load", "[serialization]")
{
	SECTION("resistore")
	{
		Resistor a(1000.0), b(1.0);
		ChangeEveryValue(a);
		CheckRoundTrip(a, b);
	}
	SECTION("condensatore")
	{
		Capacitor a(1e-6), b(1.0);
		ChangeEveryValue(a);
		CheckRoundTrip(a, b);
	}
	SECTION("induttore")
	{
		Inductor a(1e-3), b(1.0);
		ChangeEveryValue(a);
		CheckRoundTrip(a, b);
	}
	SECTION("diodo")
	{
		Diode a, b;
		ChangeEveryValue(a);
		CheckRoundTrip(a, b);
	}
	SECTION("trasformatore")
	{
		Transformer a, b;
		ChangeEveryValue(a);
		CheckRoundTrip(a, b);
	}
	SECTION("transistor NPN")
	{
		Transistor a(1e-14, 100.0), b(1e-12, 50.0);
		ChangeEveryValue(a);
		CheckRoundTrip(a, b);
	}
	SECTION("transistor PNP")
	{
		Transistor a(1e-14, 100.0, true), b(1e-14, 100.0, true);
		ChangeEveryValue(a);
		CheckRoundTrip(a, b);
	}
	SECTION("massa")
	{
		Ground a, b;
		CheckRoundTrip(a, b);
	}
}

TEST_CASE("Salvataggio: lo stato aperto o chiuso degli interruttori si conserva", "[serialization]")
{
	for (bool closed : { true, false })
	{
		Switch a(closed), b(!closed);
		CheckRoundTrip(a, b);
		CHECK(b.IsSwitchClosed() == closed);

		ChangeoverSwitch c(closed), d(!closed);
		CheckRoundTrip(c, d);
		CHECK(d.IsSwitchClosed() == closed);
	}
}

TEST_CASE("Salvataggio: il generatore ricorda tipo di forma d'onda e parametri", "[serialization]")
{
	SECTION("continua")
	{
		VoltageGenerator a(std::make_unique<DCWaveForm>(7.5));
		VoltageGenerator b(std::make_unique<SineWaveForm>(1.0, 50.0, 0.0));
		CheckRoundTrip(a, b);
		CHECK(b.GetWaveFormType() == WaveFormType::dcWaveForm);
		CHECK(b.GetValues().at(ComponentValue::voltage) == Approx(7.5));
	}
	SECTION("sinusoidale")
	{
		VoltageGenerator a(std::make_unique<SineWaveForm>(3.3, 440.0, 0.7));
		VoltageGenerator b(std::make_unique<DCWaveForm>(0.0));
		CheckRoundTrip(a, b);
		CHECK(b.GetWaveFormType() == WaveFormType::sineWaveForm);
		const auto values = b.GetValues();
		CHECK(values.at(ComponentValue::amplitude) == Approx(3.3));
		CHECK(values.at(ComponentValue::frequency) == Approx(440.0));
		CHECK(values.at(ComponentValue::phase) == Approx(0.7));
	}
	SECTION("onda quadra")
	{
		VoltageGenerator a(std::make_unique<SquareWaveForm>(5.0, 1000.0));
		VoltageGenerator b(std::make_unique<DCWaveForm>(0.0));
		CheckRoundTrip(a, b);
		CHECK(b.GetWaveFormType() == WaveFormType::squareWaveForm);
		const auto values = b.GetValues();
		CHECK(values.at(ComponentValue::amplitude) == Approx(5.0));
		CHECK(values.at(ComponentValue::frequency) == Approx(1000.0));
	}
}

TEST_CASE("Salvataggio: ogni tipo di componente ha un nome nel file e lo ritrova", "[serialization]")
{
	const ComponentType types[] = {
		ComponentType::resistor, ComponentType::voltageGenerator, ComponentType::ground,
		ComponentType::capacitor, ComponentType::inductor, ComponentType::switchComponent,
		ComponentType::diode, ComponentType::transformer, ComponentType::changeoverSwitch,
		ComponentType::transistor, ComponentType::transistorPnp,
	};
	std::map<std::string, ComponentType> names;
	for (ComponentType type : types)
	{
		const nlohmann::json j = type;
		REQUIRE(j.is_string());
		CHECK(j.get<ComponentType>() == type);
		CHECK(names.emplace(j.get<std::string>(), type).second); // nomi tutti diversi
	}
}

TEST_CASE("Salvataggio: la simulazione di un circuito ricaricato da JSON da lo stesso risultato", "[serialization]")
{
	// Partitore con un generatore sinusoidale: si salvano i valori di ogni componente e si
	// ricostruisce il circuito da zero con quei valori
	Circuit original;
	const int source = original.AddComponent(SineSource(4.0, 100.0));
	const int top = original.AddComponent(std::make_unique<Resistor>(2200.0));
	const int bottom = original.AddComponent(std::make_unique<Resistor>(4700.0));
	const int ground = original.AddComponent(std::make_unique<Ground>());
	Connect(original, { { { source, 0 }, { top, 0 } }, { { top, 1 }, { bottom, 0 } },
		{ { bottom, 1 }, { ground, 0 } }, { { source, 1 }, { ground, 0 } } });

	Circuit loaded;
	const int source2 = loaded.AddComponent(DcSource(0.0));
	const int top2 = loaded.AddComponent(std::make_unique<Resistor>(1.0));
	const int bottom2 = loaded.AddComponent(std::make_unique<Resistor>(1.0));
	const int ground2 = loaded.AddComponent(std::make_unique<Ground>());
	Connect(loaded, { { { source2, 0 }, { top2, 0 } }, { { top2, 1 }, { bottom2, 0 } },
		{ { bottom2, 1 }, { ground2, 0 } }, { { source2, 1 }, { ground2, 0 } } });

	const std::pair<int, int> ids[] = { { source, source2 }, { top, top2 }, { bottom, bottom2 } };
	for (const auto &[from, to] : ids)
	{
		nlohmann::json j;
		original.GetComponentById(from)->Save(j);
		loaded.GetComponentById(to)->Load(j);
	}
	loaded.InvalidateCircuit();

	Simulation a(original, 1e-4), b(loaded, 1e-4);
	for (int i = 0; i < 50; i++)
	{
		REQUIRE(a.Step());
		REQUIRE(b.Step());
		CHECK(b.Voltage(bottom2, 0) == Approx(a.Voltage(bottom, 0)).epsilon(1e-12));
	}
}
