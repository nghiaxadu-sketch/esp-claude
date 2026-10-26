// CYD AI Chatbot: ask Claude from a "Cheap Yellow Display" touch screen.
//
// Supported boards (picked automatically from Tools -> Board, see board_pins.h):
//   "ESP32 Dev Module"   -> CYD ESP32-2432S028R. Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)".
//                           Display ILI9341 by default; set CYD_DISPLAY_ST7789 1 in config.h for "2USB" boards.
//   "ESP32C5 Dev Module" -> RockBase-iot NM-CYD-C5. Flash Size 16MB,
//                           Partition Scheme "16M Flash (3MB APP/9.9MB FATFS)", PSRAM Enabled, USB CDC On Boot Enabled.
// Libraries: "Adafruit ILI9341", "Adafruit ST7735 and ST7789 Library", "Adafruit GFX Library", "ArduinoJson" (v7).
// Settings: config.h. Setup steps: README.md.

#include "chatbot_app.h"

void setup()
{
    chatbotSetup();
}

void loop()
{
    chatbotLoop();
}
