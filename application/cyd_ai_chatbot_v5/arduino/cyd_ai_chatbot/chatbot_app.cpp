// Application flow: launcher, chat, Wi-Fi screen, app builder and app runner
// (kept out of the .ino so Arduino's prototype generator leaves it alone).

#include <WiFi.h>
#include <esp_system.h>
#include <map>
#include <time.h>

#include "chatbot_app.h"
#include "chatbot_internal.h"
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

namespace chatbot {
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

void showLauncher(const String &notice)
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

}  // namespace chatbot

using namespace chatbot;

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
