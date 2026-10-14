#include "TestSupport.h"

#include "Common/Logger.h"

namespace {
	// Il Logger scrive in console ogni collegamento tra terminali e simili: nei test sarebbe solo rumore
	const bool s_quietLogger = (CircuitLab::Logger::GetInstance().SetMinLogLevel(CircuitLab::LogLevel::Error), true);
}

namespace test {

	std::unique_ptr<VoltageGenerator> DcSource(double volts)
	{
		auto source = std::make_unique<VoltageGenerator>(WaveForm::Create(WaveFormType::dcWaveForm));
		source->SetValues({ { ComponentValue::voltage, volts } });
		return source;
	}

	std::unique_ptr<VoltageGenerator> SineSource(double amplitude, double frequencyHz)
	{
		auto source = std::make_unique<VoltageGenerator>(WaveForm::Create(WaveFormType::sineWaveForm));
		source->SetValues({ { ComponentValue::amplitude, amplitude },
			{ ComponentValue::frequency, frequencyHz },
			{ ComponentValue::phase, 0.0 } });
		return source;
	}

	void Connect(Circuit &circuit, std::initializer_list<std::pair<Pin, Pin>> links)
	{
		for (const auto &[a, b] : links)
			circuit.ConnectTerminals(a.component, a.terminal, b.component, b.terminal);
	}

	int NodeOf(const Circuit &circuit, int component, int terminal)
	{
		return circuit.GetNodesIdFromComponentId(component)[terminal];
	}

	Simulation::Simulation(Circuit &circuit, double timestep) : m_circuit(circuit), m_stepper(circuit, m_solver), m_h(timestep)
	{
		m_circuit.SetTimestep(timestep);
	}

	double Simulation::Voltage(int nodeId) const
	{
		return m_stepper.NodeVoltage(nodeId);
	}

	double Simulation::Voltage(int component, int terminal) const
	{
		return Voltage(NodeOf(m_circuit, component, terminal));
	}

	bool Simulation::StartFromDcOperatingPoint()
	{
		return m_stepper.StartFromDcOperatingPoint();
	}

	bool Simulation::Step()
	{
		if (m_stepper.Step(m_time, m_h) != TransientStepper::Status::ok)
			return false;

		m_time += m_h;
		return true;
	}

	CommonEmitterAmplifier BuildCommonEmitterAmplifier(Circuit &circuit, double supplyVolts, double signalAmplitude, double r2Ohms)
	{
		CommonEmitterAmplifier a;
		a.q1 = circuit.AddComponent(std::make_unique<Transistor>(1e-14, 100.0));
		a.r2 = circuit.AddComponent(std::make_unique<Resistor>(r2Ohms));
		a.r3 = circuit.AddComponent(std::make_unique<Resistor>(10000.0));
		a.r4 = circuit.AddComponent(std::make_unique<Resistor>(10000.0));
		a.r5 = circuit.AddComponent(std::make_unique<Resistor>(1000.0));
		a.c6 = circuit.AddComponent(std::make_unique<Capacitor>(5e-6));
		a.c7 = circuit.AddComponent(std::make_unique<Capacitor>(5e-6));
		a.r8 = circuit.AddComponent(std::make_unique<Resistor>(1000000.0));
		a.signal = circuit.AddComponent(SineSource(signalAmplitude, 80.0));
		a.supply = circuit.AddComponent(DcSource(supplyVolts));
		const int ground1 = circuit.AddComponent(std::make_unique<Ground>());
		const int ground2 = circuit.AddComponent(std::make_unique<Ground>());

		Connect(circuit, {
			{ { a.r2, 1 }, { a.r3, 0 } }, { { a.r2, 1 }, { a.q1, 0 } },       // partitore sulla base
			{ { a.q1, 2 }, { a.r5, 0 } },                                   // emettitore
			{ { a.q1, 1 }, { a.r4, 1 } }, { { a.q1, 1 }, { a.c7, 1 } },       // collettore: carico e uscita
			{ { a.r3, 1 }, { a.r5, 1 } },
			{ { a.c6, 0 }, { a.r2, 1 } },                                   // ingresso accoppiato in alternata
			{ { a.r2, 0 }, { a.r4, 0 } }, { { a.signal, 0 }, { a.c6, 1 } },
			{ { ground1, 0 }, { a.r3, 1 } }, { { a.signal, 1 }, { a.r3, 1 } },
			{ { a.supply, 0 }, { a.r2, 0 } }, { { a.r3, 1 }, { a.supply, 1 } },
			{ { a.c7, 0 }, { a.r8, 0 } }, { { a.r8, 1 }, { ground2, 0 } },
		});
		return a;
	}

	SchmittTrigger BuildSchmittTrigger(Circuit &circuit, double fastAmplitude, double slowAmplitude)
	{
		SchmittTrigger s;
		s.fastSource = circuit.AddComponent(SineSource(fastAmplitude, 2000.0));
		s.slowSource = circuit.AddComponent(SineSource(slowAmplitude, 40.0));
		s.q3 = circuit.AddComponent(std::make_unique<Transistor>());
		const int r4 = circuit.AddComponent(std::make_unique<Resistor>(100.0));  // emettitore comune
		const int r5 = circuit.AddComponent(std::make_unique<Resistor>(100.0));  // collettore Q3 -> base Q6
		s.q6 = circuit.AddComponent(std::make_unique<Transistor>());
		const int r7 = circuit.AddComponent(std::make_unique<Resistor>(300.0));  // base Q6 verso massa
		const int r8 = circuit.AddComponent(std::make_unique<Resistor>(500.0));  // carico di Q6
		const int ground = circuit.AddComponent(std::make_unique<Ground>());
		const int r10 = circuit.AddComponent(std::make_unique<Resistor>(500.0)); // carico di Q3
		const int supply = circuit.AddComponent(DcSource(5.0));

		Connect(circuit, {
			{ { s.fastSource, 0 }, { s.slowSource, 1 } }, { { s.slowSource, 0 }, { s.q3, 0 } },
			{ { r4, 0 }, { s.q3, 2 } }, { { s.q3, 1 }, { r5, 1 } },
			{ { s.q6, 0 }, { r5, 0 } }, { { s.q6, 0 }, { r7, 0 } },
			{ { s.q6, 2 }, { r4, 0 } }, { { r8, 1 }, { s.q6, 1 } },
			{ { r4, 1 }, { r7, 1 } }, { { ground, 0 }, { r4, 1 } }, { { s.fastSource, 1 }, { r4, 1 } },
			{ { r10, 1 }, { s.q3, 1 } }, { { r10, 0 }, { r8, 0 } },
			{ { supply, 0 }, { r8, 0 } }, { { supply, 1 }, { r4, 1 } },
		});
		return s;
	}

	AstableMultivibrator BuildAstableMultivibrator(Circuit &circuit, double baseResistance)
	{
		AstableMultivibrator m;
		m.q1 = circuit.AddComponent(std::make_unique<Transistor>());
		m.q2 = circuit.AddComponent(std::make_unique<Transistor>());
		const int r3 = circuit.AddComponent(std::make_unique<Resistor>(330.0));          // carico di Q2
		const int r4 = circuit.AddComponent(std::make_unique<Resistor>(baseResistance)); // base di Q1
		const int r5 = circuit.AddComponent(std::make_unique<Resistor>(baseResistance)); // base di Q2
		const int r6 = circuit.AddComponent(std::make_unique<Resistor>(320.0));          // carico di Q1
		const int c7 = circuit.AddComponent(std::make_unique<Capacitor>(18e-6));
		const int c8 = circuit.AddComponent(std::make_unique<Capacitor>(18e-6));
		const int ground = circuit.AddComponent(std::make_unique<Ground>());
		const int supply = circuit.AddComponent(DcSource(5.0));

		Connect(circuit, {
			{ { r3, 0 }, { r4, 0 } }, { { r3, 0 }, { r5, 0 } }, { { r3, 0 }, { r6, 0 } },  // rail di alimentazione
			{ { r3, 1 }, { c7, 1 } }, { { c7, 0 }, { r4, 1 } },
			{ { r5, 1 }, { c8, 1 } }, { { r6, 1 }, { c8, 0 } },
			{ { m.q1, 1 }, { r6, 1 } }, { { r3, 1 }, { m.q2, 1 } },
			{ { m.q2, 0 }, { r5, 1 } }, { { m.q1, 0 }, { c7, 0 } },
			{ { ground, 0 }, { m.q2, 2 } }, { { m.q1, 2 }, { ground, 0 } },
			{ { supply, 0 }, { r3, 0 } }, { { supply, 1 }, { ground, 0 } },
		});
		return m;
	}
}
