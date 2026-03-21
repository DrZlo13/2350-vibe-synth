#pragma once
#include "pico/stdlib.h"

// Velocity-based encoder acceleration.
// Measures time between ticks: fast spin -> large multiplier, slow spin -> x1.
// Usage: call apply(delta) on each non-zero encoder delta to get the scaled value.
class EncoderAccel {
public:
    EncoderAccel(float base_dt_ms, float max_accel)
        : base_dt_us(base_dt_ms * 1000.0f)
        , max_accel(max_accel)
        , last_tick_us(0) {
    }

    float apply(int32_t delta) {
        uint64_t now = time_us_64();
        uint64_t dt_us = now - last_tick_us;
        last_tick_us = now;

        float accel = base_dt_us / (float)(dt_us < 1 ? 1 : dt_us);
        if(accel < 1.0f) accel = 1.0f;
        if(accel > max_accel) accel = max_accel;

        return (float)delta * accel;
    }

private:
    float base_dt_us;
    float max_accel;
    uint64_t last_tick_us;
};
