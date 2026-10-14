#pragma once
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include <Eigen/Dense>

#include "Core/Circuit.h"
#include "Core/NonlinearSolve.h"
#include "Core/Solver.h"
#include "Core/TransientStepper.h"
#include "Components/Capacitor.h"
#include "Components/Diode.h"
#include "Components/Ground.h"
#include "Components/Inductor.h"
#include "Components/Resistor.h"
#include "Components/Transformer.h"
#include "Components/Transistor.h"
#include "Components/VoltageGenerator.h"

// Strumenti comuni ai test: costruzione veloce di circuiti e un simulatore a passi che usa lo stesso
// TransientStepper dell'applicazione (matrice, vettore, soluzione lineare o con Newton, stato dei
// componenti con memoria), senza interfaccia grafica, SFML o thread.
namespace test {

	using namespace CircuitLab;

	std::unique_ptr<VoltageGenerator> DcSource(double volts);
	std::unique_ptr<VoltageGenerator> SineSource(double amplitude, double frequencyHz);

	// Collega a coppie {terminale, terminale}, ciascuno scritto come {idComponente, indiceTerminale}
	struct Pin { int component; int terminal; };
	void Connect(Circuit &circuit, std::initializer_list<std::pair<Pin, Pin>> links);

	// nodeId del terminale (0 = massa, -1 = libero)
	int NodeOf(const Circuit &circuit, int component, int terminal);

	class Simulation {
	public:
		Simulation(Circuit &circuit, double timestep);

		// Un passo di transitorio; restituisce false se il solver non converge (e allora lo
		// stato non cambia, esattamente come nell'applicazione)
		bool Step();

		// Tensione di un nodo rispetto a massa nell'ultima soluzione
		double Voltage(int nodeId) const;
		double Voltage(int component, int terminal) const;

		// Adotta come soluzione corrente quella del punto di lavoro DC (vedi SolveOperatingPoint)
		bool StartFromDcOperatingPoint();

		double time() const { return m_time; }
		double timestep() const { return m_h; }
		const Eigen::VectorXd &solution() const { return m_stepper.Solution(); }

	private:
		Circuit &m_circuit;
		Solver m_solver;
		TransientStepper m_stepper;
		double m_h;
		double m_time = 0.0;
	};

	// ---- Circuiti usati da più file di test --------------------------------------------------

	// Amplificatore a emettitore comune accoppiato in alternata (R2/R3 polarizzano la base,
	// R4 è il carico di collettore, R5 la resistenza di emettitore, C6 accoppia l'ingresso e C7
	// l'uscita su R8). I componenti si indicano con i nomi usati sullo schema.
	struct CommonEmitterAmplifier {
		int q1, r2, r3, r4, r5, c6, c7, r8, signal, supply;
	};
	CommonEmitterAmplifier BuildCommonEmitterAmplifier(Circuit &circuit, double supplyVolts, double signalAmplitude, double r2Ohms = 46000.0);

	// Trigger di Schmitt a due transistor con l'emettitore in comune (il circuito di
	// "trigger.json"): ingresso = serie di una sinusoide da 2 kHz e una da 40 Hz sulla base di Q3,
	// uscita = collettore di Q6.
	struct SchmittTrigger {
		int fastSource, slowSource, q3, q6;
	};
	SchmittTrigger BuildSchmittTrigger(Circuit &circuit, double fastAmplitude, double slowAmplitude);

	// Multivibratore astabile (il circuito di "Oscillatore astabile.json"): collettori su
	// 330/320 ohm, basi su rBase, condensatori da 18 uF, alimentazione 5 V.
	struct AstableMultivibrator {
		int q1, q2;
	};
	AstableMultivibrator BuildAstableMultivibrator(Circuit &circuit, double baseResistance);
}
