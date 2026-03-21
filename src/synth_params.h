#pragma once
#include <stdint.h>
#include "daisysp.h"

// Shared parameters written by UI (core1) and read by DSP (core0).
// All fields are volatile; 32-bit writes on ARM Cortex-M33 are atomic.
struct SynthParams {
    volatile int32_t waveform = daisysp::Oscillator::WAVE_POLYBLEP_SQUARE;

    volatile float env_attack  = 0.01f;
    volatile float env_decay   = 0.10f;
    volatile float env_sustain = 0.70f;
    volatile float env_release = 0.30f;

    volatile float fenv_attack  = 0.10f;
    volatile float fenv_decay   = 0.10f;
    volatile float fenv_sustain = 0.70f;
    volatile float fenv_release = 0.50f;

    volatile float base_note     = 60.0f; // MIDI note offset; 60 = no transpose
    volatile float filter_cutoff = 1000.0f;
    volatile float filter_res    = 0.80f;
    volatile float filter_drive  = 1.00f;
    volatile float overdrive     = 0.30f;
};

extern SynthParams g_synth_params;
