#pragma once
#include "hardware/gpio.h"

template <uint32_t pin>
class Button {
public:
    Button() {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_set_pulls(pin, false, false);
    }

    void poll() {
        prev_state = current_state;
        current_state = !gpio_get(pin);
    }

    bool is_pressed() {
        return current_state && !prev_state;
    }

private:
    bool prev_state = false;
    bool current_state = false;
};
