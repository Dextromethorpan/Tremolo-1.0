# SmartTremolo

A tiny, dependency-free C++17 console app that applies a **tremolo** effect to a WAV file.  
No JUCE, no external libraries. Includes a minimal WAV reader/writer (PCM 16-bit mono/stereo).

> **Tremolo** is periodic amplitude modulation. Parameters:
> - **rateHz**: LFO frequency in Hz
> - **depth**: modulation amount [0..1]
> - **shape**: `sine`, `triangle`, `square`, optionally `square-soft`
> - **stereophase**: phase offset (deg) for right channel [0..180]
> - **wet**: wet/dry mix [0..1]
