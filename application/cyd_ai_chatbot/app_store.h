// Saved apps on the flash file system (LittleFS on the "spiffs" partition, or FFat on a FAT partition).
// Each app is /apps/<id>.json; values of its "persist" variables live in /apps/<id>.state.
#pragma once

#include <Arduino.h>
#include <vector>

#include "launcher_ui.h"

namespace app_store {

// Mounts the file system (formats it on first use) and installs the sample apps when there are none.
bool begin();
bool ready();

std::vector<AppTile> list();
bool load(const String &id, String &json);
bool save(const String &id, const String &json);
bool remove(const String &id);

String loadState(const String &id);
void saveState(const String &id, const String &json);

// File-safe id from an app name ("Đồng hồ" -> "dong_ho"), unique among saved apps unless it equals `keep`.
String makeId(const String &name, const String &keep = String());

// Name and tile color from an app's JSON.
AppTile tileFor(const String &id, const String &json);

}  // namespace app_store
