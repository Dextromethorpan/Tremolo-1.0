#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "Tremolo.h"
#include <algorithm>
#include <cmath>
#include <cctype>

static inline float clamp01(float x){ return std::max(0.f, std::min(1.f, x)); }
static inline float clamp11(float x){ return std::max(-1.f, std::min(1.f, x)); }

void Tremolo::setSampleRate(double fs) {
    sr_ = fs > 0.0 ? fs : 48000.0;
    depthSm_.setSampleRate(sr_);
    rateSm_.setSampleRate(sr_);
    depthSm_.setTimeConstant(0.01f);
    rateSm_.setTimeConstant(0.01f);
    depthSm_.reset(depth_);
    rateSm_.reset(rateHz_);
    phaseInc_ = float(rateHz_ / sr_);
}

void Tremolo::setDepth(float d)   { depth_ = clamp01(d); }
void Tremolo::setRateHz(float r)  { rateHz_ = std::max(0.0001f, r); }
void Tremolo::setWet(float w)     { wet_ = clamp01(w); }
void Tremolo::setStereoPhaseDeg(float deg) {
    float d = std::max(0.f, std::min(180.f, deg));
    stereoPhaseR_ = d / 360.f; // convert deg to [0..0.5] cycles; 180deg -> 0.5
}
void Tremolo::setShape(LFOShape s){ shape_ = s; }

LFOShape Tremolo::parseShape(const std::string& s) {
    auto lower = s; for (auto& c : lower) c = char(std::tolower(c));
    if (lower == "sine") return LFOShape::Sine;
    if (lower == "triangle") return LFOShape::Triangle;
    if (lower == "square") return LFOShape::Square;
    if (lower == "square-soft") return LFOShape::SquareSoft;
    return LFOShape::Sine;
}

// Waveforms return lfo in [0..1]
inline float Tremolo::lfoSine(float ph) const {
    return 0.5f * (1.0f + std::sin(2.0f * float(M_PI) * ph));
}
inline float Tremolo::lfoTriangle(float ph) const {
    float t = std::fmod(ph, 1.0f);
    float tri = (t < 0.5f) ? (t * 4.0f - 1.0f) : (3.0f - t * 4.0f); // -1..1
    return 0.5f * (tri + 1.0f);
}
inline float Tremolo::lfoSquare(float ph) const {
    float s = std::sin(2.0f * float(M_PI) * ph);
    return s >= 0.0f ? 1.0f : 0.0f;
}
inline float Tremolo::lfoSquareSoft(float ph) const {
    // "soft" square via tanh(k * sin), k=3 gives rounded corners
    float s = std::sin(2.0f * float(M_PI) * ph);
    float y = std::tanh(3.0f * s); // -1..1
    return 0.5f * (y + 1.0f);
}

void Tremolo::process(float* interleaved, size_t frames, int channels) {
    if (!interleaved || channels < 1) return;
    const float tiny = 1e-20f;

    for (size_t i = 0; i < frames; ++i) {
        float rateNow = rateSm_.process(rateHz_);
        phaseInc_ = std::max(1e-9f, float(rateNow / sr_));
        float dNow = depthSm_.process(depth_);

        float phL = phase_;
        float phR = std::fmod(phase_ + stereoPhaseR_, 1.0f);

        float lfoL, lfoR;
        switch (shape_) {
            case LFOShape::Triangle:   lfoL = lfoTriangle(phL); lfoR = lfoTriangle(phR); break;
            case LFOShape::Square:     lfoL = lfoSquare(phL);   lfoR = lfoSquare(phR);   break;
            case LFOShape::SquareSoft: lfoL = lfoSquareSoft(phL); lfoR = lfoSquareSoft(phR); break;
            default:                   lfoL = lfoSine(phL);     lfoR = lfoSine(phR);     break;
        }

        float gainL = 1.0f - dNow * lfoL;
        float gainR = 1.0f - dNow * lfoR;

        if (channels == 1) {
            float x = interleaved[i] + tiny;
            float wetSig = x * gainL;
            float y = (1.0f - wet_) * x + wet_ * wetSig;
            interleaved[i] = clamp11(y);
        } else {
            float* L = &interleaved[i * 2 + 0];
            float* R = &interleaved[i * 2 + 1];
            float xL = *L + tiny;
            float xR = *R + tiny;
            float wetL = xL * gainL;
            float wetR = xR * gainR;
            float yL = (1.0f - wet_) * xL + wet_ * wetL;
            float yR = (1.0f - wet_) * xR + wet_ * wetR;
            *L = clamp11(yL);
            *R = clamp11(yR);
        }

        // advance phase per-sample
        phase_ += phaseInc_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
    }
}
