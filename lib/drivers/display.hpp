#pragma once

#include "hardware/spi.h"
#include "pwm.hpp"
#include "pico/stdlib.h"

template <
    uint32_t pin_backlight,
    uint32_t pin_reset,
    uint32_t pin_cs,
    uint32_t pin_clock,
    uint32_t pin_data,
    uint32_t pin_dc,
    size_t offset_x,
    size_t offset_y,
    size_t width,
    size_t height>
class Display {
public:
    void backlight(float value) {
        backlight_pwm.pwm(value);
    }

    void init(void) {
        uint freq = spi_init(spi, 1 * 1000 * 1000);
        spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        const enum gpio_slew_rate spi_slew_rate = GPIO_SLEW_RATE_FAST;
        const enum gpio_drive_strength spi_drive_strength = GPIO_DRIVE_STRENGTH_12MA;

        gpio_set_function(pin_clock, GPIO_FUNC_SPI);
        gpio_set_function(pin_data, GPIO_FUNC_SPI);

        gpio_set_drive_strength(pin_clock, spi_drive_strength);
        gpio_set_slew_rate(pin_clock, spi_slew_rate);

        gpio_set_drive_strength(pin_data, spi_drive_strength);
        gpio_set_slew_rate(pin_data, spi_slew_rate);

        gpio_init(pin_cs);
        gpio_set_dir(pin_cs, GPIO_OUT);
        gpio_set_drive_strength(pin_cs, spi_drive_strength);
        gpio_set_slew_rate(pin_cs, spi_slew_rate);

        gpio_init(pin_dc);
        gpio_set_dir(pin_dc, GPIO_OUT);
        gpio_set_drive_strength(pin_dc, spi_drive_strength);
        gpio_set_slew_rate(pin_dc, spi_slew_rate);

        gpio_init(pin_reset);
        gpio_set_dir(pin_reset, GPIO_OUT);
        gpio_set_drive_strength(pin_reset, spi_drive_strength);
        gpio_set_slew_rate(pin_reset, spi_slew_rate);

        cs(true);
        dc(false);

        // reset sequence
        reset(true);
        sleep_ms(20);
        reset(false);
        sleep_ms(10);
        reset(true);
        sleep_ms(10);

        // Software reset
        write_command(CMD::SWRESET);
        sleep_ms(10);

        // Initialize the display
        // st7789s_2_25_tft_initialize();
        st7789s_1_9_ips_initialize();

        freq = spi_set_baudrate(spi, 75 * 1000 * 1000);
    }

    void write_buffer(const uint8_t* buffer) {
        write_buffer(width, height, buffer, width * height);
    }

    void write_buffer(uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
        set_window(offset_x, offset_y, offset_x + w - 1, offset_y + h - 1);
        write_command(CMD::RAMWR);

        spi_set_format(spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        cs(false);
        dc(true);
        spi_write16_blocking(spi, (uint16_t*)buffer, size);
        cs(true);
        spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    }

    void fill(uint16_t color) {
        uint16_t data[width];
        for(size_t i = 0; i < width; i++) {
            data[i] = color;
        }

        set_window(offset_x, offset_y, offset_x + width - 1, offset_y + height - 1);
        write_command(CMD::RAMWR);

        spi_set_format(spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        cs(false);
        dc(true);

        for(size_t j = 0; j < height; j++) {
            spi_write16_blocking(spi, data, width);
        }

        cs(true);
        spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    }

    void eco_mode(bool enable) {
        if(enable) {
            write_command(CMD::IDMON);
        } else {
            write_command(CMD::IDMOFF);
        }
    }

private:
    PWMOutput<pin_backlight, 8, 20, false> backlight_pwm;
    spi_inst_t* spi = spi0;

    inline void reset(bool level) {
        gpio_put(pin_reset, level);
    }

    inline void cs(bool level) {
        gpio_put(pin_cs, level);
    }

    inline void dc(bool level) {
        gpio_put(pin_dc, level);
    }

    enum class CMD : uint8_t {
        NOP = 0x00,
        SWRESET = 0x01, /* Software Reset */
        RDDID = 0x04, /* Read Display ID */
        RDDST = 0x09, /* Read Display Status */
        RDDPM = 0x0A, /* Read Display Power Mode */
        RDDMADCTL = 0x0B, /* Read Display MADCTL */
        RDDCOLMOD = 0x0C, /* Read Display Pixel Format */
        RDDIM = 0x0D, /* Read Display Image Mode */
        RDDSM = 0x0E, /* Read Display Signal Mode */
        RDDSDR = 0x0F, /* Read Display Self-Diagnostic Result */
        SLPIN = 0x10, /* Sleep In */
        SLPOUT = 0x11, /* Sleep Out */
        PTLON = 0x12, /* Partial Display Mode On */
        NORON = 0x13, /* Normal Display Mode On */
        INVOFF = 0x20, /* Display Inversion Off */
        INVON = 0x21, /* Display Inversion On */
        GAMSET = 0x26, /* Gamma Set */
        DISPOFF = 0x28, /* Display Off */
        DISPON = 0x29, /* Display On */
        CASET = 0x2A, /* Column Address Set */
        RASET = 0x2B, /* Row Address Set */
        RAMWR = 0x2C, /* Memory Write */
        RAMRD = 0x2E, /* Memory Read */
        PTLAR = 0x30, /* Partial Area */
        VSCRDEF = 0x33, /* Vertical Scrolling Definition */
        TEOFF = 0x34, /* Tearing Effect Line OFF */
        TEON = 0x35, /* Tearing Effect Line ON */
        MADCTL = 0x36, /* Memory Data Access Control */
        VSCSAD = 0x37, /* Vertical Scroll Start Address of RAM */
        IDMOFF = 0x38, /* Idle Mode Off */
        IDMON = 0x39, /* Idle Mode On */
        COLMOD = 0x3A, /* Interface Pixel Format */
        WRMEMC = 0x3C, /* Write Memory Continue */
        RDMEMC = 0x3E, /* Read Memory Continue */
        STE = 0x44, /* Set Tear Scanline */
        GSCAN = 0x45, /* Get Scanline */
        WRDISBV = 0x51, /* Write Display Brightness Value */
        RDDISBV = 0x52, /* Read Display Brightness Value */
        WRCTRLD = 0x53, /* Write Control Display */
        RDCTRLD = 0x54, /* Read Control Value Display */
        WRCACE = 0x55, /* Write Content Adaptive Brightness Control and Color Enhancement */
        RDCABC = 0x56, /* Read Content Adaptive Brightness Control */
        WRCABCMB = 0x5E, /* Write CABC Minimum Brightness */
        RDCABCMB = 0x5F, /* Read CABC Minimum Brightness */
        RDABCSDR = 0x68, /* Read Adaptive Brightness Control and Self-Diagnostic Result */
        RDID1 = 0xDA, /* Read ID1 Value */
        RDID2 = 0xDB, /* Read ID2 Value */
        RDID3 = 0xDC, /* Read ID3 Value */
        RAMCTRL = 0xB0, /* RAM Control */
        RGBCTRL = 0xB1, /* RGB Interface Control */
        PORCTRL = 0xB2, /* Porch Setting */
        FRCTRL1 = 0xB3, /* Frame Rate Control in partial mode/idle colors */
        PARCTRL = 0xB5, /* Partial Mode Control */
        PWRSAVCTRL = 0xB6, /* Power Saving Control */
        GCTRL = 0xB7, /* Gate Control */
        GTADJ = 0xB8, /* Gate On Timing Adjustment */
        DGMEN = 0xBA, /* Digital Gamma Enable */
        VCOMS = 0xBB, /* VCOMS Setting */
        LCMCTRL = 0xC0, /* LCM Control */
        IDSET = 0xC1, /* ID Setting */
        VRHEN = 0xC2, /* VRH Command Enable */
        VRHS = 0xC3, /* VRH Set */
        VCMOFSET = 0xC5, /* VCOMS Offset Set */
        FRCTRL2 = 0xC6, /* Frame Rate Control in normal mode */
        CABCCTRL = 0xC7, /* CABC Control */
        REGSEL1 = 0xC8, /* Register Value Selection 1 */
        REGSEL2 = 0xCA, /* Register Value Selection 2 */
        PWMFRSEL = 0xCC, /* PWM Frequency Selection */
        VDVS = 0xC4, /* VDV Set */
        PWCTRL1 = 0xD0, /* Power Control 1 */
        VAPVANEN = 0xD2, /* Enable VAP/VAN signal output */
        GATESEL = 0xD6, /* Gate Output Selection in Sleep In Mode */
        CMD2EN = 0xDF, /* Command 2 Enable */
        PVGAMCTRL = 0xE0, /* Positive Voltage Gamma Control */
        NVGAMCTRL = 0xE1, /* Negative Voltage Gamma Control */
        DGMLUTR = 0xE2, /* Digital Gamma Look-up Table for Red */
        DGMLUTB = 0xE3, /* Digital Gamma Look-up Table for Blue */
        GATECTRL = 0xE4, /* Gate Control */
        SPI2EN = 0xE7, /* SPI2 Enable */
        PWCTRL2 = 0xE8, /* Power Control 2 */
        EQCTRL = 0xE9, /* Equalize time control */
        PROMCTRL = 0xEC, /* Program Mode Control*/
        PROMEN = 0xFA, /* Program Mode Enable*/
        NVMSET = 0xFC, /* NVM Setting */
        PROMACT = 0xFE, /* Program action */
    };

    inline void write_command(CMD command) {
        write_command(static_cast<uint8_t>(command));
    }

    inline void write_command(uint8_t command) {
        cs(false);
        dc(false);
        spi_write_blocking(spi, &command, 1);
        cs(true);
    }

    inline void write_data(uint8_t data) {
        cs(false);
        dc(true);
        spi_write_blocking(spi, &data, 1);
        cs(true);
    }

    inline void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
        write_command(CMD::CASET); // Column address set
        write_data(x0 >> 8);
        write_data(x0 & 0xFF);
        write_data(x1 >> 8);
        write_data(x1 & 0xFF);

        write_command(CMD::RASET); // Row address set
        write_data(y0 >> 8);
        write_data(y0 & 0xFF);
        write_data(y1 >> 8);
        write_data(y1 & 0xFF);
    }

    void st7789s_1_9_ips_initialize() {
        write_command(CMD::COLMOD);
        write_data(0x05); // 16-bit color (RGB565)

        write_command(CMD::PORCTRL);
        write_data(0x0C);
        write_data(0x0C);
        write_data(0x00);
        write_data(0x33);
        write_data(0x33);

        write_command(CMD::RAMCTRL);
        write_data(0x00);
        write_data(0xE0);

        write_command(CMD::MADCTL);
        write_data(0x60); // MX | MV = 90° CW rotation

        write_command(CMD::GCTRL);
        write_data(0x35);

        write_command(CMD::VCOMS);
        write_data(0x1A);

        write_command(CMD::LCMCTRL);
        write_data(0x2C);

        write_command(CMD::VRHEN);
        write_data(0x01);

        write_command(CMD::VRHS);
        write_data(0x0B);

        write_command(CMD::VDVS);
        write_data(0x20);

        write_command(CMD::FRCTRL2);
        write_data(0x0F);

        write_command(CMD::PWCTRL1);
        write_data(0xA4);
        write_data(0xA1);

        write_command(CMD::INVON);

        write_command(CMD::PVGAMCTRL);
        write_data(0xF0);
        write_data(0x00);
        write_data(0x04);
        write_data(0x04);
        write_data(0x04);
        write_data(0x05);
        write_data(0x29);
        write_data(0x33);
        write_data(0x3E);
        write_data(0x38);
        write_data(0x12);
        write_data(0x12);
        write_data(0x28);
        write_data(0x30);

        write_command(CMD::NVGAMCTRL);
        write_data(0xF0);
        write_data(0x07);
        write_data(0x0A);
        write_data(0x0D);
        write_data(0x0B);
        write_data(0x07);
        write_data(0x28);
        write_data(0x33);
        write_data(0x3E);
        write_data(0x36);
        write_data(0x14);
        write_data(0x14);
        write_data(0x29);
        write_data(0x32);

        write_command(CMD::SLPOUT);
        sleep_ms(120);

        write_command(CMD::DISPON);
    }

    void st7789s_2_25_tft_initialize(void) {
        write_command(CMD::SLPOUT);
        sleep_ms(120);

        write_command(CMD::PORCTRL);
        write_data(0x0C);
        write_data(0x0C);
        write_data(0x00);
        write_data(0x33);
        write_data(0x33);

        write_command(CMD::RAMCTRL);
        write_data(0x00);
        write_data(0xE0);

        write_command(CMD::MADCTL);
        write_data(0x60); // MX | MV = 90° CW rotation

        write_command(CMD::COLMOD);
        write_data(0x05);

        write_command(CMD::GCTRL);
        write_data(0x45);

        write_command(CMD::VCOMS);
        write_data(0x1D);

        write_command(CMD::LCMCTRL);
        write_data(0x2C);

        write_command(CMD::VRHEN);
        write_data(0x01);

        write_command(CMD::VRHS);
        write_data(0x19);

        write_command(CMD::VDVS);
        write_data(0x20);

        write_command(CMD::FRCTRL2);
        write_data(0x0F);

        write_command(CMD::PWCTRL1);
        write_data(0xA4);
        write_data(0xA1);

        write_command(CMD::GATESEL);
        write_data(0xA1);

        write_command(CMD::PVGAMCTRL);
        write_data(0xD0);
        write_data(0x10);
        write_data(0x21);
        write_data(0x14);
        write_data(0x15);
        write_data(0x2D);
        write_data(0x41);
        write_data(0x44);
        write_data(0x4F);
        write_data(0x28);
        write_data(0x0E);
        write_data(0x0C);
        write_data(0x1D);
        write_data(0x1F);

        write_command(CMD::NVGAMCTRL);
        write_data(0xD0);
        write_data(0x0F);
        write_data(0x1B);
        write_data(0x0D);
        write_data(0x0D);
        write_data(0x26);
        write_data(0x42);
        write_data(0x54);
        write_data(0x50);
        write_data(0x3E);
        write_data(0x1A);
        write_data(0x18);
        write_data(0x22);
        write_data(0x25);

        write_command(CMD::SLPOUT);
        sleep_ms(120);

        write_command(CMD::DISPON);
    }
};
