// "Dong ho": a big digital or analog clock with the solar and lunar date, today's holiday and the current
// weather for the chosen location; a stopwatch ("Bam gio") with laps; and the hourly chime switch
// ("Bao gio"). The time comes from NTP (needs Wi-Fi once after power-up).
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include "ui_common.h"
#include "weather_client.h"

enum class ClockAction : uint8_t { None, Exit, SettingsChanged };

class ClockUi {
public:
    explicit ClockUi(Adafruit_GFX &gfx);

    void show();
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }

    ClockAction press(int16_t x, int16_t y);
    ClockAction back();
    void loop(uint32_t nowMs);

    // Settings shown and changed here (saved by the caller on SettingsChanged).
    void setOptions(uint8_t face, bool hourlyChime);
    uint8_t face() const { return face_; }
    bool hourlyChime() const { return chime_; }
    void setLocation(const String &name, float latitude, float longitude);
    // Weather is fetched by the caller (it blocks) when this returns true.
    bool weatherDue(uint32_t nowMs) const;
    void setWeather(const WeatherNow &weather);

private:
    enum class Page : uint8_t { Clock, Stopwatch };
    static constexpr size_t kMaxLaps = 4;

    void draw();
    void drawClockPage();
    void drawDigital(const struct tm &t, bool full);
    void drawAnalog(const struct tm &t, bool full);
    void drawDateLines(const struct tm &t);
    void drawWeather();
    void drawStopwatch(bool full);
    void drawFooter();

    Adafruit_GFX &gfx_;
    Page page_ = Page::Clock;
    bool visible_ = false;
    uint8_t face_ = 0;
    bool chime_ = false;
    String location_;
    float lat_ = 0;
    float lon_ = 0;
    WeatherNow weather_;
    uint32_t weatherTriedMs_ = 0;
    bool timeShown_ = false;  // the clock page was drawn with a valid time
    int lastSecond_ = -1;
    int lastMinute_ = -1;
    int lastDay_ = -1;
    // Analog hands drawn last time (to erase them).
    int16_t handX_[3] = {0, 0, 0};
    int16_t handY_[3] = {0, 0, 0};
    // Stopwatch.
    bool swRunning_ = false;
    uint32_t swStartMs_ = 0;
    uint32_t swElapsedMs_ = 0;
    uint32_t laps_[kMaxLaps] = {};
    size_t lapCount_ = 0;
    uint32_t lastSwDrawMs_ = 0;
};
