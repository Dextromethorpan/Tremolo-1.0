#pragma once
#include <string>
#include <cstdint>
#include "OnePoleSmoother.h"

enum class LFOShape { Sine, Triangle, Square, SquareSoft };

struct Tremolo {
    void setSampleRate(double fs);
    void setDepth(float d);        // [0..1]
    void setRateHz(float r);       // >0
    void setWet(float w);          // [0..1]
    void setStereoPhaseDeg(float deg); // [0..180]
    void setShape(LFOShape s);

    // Process in-place float buffers [-1..1]. Supports mono or stereo.
    void process(float* interleaved, size_t frames, int channels);

    // Utility:
    static LFOShape parseShape(const std::string& s);

private:
    inline float lfoSine(float ph) const;
    inline float lfoTriangle(float ph) const;
    inline float lfoSquare(float ph) const;
    inline float lfoSquareSoft(float ph) const;

    double sr_ = 48000.0;
    float rateHz_ = 5.0f;
    float depth_ = 0.6f;
    float wet_ = 1.0f;
    float phase_ = 0.0f;           // [0..1)
    float phaseInc_ = 0.0f;        // delta per sample
    float stereoPhaseR_ = 0.0f;    // right-channel phase offset [0..1)
    LFOShape shape_ = LFOShape::Sine;

    OnePoleSmoother depthSm_;
    OnePoleSmoother rateSm_;
};
