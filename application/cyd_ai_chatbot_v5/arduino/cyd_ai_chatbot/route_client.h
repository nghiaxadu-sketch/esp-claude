// Free map services without an API key, used by the "Ban do" screen: Nominatim (nominatim.openstreetmap.org)
// turns a typed address into coordinates, OSRM (routing.openstreetmap.de) finds the route and its turn-by-turn
// steps.
// Both use OpenStreetMap data. Calls block for a few seconds (HTTPS + JSON).
#pragma once

#include <Arduino.h>
#include <vector>

namespace route_client {

enum class Mode : uint8_t { Car, Bike, Foot };

struct GeoPoint {
    float lat = 0;
    float lon = 0;
};

struct Place {
    GeoPoint at;
    String name;  // ASCII
};

struct Step {
    GeoPoint at;           // where the maneuver happens
    float distanceM = 0;   // distance to the next maneuver
    String text;           // ASCII instruction, e.g. "Re phai vao Pho To Hieu"
};

struct Route {
    float distanceM = 0;
    float durationS = 0;
    std::vector<GeoPoint> path;
    std::vector<Step> steps;
};

// Looks up an address (Vietnamese with or without diacritics) near MAP_HOME_LAT/LON, or reads "lat,lon".
bool findPlace(const String &query, Place &out, String &error);
bool findRoute(const GeoPoint &from, const GeoPoint &to, Mode mode, Route &out, String &error);

// "350 m", "1.2 km".
String formatDistance(float meters);
// "25 phut", "1 gio 05".
String formatDuration(float seconds);

}  // namespace route_client
