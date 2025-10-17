#pragma once
#include <vector>
#include <cmath>
#include <cstdint>

// Lightweight frame accumulator for RMS and Zero-Crossing Rate
// - pushSample(L, R) appends one stereo sample
// - when size==FrameSize (1024), ready() becomes true; read rms(), zcr() and call reset()
// Implementation note: combines L/R to mid (average) for feature computation.
struct FeatureExtractor {
    static constexpr size_t FrameSize = 1024;

    FeatureExtractor() { buf.reserve(FrameSize); }

    void pushSample(float L, float R) {
        float m = 0.5f * (L + R);
        buf.push_back(m);
        if (buf.size() > FrameSize) buf.erase(buf.begin()); // keep last N
    }

    bool ready() const { return buf.size() == FrameSize; }

    void reset() { buf.clear(); }

    float rms() const {
        if (buf.empty()) return 0.f;
        double acc = 0.0;
        for (float x : buf) acc += double(x) * double(x);
        return float(std::sqrt(acc / double(buf.size())));
    }

    // Zero-Crossing Rate: crossings per sample (0..1)
    float zcr() const {
        if (buf.size() < 2) return 0.f;
        size_t crossings = 0;
        for (size_t i = 1; i < buf.size(); ++i) {
            float a = buf[i - 1], b = buf[i];
            crossings += ((a >= 0.f && b < 0.f) || (a < 0.f && b >= 0.f)) ? 1 : 0;
        }
        return float(crossings) / float(buf.size() - 1);
    }

private:
    std::vector<float> buf;
};
