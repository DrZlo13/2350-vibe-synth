#include "pico/stdlib.h"
#include "display.hpp"
#include "pwm.hpp"
#include "ui.h"

#define D_PIN_SCL   6
#define D_PIN_SDA   7
#define D_PIN_BL    8
#define D_PIN_CS    9
#define D_PIN_WR    10
#define D_PIN_RESET 11

#define D_WIDTH  76
#define D_HEIGHT 284
#define D_OFF_X  82
#define D_OFF_Y  18

void ui_thread(void) {
    Display<D_PIN_BL, D_PIN_RESET, D_PIN_CS, D_PIN_SCL, D_PIN_SDA, D_PIN_WR, D_OFF_X, D_OFF_Y, D_WIDTH, D_HEIGHT> display;
    display.backlight(0.0f);
    display.init();
    while(true) {
        uint16_t color_red = 0xF800;
        uint16_t color_green = 0x07E0;
        uint16_t color_blue = 0x001F;

        display.fill(color_red);
        sleep_ms(200);
        display.fill(color_green);
        sleep_ms(200);
        display.fill(color_blue);
        sleep_ms(200);
    }
}
