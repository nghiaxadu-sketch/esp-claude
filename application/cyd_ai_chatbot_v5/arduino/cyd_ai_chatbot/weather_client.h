// Current weather for the clock screen from Open-Meteo (free, no API key): temperature, humidity and the
// sky condition as short ASCII Vietnamese text.
#pragma once

#include <Arduino.h>

struct WeatherNow {
    bool valid = false;
    float temperatureC = 0;
    int humidityPercent = -1;
    int code = -1;     // WMO weather code
    String text;       // "Nhieu may", "Mua rao"...
    String error;      // ASCII, when !valid
    uint32_t fetchedMs = 0;
};

namespace weather_client {

// Blocking HTTPS request (1-3 s). Needs Wi-Fi.
WeatherNow fetch(float latitude, float longitude);
const char *codeText(int wmoCode);

// UTC offset in seconds for the board's public IP (ip-api.com, plain HTTP); false when it cannot be read.
bool fetchTimezoneOffset(int32_t &offsetSec, String &error);

}  // namespace weather_client
