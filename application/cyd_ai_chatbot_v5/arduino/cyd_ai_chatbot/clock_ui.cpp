#include "clock_ui.h"

#include <math.h>
#include <time.h>

#include "lunar_calendar.h"

namespace {

using namespace ui;

constexpr time_t kValidEpoch = 1700000000;
constexpr uint32_t kWeatherEveryMs = 15UL * 60 * 1000;
constexpr uint32_t kWeatherRetryMs = 2UL * 60 * 1000;
constexpr uint32_t kStopwatchRedrawMs = 100;

// Digital face.
constexpr uint8_t kBigTextSize = 7;   // 42 x 56 px per character
constexpr int16_t kBigTimeY = 40;
constexpr uint8_t kSecondsTextSize = 3;
constexpr int16_t kDateY = 108;
constexpr int16_t kLunarY = 132;
constexpr int16_t kHolidayY = 150;
constexpr int16_t kWeatherY = 172;
// Analog face (left) with the date on the right.
constexpr int16_t kDialCx = 92;
constexpr int16_t kDialCy = 112;
constexpr int16_t kDialR = 82;
constexpr int16_t kSideX = 184;
constexpr float kPi = 3.14159265f;

const char *const kFaceNames[] = {"Mat: so", "Mat: kim"};
constexpr uint8_t kFaceCount = sizeof(kFaceNames) / sizeof(kFaceNames[0]);

bool localNow(struct tm &t)
{
    const time_t now = time(nullptr);
    if (now < kValidEpoch) {
        return false;
    }
    localtime_r(&now, &t);
    return true;
}

String twoDigits(int v)
{
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", v);
    return String(buf);
}

String durationText(uint32_t ms)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02lu:%02lu.%lu", (unsigned long)(ms / 60000), (unsigned long)(ms / 1000 % 60),
             (unsigned long)(ms / 100 % 10));
    return String(buf);
}

}  // namespace

ClockUi::ClockUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
}

void ClockUi::setOptions(uint8_t face, bool hourlyChime)
{
    face_ = face < kFaceCount ? face : 0;
    chime_ = hourlyChime;
}

void ClockUi::setLocation(const String &name, float latitude, float longitude)
{
    if (name != location_ || latitude != lat_ || longitude != lon_) {
        weather_ = WeatherNow();
        weatherTriedMs_ = 0;
    }
    location_ = name;
    lat_ = latitude;
    lon_ = longitude;
}

bool ClockUi::weatherDue(uint32_t nowMs) const
{
    if (!visible_ || page_ != Page::Clock) {
        return false;
    }
    if (weatherTriedMs_ == 0) {
        return true;
    }
    const uint32_t wait = weather_.valid ? kWeatherEveryMs : kWeatherRetryMs;
    return nowMs - weatherTriedMs_ >= wait;
}

void ClockUi::setWeather(const WeatherNow &weather)
{
    weatherTriedMs_ = millis();
    if (weather.valid || !weather_.valid) {
        weather_ = weather;
    }
    if (visible_ && page_ == Page::Clock) {
        drawWeather();
    }
}

void ClockUi::show()
{
    visible_ = true;
    draw();
}

void ClockUi::draw()
{
    gfx_.fillScreen(kColorBg);
    lastSecond_ = lastMinute_ = lastDay_ = -1;
    timeShown_ = false;
    if (page_ == Page::Clock) {
        drawBackHeader(gfx_, "Dong ho", chime_ ? "Bao gio: bat" : "");
        drawClockPage();
    } else {
        drawBackHeader(gfx_, "Bam gio");
        drawStopwatch(true);
    }
    drawFooter();
}

void ClockUi::drawFooter()
{
    if (page_ == Page::Clock) {
        drawButton(gfx_, footerSlot(0, 3), kFaceNames[face_], kColorButton, kColorWhite);
        drawButton(gfx_, footerSlot(1, 3), "Bam gio", kColorButton, kColorWhite);
        drawButton(gfx_, footerSlot(2, 3), chime_ ? "Bao gio: Bat" : "Bao gio: Tat", chime_ ? kColorAccent : kColorButton,
                   kColorWhite);
    } else {
        drawButton(gfx_, footerSlot(0, 3), swRunning_ ? "Dung" : "Bat dau", kColorAccent, kColorWhite);
        drawButton(gfx_, footerSlot(1, 3), swRunning_ ? "Vong" : "Dat lai", kColorButton, kColorWhite);
        drawButton(gfx_, footerSlot(2, 3), "Dong ho", kColorButton, kColorWhite);
    }
}

void ClockUi::drawClockPage()
{
    struct tm t;
    if (!localNow(t)) {
        drawCentered(gfx_, "Chua co gio", 0, kScreenW, 90, 3, kColorText);
        drawCentered(gfx_, "Bat Wi-Fi de dong bo gio (NTP)", 0, kScreenW, 130, 1, kColorMuted);
        return;
    }
    gfx_.fillRect(0, kHeaderH, kScreenW, kScreenH - kHeaderH - kFooterH, kColorBg);
    if (face_ == 1) {
        drawAnalog(t, true);
    } else {
        drawDigital(t, true);
    }
    drawDateLines(t);
    drawWeather();
    timeShown_ = true;
    lastSecond_ = t.tm_sec;
    lastMinute_ = t.tm_min;
    lastDay_ = t.tm_mday;
}

void ClockUi::drawDigital(const struct tm &t, bool full)
{
    const String hm = twoDigits(t.tm_hour) + ":" + twoDigits(t.tm_min);
    const int16_t charW = kFontW * kBigTextSize;
    const int16_t secW = 2 * kFontW * kSecondsTextSize;
    const int16_t totalW = (int16_t)hm.length() * charW + 6 + secW;
    const int16_t x = (kScreenW - totalW) / 2;
    if (full || t.tm_min != lastMinute_) {
        gfx_.fillRect(0, kBigTimeY, kScreenW, kFontH * kBigTextSize, kColorBg);
        gfx_.setTextSize(kBigTextSize);
        gfx_.setTextColor(kColorText);
        gfx_.setCursor(x, kBigTimeY);
        gfx_.print(hm);
    }
    gfx_.setTextSize(kSecondsTextSize);
    gfx_.setTextColor(kColorAccent, kColorBg);
    gfx_.setCursor(x + (int16_t)hm.length() * charW + 6, kBigTimeY + kFontH * (kBigTextSize - kSecondsTextSize));
    gfx_.print(twoDigits(t.tm_sec));
}

void ClockUi::drawAnalog(const struct tm &t, bool full)
{
    if (full) {
        gfx_.fillCircle(kDialCx, kDialCy, kDialR, kColorHeader);
        gfx_.drawCircle(kDialCx, kDialCy, kDialR, kColorAccent);
        for (int i = 0; i < 12; ++i) {
            const float a = i * kPi / 6;
            const int16_t r0 = i % 3 == 0 ? kDialR - 12 : kDialR - 7;
            gfx_.drawLine(kDialCx + (int16_t)(sinf(a) * r0), kDialCy - (int16_t)(cosf(a) * r0),
                          kDialCx + (int16_t)(sinf(a) * (kDialR - 2)), kDialCy - (int16_t)(cosf(a) * (kDialR - 2)),
                          kColorText);
        }
        for (int i = 0; i < 3; ++i) {
            handX_[i] = kDialCx;
            handY_[i] = kDialCy;
        }
    }
    const float angles[3] = {((t.tm_hour % 12) + t.tm_min / 60.0f) * kPi / 6, (t.tm_min + t.tm_sec / 60.0f) * kPi / 30,
                             t.tm_sec * kPi / 30};
    const int16_t lengths[3] = {kDialR * 5 / 10, kDialR * 75 / 100, kDialR * 85 / 100};
    const uint16_t colors[3] = {kColorText, kColorText, kColorAccent};
    for (int i = 0; i < 3; ++i) {  // erase old hands
        gfx_.drawLine(kDialCx, kDialCy, handX_[i], handY_[i], kColorHeader);
        if (i < 2) {
            gfx_.drawLine(kDialCx + 1, kDialCy, handX_[i] + 1, handY_[i], kColorHeader);
        }
    }
    for (int i = 0; i < 3; ++i) {
        handX_[i] = kDialCx + (int16_t)(sinf(angles[i]) * lengths[i]);
        handY_[i] = kDialCy - (int16_t)(cosf(angles[i]) * lengths[i]);
        gfx_.drawLine(kDialCx, kDialCy, handX_[i], handY_[i], colors[i]);
        if (i < 2) {
            gfx_.drawLine(kDialCx + 1, kDialCy, handX_[i] + 1, handY_[i], colors[i]);
        }
    }
    gfx_.fillCircle(kDialCx, kDialCy, 3, kColorAccent);
    // Digital time on the side.
    gfx_.setTextSize(3);
    gfx_.setTextColor(kColorText, kColorBg);
    gfx_.setCursor(kSideX, 40);
    gfx_.print(twoDigits(t.tm_hour) + ":" + twoDigits(t.tm_min));
}

void ClockUi::drawDateLines(const struct tm &t)
{
    const int day = t.tm_mday, month = t.tm_mon + 1, year = t.tm_year + 1900;
    const lunar::LunarDate l = lunar::fromSolar(day, month, year);
    char yearName[24];
    lunar::yearCanChi(l.year, yearName, sizeof(yearName));
    char solar[48];
    snprintf(solar, sizeof(solar), "%s, %02d/%02d/%d", lunar::weekdayName(t.tm_wday), day, month, year);
    char lunarText[64];
    snprintf(lunarText, sizeof(lunarText), "Am lich %d/%d%s - nam %s", l.day, l.month, l.leap ? " (nhuan)" : "",
             yearName);
    const char *holiday = lunar::holiday(day, month, l, lunar::lunarMonthLength(day, month, year));
    if (face_ == 1) {
        int16_t y = 80;
        gfx_.fillRect(kSideX, y, kScreenW - kSideX, 70, kColorBg);
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorText);
        gfx_.setCursor(kSideX, y);
        gfx_.print(lunar::weekdayName(t.tm_wday));
        gfx_.setCursor(kSideX, y += 12);
        gfx_.printf("%02d/%02d/%d", day, month, year);
        gfx_.setTextColor(kColorAccent);
        gfx_.setCursor(kSideX, y += 16);
        gfx_.printf("AL %d/%d%s", l.day, l.month, l.leap ? " nhuan" : "");
        gfx_.setCursor(kSideX, y += 12);
        gfx_.print(yearName);
        if (holiday != nullptr) {
            gfx_.setTextColor(kColorSystem);
            gfx_.setCursor(kSideX, y += 14);
            gfx_.print(ellipsize(holiday, (kScreenW - kSideX) / kFontW));
        }
        return;
    }
    gfx_.fillRect(0, kDateY, kScreenW, kWeatherY - kDateY, kColorBg);
    drawCentered(gfx_, solar, 0, kScreenW, kDateY, 2, kColorText);
    drawCentered(gfx_, lunarText, 0, kScreenW, kLunarY, 1, kColorAccent);
    if (holiday != nullptr) {
        drawCentered(gfx_, holiday, 0, kScreenW, kHolidayY, 1, kColorSystem);
    }
}

void ClockUi::drawWeather()
{
    const int16_t y = face_ == 1 ? 156 : kWeatherY;
    const int16_t x = face_ == 1 ? kSideX : 0;
    const int16_t w = kScreenW - x;
    gfx_.fillRect(x, y, w, 2 * kFontH + 8, kColorBg);
    String line1, line2;
    const String place = location_.length() > 0 ? location_ : String("Hai Phong");
    if (weather_.valid) {
        line1 = String((int)lroundf(weather_.temperatureC)) + " C  " + weather_.text;
        line2 = place + (weather_.humidityPercent >= 0 ? "  Do am " + String(weather_.humidityPercent) + "%" : "");
    } else {
        line1 = weather_.error.length() > 0 ? weather_.error : String("Dang tai thoi tiet...");
        line2 = place;
    }
    if (face_ == 1) {
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorText);
        gfx_.setCursor(x, y);
        gfx_.print(ellipsize(line1, w / kFontW));
        gfx_.setTextColor(kColorMuted);
        gfx_.setCursor(x, y + 12);
        gfx_.print(ellipsize(line2, w / kFontW));
        return;
    }
    drawCentered(gfx_, ellipsize(line1, w / (2 * kFontW)), x, w, y, 2, kColorText);
    drawCentered(gfx_, ellipsize(line2, w / kFontW), x, w, y + 20, 1, kColorMuted);
}

void ClockUi::drawStopwatch(bool full)
{
    const uint32_t elapsed = swElapsedMs_ + (swRunning_ ? millis() - swStartMs_ : 0);
    drawCentered(gfx_, durationText(elapsed), 0, kScreenW, 56, 5, kColorText);
    if (!full) {
        return;
    }
    gfx_.fillRect(0, 110, kScreenW, kScreenH - kFooterH - 110, kColorBg);
    for (size_t i = 0; i < lapCount_; ++i) {
        const String line = "Vong " + String((unsigned)(i + 1)) + ":  " + durationText(laps_[i]);
        drawCentered(gfx_, line, 0, kScreenW, 116 + (int16_t)i * 20, 2, i + 1 == lapCount_ ? kColorAccent : kColorMuted);
    }
}

ClockAction ClockUi::back()
{
    if (page_ == Page::Stopwatch) {
        page_ = Page::Clock;
        draw();
        return ClockAction::None;
    }
    return ClockAction::Exit;
}

ClockAction ClockUi::press(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        return back();
    }
    for (size_t i = 0; i < 3; ++i) {
        if (!footerSlot(i, 3).contains(x, y)) {
            continue;
        }
        if (page_ == Page::Clock) {
            if (i == 0) {
                face_ = (uint8_t)((face_ + 1) % kFaceCount);
                draw();
                return ClockAction::SettingsChanged;
            }
            if (i == 1) {
                page_ = Page::Stopwatch;
                draw();
                return ClockAction::None;
            }
            chime_ = !chime_;
            draw();
            return ClockAction::SettingsChanged;
        }
        const uint32_t now = millis();
        if (i == 0) {
            if (swRunning_) {
                swElapsedMs_ += now - swStartMs_;
            } else {
                swStartMs_ = now;
            }
            swRunning_ = !swRunning_;
        } else if (i == 1) {
            if (swRunning_) {
                const uint32_t lap = swElapsedMs_ + now - swStartMs_;
                if (lapCount_ == kMaxLaps) {
                    memmove(laps_, laps_ + 1, sizeof(laps_[0]) * (kMaxLaps - 1));
                    --lapCount_;
                }
                laps_[lapCount_++] = lap;
            } else {
                swElapsedMs_ = 0;
                lapCount_ = 0;
            }
        } else {
            page_ = Page::Clock;
        }
        draw();
        return ClockAction::None;
    }
    return ClockAction::None;
}

void ClockUi::loop(uint32_t nowMs)
{
    if (!visible_) {
        return;
    }
    if (page_ == Page::Stopwatch) {
        if (swRunning_ && nowMs - lastSwDrawMs_ >= kStopwatchRedrawMs) {
            lastSwDrawMs_ = nowMs;
            drawStopwatch(false);
        }
        return;
    }
    struct tm t;
    if (!localNow(t) || t.tm_sec == lastSecond_) {
        return;
    }
    if (!timeShown_) {
        drawClockPage();  // the page was drawn before NTP answered
        return;
    }
    if (face_ == 1) {
        drawAnalog(t, false);
    } else {
        drawDigital(t, t.tm_min != lastMinute_);
    }
    if (t.tm_mday != lastDay_ && lastDay_ >= 0) {
        drawDateLines(t);
    }
    lastSecond_ = t.tm_sec;
    lastMinute_ = t.tm_min;
    lastDay_ = t.tm_mday;
}
