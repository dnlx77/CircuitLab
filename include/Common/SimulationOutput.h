#pragma once
#include <vector>
#include <string>

namespace CircuitLab {
	enum class SimulationResult {
		success,
		empty_circuit,
		no_circuit,
		solve_error
	};

	struct SimulationOutput {
		SimulationResult simRes;
		std::vector<std::pair<std::string, double>> res;
	};
}