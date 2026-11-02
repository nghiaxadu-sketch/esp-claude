// Application flow: launcher, chat, Wi-Fi screen, app builder and app runner
// (kept out of the .ino so Arduino's prototype generator leaves it alone).

#include <WiFi.h>
#include <esp_system.h>
#include <map>
#include <time.h>

#include "chatbot_app.h"
#include "app_builder.h"
#include "app_device.h"
#include "app_store.h"
#include "app_validator.h"
#include "audio_player.h"
#include "board_hw.h"
#include "board_pins.h"
#include "calendar_ui.h"
#include "chat_ui.h"
#include "claude_async.h"
#include "claude_client.h"
#include "clock_ui.h"
#include "config.h"
#include "face_ui.h"
#include "firmware_version.h"
#include "games_ui.h"
#include "launcher_ui.h"
#include "lunar_calendar.h"
#include "map_ui.h"
#include "media_ui.h"
#include "mini_app.h"
#include "mini_json.h"
#include "obd_ui.h"
#include "personas.h"
#include "pin_ui.h"
#include "qr_code.h"
#include "radio_stations.h"
#include "route_client.h"
#include "settings_store.h"
#include "settings_ui.h"
#include "touch_keyboard.h"
#include "tpms_ui.h"
#include "ui_common.h"
#include "weather_client.h"
#include "web_portal.h"
#include "wifi_manager.h"
#include "wifi_ui.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kWifiRetryIntervalMs = 60000;
constexpr uint32_t kClockSyncWaitMs = 5000;
constexpr uint32_t kStatusRefreshMs = 1000;
constexpr uint32_t kBootHoldWindowMs = 1500;
constexpr uint32_t kTouchReleaseMs = 60;      // touch must be gone this long to count as released
constexpr uint32_t kLauncherHoldMs = 700;     // long press on a tile opens its menu
constexpr int16_t kDragMinPx = 2;             // stylus movement that counts as a drag
constexpr int16_t kHoldSlopPx = 12;           // a long press must stay this close to where it started
constexpr int16_t kSwipeMinPx = 50;           // horizontal movement that turns a tap into a page swipe
constexpr uint32_t kProgressRedrawMs = 300;
constexpr size_t kSerialLineMax = 512;
constexpr size_t kChatInputMax = 300;
constexpr size_t kFaceInputMax = 200;
constexpr size_t kAppInputMax = 300;
constexpr size_t kPasswordMax = 64;
constexpr size_t kMapInputMax = 80;
constexpr size_t kNameInputMax = 32;
constexpr size_t kLocationInputMax = 80;
constexpr int16_t kSwipeBackPx = 80;      // left-to-right swipe that goes back ("vuot trai -> phai de thoat")
constexpr int16_t kSwipeMaxDy = 60;
constexpr int kChimeFirstHour = 6;
constexpr int kChimeLastHour = 22;
constexpr uint32_t kChimeToneHz = 1320;
constexpr uint32_t kChimeToneMs = 180;
constexpr uint32_t kChimeGapMs = 260;
constexpr const char *kApNamePrefix = "CYD-Claude-";
constexpr int16_t kApQrX = 214;
constexpr int16_t kApQrY = 70;
constexpr int16_t kApQrMaxSide = 100;
constexpr size_t kSplashChars = 52;
constexpr int16_t kProgressY = 150;
constexpr int16_t kProgressH = 12;
constexpr const char *kNtpServer1 = "pool.ntp.org";
constexpr const char *kNtpServer2 = "time.google.com";
constexpr time_t kValidEpoch = 1700000000;  // anything earlier means NTP has not answered yet

constexpr const char *kChatSystemPrompt =
    "You are a friendly AI assistant running on a tiny 2.8-inch 320x240 screen (ESP32 \"Cheap Yellow Display\"). "
    "The user types on a small touch keyboard, so questions are short and often Vietnamese without diacritics; "
    "read them as normal Vietnamese. Reply in the user's language. "
    "Keep answers short: at most about 80 words unless the user asks for more detail. "
    "Use plain text only: no Markdown, no tables, no emoji. Short lines and simple '-' lists are fine. "
    "If the user wants a tool, game or gadget on the device, tell them to press \"+ App\" on the home screen "
    "and describe it: you will then write it as an app that runs on the board.";

// The face shows the mood tag as an expression, so every reply starts with one.
constexpr const char *kFaceSystemPrompt =
    "You are Claude, appearing as a cute robot face with big glowing eyes and a mouth on the small 320x240 screen "
    "of an ESP32 gadget on the user's desk. You have no camera or microphone: the user types short messages on a "
    "touch keyboard, often Vietnamese without diacritics; read them as normal Vietnamese and answer in the user's "
    "language. Start EVERY reply with exactly one mood tag that matches your feeling: [happy] [sad] [angry] "
    "[surprised] [love] [thinking] [sleepy] [wink] [neutral]. After the tag write 1 to 3 short spoken sentences, "
    "at most 45 words, warm and playful like a friendly desk pet. Plain text only: no Markdown, lists, emoji or "
    "other brackets.";

// Models offered by Settings > AI Model (tap to switch).
constexpr const char *kModels[] = {"claude-sonnet-5-5", "claude-opus-5-5", "claude-haiku-5-5"};

enum class Screen : uint8_t {
    Launcher,
    Chat,
    Keyboard,
    Wifi,
    App,
    Busy,
    Settings,
    Games,
    Face,
    Map,
    Clock,
    Calendar,
    Media,
    Obd,
    Tpms,
    Pin,
    ApSetup,
};
enum class InputPurpose : uint8_t {
    ChatQuestion,
    NewApp,
    EditApp,
    WifiPassword,
    FaceQuestion,
    MapFrom,
    MapTo,
    UserName,
    Location,
    ObdWifiPassword,
};
// What a PIN pad shown right now is for.
enum class PinPurpose : uint8_t { OpenLocked, UnlockSettings, SetPin, ChangePin };

Adafruit_GFX &tft = board_hw::display();
ChatUi chat(tft);
LauncherUi launcher(tft);
WifiUi wifiScreen(tft);
TouchKeyboard keyboard(tft);
SettingsUi settingsUi(tft);
GamesUi games(tft);
MapUi mapUi(tft);
FaceUi face(tft);
ClockUi clockUi(tft);
CalendarUi calendarUi(tft);
MediaUi media(tft);
ObdUi obdUi(tft);
TpmsUi tpmsUi(tft);
PinUi pinUi(tft);
ClaudeClient claude;
ClaudeAsync claudeAsync(claude);
AppBuilder builder(claude);
GfxCanvas canvas(tft);
BoardDevice device;
mini_app::MiniApp runningApp(canvas, device);
AppSettings settings;
DeviceSettings deviceCfg;
ProfileSettings profile;
std::vector<ChatMessage> history;
std::vector<ChatMessage> faceHistory;

Screen screen = Screen::Launcher;
Screen pressScreen = Screen::Launcher;  // screen that received the current press
InputPurpose inputPurpose = InputPurpose::ChatQuestion;
std::vector<AppTile> tiles;
std::vector<WifiNetwork> scanResults;
std::map<String, String> lastErrors;  // app id -> most recent runtime error, offered to Claude on "Sua"
String runningAppId;
String editAppId;
String pendingSsid;
String pendingWebCreate;
String serialLine;

bool touchDown = false;
bool longFired = false;
bool swallowTouch = false;  // the press that woke the screen does nothing else
bool deviceDirty = false;   // Settings changed, saved when the stylus lifts
bool screenAsleep = false;
bool wifiFromSettings = false;  // "<" on the Wi-Fi screen returns to Settings
uint32_t lastActivityMs = 0;
int16_t pressX = 0, pressY = 0;
int16_t lastX = 0, lastY = 0;
uint32_t pressStartMs = 0;
uint32_t lastTouchSeenMs = 0;
bool bootWasDown = false;
bool wasConnected = false;
uint32_t lastStatusMs = 0;
uint32_t lastWifiAttemptMs = 0;
int16_t pressDy = 0;               // vertical travel of the current press (swipe-back detection)
bool appsUnlocked = false;         // PIN typed since the screen last went to sleep
PinPurpose pinPurpose = PinPurpose::OpenLocked;
LauncherAction pendingLocked = LauncherAction::None;  // opened after the PIN is accepted
size_t pendingLockedApp = 0;
String pendingWebSsid;             // Wi-Fi chosen on the web page, joined by the main loop
String pendingWebPassword;
bool timezoneSynced = false;
int lastChimeHour = -1;
uint32_t chimeStepMs = 0;
uint8_t chimeBeepsLeft = 0;
qr_code::Code apQr;

// ---------------------------------------------------------------- helpers

String clockText()
{
    const time_t now = time(nullptr);
    if (now < kValidEpoch) {
        return String();
    }
    struct tm local;
    localtime_r(&now, &local);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", local.tm_hour, local.tm_min);
    return String(buf);
}

String statusText()
{
    if (!wifi_manager::connected()) {
        return "Mat Wi-Fi";
    }
    const String clock = clockText();
    return clock.length() > 0 ? clock : wifi_manager::ip();
}

// Right side of the launcher's top line.
String launcherStatusText()
{
    return wifi_manager::connected() ? ui::ellipsize(ui::toAscii(wifi_manager::ssid()), 24) : String("Mat Wi-Fi");
}

String wifiStatusText()
{
    if (!wifi_manager::connected()) {
        return settings.wifiSsid.length() > 0 ? "Chua ket noi (da luu: " + ui::toAscii(settings.wifiSsid) + ")"
                                               : String("Chua ket noi. Cham vao mot mang de noi.");
    }
    String text = "Da noi: " + ui::toAscii(wifi_manager::ssid()) + "  " + wifi_manager::ip();
    if (web_portal::running()) {
        text += "  (web: http://" + wifi_manager::ip() + ")";
    }
    return text;
}

int32_t timezoneOffset()
{
    return deviceCfg.autoTimezone ? deviceCfg.tzOffsetSec : (int32_t)TIMEZONE_OFFSET_SECONDS;
}

String timezoneText()
{
    const int32_t off = timezoneOffset();
    char buf[16];
    if (off % 3600 == 0) {
        snprintf(buf, sizeof(buf), "UTC%+ld", (long)(off / 3600));
    } else {
        snprintf(buf, sizeof(buf), "UTC%+ld:%02ld", (long)(off / 3600), (long)(labs(off) / 60 % 60));
    }
    return String(buf);
}

void startClockSync(bool wait)
{
    configTime(timezoneOffset(), 0, kNtpServer1, kNtpServer2);
    const uint32_t start = millis();
    while (wait && time(nullptr) < kValidEpoch && millis() - start < kClockSyncWaitMs) {
        delay(100);
    }
}

void hideAll()
{
    if (screen == Screen::ApSetup) {
        web_portal::stopAccessPoint();
    }
    chat.hide();
    launcher.hide();
    wifiScreen.hide();
    settingsUi.hide();
    games.hide();
    mapUi.hide();
    face.hide();
    clockUi.hide();
    calendarUi.hide();
    media.hide();  // playback goes on in the background
    if (obdUi.visible()) {
        obdUi.hide();  // closes the adapter and turns Bluetooth off
    }
    if (tpmsUi.visible()) {
        tpmsUi.hide();
    }
    pinUi.hide();
    // The face, games, map and Claude apps are drawn for a dark background: they switch to the dark palette
    // (keeping the accent) and every other screen gets the chosen theme back here.
    ui::applyTheme(deviceCfg.lightTheme, deviceCfg.accent);
}

void useDarkPalette()
{
    ui::applyTheme(false, deviceCfg.accent);
}

void showLauncher(const String &notice = String())
{
    hideAll();
    wifiFromSettings = false;
    screen = Screen::Launcher;
    tiles = app_store::list();
    launcher.setStatus(clockText(), launcherStatusText());
    launcher.setNotice(notice);
    launcher.show(tiles);
}

// Short ASCII reason for the last restart; a crash or watchdog here points at a firmware bug.
String resetReasonText()
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "bat nguon";
    case ESP_RST_SW: return "khoi dong lai (lenh)";
    case ESP_RST_PANIC: return "LOI (crash)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "LOI (watchdog)";
    case ESP_RST_BROWNOUT: return "sut ap nguon";
    case ESP_RST_DEEPSLEEP: return "thuc day";
    case ESP_RST_EXT: return "nut RST";
    default: return "khac (" + String((int)esp_reset_reason()) + ")";
    }
}

SettingsInfo settingsInfo()
{
    SettingsInfo info;
    info.model = claude.model();
    info.hasApiKey = claude.hasApiKey();
    info.wifiConnected = wifi_manager::connected();
    info.ssid = ui::toAscii(wifi_manager::connected() ? wifi_manager::ssid() : settings.wifiSsid);
    info.ip = wifi_manager::ip();
    info.board = CYD_BOARD_NAME;
    info.hasSpeaker = board_hw::hasSpeaker();
    info.freeHeap = ESP.getFreeHeap();
    info.maxHeapBlock = ESP.getMaxAllocHeap();
    info.resetReason = resetReasonText();
    info.freePsram = ESP.getFreePsram();
    info.appCount = app_store::list().size();
    info.uptimeSec = millis() / 1000;
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macText[18], code[8];
    snprintf(macText, sizeof(macText), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(code, sizeof(code), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    info.mac = macText;
    info.deviceCode = code;
    for (const SavedWifi &w : settings_store::loadWifiList()) {
        info.savedWifi.push_back(ui::ellipsize(ui::toAscii(w.ssid), 24));
    }
    info.userName = ui::toAscii(profile.userName);
    info.locationName = ui::toAscii(profile.locationName);
    info.pinSet = profile.pin.length() == PinUi::kDigits;
    info.timezoneText = timezoneText();
    return info;
}

void showSettings()
{
    hideAll();
    screen = Screen::Settings;
    settingsUi.show(settingsInfo(), deviceCfg);
}

// Back to the Settings page that opened the keyboard or the PIN pad.
void resumeSettings()
{
    hideAll();
    screen = Screen::Settings;
    settingsUi.resume(settingsInfo(), deviceCfg);
}

void showGames()
{
    hideAll();
    useDarkPalette();
    screen = Screen::Games;
    games.show();
}

void showMap()
{
    hideAll();
    useDarkPalette();
    screen = Screen::Map;
    mapUi.show();
}

void showFace()
{
    hideAll();
    useDarkPalette();
    screen = Screen::Face;
    face.show();
}

String locationName()
{
    return profile.locationName.length() > 0 ? ui::toAscii(profile.locationName) : String("Hai Phong");
}

void showClock()
{
    hideAll();
    screen = Screen::Clock;
    clockUi.setOptions(deviceCfg.clockFace, deviceCfg.hourlyChime);
    clockUi.setLocation(locationName(), profile.latitude, profile.longitude);
    clockUi.show();
}

void showCalendar()
{
    hideAll();
    screen = Screen::Calendar;
    calendarUi.show();
}

void showMedia(MediaMode mode)
{
    hideAll();
    screen = Screen::Media;
    media.setVolume(deviceCfg.volume);
    media.show(mode);
}

void showObd(bool autoConnect)
{
    hideAll();
    screen = Screen::Obd;
    obdUi.show(autoConnect);
}

void showTpms()
{
    hideAll();
    screen = Screen::Tpms;
    tpmsUi.show();
}

void showPin(PinPurpose purpose, PinMode mode, const char *title)
{
    hideAll();
    screen = Screen::Pin;
    pinPurpose = purpose;
    pinUi.show(mode, profile.pin, title);
}

// "Che do cau hinh Wi-Fi": the board's own hotspot with a QR code to join it and the address to open.
void showApSetup()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char name[32];
    snprintf(name, sizeof(name), "%s%02X%02X", kApNamePrefix, mac[4], mac[5]);
    hideAll();
    screen = Screen::ApSetup;
    tft.fillScreen(ui::kColorBg);
    ui::drawBackHeader(tft, "Cau hinh Wi-Fi");
    if (!web_portal::startAccessPoint(name)) {
        ui::drawCentered(tft, "Khong mo duoc diem phat Wi-Fi", 0, ui::kScreenW, 100, 1, ui::kColorDanger);
        return;
    }
    const String url = "http://" + web_portal::accessPointIp() + "/";
    int16_t y = 34;
    auto line = [&](const String &text, uint8_t size, uint16_t color) {
        tft.setTextSize(size);
        tft.setTextColor(color);
        tft.setCursor(8, y);
        tft.print(text);
        y += size * ui::kFontH + 8;
    };
    line("1. Dien thoai vao Wi-Fi:", 1, ui::kColorMuted);
    line(name, 2, ui::kColorAccent);
    line("(mang mo, khong mat khau)", 1, ui::kColorMuted);
    line("2. Mo Chrome, nhap:", 1, ui::kColorMuted);
    line(url, 2, ui::kColorText);
    line("3. Chon mang nha, nhap mat khau", 1, ui::kColorMuted);
    line("   -> Luu va ket noi", 1, ui::kColorMuted);
    const String join = "WIFI:S:" + String(name) + ";T:nopass;;";
    if (qr_code::encode(join.c_str(), apQr)) {
        const int16_t modules = (int16_t)apQr.size + 4;
        const int16_t scale = max<int16_t>(1, kApQrMaxSide / modules);
        tft.fillRect(kApQrX, kApQrY, modules * scale, modules * scale, ui::kColorWhite);
        for (int16_t my = 0; my < apQr.size; ++my) {
            for (int16_t mx = 0; mx < apQr.size; ++mx) {
                if (apQr.dark(mx, my)) {
                    tft.fillRect(kApQrX + (mx + 2) * scale, kApQrY + (my + 2) * scale, scale, scale, ui::kColorBlack);
                }
            }
        }
        ui::drawCentered(tft, "Quet de vao Wi-Fi", kApQrX - 10, modules * scale + 20, kApQrY + modules * scale + 6, 1,
                         ui::kColorMuted);
    }
}

// ---------------------------------------------------------------- prompts with the AI role

String facePromptText;  // ClaudeAsync keeps only the pointer, so the text lives here

String profilePrompt()
{
    String extra;
    const personas::Persona &role = personas::get(deviceCfg.persona);
    if (role.prompt[0] != '\0') {
        extra += " ";
        extra += role.prompt;
    }
    if (profile.userName.length() > 0) {
        extra += " The user's name is " + profile.userName + ".";
    }
    if (profile.locationName.length() > 0) {
        extra += " The user lives in " + profile.locationName + ".";
    }
    extra += personas::languagePrompt((personas::Language)deviceCfg.language);
    return extra;
}

String chatPrompt()
{
    return String(kChatSystemPrompt) + profilePrompt();
}

void showChat()
{
    hideAll();
    screen = Screen::Chat;
    chat.setStatus(statusText());
    chat.show();
}

void showKeyboard(InputPurpose purpose, const char *title, const String &initial, const char *placeholder,
                  size_t maxChars, const char *doneLabel)
{
    hideAll();
    screen = Screen::Keyboard;
    inputPurpose = purpose;
    keyboard.show(title, initial, placeholder, maxChars, doneLabel);
}

void showWifi(bool rescan)
{
    hideAll();
    screen = Screen::Wifi;
    if (rescan || scanResults.empty()) {
        wifiScreen.showScanning();
        scanResults = wifi_manager::scan();
    }
    wifiScreen.show(scanResults, wifiStatusText());
}

// ---------------------------------------------------------------- Wi-Fi

// Remembers the network just joined (saved list, most recent first, and its access point).
void rememberWifi(const String &ssid, const String &password)
{
    settings.wifiSsid = ssid;
    settings.wifiPassword = password;
    settings_store::addWifi(ssid, password);
    settings_store::saveBssid(ssid, WiFi.BSSID());
}

// Joins the strongest of the saved networks (up to 5); falls back to the most recent one when none is seen.
bool connectSaved(bool showProgress)
{
    lastWifiAttemptMs = millis();
    const std::vector<SavedWifi> saved = settings_store::loadWifiList();
    if (saved.empty()) {
        return false;
    }
    const SavedWifi *best = &saved[0];
    if (saved.size() > 1) {
        if (showProgress) {
            ui::drawSplash(tft, "Dang tim Wi-Fi da luu", "Quet mang xung quanh...");
        }
        int32_t bestRssi = INT32_MIN;
        for (const WifiNetwork &n : wifi_manager::scan()) {
            for (const SavedWifi &w : saved) {
                if (w.ssid == n.ssid && n.rssi > bestRssi) {
                    bestRssi = n.rssi;
                    best = &w;
                }
            }
        }
    }
    if (showProgress) {
        ui::drawSplash(tft, "Dang ket noi Wi-Fi", ui::toAscii(best->ssid).c_str());
    }
    const bool ok = wifi_manager::connect(best->ssid, best->password, kWifiConnectTimeoutMs);
    if (ok) {
        rememberWifi(best->ssid, best->password);
        startClockSync(true);
        wasConnected = true;
    }
    return ok;
}

// Arguments by value: the scan list they may come from is replaced while connecting.
void connectTo(const String ssid, const String password)
{
    hideAll();
    screen = Screen::Busy;
    ui::drawSplash(tft, "Dang ket noi Wi-Fi", ui::toAscii(ssid).c_str(), "Toi da 20 giay");
    board_hw::setLed(0, 0, 255);
    if (wifi_manager::connect(ssid, password, kWifiConnectTimeoutMs)) {
        rememberWifi(ssid, password);
        web_portal::stopAccessPoint();
        startClockSync(true);
        wasConnected = true;
        board_hw::setLed(0, 255, 0);
        showLauncher("Da ket noi " + ui::toAscii(ssid) + ". Web cai dat: http://" + wifi_manager::ip());
        return;
    }
    board_hw::setLed(255, 0, 0);
    // Go back to the saved network if there is one, without blocking the UI.
    if (settings.wifiSsid.length() > 0) {
        WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
        lastWifiAttemptMs = millis();
    }
    showWifi(false);
    wifiScreen.setStatus("Khong noi duoc \"" + ui::toAscii(ssid) + "\". Sai mat khau hoac song yeu?");
}

void selectNetwork(const WifiNetwork net)
{
    if (net.open) {
        connectTo(net.ssid, String());
        return;
    }
    pendingSsid = net.ssid;
    const String saved = net.ssid == settings.wifiSsid ? settings.wifiPassword : String();
    const String title = "Mat khau " + ui::toAscii(net.ssid);
    showKeyboard(InputPurpose::WifiPassword, title.c_str(), saved, "Mat khau Wi-Fi...", kPasswordMax, "Noi");
}

// Never blocks: the driver keeps trying in the background after WiFi.begin().
void maintainWifi()
{
    const bool connected = wifi_manager::connected();
    if (connected && !wasConnected) {
        startClockSync(false);
        Serial.printf("[wifi] back online, IP %s\n", wifi_manager::ip().c_str());
    }
    wasConnected = connected;
    // The OBD screen may be on the adapter's own Wi-Fi, and the setup hotspot handles its own joining.
    if (!connected && settings.wifiSsid.length() > 0 && screen != Screen::Wifi && screen != Screen::Obd &&
        screen != Screen::ApSetup && millis() - lastWifiAttemptMs > kWifiRetryIntervalMs) {
        lastWifiAttemptMs = millis();
        WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    }
}

// ---------------------------------------------------------------- chat

void trimHistory()
{
    while (history.size() > CHAT_HISTORY_MAX_MESSAGES) {
        history.erase(history.begin());
    }
    // The Messages API needs the conversation to start with a user turn.
    while (!history.empty() && history.front().role != ChatRole::User) {
        history.erase(history.begin());
    }
}

void askClaude(const String &question)
{
    if (screen != Screen::Chat) {
        showChat();
    }
    chat.addMessage(Speaker::User, question);
    if (!wifi_manager::connected()) {
        chat.addMessage(Speaker::System, "Chua co Wi-Fi. Vao Cai dat > Wi-Fi de chon mang.");
        board_hw::setLed(255, 0, 0);
        return;
    }

    history.push_back({ChatRole::User, question});
    trimHistory();

    chat.setBusy(true);
    board_hw::setLed(0, 0, 255);
    claudeAsync.waitIdle();  // the face may still be waiting for its answer
    String reply, error;
    const String prompt = chatPrompt();
    const bool ok = claude.ask(prompt.c_str(), history, CLAUDE_MAX_TOKENS, CLAUDE_EFFORT, reply, error);
    chat.setBusy(false);

    if (ok) {
        history.push_back({ChatRole::Assistant, reply});
        chat.addMessage(Speaker::Claude, reply);
        Serial.printf("Claude: %s\n", reply.c_str());
        board_hw::setLed(0, 255, 0);
    } else {
        history.pop_back();  // keep user/assistant turns alternating
        chat.addMessage(Speaker::System, error);
        Serial.printf("[claude] %s\n", error.c_str());
        board_hw::setLed(255, 0, 0);
    }
}

void clearConversation()
{
    history.clear();
    chat.clearChat();
    chat.addMessage(Speaker::System, "Da xoa hoi thoai. Bam \"Nhap\" de hoi Claude.");
    board_hw::setLed(0, 0, 0);
}

// ---------------------------------------------------------------- AI face

void trimFaceHistory()
{
    while (faceHistory.size() > FACE_HISTORY_MAX_MESSAGES) {
        faceHistory.erase(faceHistory.begin());
    }
    while (!faceHistory.empty() && faceHistory.front().role != ChatRole::User) {
        faceHistory.erase(faceHistory.begin());
    }
}

// Starts a request in the background; the face thinks until pollFaceAnswer() gets the reply.
void askFace(const String &question)
{
    if (!wifi_manager::connected()) {
        face.sayError("Minh chua co Wi-Fi. Vao Cai dat > Wi-Fi de noi mang nhe.");
        return;
    }
    if (!claude.hasApiKey()) {
        face.sayError("Minh chua co API key. Mo http://<IP>/ tren dien thoai de nhap.");
        return;
    }
    if (claudeAsync.busy()) {
        return;
    }
    faceHistory.push_back({ChatRole::User, question});
    trimFaceHistory();
    facePromptText = String(kFaceSystemPrompt) + profilePrompt();
    if (!claudeAsync.start(facePromptText.c_str(), faceHistory, FACE_MAX_TOKENS, FACE_EFFORT)) {
        faceHistory.pop_back();
        face.sayError("Khong du bo nho de hoi Claude.");
        return;
    }
    face.setThinking(ui::toAscii(question));
    board_hw::setLed(0, 0, 255);
}

void pollFaceAnswer()
{
    bool ok;
    String reply, error;
    if (!claudeAsync.takeResult(ok, reply, error)) {
        return;
    }
    if (ok) {
        faceHistory.push_back({ChatRole::Assistant, reply});
        face.say(reply);
        Serial.printf("Claude (mat): %s\n", reply.c_str());
        board_hw::setLed(0, 255, 0);
    } else {
        if (!faceHistory.empty() && faceHistory.back().role == ChatRole::User) {
            faceHistory.pop_back();
        }
        face.sayError(error);
        Serial.printf("[face] %s\n", error.c_str());
        board_hw::setLed(255, 0, 0);
    }
}

// ---------------------------------------------------------------- settings

void applyDevice(const DeviceSettings &next)
{
    const bool flipChanged = next.flipped != deviceCfg.flipped;
    deviceCfg = next;
    board_hw::setBrightnessSetting(deviceCfg.brightness);
    board_hw::setSoundEnabled(deviceCfg.sound);
    board_hw::setVolume(deviceCfg.volume);
    audio_player::setVolume(deviceCfg.volume);
    if (flipChanged) {
        board_hw::setFlipped(deviceCfg.flipped);
        settingsUi.update(settingsInfo());
    }
    deviceDirty = true;
}

void saveDeviceIfDirty()
{
    if (deviceDirty) {
        deviceDirty = false;
        settings_store::saveDevice(deviceCfg);
    }
}

void nextModel()
{
    claudeAsync.waitIdle();
    size_t i = 0;
    const size_t count = sizeof(kModels) / sizeof(kModels[0]);
    while (i < count && claude.model() != kModels[i]) {
        ++i;
    }
    settings.model = kModels[(i + 1) % count];  // an unknown custom model goes back to the first one
    settings_store::saveModel(settings.model);
    claude.configure(settings.apiKey, settings.model);
    settingsUi.update(settingsInfo());
}

// Reads the time zone for the board's IP (Settings > Dong ho > Tu dong) and restarts NTP with it.
void syncTimezone()
{
    if (deviceCfg.autoTimezone && wifi_manager::connected()) {
        int32_t offset;
        String error;
        if (weather_client::fetchTimezoneOffset(offset, error)) {
            deviceCfg.tzOffsetSec = offset;
            settings_store::saveDevice(deviceCfg);
            Serial.printf("[time] zone offset %ld s\n", (long)offset);
        } else {
            Serial.printf("[time] %s\n", error.c_str());
        }
    }
    timezoneSynced = true;
    startClockSync(false);
}

// Location typed in Settings > Ca nhan: looked up on OpenStreetMap for the weather and the map.
void setLocation(const String &text)
{
    if (text.length() == 0) {
        profile.locationName = "";
        profile.latitude = MAP_HOME_LAT;
        profile.longitude = MAP_HOME_LON;
        settings_store::saveProfile(profile);
        return;
    }
    if (!wifi_manager::connected()) {
        profile.locationName = text;  // coordinates stay as they were
        settings_store::saveProfile(profile);
        return;
    }
    ui::drawSplash(tft, "Dang tim vi tri", ui::toAscii(text).c_str());
    route_client::Place place;
    String error;
    if (route_client::findPlace(text, place, error)) {
        profile.latitude = place.at.lat;
        profile.longitude = place.at.lon;
    } else {
        Serial.printf("[profile] %s\n", error.c_str());
    }
    profile.locationName = text;
    settings_store::saveProfile(profile);
}

void forgetWifi(size_t index)
{
    const std::vector<SavedWifi> saved = settings_store::loadWifiList();
    if (index < saved.size()) {
        settings_store::removeWifi(saved[index].ssid);
        settings = settings_store::load();
    }
}

void handleSettingsAction(SettingsAction action)
{
    switch (action) {
    case SettingsAction::ThemeChanged:
        deviceCfg = settingsUi.device();
        ui::applyTheme(deviceCfg.lightTheme, deviceCfg.accent);
        settings_store::saveDevice(deviceCfg);
        settingsUi.redraw();
        break;
    case SettingsAction::EditName:
        showKeyboard(InputPurpose::UserName, "Ten ban", profile.userName, "Ten de Claude goi ban...", kNameInputMax,
                     "Luu");
        break;
    case SettingsAction::EditLocation:
        showKeyboard(InputPurpose::Location, "Vi tri", profile.locationName, "Thanh pho / dia chi...",
                     kLocationInputMax, "Luu");
        break;
    case SettingsAction::SetPin:
        showPin(PinPurpose::SetPin, PinMode::Set, "Dat ma PIN moi");
        break;
    case SettingsAction::ChangePin:
        showPin(PinPurpose::ChangePin, PinMode::Change, "Doi ma PIN");
        break;
    case SettingsAction::RequestUnlock:
        showPin(PinPurpose::UnlockSettings, PinMode::Verify, "Trang nay dang khoa");
        break;
    case SettingsAction::StartApSetup:
        showApSetup();
        break;
    case SettingsAction::ResetWifi:
        settings_store::clearWifi();
        settings = settings_store::load();
        WiFi.disconnect(false, true);
        showWifi(true);
        wifiScreen.setStatus("Da xoa Wi-Fi. Chon mang moi.");
        break;
    case SettingsAction::ForgetWifi:
        forgetWifi(settingsUi.forgetSsidIndex());
        settingsUi.update(settingsInfo());
        break;
    case SettingsAction::SyncTime:
        deviceCfg = settingsUi.device();
        settings_store::saveDevice(deviceCfg);
        syncTimezone();
        settingsUi.update(settingsInfo());
        break;
    case SettingsAction::TestSound:
        board_hw::beep(880, 150);
        break;
    case SettingsAction::Back:
        saveDeviceIfDirty();
        showLauncher();
        break;
    case SettingsAction::Wifi:
        wifiFromSettings = true;
        showWifi(true);
        break;
    case SettingsAction::Calibrate:
        board_hw::runTouchCalibration();
        showSettings();
        break;
    case SettingsAction::DeviceChanged:
        applyDevice(settingsUi.device());
        break;
    case SettingsAction::NextModel:
        nextModel();
        break;
    case SettingsAction::None:
        break;
    }
}

// ---------------------------------------------------------------- apps

void openApp(const String &id)
{
    String json;
    if (!app_store::load(id, json)) {
        showLauncher("Khong doc duoc app " + id);
        return;
    }
    std::string err;
    runningApp.setSavedState(app_store::loadState(id).c_str());
    if (!runningApp.load(json.c_str(), err)) {
        lastErrors[id] = err.c_str();
        showLauncher(ui::toAscii("App loi: " + String(err.c_str())) + " (giu tile > Sua)");
        return;
    }
    hideAll();
    useDarkPalette();  // Claude apps are written for a black background
    screen = Screen::App;
    runningAppId = id;
    device.reset();
    Serial.printf("[apps] run %s\n", id.c_str());
    canvas.invalidateOverlay();
    if (!runningApp.start(err)) {
        lastErrors[id] = err.c_str();  // the app shows the error banner; "<" or holding the screen exits
        Serial.printf("[apps] %s: %s\n", id.c_str(), err.c_str());
    }
    canvas.refreshOverlay();
}

void exitApp()
{
    app_store::saveState(runningAppId, runningApp.saveState().c_str());
    String notice;
    if (runningApp.hasError()) {
        lastErrors[runningAppId] = runningApp.runtimeError().c_str();
        notice = "App bi loi. Giu tile 1 giay > Sua de Claude sua.";
    } else {
        lastErrors.erase(runningAppId);
    }
    device.reset();
    runningAppId = "";
    showLauncher(notice);
}

String appNameOf(const String &json)
{
    mini_json::Value doc;
    std::string err;
    return mini_json::parse(json.c_str(), doc, err) ? String(doc.getString("name", "app").c_str()) : String("app");
}

// Creates a new app (editId empty) or changes an existing one, then runs it.
void buildApp(const String &request, const String &editId)
{
    if (!wifi_manager::connected()) {
        showLauncher("Can Wi-Fi de Claude viet app: Cai dat > Wi-Fi.");
        return;
    }
    if (!claude.hasApiKey()) {
        showLauncher("Chua co API key: vao web http://<IP>/ hoac go /key qua Serial.");
        return;
    }
    hideAll();
    screen = Screen::Busy;
    board_hw::setLed(0, 0, 255);
    const String title = editId.length() > 0 ? "Claude dang sua app" : "Claude dang viet app";
    ui::drawSplash(tft, title.c_str(), ui::ellipsize(ui::toAscii(request), kSplashChars).c_str());
    uint32_t lastDraw = 0;
    auto status = [&](const String &text) {
        if (millis() - lastDraw < kProgressRedrawMs) {
            return;
        }
        lastDraw = millis();
        tft.fillRect(0, kProgressY, ui::kScreenW, kProgressH, ui::kColorBg);
        tft.setTextSize(1);
        tft.setTextColor(ui::kColorSystem);
        const String line = ui::ellipsize(text, kSplashChars);
        tft.setCursor((ui::kScreenW - (int16_t)line.length() * ui::kFontW) / 2, kProgressY + 2);
        tft.print(line);
    };

    claudeAsync.waitIdle();  // one Claude request at a time
    String json, error;
    bool ok;
    if (editId.length() > 0) {
        String current;
        ok = app_store::load(editId, current) &&
             builder.edit(current, request, lastErrors[editId], json, error, status);
        if (!ok && error.length() == 0) {
            error = "Khong doc duoc app cu.";
        }
    } else {
        ok = builder.create(request, json, error, status);
    }
    if (!ok) {
        board_hw::setLed(255, 0, 0);
        Serial.printf("[builder] %s\n", error.c_str());
        showLauncher(ui::toAscii(error));
        return;
    }
    const String id = editId.length() > 0 ? editId : app_store::makeId(appNameOf(json));
    if (!app_store::save(id, json)) {
        board_hw::setLed(255, 0, 0);
        showLauncher("Khong luu duoc app (bo nho flash?)");
        return;
    }
    lastErrors.erase(id);
    board_hw::setLed(0, 255, 0);
    Serial.printf("[apps] saved %s (%u bytes)\n", id.c_str(), (unsigned)json.length());
    openApp(id);
}

String installAppJson(const String &json)
{
    std::string err;
    if (!app_validator::validate(json.c_str(), err)) {
        return String(err.c_str());
    }
    const String id = app_store::makeId(appNameOf(json));
    if (!app_store::save(id, json)) {
        return "Khong luu duoc";
    }
    if (screen == Screen::Launcher) {
        showLauncher("Da cai app " + id);
    }
    return String();
}

void deleteApp(const AppTile &tile)
{
    app_store::remove(tile.id);
    lastErrors.erase(tile.id);
    showLauncher("Da xoa " + tile.name);
}

// ---------------------------------------------------------------- input handling

void handleKeyboardDone()
{
    String text = keyboard.text();
    if (inputPurpose != InputPurpose::WifiPassword) {
        text.trim();
    }
    switch (inputPurpose) {
    case InputPurpose::ChatQuestion:
        if (text.length() == 0) {
            return;
        }
        showChat();
        askClaude(text);
        break;
    case InputPurpose::NewApp:
        if (text.length() == 0) {
            return;
        }
        buildApp(text, String());
        break;
    case InputPurpose::EditApp:
        if (text.length() == 0) {
            return;
        }
        buildApp(text, editAppId);
        break;
    case InputPurpose::WifiPassword:
        connectTo(pendingSsid, text);
        break;
    case InputPurpose::FaceQuestion:
        showFace();
        if (text.length() > 0) {
            askFace(text);
        }
        break;
    case InputPurpose::MapFrom:
        mapUi.setFrom(text);
        showMap();
        break;
    case InputPurpose::MapTo:
        mapUi.setTo(text);
        showMap();
        break;
    case InputPurpose::UserName:
        profile.userName = text;
        settings_store::saveProfile(profile);
        resumeSettings();
        break;
    case InputPurpose::Location:
        setLocation(text);
        resumeSettings();
        break;
    case InputPurpose::ObdWifiPassword: {
        const String ssid = obdUi.pendingSsid();
        showObd(false);
        obdUi.connectWifi(ssid, keyboard.text());
        break;
    }
    }
}

void handleKeyboardCancel()
{
    switch (inputPurpose) {
    case InputPurpose::ChatQuestion:
        showChat();
        break;
    case InputPurpose::WifiPassword:
        showWifi(false);
        break;
    case InputPurpose::FaceQuestion:
        showFace();
        break;
    case InputPurpose::MapFrom:
    case InputPurpose::MapTo:
        showMap();
        break;
    case InputPurpose::UserName:
    case InputPurpose::Location:
        resumeSettings();
        break;
    case InputPurpose::ObdWifiPassword:
        showObd(false);
        break;
    default:
        showLauncher();
        break;
    }
}

void handleLauncherActionUnlocked(LauncherAction action, size_t appIndex);

// Launcher entries behind Settings > Bao mat > Khoa app (Settings, the clock and the calendar stay open).
bool needsUnlock(LauncherAction action)
{
    if (!deviceCfg.lockApps || appsUnlocked || profile.pin.length() != PinUi::kDigits) {
        return false;
    }
    switch (action) {
    case LauncherAction::Settings:
    case LauncherAction::Clock:
    case LauncherAction::Calendar:
    case LauncherAction::None:
        return false;
    default:
        return true;
    }
}

void handlePinResult(PinResult result)
{
    if (result == PinResult::None) {
        return;
    }
    const bool ok = result == PinResult::Accepted;
    switch (pinPurpose) {
    case PinPurpose::OpenLocked: {
        if (!ok) {
            showLauncher();
            return;
        }
        appsUnlocked = true;
        const LauncherAction action = pendingLocked;
        pendingLocked = LauncherAction::None;
        showLauncher();
        handleLauncherActionUnlocked(action, pendingLockedApp);
        return;
    }
    case PinPurpose::UnlockSettings:
        hideAll();
        screen = Screen::Settings;
        if (ok) {
            settingsUi.resume(settingsInfo(), deviceCfg);
            settingsUi.unlock();
        } else {
            settingsUi.resume(settingsInfo(), deviceCfg);
        }
        return;
    case PinPurpose::SetPin:
    case PinPurpose::ChangePin:
        if (ok) {
            profile.pin = pinUi.newPin();
            settings_store::saveProfile(profile);
        }
        resumeSettings();
        return;
    }
}

void handleLauncherActionUnlocked(LauncherAction action, size_t appIndex)
{
    switch (action) {
    case LauncherAction::OpenApp:
        if (appIndex < tiles.size()) {
            openApp(tiles[appIndex].id);
        }
        break;
    case LauncherAction::Clock:
        showClock();
        break;
    case LauncherAction::Calendar:
        showCalendar();
        break;
    case LauncherAction::Radio:
        showMedia(MediaMode::Radio);
        break;
    case LauncherAction::Music:
        showMedia(MediaMode::Music);
        break;
    case LauncherAction::Obd:
        showObd(false);
        break;
    case LauncherAction::Tpms:
        showTpms();
        break;
    case LauncherAction::AppMenu:
        if (appIndex < tiles.size()) {
            launcher.showMenu(tiles[appIndex]);
        }
        break;
    case LauncherAction::Chat:
        showChat();
        break;
    case LauncherAction::NewApp:
        showKeyboard(InputPurpose::NewApp, "Tao app moi", String(), "Mo ta app ban muon...", kAppInputMax, "Tao");
        break;
    case LauncherAction::Settings:
        showSettings();
        break;
    case LauncherAction::Games:
        showGames();
        break;
    case LauncherAction::Map:
        showMap();
        break;
    case LauncherAction::Face:
        showFace();
        break;
    case LauncherAction::None:
        break;
    }
}

void handleLauncherAction(LauncherAction action)
{
    if (needsUnlock(action)) {
        pendingLocked = action;
        pendingLockedApp = launcher.selected();
        showPin(PinPurpose::OpenLocked, PinMode::Verify, "App dang khoa - nhap ma PIN");
        return;
    }
    handleLauncherActionUnlocked(action, launcher.selected());
}

void handleMenuAction(MenuAction action)
{
    const AppTile tile = tiles[launcher.selected()];
    switch (action) {
    case MenuAction::Run:
        openApp(tile.id);
        break;
    case MenuAction::Edit: {
        editAppId = tile.id;
        const String title = "Sua " + tile.name;
        showKeyboard(InputPurpose::EditApp, title.c_str(), String(), "Muon sua gi?", kAppInputMax, "Sua");
        break;
    }
    case MenuAction::Delete:
        deleteApp(tile);
        break;
    case MenuAction::Cancel:
        showLauncher();
        break;
    case MenuAction::None:
        break;
    }
}

// A new press (most screens react immediately, like a key).
void onPress(int16_t x, int16_t y)
{
    switch (screen) {
    case Screen::Chat:
        switch (chat.handleTouch(x, y)) {
        case ChatAction::Home:
            showLauncher();
            break;
        case ChatAction::Compose:
            showKeyboard(InputPurpose::ChatQuestion, "Hoi Claude", String(), "Go cau hoi...", kChatInputMax, "Gui");
            break;
        case ChatAction::ClearChat:
            clearConversation();
            break;
        case ChatAction::None:
            break;
        }
        break;
    case Screen::Keyboard:
        switch (keyboard.handleTouch(x, y)) {
        case KeyboardResult::Done:
            handleKeyboardDone();
            break;
        case KeyboardResult::Cancel:
            handleKeyboardCancel();
            break;
        case KeyboardResult::None:
            break;
        }
        break;
    case Screen::Wifi:
        switch (wifiScreen.handleTouch(x, y)) {
        case WifiAction::Back:
            // Scanning stopped the background reconnect: try the saved network again right away.
            lastWifiAttemptMs = millis() - kWifiRetryIntervalMs - 1;
            if (wifiFromSettings) {
                wifiFromSettings = false;
                showSettings();
            } else {
                showLauncher();
            }
            break;
        case WifiAction::Rescan:
            showWifi(true);
            break;
        case WifiAction::Select:
            selectNetwork(wifiScreen.selected());
            break;
        case WifiAction::None:
            break;
        }
        break;
    case Screen::App:
        if (ui::backTapped(x, y)) {
            exitApp();
        } else {
            runningApp.touch(x, y);
            canvas.refreshOverlay();
        }
        break;
    case Screen::Settings:
        handleSettingsAction(settingsUi.handleTouch(x, y));
        break;
    case Screen::Games:
        if (games.press(x, y) == GamesAction::Exit) {
            showLauncher();
        }
        break;
    case Screen::Map:
        switch (mapUi.press(x, y)) {
        case MapAction::Exit:
            showLauncher();
            break;
        case MapAction::EditFrom:
            showKeyboard(InputPurpose::MapFrom, "Diem di", mapUi.from(), "Dia chi, ten noi hoac lat,lon", kMapInputMax,
                         "Xong");
            break;
        case MapAction::EditTo:
            showKeyboard(InputPurpose::MapTo, "Diem den", mapUi.to(), "Dia chi, ten noi hoac lat,lon", kMapInputMax,
                         "Xong");
            break;
        case MapAction::None:
            break;
        }
        break;
    case Screen::Face:
        switch (face.press(x, y)) {
        case FaceAction::Back:
            showLauncher();
            break;
        case FaceAction::Compose:
            showKeyboard(InputPurpose::FaceQuestion, "Noi voi Claude", String(), "Go loi muon noi...", kFaceInputMax,
                         "Gui");
            break;
        case FaceAction::None:
            break;
        }
        break;
    case Screen::Clock:
        switch (clockUi.press(x, y)) {
        case ClockAction::Exit: showLauncher(); break;
        case ClockAction::SettingsChanged:
            deviceCfg.clockFace = clockUi.face();
            deviceCfg.hourlyChime = clockUi.hourlyChime();
            settings_store::saveDevice(deviceCfg);
            break;
        case ClockAction::None: break;
        }
        break;
    case Screen::Calendar:
        if (calendarUi.press(x, y) == CalendarAction::Exit) {
            showLauncher();
        }
        break;
    case Screen::Media:
        switch (media.press(x, y)) {
        case MediaAction::Exit: showLauncher(); break;
        case MediaAction::VolumeChanged:
            deviceCfg.volume = media.volume();
            deviceDirty = true;
            break;
        case MediaAction::None: break;
        }
        break;
    case Screen::Obd:
        switch (obdUi.press(x, y)) {
        case ObdAction::Exit: showLauncher(); break;
        case ObdAction::NeedWifiPassword: {
            const String title = "Mat khau " + ui::toAscii(obdUi.pendingSsid());
            showKeyboard(InputPurpose::ObdWifiPassword, title.c_str(), String(), "Mat khau Wi-Fi adapter...",
                         kPasswordMax, "Noi");
            break;
        }
        case ObdAction::None: break;
        }
        break;
    case Screen::Tpms:
        if (tpmsUi.press(x, y) == TpmsAction::Exit) {
            showLauncher();
        }
        break;
    case Screen::Pin:
        handlePinResult(pinUi.press(x, y));
        break;
    case Screen::ApSetup:
        if (ui::backTapped(x, y)) {
            showSettings();
        }
        break;
    case Screen::Launcher:
    case Screen::Busy:
        break;
    }
}

// "<", a left-to-right swipe or BOOT: one step back on the current screen.
void goBack()
{
    switch (screen) {
    case Screen::App: exitApp(); break;
    case Screen::Keyboard: handleKeyboardCancel(); break;
    case Screen::Settings: handleSettingsAction(settingsUi.back()); break;
    case Screen::Clock:
        if (clockUi.back() == ClockAction::Exit) {
            showLauncher();
        }
        break;
    case Screen::Calendar:
        if (calendarUi.back() == CalendarAction::Exit) {
            showLauncher();
        }
        break;
    case Screen::Media:
        if (media.back() == MediaAction::Exit) {
            showLauncher();
        }
        break;
    case Screen::Obd:
        if (obdUi.back() == ObdAction::Exit) {
            showLauncher();
        }
        break;
    case Screen::Tpms:
        if (tpmsUi.back() == TpmsAction::Exit) {
            showLauncher();
        }
        break;
    case Screen::Pin: handlePinResult(PinResult::Cancelled); break;
    case Screen::ApSetup: showSettings(); break;
    case Screen::Wifi:
        lastWifiAttemptMs = millis() - kWifiRetryIntervalMs - 1;
        if (wifiFromSettings) {
            wifiFromSettings = false;
            showSettings();
        } else {
            showLauncher();
        }
        break;
    case Screen::Launcher:
    case Screen::Busy: break;
    default: showLauncher(); break;
    }
}

// Screens where a sideways swipe is part of using them (paging, dragging, drawing) do not go back on it.
bool swipeBackAllowed()
{
    switch (screen) {
    case Screen::Launcher:
    case Screen::Busy:
    case Screen::Keyboard:
    case Screen::App:
    case Screen::Games:
    case Screen::Map:
    case Screen::Pin:
        return false;
    case Screen::Settings:
        return !settingsUi.sliderActive();
    default:
        return true;
    }
}

// The stylus moved while pressed.
void onDrag(int16_t x, int16_t y)
{
    if (screen == Screen::Games) {
        games.drag(x, y);
    } else if (screen == Screen::Map) {
        mapUi.drag(x, y);
    } else if (screen == Screen::Settings) {
        handleSettingsAction(settingsUi.handleDrag(x, y));
    }
}

void onRelease()
{
    if (screen == Screen::Games) {
        games.release();
    } else if (screen == Screen::Map && pressScreen == Screen::Map) {
        mapUi.release();
    }
    saveDeviceIfDirty();
}

// Released before the long-press time (launcher only: a hold must not also open the app).
// dx is how far the stylus moved sideways: a swipe changes the page instead of opening an icon.
void onTap(int16_t x, int16_t y, int16_t dx)
{
    if (screen != Screen::Launcher && pressScreen == screen) {
        if (dx >= kSwipeBackPx && abs(pressDy) < kSwipeMaxDy && swipeBackAllowed()) {
            goBack();
        } else if (screen == Screen::Calendar && dx <= -kSwipeBackPx && abs(pressDy) < kSwipeMaxDy) {
            calendarUi.swipe(dx);
        }
        return;
    }
    // A press that brought us here (e.g. "<" in Settings) must not also open an icon when it lifts.
    if (screen != Screen::Launcher || pressScreen != Screen::Launcher) {
        return;
    }
    if (launcher.menuVisible()) {
        handleMenuAction(launcher.handleMenuTap(x, y));
    } else if (abs(dx) >= kSwipeMinPx) {
        launcher.handleSwipe(dx);
    } else {
        handleLauncherAction(launcher.handleTap(x, y));
    }
}

void onLongPress(int16_t x, int16_t y)
{
    if (screen == Screen::Launcher && !launcher.menuVisible()) {
        handleLauncherAction(launcher.handleLongPress(x, y));
    } else if (screen == Screen::App) {
        exitApp();
    }
}

uint32_t longPressMs()
{
    return screen == Screen::App ? APP_EXIT_HOLD_MS : kLauncherHoldMs;
}

void wakeScreen()
{
    if (screenAsleep) {
        screenAsleep = false;
        board_hw::setBacklight(board_hw::brightnessSetting());
    }
}

// Turns the backlight off after the time chosen in Settings > Man hinh (never while Claude is working).
void maintainScreenSleep(uint32_t now)
{
    if (screenAsleep || deviceCfg.screenOffSec == 0 || screen == Screen::Busy || claudeAsync.busy()) {
        if (claudeAsync.busy()) {
            lastActivityMs = now;
        }
        return;
    }
    if (now - lastActivityMs > (uint32_t)deviceCfg.screenOffSec * 1000) {
        screenAsleep = true;
        appsUnlocked = false;  // locked apps ask for the PIN again after the screen slept
        board_hw::setBacklight(0);
    }
}

void pollTouch()
{
    int16_t x, y;
    const bool down = board_hw::readTouch(x, y);
    const uint32_t now = millis();
    if (down) {
        lastTouchSeenMs = now;
        lastActivityMs = now;
        if (screenAsleep) {
            wakeScreen();
            swallowTouch = true;
        }
        if (swallowTouch) {
            touchDown = true;
            return;
        }
        if (!touchDown) {
            touchDown = true;
            longFired = false;
            pressX = lastX = x;
            pressY = lastY = y;
            pressStartMs = now;
            pressScreen = screen;
            onPress(x, y);
            return;
        }
        if (abs(x - lastX) >= kDragMinPx || abs(y - lastY) >= kDragMinPx) {
            lastX = x;
            lastY = y;
            onDrag(x, y);
        }
        const bool stayed = abs(lastX - pressX) < kHoldSlopPx && abs(lastY - pressY) < kHoldSlopPx;
        if (!longFired && stayed && now - pressStartMs >= longPressMs() && pressScreen == screen &&
            (screen == Screen::App || screen == Screen::Launcher)) {
            longFired = true;
            onLongPress(pressX, pressY);
        }
        return;
    }
    // Short dropouts of the resistive panel are not releases.
    if (touchDown && now - lastTouchSeenMs > kTouchReleaseMs) {
        touchDown = false;
        if (swallowTouch) {
            swallowTouch = false;
            return;
        }
        onRelease();
        pressDy = lastY - pressY;
        if (!longFired) {
            onTap(pressX, pressY, lastX - pressX);
        }
    }
}

void pollBootButton()
{
    const bool down = board_hw::bootButtonDown();
    if (down && !bootWasDown) {
        lastActivityMs = millis();
        if (screenAsleep) {
            wakeScreen();
        } else if (screen == Screen::App) {
            exitApp();
        } else if (screen == Screen::Pin) {
            handlePinResult(PinResult::Cancelled);
        } else if (screen != Screen::Launcher && screen != Screen::Busy) {
            showLauncher();
        }
    }
    bootWasDown = down;
}

// ---------------------------------------------------------------- Serial commands

void printHelp()
{
    Serial.println("Lenh Serial:");
    Serial.println("  /wifi TEN_WIFI|MAT_KHAU   luu Wi-Fi va ket noi lai");
    Serial.println("  /scan                     quet Wi-Fi xung quanh");
    Serial.println("  /key sk-ant-...           luu API key");
    Serial.println("  /model claude-haiku-5-5   doi model");
    Serial.println("  /new mo ta app            Claude viet app moi");
    Serial.println("  /apps                     danh sach app");
    Serial.println("  /run ID   /del ID         chay / xoa app");
    Serial.println("  /cal                      hieu chinh cam ung");
    Serial.println("  /face loi muon noi        noi chuyen voi mat Claude");
    Serial.println("  /clear                    xoa hoi thoai");
    Serial.println("  /radio URL   /stop        phat stream MP3 / dung phat");
    Serial.println("  /lich                     am lich hom nay");
    Serial.println("  /wifilist                 cac mang Wi-Fi da luu");
    Serial.println("  /status                   xem trang thai");
    Serial.println("  (go chu binh thuong de hoi Claude)");
}

void printStatus()
{
    Serial.printf("%s %s, khoi dong do: %s\n", FW_NAME, FW_VERSION, resetReasonText().c_str());
    Serial.printf("Board: %s\n", CYD_BOARD_NAME);
    Serial.printf("Wi-Fi: %s\n", wifiStatusText().c_str());
    Serial.printf("Model: %s, API key: %s\n", claude.model().c_str(), claude.hasApiKey() ? "da co" : "CHUA CO");
    Serial.printf("Apps: %u, file system: %s\n", (unsigned)app_store::list().size(), app_store::ready() ? "ok" : "LOI");
    Serial.printf("Heap: %u (max block %u, min ever %u), PSRAM: %u\n", (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap(), (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getFreePsram());
}

void handleSerialCommand(String line)
{
    line.trim();
    if (line.length() == 0) {
        return;
    }
    if (line.startsWith("/wifi ")) {
        const String arg = line.substring(6);
        const int sep = arg.indexOf('|');
        settings.wifiSsid = sep >= 0 ? arg.substring(0, sep) : arg;
        settings.wifiPassword = sep >= 0 ? arg.substring(sep + 1) : String();
        settings_store::saveWifi(settings.wifiSsid, settings.wifiPassword);
        Serial.println(connectSaved(false) ? "Wi-Fi OK" : "Wi-Fi loi, kiem tra ten/mat khau");
    } else if (line == "/scan") {
        for (const WifiNetwork &n : wifi_manager::scan()) {
            Serial.printf("  %4d dBm  ch%-3u %s %s\n", (int)n.rssi, n.channel, n.open ? "  " : "**", n.ssid.c_str());
        }
    } else if (line.startsWith("/key ")) {
        claudeAsync.waitIdle();
        settings.apiKey = line.substring(5);
        settings.apiKey.trim();
        settings_store::saveApiKey(settings.apiKey);
        claude.configure(settings.apiKey, settings.model);
        Serial.println("Da luu API key.");
    } else if (line.startsWith("/model ")) {
        claudeAsync.waitIdle();
        settings.model = line.substring(7);
        settings.model.trim();
        settings_store::saveModel(settings.model);
        claude.configure(settings.apiKey, settings.model);
        Serial.printf("Model: %s\n", settings.model.c_str());
    } else if (line.startsWith("/new ")) {
        if (screen == Screen::App) {
            exitApp();
        }
        buildApp(line.substring(5), String());
    } else if (line == "/apps") {
        for (const AppTile &tile : app_store::list()) {
            Serial.printf("  %-22s %s\n", tile.id.c_str(), tile.name.c_str());
        }
    } else if (line.startsWith("/run ")) {
        if (screen == Screen::App) {
            exitApp();
        }
        openApp(line.substring(5));
    } else if (line.startsWith("/del ")) {
        Serial.println(app_store::remove(line.substring(5)) ? "Da xoa." : "Khong co app nay.");
        if (screen == Screen::Launcher) {
            showLauncher();
        }
    } else if (line == "/cal") {
        board_hw::runTouchCalibration();
        showLauncher();
    } else if (line.startsWith("/face ")) {
        if (screen == Screen::App) {
            exitApp();
        }
        showFace();
        askFace(line.substring(6));
    } else if (line == "/clear") {
        clearConversation();
    } else if (line.startsWith("/radio ")) {
        const String url = line.substring(7);
        Serial.println(audio_player::playUrl(url, url) ? "Dang phat..." : audio_player::status().error.c_str());
    } else if (line == "/stop") {
        audio_player::stop();
        Serial.println("Da dung phat.");
    } else if (line == "/lich") {
        const time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        const lunar::LunarDate l = lunar::fromSolar(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
        char year[24], day[24];
        lunar::yearCanChi(l.year, year, sizeof(year));
        lunar::dayCanChi(lunar::jdFromDate(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900), day, sizeof(day));
        Serial.printf("%02d/%02d/%d = am lich %d/%d%s nam %s, ngay %s\n", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900,
                      l.day, l.month, l.leap ? " (nhuan)" : "", year, day);
    } else if (line == "/wifilist") {
        for (const SavedWifi &w : settings_store::loadWifiList()) {
            Serial.printf("  %s\n", w.ssid.c_str());
        }
    } else if (line == "/status") {
        printStatus();
    } else if (line.startsWith("/")) {
        printHelp();
    } else if (screen == Screen::Face) {
        askFace(line);
    } else {
        if (screen == Screen::App) {
            exitApp();
        }
        askClaude(line);
    }
}

void pollSerial()
{
    while (Serial.available() > 0) {
        const char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialLine.length() > 0) {
                lastActivityMs = millis();
                handleSerialCommand(serialLine);
                serialLine = "";
            }
        } else if (serialLine.length() < kSerialLineMax) {
            serialLine += c;
        }
    }
}

// ---------------------------------------------------------------- web page hooks

void startWebPortal()
{
    web_portal::Hooks hooks;
    hooks.saveSettings = [](const String &key, const String &model) {
        claudeAsync.waitIdle();
        if (key.length() > 0) {
            settings.apiKey = key;
            settings_store::saveApiKey(key);
        }
        if (model.length() > 0) {
            settings.model = model;
            settings_store::saveModel(model);
        }
        claude.configure(settings.apiKey, settings.model);
    };
    // Building takes a while; the main loop picks it up so the web request returns right away.
    hooks.requestCreate = [](const String &description) { pendingWebCreate = description; };
    hooks.installApp = installAppJson;
    hooks.currentModel = [] { return claude.model(); };
    hooks.hasApiKey = [] { return claude.hasApiKey(); };
    hooks.saveWifi = [](const String &ssid, const String &password) {
        pendingWebSsid = ssid;
        pendingWebPassword = password;
    };
    web_portal::begin(hooks);
}

void runPendingWebWifi()
{
    if (pendingWebSsid.length() == 0 || screen == Screen::Busy || screen == Screen::Keyboard) {
        return;
    }
    const String ssid = pendingWebSsid;
    const String password = pendingWebPassword;
    pendingWebSsid = "";
    pendingWebPassword = "";
    if (screen == Screen::App) {
        exitApp();
    }
    connectTo(ssid, password);
}

// "Bao gio": a short double beep on every full hour between 6:00 and 22:00 (not while radio / music plays).
void maintainHourlyChime(uint32_t now)
{
    if (chimeBeepsLeft > 0 && now - chimeStepMs >= kChimeGapMs) {
        chimeStepMs = now;
        --chimeBeepsLeft;
        board_hw::beep(chimeBeepsLeft == 0 ? kChimeToneHz * 3 / 4 : kChimeToneHz, kChimeToneMs);
    }
    const time_t t = time(nullptr);
    if (!deviceCfg.hourlyChime || t < kValidEpoch) {
        return;
    }
    struct tm local;
    localtime_r(&t, &local);
    if (local.tm_min != 0 || local.tm_hour == lastChimeHour) {
        return;
    }
    lastChimeHour = local.tm_hour;
    if (local.tm_hour >= kChimeFirstHour && local.tm_hour <= kChimeLastHour && !audio_player::active()) {
        chimeBeepsLeft = 2;
        chimeStepMs = now - kChimeGapMs;
    }
}

// Weather for the clock screen, fetched when it is open (blocks 1-3 s every 15 minutes).
void maintainWeather(uint32_t now)
{
    if (screen != Screen::Clock || !wifi_manager::connected() || !clockUi.weatherDue(now)) {
        return;
    }
    clockUi.setWeather(weather_client::fetch(profile.latitude, profile.longitude));
}

// Settings > Tu khoi dong and Ca nhan > Tu dong chao, right after boot.
void runStartup()
{
    String greeting;
    if (deviceCfg.autoGreet) {
        greeting = profile.userName.length() > 0 ? "Xin chao " + ui::toAscii(profile.userName) + "!"
                                                 : String("Xin chao, ban cua toi!");
        board_hw::beep(988, 90);
    }
    switch (deviceCfg.autoStart) {
    case AutoStart::Face:
        showFace();
        if (greeting.length() > 0) {
            face.say("[happy] " + greeting);
        }
        return;
    case AutoStart::Chat: showChat(); break;
    case AutoStart::Clock: showClock(); break;
    case AutoStart::Calendar: showCalendar(); break;
    case AutoStart::Radio: showMedia(MediaMode::Radio); break;
    case AutoStart::Music: showMedia(MediaMode::Music); break;
    case AutoStart::Obd: showObd(true); break;
    case AutoStart::Tpms: showTpms(); break;
    case AutoStart::None:
    case AutoStart::Count:
        if (greeting.length() > 0) {
            launcher.setNotice(greeting);
        }
        return;
    }
    if (greeting.length() > 0 && screen == Screen::Chat) {
        chat.addMessage(Speaker::System, greeting);
    }
}

void runPendingWebCreate()
{
    if (pendingWebCreate.length() == 0 || screen == Screen::Busy || screen == Screen::Keyboard) {
        return;
    }
    const String request = pendingWebCreate;
    pendingWebCreate = "";
    if (screen == Screen::App) {
        exitApp();
    }
    buildApp(request, String());
}

void refreshStatus()
{
    if (!timezoneSynced && deviceCfg.autoTimezone && wifi_manager::connected() && screen != Screen::Obd) {
        syncTimezone();
    }
    if (screen == Screen::Launcher) {
        launcher.setStatus(clockText(), launcherStatusText());
    } else if (screen == Screen::Chat) {
        chat.setStatus(statusText());
    }
}

}  // namespace

void chatbotSetup()
{
    Serial.begin(kSerialBaud);
    Serial.printf("\n%s %s (%s) - khoi dong do: %s\n", FW_NAME, FW_VERSION, CYD_BOARD_NAME, resetReasonText().c_str());
    board_hw::begin();
    deviceCfg = settings_store::loadDevice();
    profile = settings_store::loadProfile();
    ui::applyTheme(deviceCfg.lightTheme, deviceCfg.accent);
    board_hw::setFlipped(deviceCfg.flipped);
    board_hw::setBrightnessSetting(deviceCfg.brightness);
    board_hw::setSoundEnabled(deviceCfg.sound);
    board_hw::setVolume(deviceCfg.volume);
    audio_player::setVolume(deviceCfg.volume);
    chat.begin();
    ui::drawSplash(tft, FW_NAME " " FW_VERSION, CYD_BOARD_NAME, "Giu nut BOOT de hieu chinh cam ung");

    settings = settings_store::load();
    claude.configure(settings.apiKey, settings.model);

    TouchCalibration cal;
    const bool haveCal = settings_store::loadTouchCalibration(cal);
    if (haveCal) {
        board_hw::touch().setCalibration(cal);
    }
    if (board_hw::bootButtonHeldAtStart(kBootHoldWindowMs) || !haveCal) {
        board_hw::runTouchCalibration();
    }

    ui::drawSplash(tft, "Claude Apps", "Dang mo bo nho app...");
    app_store::begin();
    face.begin();
    wifi_manager::begin();
    startWebPortal();
    chat.addMessage(Speaker::System, "Xin chao! Bam \"Nhap\" de hoi Claude, hoac go cau hoi trong Serial Monitor.");

    const bool wifiOk = connectSaved(true);
    printHelp();
    printStatus();
    if (settings.wifiSsid.length() == 0) {
        showWifi(true);
        wifiScreen.setStatus("Chon mang Wi-Fi de bat dau");
        return;
    }
    String notice;
    if (!wifiOk) {
        notice = "Khong vao duoc Wi-Fi " + ui::toAscii(settings.wifiSsid) + ". Vao Cai dat > Wi-Fi de chon mang khac.";
    } else if (!claude.hasApiKey()) {
        notice = "Chua co API key. Mo http://" + wifi_manager::ip() + "/ tren dien thoai de nhap.";
    } else {
        notice = "Web: http://" + wifi_manager::ip() + "/";
    }
    lastActivityMs = millis();
    showLauncher(notice);
    runStartup();
}

void chatbotLoop()
{
    pollSerial();
    pollTouch();
    pollBootButton();
    board_hw::service();
    web_portal::loop();
    maintainWifi();
    runPendingWebCreate();
    runPendingWebWifi();
    pollFaceAnswer();

    const uint32_t now = millis();
    face.loop(now);
    clockUi.loop(now);
    calendarUi.loop(now);
    media.loop(now);
    obdUi.loop(now);
    tpmsUi.loop(now);
    maintainWeather(now);
    maintainHourlyChime(now);
    if (screen == Screen::App) {
        runningApp.loop();
        canvas.refreshOverlay();
        if (runningApp.takeSaveRequest()) {
            app_store::saveState(runningAppId, runningApp.saveState().c_str());
        }
        if (runningApp.exitRequested()) {
            exitApp();
        }
    } else if (screen == Screen::Games) {
        games.loop(now);
    }
    if (now - lastStatusMs > kStatusRefreshMs) {
        lastStatusMs = now;
        refreshStatus();
    }
    maintainScreenSleep(now);
    const bool animating = screen == Screen::App || screen == Screen::Games || screen == Screen::Face ||
                           screen == Screen::Media || screen == Screen::Obd || screen == Screen::Tpms;
    delay(animating ? 1 : 5);
}
