#pragma once
#include <cmath>

// Simple one-pole exponential smoother for parameter changes.
// y[n] = (1 - a) * x + a * y[n-1], where a = exp(-1/(tau*fs))
// Attack/Release both share the same tau here for simplicity.
struct OnePoleSmoother {
    void setSampleRate(double fs) {
        sampleRate = fs > 0 ? fs : 48000.0;
        updateCoeff();
    }
    // tauSeconds ~ 0.005..0.02 typical for click-free smoothing
    void setTimeConstant(float tauSeconds) {
        tau = (tauSeconds > 1e-6f) ? tauSeconds : 1e-6f;
        updateCoeff();
    }
    void reset(float value) { z = value; }

    float process(float target) {
        // denormal-safe tiny offset (prevents subnormal slowdowns)
        const float tiny = 1e-20f;
        z = a * (z + tiny) + (1.0f - a) * target;
        return z;
    }

private:
    void updateCoeff() {
        a = std::exp(float(-1.0 / (tau * float(sampleRate))));
    }

    double sampleRate = 48000.0;
    float tau = 0.01f; // 10 ms default
    float a = 0.0f;
    float z = 0.0f;
};
