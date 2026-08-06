#include "Components/WaveForm.h"
#include "Components/DCWaveForm.h"
#include "Components/SineWaveForm.h"
#include "Components/SquareWaveForm.h"

// Scrive tipo e dati specifici sotto la sottochiave "waveform", così il JSON
// del componente proprietario (es. VoltageGenerator) non rischia collisioni di chiavi.
void CircuitLab::WaveForm::Save(nlohmann::json &j) const
{
    nlohmann::json waveformJson;
    waveformJson["type"] = GetType();
    SaveSpecificData(waveformJson);
    j["waveform"] = waveformJson;
}

// Crea la forma d'onda richiesta con valori di default "ragionevoli";
// i valori reali vengono impostati subito dopo da chi chiama (es. Load, o UI).
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

// Legge il tipo da j["type"], crea l'istanza via Create() e ne popola i valori
// specifici leggendo j["value"][0][...] (struttura scritta da SaveSpecificData).
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