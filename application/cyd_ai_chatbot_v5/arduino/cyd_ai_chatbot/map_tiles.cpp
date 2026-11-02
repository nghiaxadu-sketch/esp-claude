#include "map_tiles.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <PNGdec.h>
#include <WiFi.h>
#include <math.h>
#include <new>
#include <vector>

#include "app_store.h"
#include "board_hw.h"
#include "config.h"
#include "net_get.h"

namespace map_tiles {
namespace {

constexpr const char *kCacheDir = "/tiles";
constexpr const char *kTempPath = "/tiles/download.tmp";
constexpr size_t kMaxCachedTiles = 24;  // ~25 KB each; the flash also holds the apps
constexpr uint16_t kHttpTimeoutMs = 10000;
constexpr const char *kUserAgent = "CYD-Claude-Apps/1.0 (personal ESP32 directions display)";
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthCircumferenceM = 40075016.686;

struct Tile {
    int z, x, y;  // x already wrapped around the world
    int16_t screenX, screenY;
};

// The tiles covering `view`, with where their top-left corner lands on the screen.
std::vector<Tile> tilesOf(const View &view)
{
    std::vector<Tile> tiles;
    const double left = view.centerX - view.width / 2.0;
    const double top = view.centerY - view.height / 2.0;
    const int count = 1 << view.zoom;
    const int x0 = (int)floor(left / kTileSize), x1 = (int)floor((left + view.width - 1) / kTileSize);
    const int y0 = (int)floor(top / kTileSize), y1 = (int)floor((top + view.height - 1) / kTileSize);
    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            Tile t;
            t.z = view.zoom;
            t.x = ((tx % count) + count) % count;
            t.y = ty;
            t.screenX = (int16_t)(view.left + (int)floor(tx * (double)kTileSize - left));
            t.screenY = (int16_t)(view.top + (int)floor(ty * (double)kTileSize - top));
            tiles.push_back(t);
        }
    }
    return tiles;
}

bool validTile(const Tile &t)
{
    return t.y >= 0 && t.y < (1 << t.z);
}

String tilePath(const Tile &t)
{
    return String(kCacheDir) + "/" + String(t.z) + "_" + String(t.x) + "_" + String(t.y) + ".png";
}

String tileUrl(const Tile &t)
{
    char url[128];
    snprintf(url, sizeof(url), MAP_TILE_URL_FORMAT, t.z, t.x, t.y);
    return String(url);
}

// Keeps the cache small: when new tiles would not fit, every tile the current view does not need goes.
void makeRoom(fs::FS &fsys, const std::vector<String> &keep, size_t incoming)
{
    std::vector<String> names;
    File dir = fsys.open(kCacheDir);
    if (!dir || !dir.isDirectory()) {
        return;
    }
    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        String name = entry.name();
        entry.close();
        const int slash = name.lastIndexOf('/');
        names.push_back(String(kCacheDir) + "/" + (slash >= 0 ? name.substring(slash + 1) : name));
    }
    dir.close();
    if (names.size() + incoming <= kMaxCachedTiles) {
        return;
    }
    for (const String &path : names) {
        bool needed = false;
        for (const String &k : keep) {
            needed = needed || k == path;
        }
        if (!needed) {
            fsys.remove(path);
        }
    }
}

// ---------------------------------------------------------------- PNG decoding from the cache

fs::FS *decodeFs = nullptr;
String fetchError;  // why the last fetch() left tiles missing, empty when none are

struct Decoder {
    PNG png;
    uint16_t line[kTileSize];
    int16_t screenX = 0, screenY = 0;
    int16_t clipLeft = 0, clipRight = 0, clipTop = 0, clipBottom = 0;
};

void *openTile(const char *path, int32_t *size)
{
    File *f = new (std::nothrow) File(decodeFs->open(path, FILE_READ));
    if (f == nullptr || !*f) {
        delete f;
        return nullptr;
    }
    *size = (int32_t)f->size();
    return f;
}

void closeTile(void *handle)
{
    File *f = static_cast<File *>(handle);
    if (f != nullptr) {
        f->close();
        delete f;
    }
}

int32_t readTile(PNGFILE *file, uint8_t *buffer, int32_t length)
{
    File *f = static_cast<File *>(file->fHandle);
    const int32_t n = (int32_t)f->read(buffer, (size_t)length);
    file->iPos = (int32_t)f->position();
    return n;
}

int32_t seekTile(PNGFILE *file, int32_t position)
{
    File *f = static_cast<File *>(file->fHandle);
    f->seek((uint32_t)position);
    file->iPos = (int32_t)f->position();
    return file->iPos;
}

// One decoded row of a tile: only the part inside the map area is sent to the display.
int drawRow(PNGDRAW *row)
{
    Decoder *d = static_cast<Decoder *>(row->pUser);
    const int16_t y = d->screenY + row->y;
    if (y >= d->clipBottom) {
        return 0;  // below the map: stop decoding this tile
    }
    if (y < d->clipTop) {
        return 1;
    }
    d->png.getLineAsRGB565(row, d->line, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
    const int16_t from = max<int16_t>(0, d->clipLeft - d->screenX);
    const int16_t to = min<int16_t>((int16_t)row->iWidth, d->clipRight - d->screenX);
    if (to > from) {
        board_hw::pushImage(d->screenX + from, y, to - from, 1, d->line + from);
    }
    return 1;
}

}  // namespace

double lonToX(double lon, int zoom)
{
    return (lon + 180.0) / 360.0 * kTileSize * (double)(1 << zoom);
}

double latToY(double lat, int zoom)
{
    const double rad = lat * kPi / 180.0;
    return (1.0 - log(tan(rad) + 1.0 / cos(rad)) / kPi) / 2.0 * kTileSize * (double)(1 << zoom);
}

double yToLat(double y, int zoom)
{
    const double n = kPi - 2.0 * kPi * y / (kTileSize * (double)(1 << zoom));
    return atan(sinh(n)) * 180.0 / kPi;
}

double metersPerPixel(double lat, int zoom)
{
    return kEarthCircumferenceM * cos(lat * kPi / 180.0) / (kTileSize * (double)(1 << zoom));
}

int fetch(const View &view, const std::function<void(const String &)> &status)
{
    const std::vector<Tile> tiles = tilesOf(view);
    fs::FS *fsys = app_store::fileSystem();
    std::vector<Tile> missing;
    std::vector<String> keep;
    for (const Tile &t : tiles) {
        if (!validTile(t)) {
            continue;
        }
        keep.push_back(tilePath(t));
        if (fsys == nullptr || !fsys->exists(keep.back())) {
            missing.push_back(t);
        }
    }
    fetchError = String();
    if (!missing.empty() && fsys == nullptr) {
        fetchError = "Khong co bo nho de luu ban do";
    } else if (!missing.empty() && WiFi.status() != WL_CONNECTED) {
        fetchError = "Chua co Wi-Fi";
    }
    if (missing.empty() || fetchError.length() > 0) {
        return (int)missing.size();
    }
    if (!fsys->exists(kCacheDir)) {
        fsys->mkdir(kCacheDir);
    }
    makeRoom(*fsys, keep, missing.size());

    NetworkClientSecure tls;
    net_get::useCaBundle(tls);
    HTTPClient http;
    http.setReuse(true);  // one TLS handshake for all tiles of the view
    http.setTimeout(kHttpTimeoutMs);
    http.setConnectTimeout(kHttpTimeoutMs);
    http.setUserAgent(kUserAgent);
    int failed = 0;
    for (size_t i = 0; i < missing.size(); ++i) {
        status("Dang tai ban do " + String(i + 1) + "/" + String(missing.size()) + "...");
        const String url = tileUrl(missing[i]);
        bool ok = false;
        String error;
        const int code = net_get::get(http, tls, url, error);
        if (code == HTTP_CODE_OK) {
            File out = fsys->open(kTempPath, FILE_WRITE);
            const int written = out ? http.writeToStream(&out) : -1;
            out.close();
            ok = written > 0 && fsys->rename(kTempPath, tilePath(missing[i]));
        } else {
            Serial.printf("[map] tile %d %s %s\n", code, error.c_str(), url.c_str());
        }
        http.end();
        if (code < 0) {
            // The server cannot be reached: the other tiles would only wait for the same timeouts.
            failed += (int)(missing.size() - i);
            fetchError = error;
            break;
        }
        if (!ok) {
            fsys->remove(kTempPath);
            ++failed;
            fetchError = code == HTTP_CODE_OK ? String("Khong luu duoc o ban do (bo nho day?)")
                                              : "May chu ban do bao loi " + String(code);
        }
    }
    return failed;
}

const String &lastError()
{
    return fetchError;
}

bool draw(const View &view, uint16_t fallback)
{
    Adafruit_GFX &gfx = board_hw::display();
    decodeFs = app_store::fileSystem();
    // The decoder is ~45 KB; it only exists while the tiles are drawn.
    Decoder *d = decodeFs != nullptr ? new (std::nothrow) Decoder() : nullptr;
    bool drewAny = false;
    for (const Tile &t : tilesOf(view)) {
        const int16_t x0 = max<int16_t>(view.left, t.screenX);
        const int16_t y0 = max<int16_t>(view.top, t.screenY);
        const int16_t x1 = min<int16_t>(view.left + view.width, t.screenX + kTileSize);
        const int16_t y1 = min<int16_t>(view.top + view.height, t.screenY + kTileSize);
        if (x1 <= x0 || y1 <= y0) {
            continue;
        }
        bool ok = false;
        const String path = tilePath(t);
        if (d != nullptr && validTile(t) && decodeFs->exists(path)) {
            d->screenX = t.screenX;
            d->screenY = t.screenY;
            d->clipLeft = view.left;
            d->clipRight = view.left + view.width;
            d->clipTop = view.top;
            d->clipBottom = view.top + view.height;
            if (d->png.open(path.c_str(), openTile, closeTile, readTile, seekTile, drawRow) == PNG_SUCCESS) {
                const int rc = d->png.decode(d, 0);
                d->png.close();
                ok = rc == PNG_SUCCESS || rc == PNG_QUIT_EARLY;
            }
            if (!ok) {
                decodeFs->remove(path);  // a broken download: fetch it again next time
            }
        }
        if (!ok) {
            gfx.fillRect(x0, y0, x1 - x0, y1 - y0, fallback);
        }
        drewAny = drewAny || ok;
    }
    delete d;
    return drewAny;
}

}  // namespace map_tiles
