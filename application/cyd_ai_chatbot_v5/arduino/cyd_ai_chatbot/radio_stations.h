// Radio station list: kept in /radio.txt on the flash file system ("Ten dai|http://...", one per line) so it
// can be edited on the board's web page; a few public MP3 streams are installed the first time.
// Only plain-HTTP MP3 (Shoutcast / Icecast) streams play on the CYD.
#pragma once

#include <Arduino.h>
#include <vector>

struct RadioStation {
    String name;  // UTF-8 as typed
    String url;
};

namespace radio_stations {

std::vector<RadioStation> load();
bool save(const std::vector<RadioStation> &stations);
// The whole file as text, for the web page editor.
String text();
// Replaces the list with parsed text; returns how many stations were kept.
size_t setText(const String &text);

}  // namespace radio_stations
