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
// con LoadSpecificData (che legge la struttura scritta da SaveSpecificData).
std::unique_ptr<CircuitLab::WaveForm> CircuitLab::WaveForm::Load(const nlohmann::json &j)
{
    WaveFormType type = j["type"].get<WaveFormType>();
    auto waveform = Create(type);

    // "none" non ha parametri da leggere: resta la forma d'onda continua di default
    if (type != WaveFormType::none)
        waveform->LoadSpecificData(j);
    return waveform;
}