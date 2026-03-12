#pragma once

#include "hardware/pio.h"
#include "hardware/timer.h"
#include "quadrature_encoder.pio.h"

template <uint32_t pin_a, uint32_t pin_b, bool invert_direction = false, size_t click_per_detent = 4>
class QuadratureEncoder {
public:
    QuadratureEncoder() {
        uint pin_ab = pin_a;
        if(pin_ab > pin_b) {
            pin_ab = pin_b;
            invert = !invert;
        }

        pio = pio0;
        sm = pio_claim_unused_sm(pio, false);
        hard_assert(sm >= 0);
        pio_set_gpio_base(pio, 16); // all encoders happily use GPIO 16+
        pio_add_program(pio, &quadrature_encoder_program);
        quadrature_encoder_program_init(pio, sm, pin_ab, 0);
    }

    int32_t get_delta() {
        int32_t count = quadrature_encoder_get_count(pio, sm);
        int32_t delta = (count - value) / (int32_t)click_per_detent;
        if(delta == 0) {
            return 0;
        }
        value = count;

        return delta * (invert ? -1 : 1);
    }

private:
    bool invert = invert_direction;
    int32_t value = 0;

    PIO pio;
    uint sm;
};
