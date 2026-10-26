#include "app_store.h"

#include <FFat.h>
#include <LittleFS.h>
#include <algorithm>

#include "builtin_apps.h"
#include "mini_app.h"
#include "mini_json.h"
#include "text_utils.h"
#include "ui_common.h"

namespace app_store {
namespace {

constexpr const char *kDir = "/apps";
constexpr const char *kAppExt = ".json";
constexpr const char *kStateExt = ".state";
constexpr const char *kSamplesMarker = "/apps/.samples_v2";  // bump when new sample apps are added
constexpr size_t kMaxIdChars = 20;
constexpr size_t kMaxAppBytes = 16384;
constexpr int kMaxIdSuffix = 99;
constexpr uint16_t kDefaultTileColor = ui::rgb565(0x3A, 0x3A, 0x40);

fs::FS *fsys = nullptr;

String appPath(const String &id)
{
    return String(kDir) + "/" + id + kAppExt;
}

String statePath(const String &id)
{
    return String(kDir) + "/" + id + kStateExt;
}

bool readFile(const String &path, String &out, size_t maxBytes)
{
    File f = fsys->open(path, FILE_READ);
    if (!f) {
        return false;
    }
    const size_t size = f.size();
    if (size > maxBytes) {
        f.close();
        return false;
    }
    out = "";
    out.reserve(size + 1);
    while (f.available()) {
        out += (char)f.read();
    }
    f.close();
    return true;
}

bool writeFile(const String &path, const String &text)
{
    File f = fsys->open(path, FILE_WRITE);
    if (!f) {
        return false;
    }
    const size_t written = f.print(text);
    f.close();
    return written == text.length();
}

// Installs the sample apps that are missing, once per sample set: boards upgraded from an older
// version get the new samples (e.g. the weather app) without touching the apps already there.
void installSamples()
{
    if (fsys->exists(kSamplesMarker)) {
        return;
    }
    size_t installed = 0;
    for (size_t i = 0; i < builtin_apps::kAppCount; ++i) {
        const String id = builtin_apps::kApps[i].id;
        if (!fsys->exists(appPath(id)) && save(id, builtin_apps::kApps[i].json)) {
            ++installed;
        }
    }
    writeFile(kSamplesMarker, "1");
    Serial.printf("[apps] installed %u sample apps\n", (unsigned)installed);
}

}  // namespace

bool begin()
{
    // "Huge APP" schemes have a SPIFFS partition (LittleFS uses it); "... FATFS" schemes have an FFat one.
    if (LittleFS.begin(true)) {
        fsys = &LittleFS;
        Serial.println("[apps] LittleFS mounted");
    } else if (FFat.begin(true)) {
        fsys = &FFat;
        Serial.println("[apps] FFat mounted");
    } else {
        Serial.println("[apps] no file system partition, apps cannot be saved");
        return false;
    }
    if (!fsys->exists(kDir)) {
        fsys->mkdir(kDir);
    }
    installSamples();
    return true;
}

bool ready()
{
    return fsys != nullptr;
}

AppTile tileFor(const String &id, const String &json)
{
    AppTile tile = {id, id, kDefaultTileColor};
    mini_json::Value doc;
    std::string err;
    if (mini_json::parse(json.c_str(), doc, err)) {
        const std::string name = doc.getString("name");
        if (!name.empty()) {
            tile.name = String(text_utils::utf8ToAscii(name).c_str());
        }
        uint16_t color;
        if (mini_app::parseColor(doc.getString("color"), color)) {
            tile.color = color;
        }
    }
    return tile;
}

std::vector<AppTile> list()
{
    std::vector<AppTile> tiles;
    if (!ready()) {
        return tiles;
    }
    File dir = fsys->open(kDir);
    if (!dir || !dir.isDirectory()) {
        return tiles;
    }
    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        String name = entry.name();
        entry.close();
        const int slash = name.lastIndexOf('/');
        if (slash >= 0) {
            name = name.substring(slash + 1);
        }
        if (!name.endsWith(kAppExt)) {
            continue;
        }
        const String id = name.substring(0, name.length() - strlen(kAppExt));
        String json;
        if (load(id, json)) {
            tiles.push_back(tileFor(id, json));
        }
    }
    dir.close();
    std::sort(tiles.begin(), tiles.end(), [](const AppTile &a, const AppTile &b) { return a.name < b.name; });
    return tiles;
}

bool load(const String &id, String &json)
{
    return ready() && readFile(appPath(id), json, kMaxAppBytes);
}

bool save(const String &id, const String &json)
{
    return ready() && writeFile(appPath(id), json);
}

bool remove(const String &id)
{
    if (!ready()) {
        return false;
    }
    fsys->remove(statePath(id));
    return fsys->remove(appPath(id));
}

String loadState(const String &id)
{
    String json;
    if (ready() && fsys->exists(statePath(id)) && readFile(statePath(id), json, kMaxAppBytes)) {
        return json;
    }
    return String();
}

void saveState(const String &id, const String &json)
{
    if (ready() && json.length() > 0) {
        writeFile(statePath(id), json);
    }
}

String makeId(const String &name, const String &keep)
{
    const std::string ascii = text_utils::utf8ToAscii(name.c_str());
    String base;
    for (char c : ascii) {
        if (base.length() >= kMaxIdChars) {
            break;
        }
        if (c >= 'A' && c <= 'Z') {
            base += (char)(c - 'A' + 'a');
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            base += c;
        } else if (base.length() > 0 && !base.endsWith("_")) {
            base += '_';
        }
    }
    while (base.endsWith("_")) {
        base.remove(base.length() - 1);
    }
    if (base.length() == 0) {
        base = "app";
    }
    if (!ready() || base == keep || !fsys->exists(appPath(base))) {
        return base;
    }
    for (int n = 2; n <= kMaxIdSuffix; ++n) {
        const String candidate = base + "_" + String(n);
        if (candidate == keep || !fsys->exists(appPath(candidate))) {
            return candidate;
        }
    }
    return base + "_" + String(millis() % 100000);
}

}  // namespace app_store
