#pragma once
#include "signalsmith-stretch/signalsmith-stretch.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace pitch {
// Setup/destruction belong on a non-audio thread. Fixed default preset, mono.
class SpectralHybridShift {
public:
    SpectralHybridShift(double sampleRate, std::size_t maxBlock) : stretch_(12345), input_(maxBlock) {
        if (!std::isfinite(sampleRate) || sampleRate < 44100 || sampleRate > 96000 ||
            maxBlock == 0 || maxBlock > 65536) throw std::invalid_argument("Invalid preparation");
        stretch_.presetDefault(1, float(sampleRate), false);
    }
    int latencySamples() const { return stretch_.inputLatency()+stretch_.outputLatency(); }
    // Return false and clear output for invalid controls/buffers; leave DSP state unchanged.
    bool process(const float* input, float* output, std::size_t frames, float ratio) noexcept {
        if (!output) return false;
        if (!input || frames > input_.size() || !std::isfinite(ratio) || ratio < .5f || ratio > 2.f) {
            std::fill_n(output, frames, 0.f); return false;
        }
        for (std::size_t i=0;i<frames;++i) input_[i]=std::isfinite(input[i]) ? input[i] : 0.f;
        const float* in[] = {input_.data()}; float* out[] = {output};
        stretch_.setTransposeFactor(ratio);
        stretch_.process(in, int(frames), out, int(frames));
        bool finite=true;
        for (std::size_t i=0;i<frames;++i) if (!std::isfinite(output[i])) { output[i]=0.f; finite=false; }
        return finite;
    }
private:
    signalsmith::stretch::SignalsmithStretch<float> stretch_;
    std::vector<float> input_; // Protect upstream from SC's permitted buffer aliasing.
};
}
