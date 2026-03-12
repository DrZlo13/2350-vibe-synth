#include "pico/stdlib.h"
#include "display.hpp"
#include "pwm.hpp"
#include "ui.h"
#include "display.h"
#include "button.hpp"
#include "quadrature_encoder.hpp"
#include "render/render.h"
#include "daisysp.h"

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

typedef struct {
    const ListItem* items;
    const size_t count;
    const size_t default_index;
    size_t index;
} ParameterList;

constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((Color)(r >> 3) << 11) | ((Color)(g >> 2) << 5) | (b >> 3);
}

const size_t parameter_width = 36;
const size_t parameter_height = 36;

extern float volatile dsp_load;

class Parameter {
public:
    Parameter(const char* name, const ListItem* items, size_t count, size_t default_index)
        : name(name)
        , list{items, count, default_index, default_index} {
        inbetween_value = 0;
    }

    void increase(float amount) {
        inbetween_value += amount;
        if(inbetween_value >= 1.0f) {
            inbetween_value = 0.0f;
            if(list.index < list.count - 1) {
                list.index = list.index + 1;
            }
        }

        show_value_timer = 120;
    }

    void decrease(float amount) {
        inbetween_value -= amount;
        if(inbetween_value <= -1.0f) {
            inbetween_value = 0.0f;
            if(list.index > 0) {
                list.index = list.index - 1;
            }
        }

        show_value_timer = 120;
    }

    void render(int32_t center_x, int32_t center_y, Color border_color) {
        const size_t str_buffer_size = 32;
        const Color color = 0xFFFF;
        const Font font_id = FontBody;

        render_border(center_x - parameter_width / 2, center_y - parameter_height / 2, parameter_width, parameter_height, 3, 3, 3, 3, 1, 1, 1, 1, border_color);

        if(show_value_timer > 0) {
            show_value_timer--;
            render_text_aligned(center_x, center_y + 14, Center, Center, list.items[list.index].name, font_id, color);
        } else {
            render_text_aligned(center_x, center_y + 14, Center, Center, name, font_id, color);
        }

        // track bar with index notch
        const int32_t bar_x = center_x - 14;
        const int32_t bar_y = center_y - 4;
        const int32_t bar_w = 28;
        render_rectangle(bar_x, bar_y, bar_w, 1, 0, 0, 0, 0, rgb(80, 80, 80));
        if(list.count > 1) {
            int32_t tick_x = bar_x + (int32_t)(list.index * bar_w / (list.count - 1));
            render_rectangle(tick_x - 1, bar_y - 3, 2, 7, 0, 0, 0, 0, color);
        }
    }

private:
    const char* name;
    ParameterList list;
    int32_t show_value_timer;
    float inbetween_value;
};

// TODO: we need +-5 octaves with 0.01 step between notes, like in Digitakt
const ListItem note_items[] = {
    {"C0", 12}, {"C#0", 13}, {"D0", 14}, {"D#0", 15}, {"E0", 16},  {"F0", 17},  {"F#0", 18},  {"G0", 19},  {"G#0", 20},  {"A0", 21},  {"A#0", 22},  {"B0", 23},
    {"C1", 24}, {"C#1", 25}, {"D1", 26}, {"D#1", 27}, {"E1", 28},  {"F1", 29},  {"F#1", 30},  {"G1", 31},  {"G#1", 32},  {"A1", 33},  {"A#1", 34},  {"B1", 35},
    {"C2", 36}, {"C#2", 37}, {"D2", 38}, {"D#2", 39}, {"E2", 40},  {"F2", 41},  {"F#2", 42},  {"G2", 43},  {"G#2", 44},  {"A2", 45},  {"A#2", 46},  {"B2", 47},
    {"C3", 48}, {"C#3", 49}, {"D3", 50}, {"D#3", 51}, {"E3", 52},  {"F3", 53},  {"F#3", 54},  {"G3", 55},  {"G#3", 56},  {"A3", 57},  {"A#3", 58},  {"B3", 59},
    {"C4", 60}, {"C#4", 61}, {"D4", 62}, {"D#4", 63}, {"E4", 64},  {"F4", 65},  {"F#4", 66},  {"G4", 67},  {"G#4", 68},  {"A4", 69},  {"A#4", 70},  {"B4", 71},
    {"C5", 72}, {"C#5", 73}, {"D5", 74}, {"D#5", 75}, {"E5", 76},  {"F5", 77},  {"F#5", 78},  {"G5", 79},  {"G#5", 80},  {"A5", 81},  {"A#5", 82},  {"B5", 83},
    {"C6", 84}, {"C#6", 85}, {"D6", 86}, {"D#6", 87}, {"E6", 88},  {"F6", 89},  {"F#6", 90},  {"G6", 91},  {"G#6", 92},  {"A6", 93},  {"A#6", 94},  {"B6", 95},
    {"C7", 96}, {"C#7", 97}, {"D7", 98}, {"D#7", 99}, {"E7", 100}, {"F7", 101}, {"F#7", 102}, {"G7", 103}, {"G#7", 104}, {"A7", 105}, {"A#7", 106}, {"B7", 107},
};

const ListItem adsr_items[] = {
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

Parameter parameters[] = {
    Parameter("Wave", wave_items, sizeof(wave_items) / sizeof(wave_items[0]), 0),
    Parameter("Note", note_items, sizeof(note_items) / sizeof(note_items[0]), 48),
    Parameter("Atk", adsr_items, sizeof(adsr_items) / sizeof(adsr_items[0]), 0),
    Parameter("Dec", adsr_items, sizeof(adsr_items) / sizeof(adsr_items[0]), 0),
    Parameter("Sus", adsr_items, sizeof(adsr_items) / sizeof(adsr_items[0]), 100),
    Parameter("Rel", adsr_items, sizeof(adsr_items) / sizeof(adsr_items[0]), 0),
};

void ui_thread(void) {
    RenderBuffer* buffer = render_alloc_buffer();
    render_set_current_buffer(buffer);

    Display<D_PIN_BL, D_PIN_RST, D_PIN_CS, D_PIN_SCL, D_PIN_SDA, D_PIN_DC, D_OFF_X, D_OFF_Y, D_WIDTH, D_HEIGHT> display;
    display.backlight(0.5f);
    display.init();

    QuadratureEncoder<ENCODER_PIN_A, ENCODER_PIN_B> encoder;
    Button<ENCODER_PIN_BTN> encoder_button;

    const size_t parameter_count = sizeof(parameters) / sizeof(parameters[0]);
    size_t current_parameter = 0;
    bool editing_parameter = false;

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
            if(delta > 0) {
                parameters[current_parameter].increase((float)delta);
            } else if(delta < 0) {
                parameters[current_parameter].decrease((float)(-delta));
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
