// User settings for Claude Apps on the CYD (chat + apps that Claude writes, ESP-Mosaico style).
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
#define CLAUDE_EFFORT "low"            // chat: low | medium | high | xhigh | max ; "" = API default
#define CLAUDE_MAX_TOKENS 4096         // chat; includes Claude's thinking, replies are kept short by the system prompt
#define CLAUDE_HTTP_TIMEOUT_MS 60000   // wait for the first response bytes (HTTPClient limit is 65535 ms)
#define CHAT_HISTORY_MAX_MESSAGES 10   // messages sent as context (user + assistant)

// ---- Apps written by Claude ----
#define CLAUDE_APP_EFFORT "medium"     // more care than chat: apps must be valid on the first try
#define CLAUDE_APP_MAX_TOKENS 16000    // replies are streamed, so long apps do not time out
#define APP_REPAIR_ROUNDS 2            // times Claude may fix an app the board rejected
#define APP_EXIT_HOLD_MS 1500          // hold the screen this long (or press BOOT) to leave an app
// Where "weather"-style apps should look (Claude uses it for API coordinates).
#define APP_LOCATION_TEXT "Hai Phong, Viet Nam (latitude 20.86, longitude 106.68)"

// ---- "Ban do" (directions, OpenStreetMap; the board has no GPS so the start is typed) ----
#define MAP_DEFAULT_FROM "AEON Mall Hai Phong Le Chan"  // first start point, later the last one used is kept
#define MAP_HOME_LAT 20.86f   // addresses near here are preferred when searching
#define MAP_HOME_LON 106.68f
// Map background: 256x256 PNG tiles, %d = zoom, x, y. OpenStreetMap's own server is free for light personal
// use (keep the "(c) OpenStreetMap" credit shown on the map). Needs the "PNGdec" library.
#define MAP_TILE_URL_FORMAT "https://tile.openstreetmap.org/%d/%d/%d.png"

// ---- Web page on the board (http://<IP>/): API key, model, create apps from a phone ----
#define WEB_PORTAL_ENABLED 1

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
// -1 = board default, 0 = normal, 1 = inverted. Set to 0 for this board: with the default its colors came
// out as a negative (black backgrounds white, orange blue). If that happens again, switch between 0 and 1.
#define LCD_INVERT_COLORS 0
#define LCD_SPI_HZ 40000000
#define LCD_BRIGHTNESS_PERCENT 80
#define CHAT_TEXT_SIZE 1        // 1 = 51 chars x 17 lines, 2 = 25 chars x 9 lines
#define SCREEN_OFF_DEFAULT_SEC 120  // backlight off after this long without touch (changeable in Settings > Man hinh)

// ---- RGB LED status ----
#define RGB_LED_ENABLED 1
#define RGB_LED_LEVEL 24        // 0..255, the LEDs are very bright

// ---- Speaker (ESP32-2432S028R "SPEAK" connector; NM-CYD-C5 has none) ----
#define SPEAKER_ENABLED 1

// ---- AI face ("Claude" on the home screen): a robot face with eyes, mouth and moods that talks back ----
#define FACE_MAX_TOKENS 2048      // includes thinking; replies are kept to a few sentences by the prompt
#define FACE_EFFORT "low"
#define FACE_HISTORY_MAX_MESSAGES 8
#define FACE_VOICE_BLIPS 1        // 1 = little "robot voice" beeps while the face talks (needs a speaker)
#define FACE_SLEEP_AFTER_MS 90000 // no touch for this long: the face gets sleepy, then falls asleep
