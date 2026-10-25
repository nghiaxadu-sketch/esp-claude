// RockBase-iot NM-CYD-C5 (ESP32-C5-WROOM-1, 16 MB flash, 8 MB PSRAM, 2.8" ST7789 + XPT2046).
// Same mapping as application/edge_agent/boards/rockbase-iot/nm_cyd_c5/board_peripherals.yaml.
#pragma once

#include <stdint.h>

// LCD, touch and microSD share one SPI bus.
constexpr int8_t CYD_PIN_SPI_SCK = 6;
constexpr int8_t CYD_PIN_SPI_MISO = 2;
constexpr int8_t CYD_PIN_SPI_MOSI = 7;

constexpr int8_t CYD_PIN_LCD_CS = 23;
constexpr int8_t CYD_PIN_LCD_DC = 24;
constexpr int8_t CYD_PIN_LCD_RST = -1;  // tied to the board reset
constexpr int8_t CYD_PIN_LCD_BACKLIGHT = 25;

constexpr int8_t CYD_PIN_TOUCH_CS = 1;
constexpr int8_t CYD_PIN_SD_CS = 10;  // unused here, held high so the card stays off the bus

constexpr int8_t CYD_PIN_RGB_LED = 27;  // one WS2812
constexpr int8_t CYD_PIN_BOOT_BUTTON = 0;  // active low

// ST7789 panel in its native (portrait) orientation; rotated to 320x240 landscape at runtime.
constexpr uint16_t LCD_NATIVE_WIDTH = 240;
constexpr uint16_t LCD_NATIVE_HEIGHT = 320;

constexpr uint32_t BACKLIGHT_PWM_HZ = 5000;
constexpr uint8_t BACKLIGHT_PWM_BITS = 10;
