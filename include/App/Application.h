#pragma once
#include <memory>
#include <Eigen/Dense>
#include "Core/Circuit.h"
#include "Common/ComponentType.h"
#include "Common/SimulationOutput.h"

namespace CircuitLab {

	class UI;

	class Application {
	private:
		std::unique_ptr<Circuit> m_circuit;
		std::unique_ptr<UI> m_ui;
		Eigen::VectorXd m_simulationResult;

		std::unique_ptr<Component> MakeComponent(ComponentType type, double value);
	public:
		Application();
		~Application();
		
		SimulationOutput RunSimulation();
		const Eigen::VectorXd &GetResult() const { return m_simulationResult; }

		void Run();
	};
}