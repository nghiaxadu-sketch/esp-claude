// Phần cứng NM-CYD-C5 (ESP32-C5) cho các sketch kiểu "CYD": màn ST7789 320x240, đèn nền, đèn RGB.
// Thay cho cyd.h của ESP32-2432S028. API giữ nguyên: tft, màu C_*, cydBegin(), rgb(r,g,b).
// Chân theo application/edge_agent/boards/rockbase-iot/nm_cyd_c5/board_peripherals.yaml.
#pragma once

#include <Adafruit_ST7789.h>
#include <Arduino.h>
#include <SPI.h>

namespace cyd_pins {
constexpr int8_t SCK_PIN = 6, MISO_PIN = 2, MOSI_PIN = 7;
constexpr int8_t LCD_CS = 23, LCD_DC = 24, LCD_RST = -1;  // RST nối reset của board
constexpr int8_t BACKLIGHT = 25;
constexpr int8_t TOUCH_CS = 1, SD_CS = 10;  // dùng chung bus SPI, phải giữ mức cao
constexpr int8_t RGB_LED = 27;              // 1 LED WS2812
}  // namespace cyd_pins

constexpr uint8_t CYD_ROTATION = 1;         // 1 = ngang; nếu ngược đầu đổi thành 3
constexpr bool CYD_INVERT = false;          // đặt true nếu màu bị âm bản
constexpr uint8_t CYD_BRIGHTNESS = 80;      // % đèn nền
constexpr uint8_t CYD_LED_LEVEL = 24;       // 0..255, WS2812 rất chói
constexpr uint32_t CYD_SPI_HZ = 40000000;

constexpr uint16_t cydColor(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
constexpr uint16_t C_BLACK = 0x0000;
constexpr uint16_t C_WHITE = 0xFFFF;
constexpr uint16_t C_NAVY = cydColor(0, 0, 128);
constexpr uint16_t C_GREEN = cydColor(0, 220, 0);
constexpr uint16_t C_YELLOW = cydColor(255, 230, 0);
constexpr uint16_t C_ORANGE = cydColor(255, 140, 0);
constexpr uint16_t C_RED = cydColor(255, 0, 0);
constexpr uint16_t C_DARK = cydColor(40, 40, 40);

Adafruit_ST7789 tft(&SPI, cyd_pins::LCD_CS, cyd_pins::LCD_DC, cyd_pins::LCD_RST);

inline void cydBegin() {
  for (int8_t cs : {cyd_pins::LCD_CS, cyd_pins::TOUCH_CS, cyd_pins::SD_CS}) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }
  SPI.begin(cyd_pins::SCK_PIN, cyd_pins::MISO_PIN, cyd_pins::MOSI_PIN);

  ledcAttach(cyd_pins::BACKLIGHT, 5000, 10);
  ledcWrite(cyd_pins::BACKLIGHT, 1023UL * CYD_BRIGHTNESS / 100);

  tft.init(240, 320);  // kích thước gốc của panel (dọc), xoay ngang bên dưới
  tft.setSPISpeed(CYD_SPI_HZ);
  tft.setRotation(CYD_ROTATION);
  tft.invertDisplay(CYD_INVERT);
  tft.fillScreen(C_BLACK);
}

// Bật/tắt từng màu như đèn RGB rời của CYD gốc (true = sáng).
inline void rgb(bool r, bool g, bool b) {
  rgbLedWrite(cyd_pins::RGB_LED, r ? CYD_LED_LEVEL : 0, g ? CYD_LED_LEVEL : 0, b ? CYD_LED_LEVEL : 0);
}
