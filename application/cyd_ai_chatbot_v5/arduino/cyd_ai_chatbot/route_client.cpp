#include "route_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include "config.h"
#include "net_get.h"
#include "text_utils.h"

namespace route_client {
namespace {

constexpr uint16_t kHttpTimeoutMs = 15000;
constexpr const char *kUserAgent = "CYD-Claude-Apps/5.0 (ESP32 directions display)";
// Nominatim (OSM Foundation) still speaks TLS 1.2; Photon and routing.openstreetmap.de only accept TLS 1.3,
// which the arduino-esp32 core does not have. The route server is therefore reached over plain HTTP: the
// request only carries two coordinates.
constexpr const char *kGeocodeUrl =
    "https://nominatim.openstreetmap.org/search?format=jsonv2&limit=1&countrycodes=vn&accept-language=vi";
constexpr float kGeocodeBiasDeg = 0.3f;        // preferred box around MAP_HOME_LAT/LON (not a hard limit)
constexpr uint32_t kGeocodeMinGapMs = 1100;    // Nominatim usage policy: at most 1 request per second
constexpr size_t kPlaceNameParts = 3;          // "Aeon Mall Hai Phong - Le Chan, Duong Vo Nguyen Giap, ..."
constexpr const char *kRouteUrlBase = "http://routing.openstreetmap.de/routed-";
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
    NetworkClientSecure tls;
    NetworkClient plain;
    net_get::useCaBundle(tls);
    HTTPClient http;
    http.useHTTP10(true);  // no chunked encoding, so JSON can be parsed from the stream
    http.setTimeout(kHttpTimeoutMs);
    http.setConnectTimeout(kHttpTimeoutMs);
    http.setUserAgent(kUserAgent);
    const int status =
        url.startsWith("https://") ? net_get::get(http, tls, url, error) : net_get::get(http, plain, url, error);
    Serial.printf("[map] %d %s\n", status, url.c_str());
    if (status != HTTP_CODE_OK) {
        if (status > 0) {
            error = "May chu bao loi " + String(status);
        }
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
    static uint32_t lastGeocodeMs = 0;
    const uint32_t sinceLast = millis() - lastGeocodeMs;
    if (lastGeocodeMs != 0 && sinceLast < kGeocodeMinGapMs) {
        delay(kGeocodeMinGapMs - sinceLast);
    }
    // viewbox = left,top,right,bottom (lon,lat,lon,lat).
    const String url = String(kGeocodeUrl) + "&viewbox=" + String(MAP_HOME_LON - kGeocodeBiasDeg, 3) + "," +
                       String(MAP_HOME_LAT + kGeocodeBiasDeg, 3) + "," + String(MAP_HOME_LON + kGeocodeBiasDeg, 3) +
                       "," + String(MAP_HOME_LAT - kGeocodeBiasDeg, 3) + "&q=" + urlEncode(text);
    JsonDocument doc;
    const bool ok = getJson(url, "[{\"lat\":true,\"lon\":true,\"name\":true,\"display_name\":true}]", doc, error);
    lastGeocodeMs = millis();
    if (!ok) {
        return false;
    }
    JsonVariantConst place = doc[0];
    if (place.isNull() || !place["lat"].is<const char *>() || !place["lon"].is<const char *>()) {
        error = "Khong tim thay \"" + text + "\"";
        return false;
    }
    out.at.lat = atof(place["lat"].as<const char *>());
    out.at.lon = atof(place["lon"].as<const char *>());
    out.name = String();
    appendPart(out.name, place["name"]);
    // The first parts of the full address: house/street, ward...
    const String full = place["display_name"] | "";
    int from = 0;
    for (size_t part = 0; part < kPlaceNameParts && from < (int)full.length(); ++part) {
        int comma = full.indexOf(',', from);
        if (comma < 0) {
            comma = full.length();
        }
        String piece = full.substring(from, comma);
        piece.trim();
        appendPart(out.name, piece.c_str());
        from = comma + 1;
    }
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
