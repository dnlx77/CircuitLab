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
        std::deque<double> samples;  // sempre 512 campioni max
        // Tempo simulato dell'ULTIMO campione in samples, scritto da SampleChannels
        // sotto lo stesso mutex che protegge samples. Serve alla modalità "sweep"
        // dell'oscilloscopio: leggere il tempo di simulazione a parte, in un altro
        // istante rispetto alla copia dei campioni, sfaserebbe la traccia di
        // qualche campione ad ogni frame e l'immagine "ferma" sembrerebbe tremare.
        double lastSampleTime = 0.0;
        bool active = true;
        static constexpr int MAX_SAMPLES = 4096;
        Color channelColor;
    };
}