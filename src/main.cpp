#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <map>
#include <cmath>
#include <chrono>
#include <cstring>

#include "WavIO.h"
#include "Tremolo.h"
#include "FeatureExtractor.h"
#include "Controller.h"

struct NoOpController : Controller {
    void update(double /*timeSeconds*/, float /*rms*/, float /*zcr*/,
                float& /*rateHz*/, float& /*depth*/) override {
        // no changes
    }
};

// Simple CLI parsing
struct Args {
    std::string in = "assets/input.wav";
    std::string out = "assets/output_without_smoother.wav";
    float rate = 5.0f;
    float depth = 0.6f;
    float wet = 1.0f;
    float stereophase = 0.0f;
    std::string shape = "sine";
    bool analyze = false;
    bool demo = false;
    std::string rateSync; // e.g., "bpm:120,div:1/8"
};

static void print_help() {
    std::cout <<
R"(SmartTremolo - dependency-free tremolo (PCM16 WAV)
Usage:
  smart_tremolo --in <in.wav> --out <out.wav>
                --rate <Hz> --depth <0..1> --shape <sine|triangle|square|square-soft>
                --stereophase <0..180> --wet <0..1>
                [--rate-sync bpm:120,div:1/8] [--analyze] [--demo] [--help]
Defaults:
  --in assets/input.wav --out assets/output.wav --rate 5.0 --depth 0.6
  --shape sine --stereophase 0 --wet 1.0
Notes:
  - Only PCM 16-bit mono/stereo WAV supported.
  - If assets/input.wav is missing, a short test file is generated automatically.
)" << std::endl;
}

static bool parseArgs(int argc, char** argv, Args& a) {
    for (int i=1;i<argc;++i){
        std::string k = argv[i];
        auto need = [&](const char* name)->std::string{
            if (i+1>=argc) { std::cerr<<"Missing value for "<<name<<"\n"; std::exit(1); }
            return argv[++i];
        };
        if (k=="--in") a.in = need("--in");
        else if (k=="--out") a.out = need("--out");
        else if (k=="--rate") a.rate = std::stof(need("--rate"));
        else if (k=="--depth") a.depth = std::stof(need("--depth"));
        else if (k=="--wet") a.wet = std::stof(need("--wet"));
        else if (k=="--stereophase") a.stereophase = std::stof(need("--stereophase"));
        else if (k=="--shape") a.shape = need("--shape");
        else if (k=="--rate-sync") a.rateSync = need("--rate-sync");
        else if (k=="--analyze") a.analyze = true;
        else if (k=="--demo") a.demo = true;
        else if (k=="--help" || k=="-h") { print_help(); std::exit(0); }
        else { std::cerr<<"Unknown flag: "<<k<<"\n"; return false; }
    }
    if (a.rate <= 0.0f) { std::cerr<<"rate must be > 0\n"; return false; }
    if (a.depth < 0.0f || a.depth > 1.0f) { std::cerr<<"depth must be [0..1]\n"; return false; }
    if (a.wet < 0.0f || a.wet > 1.0f) { std::cerr<<"wet must be [0..1]\n"; return false; }
    if (a.stereophase < 0.0f || a.stereophase > 180.0f) { std::cerr<<"stereophase must be [0..180]\n"; return false; }
    return true;
}

// Very small rate-sync parser: "bpm:120,div:1/8"
static bool parseRateSync(const std::string& s, float& bpm, std::string& div){
    if (s.empty()) return false;
    float tbpm = 0.f; std::string tdiv;
    size_t p1 = s.find("bpm:");
    size_t p2 = s.find(",div:");
    if (p1==std::string::npos || p2==std::string::npos || p2 <= p1) return false;
    try {
        tbpm = std::stof(s.substr(p1+4, p2-(p1+4)));
    } catch (...) { return false; }
    tdiv = s.substr(p2+5);
    bpm = tbpm; div = tdiv; return true;
}

static float divisionToHz(float bpm, const std::string& div){
    // cycles per second = (BPM/60) / beatsPerCycle
    // beatsPerCycle for common divisions:
    // 1 -> 4 beats (whole note), 1/2 -> 2 beats, 1/4 -> 1 beat, 1/8 -> 0.5, 1/16 -> 0.25
    std::map<std::string, float> beats = {
        {"1", 4.f}, {"1/2", 2.f}, {"1/4", 1.f}, {"1/8", 0.5f}, {"1/16", 0.25f}
    };
    auto it = beats.find(div);
    if (it==beats.end()) return -1.f;
    float beatsPerCycle = it->second;
    return (bpm/60.f) / beatsPerCycle;
}

int main(int argc, char** argv) {
    Args args;
    if (!parseArgs(argc, argv, args)) { print_help(); return 1; }

    // Input handling (auto-generate tiny test file if missing and using default path)
    {
        std::ifstream test(args.in, std::ios::binary);
        if (!test.good()) {
            std::cerr << "[info] Input file not found: " << args.in
                      << " -> generating a tiny test pad.\n";
            auto w = WavIO::makeTestPad(10.0f, 44100);
            // Ensure assets/ exists
            std::string dir = args.in.substr(0, args.in.find_last_of("/\\"));
            if (!dir.empty()) {
#if defined(_WIN32)
                std::string cmd = "mkdir \"" + dir + "\" >nul 2>nul";
#else
                std::string cmd = "mkdir -p \"" + dir + "\"";
#endif
                std::system(cmd.c_str());
            }
            if (!WavIO::write16(args.in, w)) {
                std::cerr << "Failed to write generated input to " << args.in << "\n";
                return 1;
            }
        }
    }

    // Load WAV
    WavData audio;
    try {
        if (!WavIO::read16(args.in, audio)) {
            std::cerr << "Failed to read input WAV.\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "WAV error: " << e.what() << "\n";
        return 1;
    }

    const double durationSec = double(audio.samples.size()) / double(audio.sampleRate * audio.channels);

    // Parse shape and stereo phase
    Tremolo trem;
    trem.setSampleRate(audio.sampleRate);
    trem.setDepth(args.depth);
    trem.setRateHz(args.rate);
    trem.setWet(args.wet);
    trem.setStereoPhaseDeg(args.stereophase);
    trem.setShape(Tremolo::parseShape(args.shape));

    // Optional: rate sync overrides rate
    if (!args.rateSync.empty()) {
        float bpm; std::string div;
        if (parseRateSync(args.rateSync, bpm, div)) {
            float hz = divisionToHz(bpm, div);
            if (hz > 0.f) {
                std::cerr << "[info] rate-sync: bpm=" << bpm << " div=" << div
                          << " -> rate=" << hz << " Hz\n";
                trem.setRateHz(hz);
            } else {
                std::cerr << "[warn] Unsupported division: " << div << " (ignored)\n";
            }
        } else {
            std::cerr << "[warn] Bad --rate-sync format. Expected bpm:120,div:1/8 (ignored)\n";
        }
    }

    // Log
    std::cout << "SmartTremolo\n";
    std::cout << "  Input          : " << args.in << "\n";
    std::cout << "  Output         : " << args.out << "\n";
    std::cout << "  SampleRate     : " << audio.sampleRate << "\n";
    std::cout << "  Channels       : " << audio.channels << "\n";
    std::cout << "  Duration       : " << durationSec << " s\n";
    std::cout << "  Params         : rate=" << args.rate
              << " depth=" << args.depth
              << " shape=" << args.shape
              << " stereophase=" << args.stereophase
              << " wet=" << args.wet << "\n";

    // Feature extractor & controller
    FeatureExtractor feat;
    NoOpController ctrl;
    double timeSec = 0.0;
    const double dt = 1.0 / double(audio.sampleRate);

    // DEMO: simulate a depth ramp between 5s..8s via smoother
    bool demoActive = args.demo;
    const double demoStart = 5.0;
    const double demoEnd   = 8.0;
    float baseDepth = args.depth;

    // Process in blocks (512 frames) with per-sample LFO inside Tremolo::process
    const size_t frames = audio.samples.size() / size_t(audio.channels);
    const size_t block = 512;
    std::vector<float> blockBuf;
    blockBuf.resize(block * size_t(audio.channels));

    size_t processed = 0;
    size_t lastSecMark = 0;
    double rmsAcc = 0.0;
    size_t rmsCount = 0;

    while (processed < frames) {
        size_t todo = std::min(block, frames - processed);
        // copy to block
        std::memcpy(blockBuf.data(),
                    audio.samples.data() + processed * audio.channels,
                    todo * audio.channels * sizeof(float));

        // features + controller (per-sample loop for features)
        for (size_t i=0;i<todo;++i){
            float L = (audio.channels==1) ? blockBuf[i] : blockBuf[i*2+0];
            float R = (audio.channels==1) ? blockBuf[i] : blockBuf[i*2+1];
            feat.pushSample(L, R);

            // When a frame is ready, call controller
            if (feat.ready()) {
                float rms = feat.rms();
                float zcr = feat.zcr();
                float rate = args.rate;
                float depth = args.depth;
                ctrl.update(timeSec, rms, zcr, rate, depth);
                trem.setRateHz(rate);
                trem.setDepth(depth);
                if (args.analyze) {
                    rmsAcc += rms;
                    rmsCount++;
                }
                feat.reset();
            }

            // DEMO scripted depth ramp
            if (demoActive && timeSec >= demoStart && timeSec <= demoEnd) {
                float t = float((timeSec - demoStart) / (demoEnd - demoStart)); // 0..1
                float d = baseDepth * (0.2f + 0.8f * t); // ramp from 20% to 100% of baseDepth
                trem.setDepth(d);
            } else if (demoActive && timeSec > demoEnd) {
                trem.setDepth(baseDepth);
            }

            timeSec += dt;
        }

        // Process tremolo in-place for this block
        trem.process(blockBuf.data(), todo, audio.channels);

        // write back
        std::memcpy(audio.samples.data() + processed * audio.channels,
                    blockBuf.data(),
                    todo * audio.channels * sizeof(float));

        // per-second analysis print
        size_t curSec = size_t(std::floor(timeSec));
        if (args.analyze && curSec != lastSecMark) {
            if (rmsCount > 0) {
                double meanRMS = rmsAcc / double(rmsCount);
                std::cout << "[analyze] t=" << lastSecMark
                          << "s.."<< curSec << "s, avg RMS=" << meanRMS
                          << " (ZCR shown when frames align)\n";
            }
            rmsAcc = 0.0; rmsCount = 0;
            lastSecMark = curSec;
        }

        processed += todo;
    }

    // Write output
    if (!WavIO::write16(args.out, audio)) {
        std::cerr << "Failed to write output WAV.\n";
        return 1;
    }

    std::cout << "Done. Stereo phase offset = " << args.stereophase << " deg.\n";
    // Example for future AI control:
    //   // Map loudness to deeper tremolo
    //   // ctrl.update(...) could set: depth = std::clamp(0.2f + 1.5f * rms, 0.0f, 1.0f);
    return 0;
}
