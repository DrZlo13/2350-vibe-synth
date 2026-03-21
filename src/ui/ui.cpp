#include "pico/stdlib.h"
#include <cstdio>
#include "display.hpp"
#include "pwm.hpp"
#include "ui.h"
#include "display.h"
#include "button.hpp"
#include "quadrature_encoder.hpp"
#include "render/render.h"
#include "daisysp.h"
#include "log.h"
#include "encoder_accel.hpp"
#include "../synth_params.h"

#define D_PIN_SCL 6
#define D_PIN_SDA 7
#define D_PIN_BL  8
#define D_PIN_CS  9
#define D_PIN_DC  10
#define D_PIN_RST 11

#define ENCODER_PIN_A   12
#define ENCODER_PIN_B   13
#define ENCODER_PIN_BTN 14

typedef struct {
    const char* name;
    const int32_t value;
} ListItem;

constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((Color)(r >> 3) << 11) | ((Color)(g >> 2) << 5) | (b >> 3);
}

const size_t parameter_width = 36;
const size_t parameter_height = 36;

extern float volatile dsp_load;

class Parameter {
public:
    // List mode: discrete items (wave selection, ADSR, etc.)
    using Callback = void (*)(float);
    using Formatter = void (*)(float value, char* buf, size_t size);

    Parameter(const char* name, const ListItem* items, size_t count, size_t default_index, Callback on_change = nullptr)
        : name(name)
        , mode(MODE_LIST)
        , show_value_timer(0)
        , inbetween_value(0.0f)
        , on_change(on_change)
        , formatter(nullptr) {
        list.items = items;
        list.count = count;
        list.index = default_index;
        cont = {};
        if(on_change) on_change(get_float_value());
    }

    // Continuous mode: float range with step.
    // formatter: optional display function; nullptr = note name (e.g. "C4+25")
    Parameter(const char* name, float min_val, float max_val, float step, float default_val, Formatter formatter = nullptr, Callback on_change = nullptr)
        : name(name)
        , mode(MODE_CONTINUOUS)
        , show_value_timer(0)
        , inbetween_value(0.0f)
        , on_change(on_change)
        , formatter(formatter) {
        list = {};
        cont.min_val = min_val;
        cont.max_val = max_val;
        cont.step = step;
        cont.value = default_val;
        if(on_change) on_change(get_float_value());
    }

    int32_t get_int_value() const {
        if(mode == MODE_LIST) return list.items[list.index].value;
        return (int32_t)cont.value;
    }

    float get_float_value() const {
        if(mode == MODE_LIST) return (float)list.items[list.index].value;
        return cont.value;
    }

    void increase(float amount) {
        Log::info("Increase %s by %d.%02d\n", name, (int)amount, (int)(amount * 100) % 100);
        if(mode == MODE_LIST) {
            inbetween_value += amount;
            if(inbetween_value >= 1.0f) {
                inbetween_value = 0.0f;
                if(list.index < list.count - 1) list.index++;
            }
        } else {
            cont.value += amount * cont.step;
            if(cont.value > cont.max_val) cont.value = cont.max_val;
        }
        if(on_change) on_change(get_float_value());
        show_value_timer = 120;
    }

    void decrease(float amount) {
        if(mode == MODE_LIST) {
            inbetween_value -= amount;
            if(inbetween_value <= -1.0f) {
                inbetween_value = 0.0f;
                if(list.index > 0) list.index--;
            }
        } else {
            cont.value -= amount * cont.step;
            if(cont.value < cont.min_val) cont.value = cont.min_val;
        }
        if(on_change) on_change(get_float_value());
        show_value_timer = 120;
    }

    void render(int32_t center_x, int32_t center_y, Color border_color) {
        const Color color = 0xFFFF;
        const Font font_id = FontBody;

        render_border(center_x - parameter_width / 2, center_y - parameter_height / 2, parameter_width, parameter_height, 3, 3, 3, 3, 1, 1, 1, 1, border_color);

        if(show_value_timer > 0) {
            show_value_timer--;
            render_text_aligned(center_x, center_y + 14, Center, Center, current_value_str(), font_id, color);
        } else {
            render_text_aligned(center_x, center_y + 14, Center, Center, name, font_id, color);
        }

        // track bar with position notch
        const int32_t bar_x = center_x - 14;
        const int32_t bar_y = center_y - 4;
        const int32_t bar_w = 28;
        render_rectangle(bar_x, bar_y, bar_w, 1, 0, 0, 0, 0, rgb(80, 80, 80));

        float pos = 0.0f;
        if(mode == MODE_LIST && list.count > 1) {
            pos = (float)list.index / (float)(list.count - 1);
        } else if(mode == MODE_CONTINUOUS) {
            pos = (cont.value - cont.min_val) / (cont.max_val - cont.min_val);
        }
        int32_t tick_x = bar_x + (int32_t)(pos * (float)bar_w);
        render_rectangle(tick_x - 1, bar_y - 3, 2, 7, 0, 0, 0, 0, color);
    }

private:
    const char* current_value_str() {
        if(mode == MODE_LIST) return list.items[list.index].name;

        formatter(cont.value, value_buf, sizeof(value_buf));
        return value_buf;
    }

    enum Mode {
        MODE_LIST,
        MODE_CONTINUOUS
    };

    const char* name;
    Mode mode;
    int32_t show_value_timer;
    float inbetween_value;
    Callback on_change;
    Formatter formatter;
    char value_buf[16];

    struct {
        const ListItem* items;
        size_t count;
        size_t index;
    } list;

    struct {
        float min_val;
        float max_val;
        float step;
        float value;
    } cont;
};

const ListItem float_items[] = {
    {"0.00", 0},  {"0.01", 1},   {"0.02", 2},  {"0.03", 3},  {"0.04", 4},  {"0.05", 5},  {"0.06", 6},  {"0.07", 7},  {"0.08", 8},  {"0.09", 9},  {"0.10", 10},
    {"0.11", 11}, {"0.12", 12},  {"0.13", 13}, {"0.14", 14}, {"0.15", 15}, {"0.16", 16}, {"0.17", 17}, {"0.18", 18}, {"0.19", 19}, {"0.20", 20}, {"0.21", 21},
    {"0.22", 22}, {"0.23", 23},  {"0.24", 24}, {"0.25", 25}, {"0.26", 26}, {"0.27", 27}, {"0.28", 28}, {"0.29", 29}, {"0.30", 30}, {"0.31", 31}, {"0.32", 32},
    {"0.33", 33}, {"0.34", 34},  {"0.35", 35}, {"0.36", 36}, {"0.37", 37}, {"0.38", 38}, {"0.39", 39}, {"0.40", 40}, {"0.41", 41}, {"0.42", 42}, {"0.43", 43},
    {"0.44", 44}, {"0.45", 45},  {"0.46", 46}, {"0.47", 47}, {"0.48", 48}, {"0.49", 49}, {"0.50", 50}, {"0.51", 51}, {"0.52", 52}, {"0.53", 53}, {"0.54", 54},
    {"0.55", 55}, {"0.56", 56},  {"0.57", 57}, {"0.58", 58}, {"0.59", 59}, {"0.60", 60}, {"0.61", 61}, {"0.62", 62}, {"0.63", 63}, {"0.64", 64}, {"0.65", 65},
    {"0.66", 66}, {"0.67", 67},  {"0.68", 68}, {"0.69", 69}, {"0.70", 70}, {"0.71", 71}, {"0.72", 72}, {"0.73", 73}, {"0.74", 74}, {"0.75", 75}, {"0.76", 76},
    {"0.77", 77}, {"0.78", 78},  {"0.79", 79}, {"0.80", 80}, {"0.81", 81}, {"0.82", 82}, {"0.83", 83}, {"0.84", 84}, {"0.85", 85}, {"0.86", 86}, {"0.87", 87},
    {"0.88", 88}, {"0.89", 89},  {"0.90", 90}, {"0.91", 91}, {"0.92", 92}, {"0.93", 93}, {"0.94", 94}, {"0.95", 95}, {"0.96", 96}, {"0.97", 97}, {"0.98", 98},
    {"0.99", 99}, {"1.00", 100},
};

const ListItem wave_items[] = {
    {"Sine", daisysp::Oscillator::WAVE_SIN},
    {"Tri", daisysp::Oscillator::WAVE_POLYBLEP_TRI},
    {"Saw", daisysp::Oscillator::WAVE_POLYBLEP_SAW},
    {"Sqr", daisysp::Oscillator::WAVE_POLYBLEP_SQUARE},
};

static void fmt_note(float v, char* buf, size_t n) {
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int midi = (int)v;
    int cents = (int)((v - (float)midi) * 100.0f + 0.5f);
    int octave = midi / 12 - 1;
    int semi = midi % 12;
    if(cents == 0)
        snprintf(buf, n, "%s%d", names[semi], octave);
    else
        snprintf(buf, n, "%s%d+%02d", names[semi], octave, cents);
}

static void fmt_hz(float v, char* buf, size_t n) {
    snprintf(buf, n, "%dHz", (int)v);
}

// Note: C-1 (MIDI 0) to C9+99c (MIDI 120.99), step 0.01 (~1 cent), default C4 (MIDI 60)
// That covers -5 to +5 octaves from C4. 100 encoder ticks per semitone.
Parameter parameters[] = {
    Parameter("Wave", wave_items, sizeof(wave_items) / sizeof(wave_items[0]), 3, [](float v) { g_synth_params.waveform = (int32_t)v; }),
    Parameter("BNote", 0.0f, 120.99f, 0.01f, 60.0f, fmt_note, [](float v) { g_synth_params.base_note = v; }),
    Parameter("Atk", float_items, sizeof(float_items) / sizeof(float_items[0]), 1, [](float v) { g_synth_params.env_attack = v / 100.0f; }),
    Parameter("Dec", float_items, sizeof(float_items) / sizeof(float_items[0]), 10, [](float v) { g_synth_params.env_decay = v / 100.0f; }),
    Parameter("Sus", float_items, sizeof(float_items) / sizeof(float_items[0]), 70, [](float v) { g_synth_params.env_sustain = v / 100.0f; }),
    Parameter("Rel", float_items, sizeof(float_items) / sizeof(float_items[0]), 30, [](float v) { g_synth_params.env_release = v / 100.0f; }),
    Parameter("FAtk", float_items, sizeof(float_items) / sizeof(float_items[0]), 10, [](float v) { g_synth_params.fenv_attack = v / 100.0f; }),
    Parameter("FDec", float_items, sizeof(float_items) / sizeof(float_items[0]), 10, [](float v) { g_synth_params.fenv_decay = v / 100.0f; }),
    Parameter("FSus", float_items, sizeof(float_items) / sizeof(float_items[0]), 70, [](float v) { g_synth_params.fenv_sustain = v / 100.0f; }),
    Parameter("FRel", float_items, sizeof(float_items) / sizeof(float_items[0]), 50, [](float v) { g_synth_params.fenv_release = v / 100.0f; }),
    Parameter("FCut", 20.0f, 6000.0f, 10.0f, 1000.0f, fmt_hz, [](float v) { g_synth_params.filter_cutoff = v; }),
    Parameter("FRes", float_items, sizeof(float_items) / sizeof(float_items[0]), 80, [](float v) { g_synth_params.filter_res = v / 100.0f; }),
    Parameter("FDrv", float_items, sizeof(float_items) / sizeof(float_items[0]), 100, [](float v) { g_synth_params.filter_drive = v / 100.0f; }),
    Parameter("Over", float_items, sizeof(float_items) / sizeof(float_items[0]), 30, [](float v) { g_synth_params.overdrive = v / 100.0f; }),
};

void ui_thread(void) {
    RenderBuffer* buffer = render_alloc_buffer();
    render_set_current_buffer(buffer);

    Display<D_PIN_BL, D_PIN_RST, D_PIN_CS, D_PIN_SCL, D_PIN_SDA, D_PIN_DC, D_OFF_X, D_OFF_Y, D_WIDTH, D_HEIGHT> display;
    display.backlight(1.0f);
    display.init();

    QuadratureEncoder<ENCODER_PIN_A, ENCODER_PIN_B> encoder;
    Button<ENCODER_PIN_BTN> encoder_button;

    const size_t parameter_count = sizeof(parameters) / sizeof(parameters[0]);
    size_t current_parameter = 0;
    bool editing_parameter = false;
    EncoderAccel encoder_accel(400.0f, 100.0f);

    while(true) {
        const int32_t x_start = 0;
        const int32_t y_start = 0;

        render_clear_buffer(0x0000);
        encoder_button.poll();

        if(encoder_button.is_pressed()) {
            editing_parameter = !editing_parameter;
        }

        int32_t delta = encoder.get_delta();
        if(editing_parameter) {
            if(delta != 0) {
                // Velocity-based acceleration: measure time between encoder ticks.
                // Fast spin (small dt) -> large multiplier; slow spin -> multiplier=1.
                //   dt >= 80ms -> x1  (slow, precise)
                //   dt =  20ms -> x4
                //   dt =  5ms  -> x16 (fast)
                // max capped at 32x
                float effective = encoder_accel.apply(delta);
                if(effective > 0.0f) {
                    parameters[current_parameter].increase(effective);
                } else {
                    parameters[current_parameter].decrease(-effective);
                }
            }
        } else {
            if(delta > 0) {
                current_parameter = (current_parameter + delta) % parameter_count;
            } else if(delta < 0) {
                current_parameter = (current_parameter + parameter_count - (-delta % parameter_count)) % parameter_count;
            }
        }

        for(size_t i = 0; i < parameter_count; i++) {
            int32_t col = (int32_t)(i / 2);
            int32_t row = (int32_t)(i % 2);
            int32_t cx = x_start + col * (int32_t)(parameter_width + 2) + (int32_t)(parameter_width / 2);
            int32_t cy = y_start + 1 + row * (int32_t)(parameter_height + 2) + (int32_t)(parameter_height / 2);

            Color border_color = rgb(0, 0, 0);
            if(i == current_parameter) {
                if(editing_parameter) {
                    border_color = rgb(255, 0, 0);
                } else {
                    border_color = rgb(255, 166, 0);
                }
            }

            parameters[i].render(cx, cy, border_color);
        }

        {
            float load = dsp_load;
            int32_t bar_width = (int32_t)(load * D_WIDTH);
            if(bar_width > D_WIDTH) bar_width = D_WIDTH;
            Color bar_color = load < 1.0f ? rgb(0, 48, 0) : rgb(96, 0, 0);
            render_rectangle(0, D_HEIGHT - 1, bar_width, 1, 0, 0, 0, 0, bar_color);
        }

        display.write_buffer((uint8_t*)render_get_buffer_data(buffer));
    }
}
