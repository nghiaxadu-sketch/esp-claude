#include "weather_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include "net_get.h"

namespace weather_client {
namespace {

constexpr uint32_t kTimeoutMs = 8000;
constexpr const char *kWeatherUrl =
    "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
    "&current=temperature_2m,relative_humidity_2m,weather_code&timezone=auto";
constexpr const char *kTimezoneUrl = "http://ip-api.com/json/?fields=status,offset";
constexpr size_t kUrlMax = 200;
constexpr int kHttpOk = 200;

}  // namespace

const char *codeText(int code)
{
    if (code == 0) return "Troi quang";
    if (code == 1) return "It may";
    if (code == 2) return "May rai rac";
    if (code == 3) return "Nhieu may";
    if (code == 45 || code == 48) return "Suong mu";
    if (code >= 51 && code <= 57) return "Mua phun";
    if (code >= 61 && code <= 67) return "Mua";
    if (code >= 71 && code <= 77) return "Tuyet";
    if (code >= 80 && code <= 82) return "Mua rao";
    if (code >= 85 && code <= 86) return "Mua tuyet";
    if (code >= 95) return "Giong bao";
    return "Khong co du lieu";
}

WeatherNow fetch(float latitude, float longitude)
{
    WeatherNow w;
    char url[kUrlMax];
    snprintf(url, sizeof(url), kWeatherUrl, latitude, longitude);
    NetworkClientSecure tls;
    net_get::useCaBundle(tls);
    HTTPClient http;
    http.setTimeout(kTimeoutMs);
    http.setConnectTimeout(kTimeoutMs);
    String error;
    const int status = net_get::get(http, tls, url, error);
    if (status != kHttpOk) {
        w.error = status > 0 ? "Thoi tiet: HTTP " + String(status) : error;
        http.end();
        return w;
    }
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) {
        w.error = "Thoi tiet: du lieu loi";
        return w;
    }
    JsonObjectConst current = doc["current"];
    if (current.isNull()) {
        w.error = "Thoi tiet: khong co du lieu";
        return w;
    }
    w.temperatureC = current["temperature_2m"] | 0.0f;
    w.humidityPercent = current["relative_humidity_2m"] | -1;
    w.code = current["weather_code"] | -1;
    w.text = codeText(w.code);
    w.valid = true;
    w.fetchedMs = millis();
    return w;
}

bool fetchTimezoneOffset(int32_t &offsetSec, String &error)
{
    NetworkClient plain;
    HTTPClient http;
    http.setTimeout(kTimeoutMs);
    const int status = net_get::get(http, plain, kTimezoneUrl, error);
    if (status != kHttpOk) {
        if (status > 0) {
            error = "Mui gio: HTTP " + String(status);
        }
        http.end();
        return false;
    }
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err || strcmp(doc["status"] | "", "success") != 0) {
        error = "Mui gio: khong doc duoc";
        return false;
    }
    offsetSec = doc["offset"] | 0;
    return true;
}

}  // namespace weather_client
