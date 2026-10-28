// HTTPS GET with the checks the map services need on the CYD: Wi-Fi and DNS are checked first, failed TLS
// connections are retried, and the error text says why (DNS, certificate, RAM...) instead of only
// "connection refused", which HTTPClient reports for every failed connect.
#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

namespace net_get {

// Prepares `tls` with the root CA bundle of the arduino-esp32 core.
void useCaBundle(NetworkClientSecure &tls);

// Opens `url` on `http` (already configured: timeouts, user agent...) and sends the GET. Returns the HTTP
// status (> 0) with the response ready to read, or a negative HTTPClient error with `error` set to short
// ASCII text. The certificate is always checked first; when only the check fails, one more try is made
// without it, since this is only used for public map data that carries no secrets.
int get(HTTPClient &http, NetworkClientSecure &tls, const String &url, String &error);
// Same over plain HTTP, for services whose HTTPS needs TLS 1.3 (the arduino-esp32 core only has TLS 1.2).
int get(HTTPClient &http, NetworkClient &plain, const String &url, String &error);

}  // namespace net_get
