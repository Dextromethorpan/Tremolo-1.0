#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct WavData {
    int sampleRate = 44100;
    int channels = 2;
    std::vector<float> samples; // interleaved, normalized [-1..1]
};

// Minimal 16-bit PCM WAV reader/writer (little-endian)
namespace WavIO {
    // Returns true on success; throws std::runtime_error on critical format errors
    bool read16(const std::string& path, WavData& out);
    bool write16(const std::string& path, const WavData& in);

    // Utility: generate a tiny stereo sine "pad" (auto-used if input missing)
    WavData makeTestPad(float seconds = 2.0f, int sr = 44100);
}
