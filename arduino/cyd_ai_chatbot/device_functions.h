// Functions the board offers to app scripts. Shared by the board implementation (app_device.cpp),
// the validator, the host tests and the guide sent to Claude, so all of them agree.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace device_functions {

struct Info {
    const char *name;
    uint8_t minArgs;
    uint8_t maxArgs;
    const char *help;  // one line for Claude
};

constexpr Info kFunctions[] = {
    {"led", 3, 3, "led(r,g,b) RGB LED 0..255, led(0,0,0) = off"},
    {"beep", 1, 2, "beep(freq_hz, ms=150) speaker tone (only some boards have a speaker)"},
    {"millis", 0, 0, "millis() ms since boot"},
    {"time", 0, 0, "time() local Unix time in seconds (0 if the clock is not synced yet)"},
    {"hour", 0, 0, "hour() 0..23"},
    {"minute", 0, 0, "minute() 0..59"},
    {"second", 0, 0, "second() 0..59"},
    {"day", 0, 0, "day() 1..31"},
    {"month", 0, 0, "month() 1..12"},
    {"year", 0, 0, "year() e.g. 2026"},
    {"weekday", 0, 0, "weekday() 0=Sunday..6=Saturday"},
    {"light", 0, 0, "light() ambient light 0..100, -1 if the board has no light sensor"},
    {"wifi", 0, 0, "wifi() 1 if Wi-Fi is connected"},
    {"brightness", 1, 1, "brightness(percent) screen backlight 5..100"},
    {"http", 1, 2,
     "http(url, path) HTTPS GET JSON, returns the value at a dotted path such as \"current.temperature_2m\" or "
     "\"list.0.main.temp\" (number or string); path \"\" returns the start of the body as text. Returns \"\" on "
     "error. Takes 1-5 s: only call it in init/timers/on_tap (never in item fields), store the result in a var"},
};

inline const Info *find(const std::string &name)
{
    for (const Info &f : kFunctions) {
        if (name == f.name) {
            return &f;
        }
    }
    return nullptr;
}

inline bool accepts(const std::string &name, size_t argCount)
{
    const Info *f = find(name);
    return f != nullptr && argCount >= f->minArgs && argCount <= f->maxArgs;
}

}  // namespace device_functions
