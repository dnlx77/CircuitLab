#include <cmath>

#include "catch_amalgamated.hpp"
#include "TestSupport.h"

using Catch::Approx;
using namespace test;

namespace {

	// Circuito di prova: alimentazione vcc, rb verso la base, rc verso il collettore, emettitore a massa
	struct BiasCircuit {
		Circuit circuit;
		int transistor, supply;
		Simulation simulation;

		BiasCircuit(bool pnp, double vcc, double rb, double rc) : simulation(circuit, 1e-4)
		{
			// Per un PNP lo stesso circuito specchiato: alimentazione negativa
			const double sign = pnp ? -1.0 : 1.0;
			transistor = circuit.AddComponent(std::make_unique<Transistor>(1e-14, 100.0, pnp));
			const int baseResistor = circuit.AddComponent(std::make_unique<Resistor>(rb));
			const int collectorResistor = circuit.AddComponent(std::make_unique<Resistor>(rc));
			supply = circuit.AddComponent(DcSource(sign * vcc));
			const int ground = circuit.AddComponent(std::make_unique<Ground>());
			Connect(circuit, {
				{ { supply, 0 }, { baseResistor, 0 } }, { { supply, 0 }, { collectorResistor, 0 } },
				{ { baseResistor, 1 }, { transistor, 0 } }, { { collectorResistor, 1 }, { transistor, 1 } },
				{ { transistor, 2 }, { ground, 0 } }, { { supply, 1 }, { ground, 0 } },
			});
		}

		bool Solve()
		{
			// Newton da zero: per un componente non lineare un solo passo basta (non ci sono
			// elementi con memoria)
			return simulation.Step();
		}

		Transistor &Q() { return *static_cast<Transistor *>(circuit.GetComponentById(transistor)); }
	};

	// Soluzione di riferimento INDIPENDENTE dallo stamp MNA: risolve a mano le due equazioni di
	// Kirchhoff (base e collettore) di un NPN con emettitore a massa, usando solo le correnti
	// del modello (Transistor::Currents). Newton 2D con jacobiano numerico e passo limitato.
	std::pair<double, double> ReferenceNpn(const Transistor &q, double vcc, double rb, double rc, double vbStart, double vcStart)
	{
		auto residual = [&](double vb, double vc) {
			const auto currents = q.Currents(vb, vb - vc);
			return std::pair<double, double>{ (vcc - vb) / rb - currents.ib, (vcc - vc) / rc - currents.ic };
		};
		double vb = vbStart, vc = vcStart;
		for (int i = 0; i < 400; i++)
		{
			const auto f = residual(vb, vc);
			const double h = 1e-9;
			const auto fb = residual(vb + h, vc);
			const auto fc = residual(vb, vc + h);
			const double a = (fb.first - f.first) / h, b = (fc.first - f.first) / h;
			const double c = (fb.second - f.second) / h, d = (fc.second - f.second) / h;
			const double det = a * d - b * c;
			double dvb = (-f.first * d + f.second * b) / det;
			double dvc = (-a * f.second + c * f.first) / det;
			const double largest = std::max(std::abs(dvb), std::abs(dvc));
			const double scale = largest > 0.05 ? 0.05 / largest : 1.0; // passo massimo 50 mV
			vb += scale * dvb;
			vc += scale * dvc;
		}
		return { vb, vc };
	}
}

TEST_CASE("NPN: il punto di lavoro coincide con le equazioni del modello in ogni regione", "[transistor]")
{
	struct Case { const char *region; double rb; double vbStart; double vcStart; };
	// Interdizione/attiva/saturazione leggera/saturazione profonda
	const Case cases[] = {
		{ "attiva (Rb = 1 M)", 1e6, 0.65, 9.0 },
		{ "saturazione (Rb = 10 k)", 1e4, 0.72, 0.07 },
		{ "saturazione profonda (Rb = 1 k)", 1e3, 0.78, 0.1 },
	};

	for (const Case &c : cases)
	{
		DYNAMIC_SECTION(c.region)
		{
			BiasCircuit bias(false, 10.0, c.rb, 1e3);
			REQUIRE(bias.Solve());

			const auto [vb, vc] = ReferenceNpn(bias.Q(), 10.0, c.rb, 1e3, c.vbStart, c.vcStart);
			const int base = NodeOf(bias.circuit, bias.transistor, 0);
			const int collector = NodeOf(bias.circuit, bias.transistor, 1);

			// Newton si ferma quando le correnti predette e vere coincidono entro 1e-3 (come il
			// RELTOL di SPICE): con 1 kohm di carico e fino a 10 mA sono pochi mV. Un errore di
			// modello (la regressione di saturazione sbagliava di volt) resta ben visibile.
			CHECK(bias.simulation.Voltage(base) == Approx(vb).margin(2e-3));
			CHECK(bias.simulation.Voltage(collector) == Approx(vc).margin(5e-3));
		}
	}
}

TEST_CASE("NPN: in saturazione la corrente di collettore non supera quella del resistore di carico", "[transistor]")
{
	// Regressione: lo stamp contava due volte il termine base-collettore, e in saturazione la
	// corrente "del transistor" (26 mA) superava il massimo fisico (10 mA = (Vcc - Vcesat)/Rc)
	BiasCircuit bias(false, 10.0, 1e4, 1e3);
	REQUIRE(bias.Solve());

	const double vb = bias.simulation.Voltage(bias.transistor, 0);
	const double vc = bias.simulation.Voltage(bias.transistor, 1);
	const auto currents = bias.Q().Currents(vb, vb - vc);

	CHECK(currents.ic == Approx((10.0 - vc) / 1e3).epsilon(1e-3));
	CHECK(currents.ic < 10e-3);
	CHECK(currents.ic / currents.ib < 100.0); // saturato: guadagno forzato sotto BF
}

TEST_CASE("NPN: in zona attiva Ic/Ib = BF e le correnti sommano a zero", "[transistor]")
{
	BiasCircuit bias(false, 10.0, 1e6, 1e3);
	REQUIRE(bias.Solve());

	const double vb = bias.simulation.Voltage(bias.transistor, 0);
	const double vc = bias.simulation.Voltage(bias.transistor, 1);
	const auto currents = bias.Q().Currents(vb, vb - vc);

	CHECK(currents.ic / currents.ib == Approx(100.0).epsilon(0.01));
	CHECK(currents.ib + currents.ic + currents.ie == Approx(0.0).margin(1e-15));
	CHECK(vb > 0.55);
	CHECK(vb < 0.75);
}

TEST_CASE("PNP: è lo specchio esatto dell'NPN (tensioni e correnti opposte)", "[transistor][pnp]")
{
	for (const double rb : { 1e6, 1e4, 1e3 })
	{
		DYNAMIC_SECTION("Rb = " << rb)
		{
			BiasCircuit npn(false, 10.0, rb, 1e3);
			BiasCircuit pnp(true, 10.0, rb, 1e3);
			REQUIRE(npn.Solve());
			REQUIRE(pnp.Solve());

			const double nb = npn.simulation.Voltage(npn.transistor, 0), nc = npn.simulation.Voltage(npn.transistor, 1);
			const double pb = pnp.simulation.Voltage(pnp.transistor, 0), pc = pnp.simulation.Voltage(pnp.transistor, 1);
			CHECK(pb == Approx(-nb).epsilon(1e-6));
			CHECK(pc == Approx(-nc).epsilon(1e-6));

			const auto ni = npn.Q().Currents(nb, nb - nc);
			const auto pi = pnp.Q().Currents(pb, pb - pc);
			CHECK(pi.ib == Approx(-ni.ib).epsilon(1e-6));
			CHECK(pi.ic == Approx(-ni.ic).epsilon(1e-6));
			CHECK(pi.ib + pi.ic + pi.ie == Approx(0.0).margin(1e-15));
		}
	}
}

TEST_CASE("PNP: con l'emettitore all'alimentazione (configurazione high-side) conduce con VEB ~ 0.65 V", "[transistor][pnp]")
{
	Circuit circuit;
	const int q = circuit.AddComponent(std::make_unique<Transistor>(1e-14, 100.0, true));
	const int baseResistor = circuit.AddComponent(std::make_unique<Resistor>(1e6));
	const int collectorResistor = circuit.AddComponent(std::make_unique<Resistor>(1e3));
	const int supply = circuit.AddComponent(DcSource(10.0));
	const int ground = circuit.AddComponent(std::make_unique<Ground>());
	Connect(circuit, {
		{ { supply, 0 }, { q, 2 } }, { { q, 0 }, { baseResistor, 0 } }, { { q, 1 }, { collectorResistor, 0 } },
		{ { baseResistor, 1 }, { ground, 0 } }, { { collectorResistor, 1 }, { ground, 0 } }, { { supply, 1 }, { ground, 0 } },
	});

	Simulation simulation(circuit, 1e-4);
	REQUIRE(simulation.Step());

	const double ve = simulation.Voltage(q, 2), vb = simulation.Voltage(q, 0), vc = simulation.Voltage(q, 1);
	CHECK(ve == Approx(10.0).margin(1e-9));
	CHECK(ve - vb > 0.55);
	CHECK(ve - vb < 0.75);

	const auto currents = static_cast<Transistor *>(circuit.GetComponentById(q))->Currents(vb - ve, vb - vc);
	CHECK(currents.ic / currents.ib == Approx(100.0).epsilon(0.01));
	CHECK(currents.ie > 0.0); // la corrente entra dall'emettitore
	CHECK(vc == Approx(1e3 * -currents.ic).margin(1e-4)); // esce dal collettore verso il resistore
}
