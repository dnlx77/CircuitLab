#include "Components/DCWaveForm.h"

CircuitLab::DCWaveForm::DCWaveForm(double voltage) : m_voltage(voltage)
{
	m_waveFormType = WaveFormType::dcWaveForm;
}

double CircuitLab::DCWaveForm::Evaluate(double t)
{
	(void)t;  // t non usato: la forma d'onda DC è costante nel tempo
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

// Scrive il valore sotto forma di array con un solo elemento (j["value"][0]),
// per uniformità di formato con le altre WaveForm che hanno più parametri.
void CircuitLab::DCWaveForm::SaveSpecificData(nlohmann::json &j) const
{
	j["value"] = nlohmann::json::array();
	nlohmann::json dcWaveFormValueJson;
	dcWaveFormValueJson["voltage"] = m_voltage;
	j["value"].push_back(dcWaveFormValueJson);
}