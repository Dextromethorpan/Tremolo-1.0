#pragma once

// ------------------------------------------------------------
// Controller Interface
// ------------------------------------------------------------
// This interface defines how an external controller (e.g. an AI agent)
// can modify real-time tremolo parameters based on extracted audio features.
//
// The Tremolo effect calls Controller::update() periodically (once every
// analysis frame, e.g. every 1024 samples). You can implement this interface
// to make parameters like rateHz or depth adapt dynamically.
//
// Example use case:
// - Make tremolo depth follow RMS loudness
// - Make rate increase with Zero-Crossing Rate (ZCR)
// - Connect a neural network or rule-based system later
// ------------------------------------------------------------

struct Controller {
    virtual ~Controller() = default;

    // Called periodically by main processing loop.
    //
    // Parameters:
    //   timeSeconds : current time in seconds since processing start
    //   rms         : root-mean-square loudness of last frame
    //   zcr         : zero-crossing rate of last frame
    //   rateHz      : (in/out) current LFO rate parameter; modify to change it
    //   depth       : (in/out) current tremolo depth parameter; modify to change it
    //
    // Notes:
    // - All parameters are per-channel global values (no per-channel control yet)
    // - The function should be lightweight (no blocking or heavy allocations)
    // - You can implement your own derived controller to add intelligence
    virtual void update(double timeSeconds,
                        float rms,
                        float zcr,
                        float& rateHz,
                        float& depth) = 0;
};
