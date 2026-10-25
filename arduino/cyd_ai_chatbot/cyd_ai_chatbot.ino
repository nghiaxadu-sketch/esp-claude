// CYD AI Chatbot for the RockBase-iot NM-CYD-C5 (ESP32-C5 Cheap Yellow Display).
//
// Type a question on the touch keyboard (or in the Serial Monitor), the board asks Claude
// over Wi-Fi and shows the answer on the 2.8" screen. Setup steps: README.md.
//
// Arduino IDE: board "ESP32C5 Dev Module" (esp32 core >= 3.3), Flash Size "16MB",
// Partition Scheme "16M Flash (3MB APP/9.9MB FATFS)", PSRAM "Enabled", USB CDC On Boot "Enabled".
// Libraries: "Adafruit ST7735 and ST7789 Library", "Adafruit GFX Library", "ArduinoJson" (v7).
// Settings: config.h. Code: chatbot_app.cpp, chat_ui.cpp, claude_client.cpp, touch_xpt2046.cpp.

#include "chatbot_app.h"

void setup()
{
    chatbotSetup();
}

void loop()
{
    chatbotLoop();
}
