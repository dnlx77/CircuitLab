#include <numbers>
#include "Components/SineWaveForm.h"


CircuitLab::SineWaveForm::SineWaveForm(double amplitude, double frequency, double phase) : m_amplitude(amplitude), m_frequency(frequency), m_phase(phase)
{
	m_waveFormType = WaveFormType::sineWaveForm;
}

double CircuitLab::SineWaveForm::Evaluate(double t)
{
	return m_amplitude * std::sin(2 * std::numbers::pi * m_frequency * t + m_phase);
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::SineWaveForm::GetValues() const
{
	std::map<ComponentValue, double> map;
	map[ComponentValue::amplitude] = m_amplitude;
	map[ComponentValue::frequency] = m_frequency;
	map[ComponentValue::phase] = m_phase;
	return map;
}

void CircuitLab::SineWaveForm::SetValues(const std::map<ComponentValue, double> &values)
{
	m_amplitude = values.at(ComponentValue::amplitude);
	m_frequency = values.at(ComponentValue::frequency);
	m_phase = values.at(ComponentValue::phase);
}

void CircuitLab::SineWaveForm::SaveSpecificData(nlohmann::json & j) const
{
	j["value"] = nlohmann::json::array();
	nlohmann::json sineWaveFormValueJson;
	sineWaveFormValueJson["amplitude"] = m_amplitude;
	sineWaveFormValueJson["frequency"] = m_frequency;
	sineWaveFormValueJson["phase"] = m_phase;
	j["value"].push_back(sineWaveFormValueJson);
}
