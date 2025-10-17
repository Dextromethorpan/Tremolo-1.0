#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "WavIO.h"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cmath>

static uint32_t read_u32(std::ifstream& f){ uint32_t v=0; f.read(reinterpret_cast<char*>(&v),4); return v; }
static uint16_t read_u16(std::ifstream& f){ uint16_t v=0; f.read(reinterpret_cast<char*>(&v),2); return v; }
static void write_u32(std::ofstream& f, uint32_t v){ f.write(reinterpret_cast<const char*>(&v),4); }
static void write_u16(std::ofstream& f, uint16_t v){ f.write(reinterpret_cast<const char*>(&v),2); }

bool WavIO::read16(const std::string& path, WavData& out) {
    std::ifstream f(path, std::ios::binary);
    if(!f.good()) return false;

    char riff[4]; f.read(riff,4);
    if(std::strncmp(riff,"RIFF",4)!=0) throw std::runtime_error("Not a RIFF file");
    (void)read_u32(f); // riff size
    char wave[4]; f.read(wave,4);
    if(std::strncmp(wave,"WAVE",4)!=0) throw std::runtime_error("Not a WAVE file");

    uint16_t audioFormat=0, numChannels=0, bitsPerSample=0;
    uint32_t sampleRate=0, byteRate=0;
    uint16_t blockAlign=0;
    uint32_t dataSize=0;
    std::streampos dataPos = 0;

    // iterate chunks
    while(f && !f.eof()){
        char id[4]; if(!f.read(id,4)) break;
        uint32_t sz = read_u32(f);
        if(std::strncmp(id,"fmt ",4)==0){
            audioFormat   = read_u16(f);
            numChannels   = read_u16(f);
            sampleRate    = read_u32(f);
            byteRate      = read_u32(f);
            blockAlign    = read_u16(f);
            bitsPerSample = read_u16(f);
            // skip any extra fmt bytes
            if (sz > 16) f.seekg(sz - 16, std::ios::cur);
        } else if(std::strncmp(id,"data",4)==0){
            dataSize = sz;
            dataPos = f.tellg();
            f.seekg(sz, std::ios::cur);
        } else {
            f.seekg(sz, std::ios::cur); // skip unknown
        }
        // pad byte if odd
        if (sz & 1) f.seekg(1, std::ios::cur);
    }

    if (audioFormat != 1) throw std::runtime_error("Unsupported WAV format (not PCM)");
    if (bitsPerSample != 16) throw std::runtime_error("Unsupported bits (need 16-bit)");
    if (numChannels < 1 || numChannels > 2) throw std::runtime_error("Unsupported channel count");

    // read audio data
    f.clear();
    f.seekg(dataPos);
    std::vector<int16_t> temp(dataSize / 2);
    f.read(reinterpret_cast<char*>(temp.data()), dataSize);

    out.sampleRate = int(sampleRate);
    out.channels = int(numChannels);
    out.samples.resize(temp.size());
    const float scale = 1.0f / 32768.0f;
    for (size_t i=0;i<temp.size();++i) {
        out.samples[i] = std::max(-1.0f, std::min(0.9999695f, temp[i] * scale));
    }
    return true;
}

bool WavIO::write16(const std::string& path, const WavData& in) {
    std::ofstream f(path, std::ios::binary);
    if(!f.good()) return false;

    const uint16_t channels = uint16_t(in.channels);
    const uint32_t sampleRate = uint32_t(in.sampleRate);
    const uint16_t bitsPerSample = 16;
    const uint16_t blockAlign = channels * bitsPerSample/8;
    const uint32_t byteRate = sampleRate * blockAlign;
    const uint32_t dataBytes = uint32_t(in.samples.size() * sizeof(int16_t));
    const uint32_t riffChunkSize = 36 + dataBytes;

    // RIFF header
    f.write("RIFF",4); write_u32(f, riffChunkSize); f.write("WAVE",4);
    // fmt chunk
    f.write("fmt ",4); write_u32(f, 16);
    write_u16(f, 1); // PCM
    write_u16(f, channels);
    write_u32(f, sampleRate);
    write_u32(f, byteRate);
    write_u16(f, blockAlign);
    write_u16(f, bitsPerSample);
    // data chunk
    f.write("data",4); write_u32(f, dataBytes);

    // samples
    for (float x : in.samples) {
        float c = std::max(-1.0f, std::min(1.0f, x));
        int16_t s = (int16_t)std::lround(c * 32767.0f);
        f.write(reinterpret_cast<const char*>(&s), sizeof(int16_t));
    }
    return true;
}

WavData WavIO::makeTestPad(float seconds, int sr) {
    WavData w;
    w.sampleRate = sr;
    w.channels = 2;
    const int frames = std::max(1, int(seconds * sr));
    w.samples.resize(size_t(frames) * 2);
    float fL = 220.0f, fR = 330.0f; // a gentle dyad
    for (int i=0;i<frames;++i){
        float t = float(i) / float(sr);
        float env = 0.5f * (1.0f - std::cos(2.0f*float(M_PI)*std::min(t/seconds,1.0f))); // slow fade-in/out-ish
        float l = 0.2f * env * std::sin(2.0f*float(M_PI)*fL*t);
        float r = 0.2f * env * std::sin(2.0f*float(M_PI)*fR*t);
        w.samples[size_t(i)*2+0] = l;
        w.samples[size_t(i)*2+1] = r;
    }
    return w;
}
