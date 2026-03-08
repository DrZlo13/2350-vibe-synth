#pragma once

#include "daisysp.h"

class Synth {
public:
    // ctrl_div: LFO, envelopes and filter cutoff run at sample_rate / ctrl_div
    void init(int sample_rate, int ctrl_div);

    void note_on(int note, int vel);
    void note_off();

    // Process one sample, returns value in -1..1
    float process();

private:
    daisysp::Oscillator osc_;
    daisysp::Oscillator lfo_;
    daisysp::Adsr env_;
    daisysp::Adsr fenv_; // filter envelope
    daisysp::Svf svf_;

    bool gate_ = false;
    float freq_ = 440.0f;

    // Cached control-rate values
    int ctrl_div_ = 16;
    int ctrl_counter_ = 0;
    float pw_ = 0.5f;
    float amplitude_ = 0.0f;
};
