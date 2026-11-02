#include "app_device.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <time.h>

#include "board_hw.h"
#include "config.h"
#include "device_functions.h"
#include "ui_common.h"

// Root CA bundle embedded in the arduino-esp32 core (see claude_client.cpp).
extern const uint8_t kCaBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t kCaBundleEnd[] asm("_binary_x509_crt_bundle_end");

namespace {

constexpr time_t kValidEpoch = 1700000000;  // earlier means NTP has not answered yet
constexpr uint32_t kDefaultBeepMs = 150;
constexpr uint8_t kMinBrightness = 5;
constexpr uint16_t kHttpTimeoutMs = 8000;
constexpr size_t kMaxRawBody = 512;
constexpr size_t kMaxBody = 8192;          // JSON responses read for http(url, path)
constexpr uint32_t kHttpCacheMs = 10000;   // same URL within this time: reuse the last response
constexpr int kHttpAttempts = 2;
constexpr uint32_t kHttpRetryDelayMs = 500;
constexpr size_t kMaxValueText = 256;

bool localTime(struct tm &out)
{
    const time_t now = time(nullptr);
    if (now < kValidEpoch) {
        return false;
    }
    localtime_r(&now, &out);
    return true;
}

bool isIndex(const std::string &s)
{
    if (s.empty()) {
        return false;
    }
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

std::string jsonKey(const std::string &key)
{
    std::string out = "\"";
    for (char c : key) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out + "\"";
}

std::vector<std::string> splitPath(const std::string &path)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= path.size()) {
        const size_t dot = path.find('.', start);
        const std::string part = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!part.empty()) {
            parts.push_back(part);
        }
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }
    return parts;
}

}  // namespace

// ---------------------------------------------------------------- canvas

void GfxCanvas::touched(int16_t x, int16_t y, int16_t w, int16_t h)
{
    const ui::Rect &b = ui::kBackHit;
    if (x < b.x + b.w && x + w > b.x && y < b.y + b.h && y + h > b.y) {
        overlayDirty_ = true;
    }
}

void GfxCanvas::refreshOverlay()
{
    if (overlayDirty_) {
        overlayDirty_ = false;
        ui::drawBackBadge(gfx_);
    }
}

void GfxCanvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    touched(x, y, w, h);
    gfx_.fillRect(x, y, w, h, color);
}

void GfxCanvas::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    touched(x, y, w, h);
    gfx_.drawRect(x, y, w, h, color);
}

void GfxCanvas::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    touched(x, y, w, h);
    gfx_.fillRoundRect(x, y, w, h, r, color);
}

void GfxCanvas::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    touched(x, y, w, h);
    gfx_.drawRoundRect(x, y, w, h, r, color);
}

void GfxCanvas::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color)
{
    touched(x - r, y - r, 2 * r + 1, 2 * r + 1);
    gfx_.fillCircle(x, y, r, color);
}

void GfxCanvas::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color)
{
    touched(x - r, y - r, 2 * r + 1, 2 * r + 1);
    gfx_.drawCircle(x, y, r, color);
}

void GfxCanvas::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    touched(min(x0, x1), min(y0, y1), abs(x1 - x0) + 1, abs(y1 - y0) + 1);
    gfx_.drawLine(x0, y0, x1, y1, color);
}

void GfxCanvas::drawText(int16_t x, int16_t y, const std::string &ascii, uint8_t size, uint16_t color)
{
    touched(x, y, (int16_t)(ascii.size() * mini_app::kFontW * size), (int16_t)(mini_app::kFontH * size));
    gfx_.setTextWrap(false);
    gfx_.setTextSize(size);
    gfx_.setTextColor(color);
    gfx_.setCursor(x, y);
    gfx_.print(ascii.c_str());
}

// ---------------------------------------------------------------- device functions

uint32_t BoardDevice::millis()
{
    return ::millis();
}

bool BoardDevice::hasFunction(const std::string &name, size_t argCount) const
{
    return device_functions::accepts(name, argCount);
}

void BoardDevice::reset()
{
    board_hw::setLed(0, 0, 0);
    board_hw::stopBeep();
    board_hw::setBacklight(board_hw::brightnessSetting());
    cachedUrl_.clear();
    cachedBody_.clear();
    cachedBody_.shrink_to_fit();
}

bool BoardDevice::callFunction(const std::string &name, const std::vector<mini_script::Value> &args,
                               mini_script::Value &result, std::string &error)
{
    using mini_script::Value;
    auto n = [&](size_t i) { return args[i].toNumber(); };
    auto byte = [&](size_t i) { return (uint8_t)constrain((long)n(i), 0L, 255L); };
    result = Value::number(0);
    struct tm t;
    const bool haveTime = localTime(t);

    if (name == "led") {
        board_hw::setLed(byte(0), byte(1), byte(2));
    } else if (name == "beep") {
        board_hw::beep((uint32_t)max(0.0, n(0)), args.size() > 1 ? (uint32_t)max(0.0, n(1)) : kDefaultBeepMs);
    } else if (name == "millis") {
        result = Value::number(::millis());
    } else if (name == "time") {
        const time_t now = time(nullptr);
        result = Value::number(now < kValidEpoch ? 0 : (double)(now + TIMEZONE_OFFSET_SECONDS));
    } else if (name == "hour") {
        result = Value::number(haveTime ? t.tm_hour : 0);
    } else if (name == "minute") {
        result = Value::number(haveTime ? t.tm_min : 0);
    } else if (name == "second") {
        result = Value::number(haveTime ? t.tm_sec : 0);
    } else if (name == "day") {
        result = Value::number(haveTime ? t.tm_mday : 1);
    } else if (name == "month") {
        result = Value::number(haveTime ? t.tm_mon + 1 : 1);
    } else if (name == "year") {
        result = Value::number(haveTime ? t.tm_year + 1900 : 1970);
    } else if (name == "weekday") {
        result = Value::number(haveTime ? t.tm_wday : 0);
    } else if (name == "light") {
        result = Value::number(board_hw::lightPercent());
    } else if (name == "wifi") {
        result = Value::number(WiFi.status() == WL_CONNECTED ? 1 : 0);
    } else if (name == "brightness") {
        board_hw::setBacklight((uint8_t)constrain((long)n(0), (long)kMinBrightness, 100L));
    } else if (name == "http") {
        result = httpGet(args[0].toString(), args.size() > 1 ? args[1].toString() : std::string());
    } else {
        error = "chua ho tro";
        return false;
    }
    return true;
}

bool BoardDevice::fetchBody(const std::string &url, std::string &body)
{
    const uint32_t now = ::millis();
    if (url == cachedUrl_ && now - cachedAtMs_ < kHttpCacheMs) {
        body = cachedBody_;
        return true;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[app http] no Wi-Fi");
        return false;
    }
    const bool https = url.rfind("https://", 0) == 0;
    for (int attempt = 1; attempt <= kHttpAttempts; ++attempt) {
        NetworkClientSecure tls;
        NetworkClient plain;
        if (https) {
            tls.setCACertBundle(kCaBundleStart, kCaBundleEnd - kCaBundleStart);
        }
        HTTPClient http;
        http.useHTTP10(true);  // no chunked encoding: the body arrives as is
        http.setTimeout(kHttpTimeoutMs);
        http.setConnectTimeout(kHttpTimeoutMs);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        const bool opened = https ? http.begin(tls, url.c_str()) : http.begin(plain, url.c_str());
        const int status = opened ? http.GET() : HTTPC_ERROR_CONNECTION_REFUSED;
        Serial.printf("[app http] try %d: %d %s (heap %u)\n", attempt, status,
                      status < 0 ? HTTPClient::errorToString(status).c_str() : "", (unsigned)ESP.getFreeHeap());
        if (status == HTTP_CODE_OK) {
            const int size = http.getSize();
            NetworkClient &stream = http.getStream();
            body.clear();
            const uint32_t start = ::millis();
            while ((size < 0 || (int)body.size() < size) && body.size() < kMaxBody &&
                   ::millis() - start < kHttpTimeoutMs) {
                const int c = stream.read();
                if (c < 0) {
                    if (!stream.connected() && !stream.available()) {
                        break;
                    }
                    delay(1);
                    continue;
                }
                body += (char)c;
            }
            http.end();
            if (!body.empty()) {
                cachedUrl_ = url;
                cachedBody_ = body;
                cachedAtMs_ = ::millis();
                return true;
            }
        } else if (opened) {
            http.end();
        }
        delay(kHttpRetryDelayMs);
    }
    return false;
}

mini_script::Value BoardDevice::httpGet(const std::string &url, const std::string &path)
{
    using mini_script::Value;
    std::string body;
    if (!fetchBody(url, body)) {
        return Value::string("");
    }
    const std::vector<std::string> parts = splitPath(path);
    if (parts.empty()) {
        return Value::string(body.substr(0, kMaxRawBody));
    }

    // Only keep the requested value while parsing, so large responses fit in RAM.
    // "list.0.main.temp" -> {"list":[{"main":{"temp":true}}]} (a one-element array filters every item).
    std::string filterJson = "true";
    for (size_t i = parts.size(); i-- > 0;) {
        filterJson = isIndex(parts[i]) ? "[" + filterJson + "]" : "{" + jsonKey(parts[i]) + ":" + filterJson + "}";
    }
    JsonDocument filter;
    deserializeJson(filter, filterJson);
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err) {
        Serial.printf("[app http] JSON error %s\n", err.c_str());
        return Value::string("");
    }
    // JsonVariantConst rebinds on assignment (a JsonVariant would overwrite the value instead).
    JsonVariantConst v = doc.as<JsonVariantConst>();
    for (const std::string &part : parts) {
        if (isIndex(part)) {
            v = v[(size_t)atoi(part.c_str())];
        } else {
            v = v[part.c_str()];
        }
    }
    if (v.is<const char *>()) {
        return Value::string(v.as<const char *>());
    }
    if (v.is<bool>()) {
        return Value::number(v.as<bool>() ? 1 : 0);
    }
    if (v.is<double>()) {
        return Value::number(v.as<double>());
    }
    if (v.isNull()) {
        return Value::string("");
    }
    std::string text;
    serializeJson(v, text);
    if (text.size() > kMaxValueText) {
        text.resize(kMaxValueText);
    }
    return Value::string(text);
}
