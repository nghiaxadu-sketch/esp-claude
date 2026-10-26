// Board pin maps, picked automatically from the chip selected in Arduino IDE (Tools -> Board):
//   "ESP32 Dev Module"    -> classic CYD ESP32-2432S028R (ESP32-WROOM-32, ILI9341 or ST7789, 4 MB flash)
//   "ESP32C5 Dev Module"  -> RockBase-iot NM-CYD-C5 (ESP32-C5, ST7789, 16 MB flash)
#pragma once

#include <sdkconfig.h>
#include <stdint.h>

#if CONFIG_IDF_TARGET_ESP32

// ---- ESP32-2432S028R "Cheap Yellow Display" ----
#define CYD_BOARD_NAME "CYD ESP32-2432S028R"
#define CYD_SHARED_SPI 0   // LCD on HSPI, touch on its own SPI bus
#define CYD_RGB_WS2812 0   // three separate active-low LEDs

constexpr int8_t CYD_PIN_LCD_SCK = 14;
constexpr int8_t CYD_PIN_LCD_MISO = 12;
constexpr int8_t CYD_PIN_LCD_MOSI = 13;
constexpr int8_t CYD_PIN_LCD_CS = 15;
constexpr int8_t CYD_PIN_LCD_DC = 2;
constexpr int8_t CYD_PIN_LCD_RST = -1;  // tied to EN
constexpr int8_t CYD_PIN_LCD_BACKLIGHT = 21;

constexpr int8_t CYD_PIN_TOUCH_SCK = 25;
constexpr int8_t CYD_PIN_TOUCH_MISO = 39;
constexpr int8_t CYD_PIN_TOUCH_MOSI = 32;
constexpr int8_t CYD_PIN_TOUCH_CS = 33;

constexpr int8_t CYD_PIN_SD_CS = 5;  // unused, held high
constexpr int8_t CYD_PIN_LED_RED = 4;
constexpr int8_t CYD_PIN_LED_GREEN = 16;
constexpr int8_t CYD_PIN_LED_BLUE = 17;
constexpr int8_t CYD_PIN_BOOT_BUTTON = 0;

#elif CONFIG_IDF_TARGET_ESP32C5

// ---- RockBase-iot NM-CYD-C5 ----
// Same mapping as application/edge_agent/boards/rockbase-iot/nm_cyd_c5/board_peripherals.yaml.
#define CYD_BOARD_NAME "NM-CYD-C5"
#define CYD_SHARED_SPI 1   // LCD, touch and microSD on one bus
#define CYD_RGB_WS2812 1

constexpr int8_t CYD_PIN_LCD_SCK = 6;
constexpr int8_t CYD_PIN_LCD_MISO = 2;
constexpr int8_t CYD_PIN_LCD_MOSI = 7;
constexpr int8_t CYD_PIN_LCD_CS = 23;
constexpr int8_t CYD_PIN_LCD_DC = 24;
constexpr int8_t CYD_PIN_LCD_RST = -1;  // tied to the board reset
constexpr int8_t CYD_PIN_LCD_BACKLIGHT = 25;

constexpr int8_t CYD_PIN_TOUCH_CS = 1;
constexpr int8_t CYD_PIN_SD_CS = 10;  // unused, held high so the card stays off the bus
constexpr int8_t CYD_PIN_RGB_LED = 27;
constexpr int8_t CYD_PIN_BOOT_BUTTON = 0;

#else
#error "Board chua ho tro. Tools -> Board: chon 'ESP32 Dev Module' (CYD ESP32-2432S028R) hoac 'ESP32C5 Dev Module' (NM-CYD-C5)."
#endif

// Panel native (portrait) size; rotated to 320x240 landscape at runtime.
constexpr uint16_t LCD_NATIVE_WIDTH = 240;
constexpr uint16_t LCD_NATIVE_HEIGHT = 320;

constexpr uint32_t BACKLIGHT_PWM_HZ = 5000;
constexpr uint8_t BACKLIGHT_PWM_BITS = 10;
constexpr uint32_t RGB_PWM_HZ = 5000;
constexpr uint8_t RGB_PWM_BITS = 8;
