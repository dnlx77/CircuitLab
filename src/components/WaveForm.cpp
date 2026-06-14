#include "Components/WaveForm.h"
#include "Components/DCWaveForm.h"
#include "Components/SineWaveForm.h"
#include "Components/SquareWaveForm.h"

void CircuitLab::WaveForm::Save(nlohmann::json &j) const
{
    nlohmann::json waveformJson;
    waveformJson["type"] = GetType();
    SaveSpecificData(waveformJson);
    j["waveform"] = waveformJson;
}

std::unique_ptr<CircuitLab::WaveForm> CircuitLab::WaveForm::Create(WaveFormType type)
{
	switch (type)
	{
	case WaveFormType::dcWaveForm:     return std::make_unique<DCWaveForm>(0.0);
	case WaveFormType::sineWaveForm:   return std::make_unique<SineWaveForm>(1.0, 50.0, 0.0);
	case WaveFormType::squareWaveForm: return std::make_unique<SquareWaveForm>(1.0, 50.0);
	default:                           return std::make_unique<DCWaveForm>(0.0);
	}
}

std::unique_ptr<CircuitLab::WaveForm> CircuitLab::WaveForm::Load(const nlohmann::json &j)
{
    WaveFormType type = j["type"].get<WaveFormType>();
    auto waveform = Create(type);

    switch (type)
    {
        case WaveFormType::dcWaveForm:
        {
            std::map<ComponentValue, double> values;
            values[ComponentValue::voltage] = j["value"][0]["voltage"];
            waveform->SetValues(values);
            break;
        }
        case WaveFormType::sineWaveForm:
        {
            std::map<ComponentValue, double> values;
            values[ComponentValue::amplitude] = j["value"][0]["amplitude"];
            values[ComponentValue::frequency] = j["value"][0]["frequency"];
            values[ComponentValue::phase] = j["value"][0]["phase"];
            waveform->SetValues(values);
            break;
        }
        case WaveFormType::squareWaveForm:
        {
            std::map<ComponentValue, double> values;
            values[ComponentValue::amplitude] = j["value"][0]["amplitude"];
            values[ComponentValue::frequency] = j["value"][0]["frequency"];
            waveform->SetValues(values);
            break;
        }
    }
    return waveform;
}
