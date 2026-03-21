#pragma once

#include "daisysp.h"
#include "synth_params.h"
#include <cmath>

template <int SAMPLE_RATE, int CTRL_DIV = 16>
class Synth {
public:
    static constexpr int CTRL_RATE = SAMPLE_RATE / CTRL_DIV;

    Synth() {
        osc_.Init(SAMPLE_RATE);
        osc_.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SQUARE);
        osc_.SetAmp(1.0f);

        // LFO and envelopes run at control rate
        // lfo_.Init(CTRL_RATE);
        // lfo_.SetWaveform(daisysp::Oscillator::WAVE_TRI);
        // lfo_.SetFreq(0.4f);
        // lfo_.SetAmp(1.0f);

        env_.Init(CTRL_RATE);
        env_.SetAttackTime(0.01f);
        env_.SetDecayTime(0.1f);
        env_.SetSustainLevel(0.7f);
        env_.SetReleaseTime(0.3f);

        fenv_.Init(CTRL_RATE);
        fenv_.SetAttackTime(0.1f);
        fenv_.SetDecayTime(0.1f);
        fenv_.SetSustainLevel(0.7f);
        fenv_.SetReleaseTime(0.5f);

        svf_.Init(SAMPLE_RATE);
        svf_.SetRes(0.8f);
        svf_.SetDrive(1.0f);

        od_.Init();
        od_.SetDrive(0.3f);
    }

    void note_on(int note, int vel) {
        (void)vel;
        midi_note_ = (float)note;
        gate_ = true;
    }

    void note_off() {
        gate_ = false;
    }

    // Returns true when envelope has fully decayed (voice can be reused)
    bool is_silent() const {
        return (amplitude_ < 0.001f) && gate_ == false;
    }

    // Process one sample, returns value in -1..1
    float process() {
        if(ctrl_counter_ == 0) {
            osc_.SetWaveform((uint8_t)g_synth_params.waveform);
            osc_.SetFreq(daisysp::mtof(midi_note_ + g_synth_params.base_note - 60.0f));

            env_.SetAttackTime(g_synth_params.env_attack);
            env_.SetDecayTime(g_synth_params.env_decay);
            env_.SetSustainLevel(g_synth_params.env_sustain);
            env_.SetReleaseTime(g_synth_params.env_release);

            fenv_.SetAttackTime(g_synth_params.fenv_attack);
            fenv_.SetDecayTime(g_synth_params.fenv_decay);
            fenv_.SetSustainLevel(g_synth_params.fenv_sustain);
            fenv_.SetReleaseTime(g_synth_params.fenv_release);

            // LFO -1..1 -> pw 0.15..0.85 (avoid artifacts near the edges)
            // pw_ = 0.5f + lfo_.Process() * 0.35f;
            pw_ = 0.5f;

            float fenv_val = fenv_.Process(gate_);
            svf_.SetFreq(g_synth_params.filter_cutoff * (1.0f + fenv_val * 4.0f));
            svf_.SetRes(g_synth_params.filter_res);
            svf_.SetDrive(g_synth_params.filter_drive);

            od_.SetDrive(g_synth_params.overdrive);

            amplitude_ = env_.Process(gate_);
        }
        if(++ctrl_counter_ >= CTRL_DIV) ctrl_counter_ = 0;

        osc_.SetPw(pw_);
        svf_.Process(osc_.Process() * amplitude_);
        return od_.Process(svf_.Low());
    }

private:
    daisysp::Oscillator osc_;
    // daisysp::Oscillator lfo_;
    daisysp::Adsr env_;
    daisysp::Adsr fenv_; // filter envelope
    daisysp::Svf svf_;
    daisysp::Overdrive od_;

    bool gate_ = false;
    float midi_note_ = 69.0f; // A4

    // Cached control-rate values
    int ctrl_counter_ = 0;
    float pw_ = 0.5f;
    float amplitude_ = 0.0f;
};
