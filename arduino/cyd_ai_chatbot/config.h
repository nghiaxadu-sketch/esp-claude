// User settings for the CYD AI chatbot.
//
// You can leave Wi-Fi and the API key empty here and set them from the Serial Monitor instead
// (115200 baud):  /wifi TEN_WIFI|MAT_KHAU   and   /key sk-ant-...
// Values set over Serial are saved in the board's NVS and take priority over this file.
// NEVER commit a real API key to git.
#pragma once

// ---- Wi-Fi ----
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// ---- Claude API ----
#define ANTHROPIC_API_KEY ""          // https://platform.claude.com/settings/keys
#define CLAUDE_MODEL "claude-sonnet-5-5" // or "claude-opus-5-5" / "claude-haiku-5-5" (or /model over Serial)
#define CLAUDE_EFFORT "low"            // low | medium | high | xhigh | max ; "" = API default
#define CLAUDE_MAX_TOKENS 4096         // includes Claude's thinking; replies are kept short by the system prompt
#define CLAUDE_HTTP_TIMEOUT_MS 60000   // HTTPClient limit is 65535 ms
#define CHAT_HISTORY_MAX_MESSAGES 10   // messages sent as context (user + assistant)

// 0 = verify api.anthropic.com with the CA bundle built into the ESP32 core (recommended).
// 1 = skip certificate checks. Only for debugging, the API key can then be intercepted.
#define CLAUDE_TLS_INSECURE 0

// ---- Clock (shown in the header) ----
#define TIMEZONE_OFFSET_SECONDS (7 * 3600)  // Vietnam, UTC+7

// ---- Screen ----
// Only for "ESP32 Dev Module" (ESP32-2432S028R): 1 = ST7789 (newer boards with a USB-C port),
// 0 = ILI9341 (older boards with only micro-USB). Wrong choice: inverted colors and a portrait,
// cut-off picture with noise at the bottom. NM-CYD-C5 always uses ST7789.
#ifndef CYD_DISPLAY_ST7789
#define CYD_DISPLAY_ST7789 1
#endif
#define LCD_ROTATION 1          // landscape (320x240); 3 = landscape turned 180 degrees
#define LCD_INVERT_COLORS -1    // -1 = board default, 0 = normal, 1 = inverted (if colors look like a negative)
#define LCD_SPI_HZ 40000000
#define LCD_BRIGHTNESS_PERCENT 80
#define CHAT_TEXT_SIZE 1        // 1 = 51 chars x 17 lines, 2 = 25 chars x 9 lines

// ---- RGB LED status ----
#define RGB_LED_ENABLED 1
#define RGB_LED_LEVEL 24        // 0..255, the LEDs are very bright
