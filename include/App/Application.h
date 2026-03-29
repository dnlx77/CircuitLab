#pragma once
#include <memory>
#include <Eigen/Dense>
#include "Core/Circuit.h"

namespace CircuitLab {
	enum class SimulationResult {
		success,
		empty_circuit,
		no_circuit,
		solve_error
	};

	class Application {
	private:
		std::unique_ptr<Circuit> m_circuit;
		// std::unique_ptr<UI> m_ui;
		Eigen::VectorXd m_simulationResult;
	public:
		Application();
		
		SimulationResult RunSimulation();
		const Eigen::VectorXd &GetResult() const { return m_simulationResult; }
	};
}