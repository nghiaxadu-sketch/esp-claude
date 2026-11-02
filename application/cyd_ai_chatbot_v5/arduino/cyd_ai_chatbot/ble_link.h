// Bluetooth LE for the OBD-II and tyre pressure screens (NimBLE-Arduino library): turned on only while one of
// those screens is open and fully released afterwards, because Wi-Fi + TLS + BLE do not fit in the ESP32's
// RAM together for long. Scan results are queued from the BLE task and read from the main loop.
#pragma once

#include <Arduino.h>
#include <functional>
#include <string>
#include <vector>

namespace ble_link {

struct Advert {
    String name;     // advertised name, may be empty
    String address;  // "aa:bb:cc:dd:ee:ff"
    uint8_t addressType;
    int rssi;
    std::string manufacturerData;  // raw bytes, company id first
};

bool begin(String &error);
void end();
bool ready();

// Starts scanning; adverts are queued (duplicates too when `duplicates`). durationMs 0 = until stopScan().
bool startScan(uint32_t durationMs, bool active, bool duplicates);
void stopScan();
bool scanning();
// Moves queued adverts into `out` (keeps at most a few dozen while nobody reads).
size_t takeAdverts(std::vector<Advert> &out);

// GATT connection to one device with a "serial port" characteristic pair (notify + write), as used by
// Bluetooth ELM327 adapters. Received bytes are buffered until readBytes().
bool connectSerial(const String &address, uint8_t addressType, String &error);
void disconnect();
bool serialConnected();
bool writeBytes(const uint8_t *data, size_t len);
size_t readBytes(char *out, size_t max);

}  // namespace ble_link
