#include "radio_stations.h"

#include "app_store.h"

namespace radio_stations {
namespace {

constexpr const char *kPath = "/radio.txt";
constexpr size_t kMaxStations = 60;
constexpr size_t kMaxFileBytes = 8192;
constexpr char kSep = '|';

// Public plain-HTTP MP3 streams found in Lily OS for the CYD (the Vietnamese stations there are resolved by
// heylily.net for activated devices only, so they are not included: add your own on the web page).
const char *const kDefaults =
    "Kick FM (EU)|http://icecast.kickfm.eu:8000/mp3-high\n"
    "FM 91 Bangkok|http://fm91bkk.com:8955/;\n"
    "FM 94 Army|http://fm94.tv5.co.th:8000/armystream\n"
    "FM 94 Ch3|http://fm94.tv5.co.th:8000/fm94ch3\n"
    "MCOT 9022|http://rstream.mcot.net:9022/;\n"
    "MCOT 9060|http://rstream.mcot.net:9060/;\n"
    "Lanna Radio|http://server1.lannaradio.com:8020/radio.mp3\n"
    "Kiss (Thailand)|http://s1.thailandstreaming.net:8020/kiss.mp3\n";

std::vector<RadioStation> parse(const String &text)
{
    std::vector<RadioStation> list;
    int start = 0;
    while (start < (int)text.length() && list.size() < kMaxStations) {
        int end = text.indexOf('\n', start);
        if (end < 0) {
            end = text.length();
        }
        String line = text.substring(start, end);
        start = end + 1;
        line.trim();
        if (line.length() == 0 || line[0] == '#') {
            continue;
        }
        const int sep = line.indexOf(kSep);
        String name = sep > 0 ? line.substring(0, sep) : line;
        String url = sep > 0 ? line.substring(sep + 1) : line;
        name.trim();
        url.trim();
        if (url.startsWith("http://") || url.startsWith("https://")) {
            list.push_back({name, url});
        }
    }
    return list;
}

String serialize(const std::vector<RadioStation> &stations)
{
    String text;
    for (const RadioStation &s : stations) {
        text += s.name + kSep + s.url + "\n";
    }
    return text;
}

}  // namespace

String text()
{
    fs::FS *fs = app_store::fileSystem();
    if (fs == nullptr || !fs->exists(kPath)) {
        return kDefaults;
    }
    File f = fs->open(kPath, FILE_READ);
    if (!f || f.size() > kMaxFileBytes) {
        return kDefaults;
    }
    String out = f.readString();
    f.close();
    return out;
}

std::vector<RadioStation> load()
{
    return parse(text());
}

bool save(const std::vector<RadioStation> &stations)
{
    fs::FS *fs = app_store::fileSystem();
    if (fs == nullptr) {
        return false;
    }
    File f = fs->open(kPath, FILE_WRITE);
    if (!f) {
        return false;
    }
    const String data = serialize(stations);
    const bool ok = f.print(data) == data.length();
    f.close();
    return ok;
}

size_t setText(const String &text)
{
    const std::vector<RadioStation> list = parse(text);
    return save(list) ? list.size() : 0;
}

}  // namespace radio_stations
