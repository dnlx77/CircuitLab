#include<numbers>
#include "Components/SquareWaveForm.h"

CircuitLab::SquareWaveForm::SquareWaveForm(double amplitude, double frequency) : m_amplitude(amplitude), m_frequency(frequency)
{
	m_waveFormType = WaveFormType::squareWaveForm;
}

double CircuitLab::SquareWaveForm::Evaluate(double t)
{
	return m_amplitude * (std::sin(2 * std::numbers::pi * m_frequency * t) >= 0 ? 1.0 : -1.0);
}

std::map<CircuitLab::ComponentValue, double> CircuitLab::SquareWaveForm::GetValues() const
{
	std::map<ComponentValue, double> map;
	map[ComponentValue::amplitude] = m_amplitude;
	map[ComponentValue::frequency] = m_frequency;
	return map;
}

void CircuitLab::SquareWaveForm::SetValues(const std::map<ComponentValue, double> &values)
{
	m_amplitude = values.at(ComponentValue::amplitude);
	m_frequency = values.at(ComponentValue::frequency);
}

void CircuitLab::SquareWaveForm::SaveSpecificData(nlohmann::json & j) const
{
	j["value"] = nlohmann::json::array();
	nlohmann::json squareWaveFormValueJson;
	squareWaveFormValueJson["amplitude"] = m_amplitude;
	squareWaveFormValueJson["frequency"] = m_frequency;
	j["value"].push_back(squareWaveFormValueJson);
}
