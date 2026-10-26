// Thay thế cho cyd.h của ESP32-2432S028: cùng tên hàm/biến (tft, cydBegin, rgb, C_*), nhưng cho
// RockBase-iot NM-CYD-C5 (ESP32-C5, ST7789 320x240, đèn WS2812). Chân lấy từ
// application/edge_agent/boards/rockbase-iot/nm_cyd_c5/board_peripherals.yaml.
// Chỉ include từ một file .ino (biến tft được định nghĩa ngay trong header).
#pragma once

#include <Adafruit_ST7789.h>
#include <SPI.h>

// Tiền tố CYD_ để không trùng macro PIN_RGB_LED có sẵn trong core ESP32-C5.
constexpr int8_t CYD_PIN_SPI_SCK = 6;
constexpr int8_t CYD_PIN_SPI_MISO = 2;
constexpr int8_t CYD_PIN_SPI_MOSI = 7;
constexpr int8_t CYD_PIN_LCD_CS = 23;
constexpr int8_t CYD_PIN_LCD_DC = 24;
constexpr int8_t CYD_PIN_LCD_RST = -1;  // nối chung chân reset của board
constexpr int8_t CYD_PIN_LCD_BACKLIGHT = 25;
constexpr int8_t CYD_PIN_TOUCH_CS = 1;
constexpr int8_t CYD_PIN_SD_CS = 10;
constexpr int8_t CYD_PIN_RGB_LED = 27;  // 1 đèn WS2812

constexpr uint16_t CYD_NATIVE_WIDTH = 240;   // panel ST7789 dọc, xoay ngang bằng setRotation
constexpr uint16_t CYD_NATIVE_HEIGHT = 320;
constexpr uint8_t CYD_ROTATION = 1;          // nếu hình bị ngược thì đổi thành 3
constexpr bool CYD_INVERT_COLORS = false;    // đặt true nếu màu bị âm bản
constexpr uint32_t CYD_LCD_SPI_HZ = 40000000;
constexpr uint8_t CYD_BRIGHTNESS_PERCENT = 80;
constexpr uint32_t CYD_BACKLIGHT_HZ = 5000;
constexpr uint8_t CYD_BACKLIGHT_BITS = 10;
constexpr uint8_t CYD_LED_LEVEL = 24;        // 0..255, WS2812 rất chói

constexpr uint16_t cydColor(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t C_BLACK = 0x0000;
constexpr uint16_t C_WHITE = 0xFFFF;
constexpr uint16_t C_RED = cydColor(255, 40, 40);
constexpr uint16_t C_GREEN = cydColor(40, 220, 80);
constexpr uint16_t C_YELLOW = cydColor(255, 220, 0);
constexpr uint16_t C_ORANGE = cydColor(255, 140, 0);
constexpr uint16_t C_NAVY = cydColor(0, 0, 96);
constexpr uint16_t C_DARK = cydColor(40, 40, 44);

Adafruit_ST7789 tft(&SPI, CYD_PIN_LCD_CS, CYD_PIN_LCD_DC, CYD_PIN_LCD_RST);

// Đèn RGB: mỗi kênh bật/tắt như trên bản ESP32-2432S028.
void rgb(bool r, bool g, bool b)
{
    rgbLedWrite(CYD_PIN_RGB_LED, r ? CYD_LED_LEVEL : 0, g ? CYD_LED_LEVEL : 0, b ? CYD_LED_LEVEL : 0);
}

void cydBegin()
{
    // LCD, cảm ứng và thẻ SD dùng chung bus SPI: bỏ chọn hết trước khi truyền dữ liệu.
    for (int8_t cs : {CYD_PIN_LCD_CS, CYD_PIN_TOUCH_CS, CYD_PIN_SD_CS}) {
        pinMode(cs, OUTPUT);
        digitalWrite(cs, HIGH);
    }
    SPI.begin(CYD_PIN_SPI_SCK, CYD_PIN_SPI_MISO, CYD_PIN_SPI_MOSI);

    ledcAttach(CYD_PIN_LCD_BACKLIGHT, CYD_BACKLIGHT_HZ, CYD_BACKLIGHT_BITS);
    const uint32_t maxDuty = (1U << CYD_BACKLIGHT_BITS) - 1;
    ledcWrite(CYD_PIN_LCD_BACKLIGHT, maxDuty * CYD_BRIGHTNESS_PERCENT / 100);

    tft.init(CYD_NATIVE_WIDTH, CYD_NATIVE_HEIGHT);
    tft.setSPISpeed(CYD_LCD_SPI_HZ);
    tft.setRotation(CYD_ROTATION);
    tft.invertDisplay(CYD_INVERT_COLORS);
    tft.setTextWrap(false);
    tft.fillScreen(C_BLACK);
    rgb(false, false, false);
}
