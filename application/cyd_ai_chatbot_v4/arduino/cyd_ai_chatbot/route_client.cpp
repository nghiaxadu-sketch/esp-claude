#include "route_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

#include "config.h"
#include "text_utils.h"

// Root CA bundle embedded in the arduino-esp32 core (see claude_client.cpp).
extern const uint8_t kCaBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t kCaBundleEnd[] asm("_binary_x509_crt_bundle_end");

namespace route_client {
namespace {

constexpr uint16_t kHttpTimeoutMs = 15000;
constexpr const char *kUserAgent = "CYD-Claude-Apps/1.0 (ESP32 directions)";
constexpr const char *kGeocodeUrl = "https://photon.komoot.io/api/?limit=1";
constexpr const char *kRouteUrlBase = "https://routing.openstreetmap.de/routed-";
constexpr const char *kRouteQuery = "?overview=full&geometries=polyline&steps=true";
constexpr size_t kMaxPathPoints = 3000;  // longer routes are thinned (the screen is only 320 px wide)
constexpr float kPolylineScale = 1e-5f;  // Google polyline, precision 5
constexpr int kPolylineOffset = 63;
constexpr int kPolylineChunkBits = 5;
constexpr int kPolylineMoreFlag = 0x20;
constexpr int kPolylineChunkMask = 0x1F;

String toAscii(const char *utf8)
{
    return String(text_utils::utf8ToAscii(utf8 != nullptr ? utf8 : "").c_str());
}

String urlEncode(const String &text)
{
    static const char kHex[] = "0123456789ABCDEF";
    String out;
    out.reserve(text.length() * 3);
    for (size_t i = 0; i < text.length(); ++i) {
        const uint8_t c = (uint8_t)text[i];
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                           c == '_' || c == '.' || c == '~';
        if (plain) {
            out += (char)c;
        } else {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0x0F];
        }
    }
    return out;
}

// GETs `url` over HTTPS and parses only the fields in `filter`.
bool getJson(const String &url, const char *filterJson, JsonDocument &doc, String &error)
{
    if (WiFi.status() != WL_CONNECTED) {
        error = "Chua co Wi-Fi";
        return false;
    }
    NetworkClientSecure tls;
    tls.setCACertBundle(kCaBundleStart, kCaBundleEnd - kCaBundleStart);
    HTTPClient http;
    http.useHTTP10(true);  // no chunked encoding, so JSON can be parsed from the stream
    http.setTimeout(kHttpTimeoutMs);
    http.setConnectTimeout(kHttpTimeoutMs);
    http.setUserAgent(kUserAgent);
    if (!http.begin(tls, url)) {
        error = "Khong mo duoc ket noi";
        return false;
    }
    const int status = http.GET();
    Serial.printf("[map] %d %s\n", status, url.c_str());
    if (status != HTTP_CODE_OK) {
        error = status < 0 ? "Loi mang: " + HTTPClient::errorToString(status) : "May chu bao loi " + String(status);
        http.end();
        return false;
    }
    JsonDocument filter;
    deserializeJson(filter, filterJson);
    const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err == DeserializationError::NoMemory) {
        error = "Duong qua dai, khong du RAM";
        return false;
    }
    if (err) {
        error = String("Loi doc du lieu: ") + err.c_str();
        return false;
    }
    return true;
}

bool parseLatLon(const String &text, GeoPoint &out)
{
    float lat = 0, lon = 0;
    char tail = 0;
    if (sscanf(text.c_str(), " %f , %f %c", &lat, &lon, &tail) != 2) {
        return false;
    }
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
        return false;
    }
    out.lat = lat;
    out.lon = lon;
    return true;
}

void appendPart(String &name, const char *part)
{
    const String ascii = toAscii(part);
    if (ascii.length() == 0 || name.indexOf(ascii) >= 0) {
        return;
    }
    if (name.length() > 0) {
        name += ", ";
    }
    name += ascii;
}

// Decodes a Google encoded polyline (precision 5) into points.
void decodePolyline(const char *s, std::vector<GeoPoint> &out)
{
    int32_t lat = 0, lon = 0;
    while (*s != '\0') {
        int32_t delta[2] = {0, 0};
        for (int k = 0; k < 2; ++k) {
            int32_t result = 0;
            int shift = 0;
            int chunk = 0;
            do {
                if (*s == '\0') {
                    return;
                }
                chunk = *s++ - kPolylineOffset;
                result |= (int32_t)(chunk & kPolylineChunkMask) << shift;
                shift += kPolylineChunkBits;
            } while (chunk >= kPolylineMoreFlag);
            delta[k] = (result & 1) ? ~(result >> 1) : (result >> 1);
        }
        lat += delta[0];
        lon += delta[1];
        out.push_back({lat * kPolylineScale, lon * kPolylineScale});
    }
}

void thinPath(std::vector<GeoPoint> &path)
{
    if (path.size() <= kMaxPathPoints) {
        return;
    }
    const size_t stride = (path.size() + kMaxPathPoints - 1) / kMaxPathPoints;
    size_t kept = 0;
    for (size_t i = 0; i < path.size(); i += stride) {
        path[kept++] = path[i];
    }
    path[kept++] = path.back();
    path.resize(kept);
    path.shrink_to_fit();
}

const char *turnWords(const String &modifier)
{
    static const struct {
        const char *modifier;
        const char *words;
    } kTurns[] = {{"uturn", "Quay dau"},         {"sharp right", "Ngoat phai"}, {"right", "Re phai"},
                  {"slight right", "Cheo phai"}, {"straight", "Di thang"},      {"slight left", "Cheo trai"},
                  {"left", "Re trai"},           {"sharp left", "Ngoat trai"}};
    for (const auto &t : kTurns) {
        if (modifier == t.modifier) {
            return t.words;
        }
    }
    return "Di tiep";
}

// Vietnamese (ASCII) instruction for an OSRM maneuver.
String stepText(const String &type, const String &modifier, int exitNumber, const String &road)
{
    String text;
    bool addRoad = true;
    if (type == "depart") {
        text = "Xuat phat";
        if (road.length() > 0) {
            text += " theo " + road;
        }
        addRoad = false;
    } else if (type == "arrive") {
        text = "Den noi";
        if (modifier.indexOf("left") >= 0) {
            text += " (ben trai)";
        } else if (modifier.indexOf("right") >= 0) {
            text += " (ben phai)";
        }
        addRoad = false;
    } else if (type == "roundabout" || type == "rotary") {
        text = "Vao vong xuyen";
        if (exitNumber > 0) {
            text += ", ra loi thu " + String(exitNumber);
        }
    } else if (type == "exit roundabout" || type == "exit rotary") {
        text = "Ra khoi vong xuyen";
    } else if (type == "fork") {
        text = modifier.indexOf("left") >= 0 ? "Giu ben trai" : "Giu ben phai";
    } else if (type == "merge") {
        text = "Nhap lan";
    } else if (type == "new name" || type == "continue") {
        text = modifier == "straight" || modifier.length() == 0 ? "Di tiep" : turnWords(modifier);
    } else {
        text = turnWords(modifier);
    }
    if (addRoad && road.length() > 0) {
        text += " vao " + road;
    }
    return text;
}

const char *profile(Mode mode)
{
    switch (mode) {
    case Mode::Bike: return "bike";
    case Mode::Foot: return "foot";
    case Mode::Car: break;
    }
    return "car";
}

}  // namespace

bool findPlace(const String &query, Place &out, String &error)
{
    String text = query;
    text.trim();
    if (parseLatLon(text, out.at)) {
        out.name = text;
        return true;
    }
    const String url = String(kGeocodeUrl) + "&lat=" + String(MAP_HOME_LAT, 4) + "&lon=" + String(MAP_HOME_LON, 4) +
                       "&q=" + urlEncode(text);
    JsonDocument doc;
    if (!getJson(url,
                 "{\"features\":[{\"geometry\":{\"coordinates\":true},\"properties\":{\"name\":true,\"street\":true,"
                 "\"housenumber\":true,\"district\":true,\"city\":true}}]}",
                 doc, error)) {
        return false;
    }
    JsonVariantConst feature = doc["features"][0];
    JsonVariantConst coords = feature["geometry"]["coordinates"];
    if (feature.isNull() || coords.size() < 2) {
        error = "Khong tim thay \"" + text + "\"";
        return false;
    }
    out.at.lon = coords[0].as<float>();
    out.at.lat = coords[1].as<float>();
    JsonVariantConst p = feature["properties"];
    out.name = String();
    if (p["name"].is<const char *>()) {
        appendPart(out.name, p["name"]);
    } else {
        appendPart(out.name, p["housenumber"]);
        appendPart(out.name, p["street"]);
    }
    appendPart(out.name, p["district"]);
    appendPart(out.name, p["city"]);
    if (out.name.length() == 0) {
        out.name = text;
    }
    return true;
}

bool findRoute(const GeoPoint &from, const GeoPoint &to, Mode mode, Route &out, String &error)
{
    // OSRM wants "lon,lat;lon,lat"; the profile is chosen by the server path, "driving" is just a placeholder.
    const String url = String(kRouteUrlBase) + profile(mode) + "/route/v1/driving/" + String(from.lon, 6) + "," +
                       String(from.lat, 6) + ";" + String(to.lon, 6) + "," + String(to.lat, 6) + kRouteQuery;
    JsonDocument doc;
    if (!getJson(url,
                 "{\"code\":true,\"routes\":[{\"distance\":true,\"duration\":true,\"geometry\":true,"
                 "\"legs\":[{\"steps\":[{\"distance\":true,\"name\":true,"
                 "\"maneuver\":{\"type\":true,\"modifier\":true,\"exit\":true,\"location\":true}}]}]}]}",
                 doc, error)) {
        if (error.indexOf("400") >= 0) {
            error = "Khong co duong di giua 2 diem nay";
        }
        return false;
    }
    JsonVariantConst route = doc["routes"][0];
    if (route.isNull() || !route["geometry"].is<const char *>()) {
        error = "Khong tim duoc duong";
        return false;
    }
    out = Route();
    out.distanceM = route["distance"].as<float>();
    out.durationS = route["duration"].as<float>();
    decodePolyline(route["geometry"].as<const char *>(), out.path);
    thinPath(out.path);
    for (JsonVariantConst s : route["legs"][0]["steps"].as<JsonArrayConst>()) {
        JsonVariantConst m = s["maneuver"];
        Step step;
        step.at.lon = m["location"][0].as<float>();
        step.at.lat = m["location"][1].as<float>();
        step.distanceM = s["distance"].as<float>();
        step.text = stepText(m["type"].as<String>(), m["modifier"].as<String>(), m["exit"] | 0,
                             toAscii(s["name"].as<const char *>()));
        out.steps.push_back(step);
    }
    if (out.path.size() < 2) {
        error = "Khong tim duoc duong";
        return false;
    }
    return true;
}

String formatDistance(float meters)
{
    char buf[16];
    if (meters < 1000) {
        snprintf(buf, sizeof(buf), "%d m", (int)(lroundf(meters / 10) * 10));
    } else if (meters < 100000) {
        snprintf(buf, sizeof(buf), "%.1f km", meters / 1000);
    } else {
        snprintf(buf, sizeof(buf), "%d km", (int)lroundf(meters / 1000));
    }
    return String(buf);
}

String formatDuration(float seconds)
{
    const long minutes = max(1L, lroundf(seconds / 60));
    char buf[16];
    if (minutes < 60) {
        snprintf(buf, sizeof(buf), "%ld phut", minutes);
    } else {
        snprintf(buf, sizeof(buf), "%ld gio %02ld", minutes / 60, minutes % 60);
    }
    return String(buf);
}

}  // namespace route_client
