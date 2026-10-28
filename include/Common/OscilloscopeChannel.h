#pragma once
#include <string>
#include <deque>

namespace CircuitLab {

    enum class ProbeType {
        nodeVoltage,
        differentialVoltage,
        componentCurrent,
        branchCurrent
    };

    struct Color {
        float r, g, b;
    };

    struct OscilloscopeChannel {
        ProbeType type;
        int idA = -1;
        int idB = -1;
        int compId = -1;
        std::string label;
        std::deque<double> samples;  // al massimo MAX_SAMPLES campioni
        // Tempo simulato dell'ULTIMO campione in samples, scritto da SampleChannels
        // sotto lo stesso mutex che protegge samples. Serve alla modalità "sweep"
        // dell'oscilloscopio: leggere il tempo di simulazione a parte, in un altro
        // istante rispetto alla copia dei campioni, sfaserebbe la traccia di
        // qualche campione ad ogni frame e l'immagine "ferma" sembrerebbe tremare.
        double lastSampleTime = 0.0;
        bool active = true;
        static constexpr int MAX_SAMPLES = 4096;
        Color channelColor;

        // Aggiunge un campione (relativo al tempo simulato `time`) scartando il più vecchio oltre MAX_SAMPLES
        void AddSample(double value, double time)
        {
            samples.push_back(value);
            lastSampleTime = time;
            if (static_cast<int>(samples.size()) > MAX_SAMPLES)
                samples.pop_front();
        }
    };
}