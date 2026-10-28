#include "net_get.h"

#include <WiFi.h>

// Root CA bundle embedded in the arduino-esp32 core (see claude_client.cpp).
extern const uint8_t kCaBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t kCaBundleEnd[] asm("_binary_x509_crt_bundle_end");

namespace net_get {
namespace {

constexpr int kAttempts = 3;
constexpr uint32_t kRetryDelayMs = 400;
constexpr size_t kTlsErrorLen = 96;
constexpr uint32_t kLowRamBytes = 24 * 1024;  // a TLS handshake needs about this much in one block
constexpr const char *kPeerAlert = "fatal alert";  // mbedTLS: the server ended the handshake

String hostOf(const String &url)
{
    int start = url.indexOf("://");
    start = start < 0 ? 0 : start + 3;
    int end = start;
    while (end < (int)url.length() && url[end] != '/' && url[end] != ':' && url[end] != '?') {
        ++end;
    }
    return url.substring(start, end);
}

// mbedTLS reports certificate problems as "X509 - ..." or with "verify" in the text.
bool certificateProblem(const char *tlsError)
{
    return strstr(tlsError, "X509") != nullptr || strstr(tlsError, "verif") != nullptr ||
           strstr(tlsError, "CERT") != nullptr;
}

// Shared by both get(): `tls` is null for plain HTTP.
int getWith(HTTPClient &http, NetworkClient &client, NetworkClientSecure *tls, const String &url, String &error)
{
    if (WiFi.status() != WL_CONNECTED) {
        error = "Chua co Wi-Fi";
        return HTTPC_ERROR_NOT_CONNECTED;
    }
    const String host = hostOf(url);
    IPAddress ip;
    if (!WiFi.hostByName(host.c_str(), ip)) {
        error = "Khong tim thay may chu " + host + " (DNS)";
        Serial.printf("[net] DNS failed: %s\n", host.c_str());
        return HTTPC_ERROR_CONNECTION_REFUSED;
    }
    bool insecureTried = false;
    char tlsError[kTlsErrorLen] = "";
    int status = HTTPC_ERROR_CONNECTION_REFUSED;
    for (int attempt = 1; attempt <= kAttempts; ++attempt) {
        tlsError[0] = '\0';
        status = http.begin(client, url) ? http.GET() : HTTPC_ERROR_CONNECTION_REFUSED;
        if (status > 0) {
            return status;
        }
        if (tls != nullptr) {
            tls->lastError(tlsError, sizeof(tlsError));
        }
        Serial.printf("[net] try %d: %d %s | tls: %s | heap %u, max block %u | %s\n", attempt, status,
                      HTTPClient::errorToString(status).c_str(), tlsError, (unsigned)ESP.getFreeHeap(),
                      (unsigned)ESP.getMaxAllocHeap(), url.c_str());
        http.end();
        if (strstr(tlsError, kPeerAlert) != nullptr) {
            break;  // the server refuses this TLS version or cipher: retrying cannot help
        }
        if (tls != nullptr && !insecureTried && certificateProblem(tlsError)) {
            Serial.println("[net] certificate check failed, retrying without it (public map data)");
            tls->setInsecure();
            insecureTried = true;
        }
        delay(kRetryDelayMs * attempt);
    }
    error = "Loi mang: " + HTTPClient::errorToString(status);
    if (strstr(tlsError, kPeerAlert) != nullptr) {
        error += " (may chu " + host + " tu choi TLS 1.2 cua ESP32)";
    } else if (tlsError[0] != '\0') {
        error += String(" (") + tlsError + ")";
    }
    if (ESP.getMaxAllocHeap() < kLowRamBytes) {
        error += " - thieu RAM (" + String(ESP.getMaxAllocHeap() / 1024) + " KB)";
    }
    return status;
}

}  // namespace

void useCaBundle(NetworkClientSecure &tls)
{
    tls.setCACertBundle(kCaBundleStart, kCaBundleEnd - kCaBundleStart);
}

int get(HTTPClient &http, NetworkClientSecure &tls, const String &url, String &error)
{
    return getWith(http, tls, &tls, url, error);
}

int get(HTTPClient &http, NetworkClient &plain, const String &url, String &error)
{
    return getWith(http, plain, nullptr, url, error);
}

}  // namespace net_get
