#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "tusb.h"
#include "pico/audio_i2s.h"
#include "SEGGER_RTT.h"

#include "ui/ui.h"
#include "voice_manager.h"

#define LOG(...) SEGGER_RTT_printf(0, __VA_ARGS__)

#define SAMPLE_RATE   44100
#define BUFFER_FRAMES 256
#define VOICES        8

// PCM5102A setup:
//   SCK - solder bridge close (SCK to GND)
//   H1L (FLT)  = L
//   H2L (DEMP) = L
//   H3L (XSMT) = H
//   H4L (FMT)  = L
#define I2S_PIN_DATA 2
#define I2S_PIN_CLK  3 // BCLK=3, LRCLK=4
#define LED_PIN      25

static VoiceManager<VOICES, SAMPLE_RATE> voice_manager;

static audio_buffer_pool_t* audio_init() {
    static audio_format_t fmt = {
        .sample_freq = SAMPLE_RATE,
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = 2,
    };
    static audio_buffer_format_t producer_fmt = {
        .format = &fmt,
        .sample_stride = 4, // 2 ch * 2 bytes
    };
    audio_buffer_pool_t* pool = audio_new_producer_pool(&producer_fmt, 3, BUFFER_FRAMES);

    static const audio_i2s_config_t i2s_cfg = {
        .data_pin = I2S_PIN_DATA,
        .clock_pin_base = I2S_PIN_CLK,
        .dma_channel = 0,
        .pio_sm = 0,
    };
    audio_i2s_setup(&fmt, &i2s_cfg);
    audio_i2s_connect(pool);
    audio_i2s_set_enabled(true);
    return pool;
}

void tud_midi_rx_cb(uint8_t itf) {
    (void)itf;
    uint8_t packet[4];
    while(tud_midi_available()) {
        tud_midi_packet_read(packet);
        uint8_t status = packet[1] & 0xF0;
        uint8_t note = packet[2];
        uint8_t vel = packet[3];

        if(status == 0x90 && vel > 0) { // Note On
            voice_manager.note_on(note, vel);
            LOG("Note On: %d (vel %d)\n", note, vel);
        } else if(status == 0x80 || (status == 0x90 && vel == 0)) { // Note Off
            voice_manager.note_off(note);
            LOG("Note Off: %d\n", note);
        }
    }
}

int main(void) {
    // somehow openocd fucks up the multicore reset
    // so we need to reset core1 manually
    sleep_ms(5);
    multicore_reset_core1();
    (void)multicore_fifo_pop_blocking();

    multicore_launch_core1(ui_thread);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    tusb_init();
    audio_buffer_pool_t* pool = audio_init();

    LOG("=== 2350 Vibe Synth ===\n");

    // Time budget per buffer in microseconds
    static const uint32_t BUDGET_US = BUFFER_FRAMES * 1000000u / SAMPLE_RATE;
    // Number of buffers per log interval (~1 sec)
    static const uint32_t LOG_INTERVAL = SAMPLE_RATE / BUFFER_FRAMES;

    uint32_t buf_count = 0;
    uint64_t load_acc_us = 0; // accumulated DSP time over the log interval

    while(true) {
        tud_task();

        audio_buffer_t* buf = take_audio_buffer(pool, false);
        if(!buf) continue;

        uint64_t t0 = time_us_64();

        int16_t* samples = (int16_t*)buf->buffer->bytes;
        for(uint32_t i = 0; i < buf->max_sample_count; i++) {
            int16_t val = (int16_t)(voice_manager.process() * 32767.0f);
            samples[i * 2 + 0] = val; // L
            samples[i * 2 + 1] = val; // R
        }

        load_acc_us += time_us_64() - t0;

        buf->sample_count = buf->max_sample_count;
        give_audio_buffer(pool, buf);

        gpio_put(LED_PIN, voice_manager.any_active());

        if(++buf_count >= LOG_INTERVAL) {
            // average DSP load over the interval in percent
            uint32_t avg_us = (uint32_t)(load_acc_us / LOG_INTERVAL);
            uint32_t load_pct = avg_us * 100 / BUDGET_US;
            LOG("CPU load: %d%% (avg %d us / budget %d us)\n", load_pct, avg_us, BUDGET_US);
            buf_count = 0;
            load_acc_us = 0;
        }
    }
}
