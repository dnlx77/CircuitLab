#include "Components/DCWaveForm.h"

CircuitLab::DCWaveForm::DCWaveForm(double voltage) : m_voltage(voltage)
{
	m_waveFormType = WaveFormType::dcWaveForm;
}

double CircuitLab::DCWaveForm::Evaluate(double t)
{
	(void)t;
	return m_voltage;
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::DCWaveForm::GetValues() const
{
	std::map<ComponentValue, double> map;
	map[ComponentValue::voltage] = m_voltage;
	return map;
}

void CircuitLab::DCWaveForm::SetValues(const std::map<ComponentValue, double> &values)
{
	m_voltage = values.at(ComponentValue::voltage);
}

void CircuitLab::DCWaveForm::SaveSpecificData(nlohmann::json & j) const
{
	j["value"] = nlohmann::json::array();
	nlohmann::json dcWaveFormValueJson;
	dcWaveFormValueJson["voltage"] = m_voltage;
	j["value"].push_back(dcWaveFormValueJson);
}
