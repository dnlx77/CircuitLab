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
        bool active = true;
        static constexpr int MAX_SAMPLES = 4096;
        Color channelColor;
    };
}