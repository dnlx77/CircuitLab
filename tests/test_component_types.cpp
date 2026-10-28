#include <set>
#include <string>

#include "catch_amalgamated.hpp"
#include "Common/ComponentType.h"

using namespace CircuitLab;

TEST_CASE("Tabella dei tipi: ogni tipo ha una sola riga con nomi e tasti distinti", "[types]")
{
	std::set<ComponentType> types;
	std::set<std::string> jsonNames;
	std::set<char> shortcuts;
	for (const ComponentTypeInfo &info : COMPONENT_TYPES)
	{
		CHECK(types.insert(info.type).second);
		CHECK(jsonNames.insert(info.jsonName).second);
		CHECK(shortcuts.insert(info.shortcut).second);
		CHECK(info.shortcut >= 'A');
		CHECK(info.shortcut <= 'Z');
		CHECK(std::string(info.prefix).size() == 1);
	}

	// Tutti i tipi tranne "node" (che non e' un componente) sono nella tabella
	CHECK(types.size() == static_cast<size_t>(ComponentType::transistorPnp) - static_cast<size_t>(ComponentType::node));
	CHECK(FindComponentType(ComponentType::node) == nullptr);
}

TEST_CASE("Tabella dei tipi: i nomi nel file non cambiano", "[types]")
{
	// Sono il formato dei file salvati: cambiarli renderebbe illeggibili i circuiti esistenti
	CHECK(nlohmann::json(ComponentType::resistor) == "Resistor");
	CHECK(nlohmann::json(ComponentType::voltageGenerator) == "VoltageGenerator");
	CHECK(nlohmann::json(ComponentType::ground) == "Ground");
	CHECK(nlohmann::json(ComponentType::capacitor) == "Capacitor");
	CHECK(nlohmann::json(ComponentType::inductor) == "Inductor");
	CHECK(nlohmann::json(ComponentType::switchComponent) == "Switch");
	CHECK(nlohmann::json(ComponentType::diode) == "Diode");
	CHECK(nlohmann::json(ComponentType::transformer) == "Transformer");
	CHECK(nlohmann::json(ComponentType::changeoverSwitch) == "ChangeoverSwitch");
	CHECK(nlohmann::json(ComponentType::transistor) == "Transistor");
	CHECK(nlohmann::json(ComponentType::transistorPnp) == "TransistorPnp");
}

TEST_CASE("Tabella dei tipi: andata e ritorno nel JSON", "[types]")
{
	for (const ComponentTypeInfo &info : COMPONENT_TYPES)
	{
		const nlohmann::json j = info.type;
		CHECK(j.get<ComponentType>() == info.type);
	}
}

TEST_CASE("Tabella dei tipi: prefissi delle etichette", "[types]")
{
	CHECK(std::string(ComponentPrefix(ComponentType::resistor)) == "R");
	CHECK(std::string(ComponentPrefix(ComponentType::transistor)) == "Q");
	CHECK(std::string(ComponentPrefix(ComponentType::transistorPnp)) == "Q");
	CHECK(std::string(ComponentPrefix(ComponentType::node)) == "?");
}
