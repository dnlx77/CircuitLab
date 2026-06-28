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
        int idB = -1;        // usato solo per differentialVoltage e branchCurrent
        int compId = -1;     // usato solo per branchCurrent
        std::string label;
        std::deque<double> samples;
        bool active = true;
        int maxSamples = 512;
        Color channelColor;
    };
}