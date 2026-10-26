#include "web_portal.h"

#include <WebServer.h>
#include <WiFi.h>

#include "app_store.h"
#include "config.h"

namespace web_portal {
namespace {

constexpr uint16_t kPort = 80;
constexpr size_t kMaxDescription = 2000;
constexpr size_t kMaxAppJson = 16000;

WebServer server(kPort);
Hooks hooks;
bool started = false;

const char *const kPageHead = R"HTML(<!doctype html><html lang="vi"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Claude Apps</title>
<style>body{font-family:system-ui,sans-serif;max-width:640px;margin:0 auto;padding:16px;background:#161618;color:#f0ede6}
h1{color:#d97757;font-size:22px}h2{font-size:17px;margin-top:28px}input,textarea,select{width:100%;box-sizing:border-box;
padding:10px;margin:6px 0;border-radius:8px;border:1px solid #444;background:#222;color:#f0ede6;font-size:16px}
button{padding:10px 18px;border:0;border-radius:8px;background:#d97757;color:#fff;font-size:16px}
.muted{color:#8a8a90;font-size:14px}a{color:#7fd1ff}li{margin:4px 0}</style></head><body>)HTML";

String htmlEscape(const String &text)
{
    String out;
    out.reserve(text.length());
    for (size_t i = 0; i < text.length(); ++i) {
        const char c = text[i];
        switch (c) {
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '&': out += "&amp;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

void sendPage(const String &body)
{
    server.send(200, "text/html; charset=utf-8", String(kPageHead) + body + "</body></html>");
}

void sendMessage(const String &title, const String &text)
{
    sendPage("<h1>" + htmlEscape(title) + "</h1><p>" + htmlEscape(text) + "</p><p><a href=\"/\">&larr; Quay lại</a></p>");
}

void handleRoot()
{
    String body = "<h1>Claude Apps</h1><p class=\"muted\">";
    body += CYD_BOARD_NAME;
    body += " &middot; ";
    body += WiFi.localIP().toString();
    body += "</p>";

    body += "<h2>Tạo app mới</h2><form method=\"post\" action=\"/create\">"
            "<textarea name=\"desc\" rows=\"4\" placeholder=\"Ví dụ: đồng hồ đếm ngược 25 phút, hết giờ thì nháy đèn đỏ\">"
            "</textarea><button>Tạo app</button></form>"
            "<p class=\"muted\">Máy sẽ hiện tiến độ trên màn hình rồi chạy app luôn.</p>";

    body += "<h2>App đã lưu</h2><ul>";
    for (const AppTile &tile : app_store::list()) {
        body += "<li>" + htmlEscape(tile.name) + " &middot; <a href=\"/app?id=" + htmlEscape(tile.id) + "\">JSON</a></li>";
    }
    body += "</ul>";

    body += "<h2>Cài app từ JSON</h2><form method=\"post\" action=\"/install\">"
            "<textarea name=\"json\" rows=\"5\" placeholder=\"{ &quot;name&quot;: ... }\"></textarea>"
            "<button>Cài</button></form>";

    const String model = hooks.currentModel ? hooks.currentModel() : String();
    body += "<h2>Cài đặt Claude</h2><form method=\"post\" action=\"/settings\">"
            "<label>API key (" +
            String(hooks.hasApiKey && hooks.hasApiKey() ? "đã có, để trống = giữ nguyên" : "CHƯA CÓ") +
            ")</label><input name=\"key\" type=\"password\" autocomplete=\"off\" placeholder=\"sk-ant-...\">"
            "<label>Model</label><input name=\"model\" value=\"" +
            htmlEscape(model) +
            "\"><button>Lưu</button></form>"
            "<p class=\"muted\">Key lấy ở platform.claude.com/settings/keys. Trang này không bao giờ hiện lại key.</p>";
    sendPage(body);
}

void handleCreate()
{
    String desc = server.arg("desc");
    desc.trim();
    if (desc.length() == 0 || desc.length() > kMaxDescription) {
        sendMessage("Chưa tạo", "Mô tả trống hoặc quá dài.");
        return;
    }
    if (hooks.requestCreate) {
        hooks.requestCreate(desc);
    }
    sendMessage("Đang tạo app", "Xem tiến độ trên màn hình của máy. Xong app sẽ tự chạy.");
}

void handleInstall()
{
    const String json = server.arg("json");
    if (json.length() == 0 || json.length() > kMaxAppJson) {
        sendMessage("Chưa cài", "JSON trống hoặc quá lớn.");
        return;
    }
    const String error = hooks.installApp ? hooks.installApp(json) : String("không hỗ trợ");
    sendMessage(error.length() == 0 ? "Đã cài app" : "App lỗi", error.length() == 0 ? "App đã có trên màn hình chính." : error);
}

void handleSettings()
{
    String key = server.arg("key");
    String model = server.arg("model");
    key.trim();
    model.trim();
    if (hooks.saveSettings) {
        hooks.saveSettings(key, model);
    }
    sendMessage("Đã lưu", "Cài đặt Claude đã được cập nhật.");
}

void handleApp()
{
    String json;
    if (!app_store::load(server.arg("id"), json)) {
        server.send(404, "text/plain; charset=utf-8", "Không có app này");
        return;
    }
    server.send(200, "application/json; charset=utf-8", json);
}

}  // namespace

void begin(const Hooks &h)
{
    hooks = h;
    server.on("/", HTTP_GET, handleRoot);
    server.on("/create", HTTP_POST, handleCreate);
    server.on("/install", HTTP_POST, handleInstall);
    server.on("/settings", HTTP_POST, handleSettings);
    server.on("/app", HTTP_GET, handleApp);
    server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
}

void loop()
{
#if WEB_PORTAL_ENABLED
    if (!started) {
        if (WiFi.status() != WL_CONNECTED) {
            return;
        }
        server.begin();
        started = true;
        Serial.printf("[web] http://%s/\n", WiFi.localIP().toString().c_str());
    }
    server.handleClient();
#endif
}

bool running()
{
    return started;
}

}  // namespace web_portal
