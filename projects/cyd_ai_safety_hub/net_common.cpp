#include "net_common.h"

#include <HTTPClient.h>

#include "ca_certs.h"

namespace net {

namespace {
constexpr uint16_t kPlugTimeoutMs = 5000;
}

void setupSecureClient(NetworkClientSecure &client) {
    client.setCACert(kRootCaBundle);
    client.setHandshakeTimeout(20);  // giây
}

String urlEncode(const String &s) {
    static const char kHex[] = "0123456789ABCDEF";
    String out;
    out.reserve(s.length() * 3);
    for (size_t i = 0; i < s.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0x0F];
        }
    }
    return out;
}

bool callPlugUrl(const String &url) {
    if (url.isEmpty() || !online()) return false;
    HTTPClient http;
    http.setTimeout(kPlugTimeoutMs);
    http.setConnectTimeout(kPlugTimeoutMs);
    int code = -1;
    if (url.startsWith("https://")) {
        NetworkClientSecure client;
        setupSecureClient(client);
        if (http.begin(client, url)) code = http.GET();
        http.end();
    } else {
        NetworkClient client;
        if (http.begin(client, url)) code = http.GET();
        http.end();
    }
    return code >= 200 && code < 300;
}

}  // namespace net
