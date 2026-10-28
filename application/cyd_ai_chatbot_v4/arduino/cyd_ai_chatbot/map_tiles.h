// OpenStreetMap raster tiles for the "Ban do" screen. The 256x256 PNG tiles a view needs are first saved
// to a small cache on the flash file system (one TLS connection for all of them), then decoded straight
// onto the display line by line (PNGdec), so the TLS buffers and the decoder are never in RAM together.
#pragma once

#include <Arduino.h>
#include <functional>

namespace map_tiles {

constexpr int kTileSize = 256;
constexpr int kMinZoom = 3;
constexpr int kMaxZoom = 18;

// A screen rectangle showing the map around a point given in world pixels at `zoom` (Web Mercator).
struct View {
    int zoom = kMinZoom;
    double centerX = 0;
    double centerY = 0;
    int16_t left = 0, top = 0, width = 0, height = 0;
};

// Web Mercator world pixel coordinates at `zoom`.
double lonToX(double lon, int zoom);
double latToY(double lat, int zoom);
double yToLat(double y, int zoom);
// Ground meters per screen pixel at a latitude.
double metersPerPixel(double lat, int zoom);

// Downloads the tiles of `view` that are not cached yet; `status` gets short ASCII progress text.
// Returns how many tiles are missing afterwards (no Wi-Fi, server error, full flash...).
int fetch(const View &view, const std::function<void(const String &)> &status);

// Draws the cached tiles of `view`; areas without a tile are filled with `fallback`.
// False when nothing could be decoded (e.g. not enough free RAM for the decoder).
bool draw(const View &view, uint16_t fallback);

}  // namespace map_tiles
