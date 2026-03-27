#pragma once
#include "Component.h"

namespace CircuitLab {
	
	class Resistor : public Component {
	private:
		double m_resistance;
	public:
		Resistor();
		void Stamp() override;
		void SaveSpecificData() override;
		void LoadSpecificData() override;
	};
}