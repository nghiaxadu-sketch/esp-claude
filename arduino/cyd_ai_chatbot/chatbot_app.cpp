// Application flow: launcher, chat, Wi-Fi screen, app builder and app runner
// (kept out of the .ino so Arduino's prototype generator leaves it alone).

#include <WiFi.h>
#include <map>
#include <time.h>

#include "chatbot_app.h"
#include "app_builder.h"
#include "app_device.h"
#include "app_store.h"
#include "app_validator.h"
#include "board_hw.h"
#include "board_pins.h"
#include "chat_ui.h"
#include "claude_async.h"
#include "claude_client.h"
#include "config.h"
#include "face_ui.h"
#include "games_ui.h"
#include "launcher_ui.h"
#include "mini_app.h"
#include "mini_json.h"
#include "settings_store.h"
#include "settings_ui.h"
#include "touch_keyboard.h"
#include "ui_common.h"
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

enum class Screen : uint8_t { Launcher, Chat, Keyboard, Wifi, App, Busy, Settings, Games, Face };
enum class InputPurpose : uint8_t { ChatQuestion, NewApp, EditApp, WifiPassword, FaceQuestion };

Adafruit_GFX &tft = board_hw::display();
ChatUi chat(tft);
LauncherUi launcher(tft);
WifiUi wifiScreen(tft);
TouchKeyboard keyboard(tft);
SettingsUi settingsUi(tft);
GamesUi games(tft);
FaceUi face(tft);
ClaudeClient claude;
ClaudeAsync claudeAsync(claude);
AppBuilder builder(claude);
GfxCanvas canvas(tft);
BoardDevice device;
mini_app::MiniApp runningApp(canvas, device);
AppSettings settings;
DeviceSettings deviceCfg;
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

void startClockSync(bool wait)
{
    configTime(TIMEZONE_OFFSET_SECONDS, 0, kNtpServer1, kNtpServer2);
    const uint32_t start = millis();
    while (wait && time(nullptr) < kValidEpoch && millis() - start < kClockSyncWaitMs) {
        delay(100);
    }
}

void hideAll()
{
    chat.hide();
    launcher.hide();
    wifiScreen.hide();
    settingsUi.hide();
    games.hide();
    face.hide();
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
    info.freePsram = ESP.getFreePsram();
    info.appCount = app_store::list().size();
    info.uptimeSec = millis() / 1000;
    return info;
}

void showSettings()
{
    hideAll();
    screen = Screen::Settings;
    settingsUi.show(settingsInfo(), deviceCfg);
}

void showGames()
{
    hideAll();
    screen = Screen::Games;
    games.show();
}

void showFace()
{
    hideAll();
    screen = Screen::Face;
    face.show();
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

bool connectSaved(bool showProgress)
{
    lastWifiAttemptMs = millis();
    if (settings.wifiSsid.length() == 0) {
        return false;
    }
    if (showProgress) {
        ui::drawSplash(tft, "Dang ket noi Wi-Fi", ui::toAscii(settings.wifiSsid).c_str());
    }
    const bool ok = wifi_manager::connect(settings.wifiSsid, settings.wifiPassword, kWifiConnectTimeoutMs);
    if (ok) {
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
        settings.wifiSsid = ssid;
        settings.wifiPassword = password;
        settings_store::saveWifi(ssid, password);
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
    if (!connected && settings.wifiSsid.length() > 0 && screen != Screen::Wifi &&
        millis() - lastWifiAttemptMs > kWifiRetryIntervalMs) {
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
    const bool ok = claude.ask(kChatSystemPrompt, history, CLAUDE_MAX_TOKENS, CLAUDE_EFFORT, reply, error);
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
    if (!claudeAsync.start(kFaceSystemPrompt, faceHistory, FACE_MAX_TOKENS, FACE_EFFORT)) {
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

void handleSettingsAction(SettingsAction action)
{
    switch (action) {
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
    default:
        showLauncher();
        break;
    }
}

void handleLauncherAction(LauncherAction action)
{
    switch (action) {
    case LauncherAction::OpenApp:
        openApp(tiles[launcher.selected()].id);
        break;
    case LauncherAction::AppMenu:
        launcher.showMenu(tiles[launcher.selected()]);
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
    case LauncherAction::Face:
        showFace();
        break;
    case LauncherAction::None:
        break;
    }
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
    case Screen::Launcher:
    case Screen::Busy:
        break;
    }
}

// The stylus moved while pressed.
void onDrag(int16_t x, int16_t y)
{
    if (screen == Screen::Games) {
        games.drag(x, y);
    } else if (screen == Screen::Settings) {
        handleSettingsAction(settingsUi.handleDrag(x, y));
    }
}

void onRelease()
{
    if (screen == Screen::Games) {
        games.release();
    }
    saveDeviceIfDirty();
}

// Released before the long-press time (launcher only: a hold must not also open the app).
// dx is how far the stylus moved sideways: a swipe changes the page instead of opening an icon.
void onTap(int16_t x, int16_t y, int16_t dx)
{
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
    Serial.println("  /status                   xem trang thai");
    Serial.println("  (go chu binh thuong de hoi Claude)");
}

void printStatus()
{
    Serial.printf("Board: %s\n", CYD_BOARD_NAME);
    Serial.printf("Wi-Fi: %s\n", wifiStatusText().c_str());
    Serial.printf("Model: %s, API key: %s\n", claude.model().c_str(), claude.hasApiKey() ? "da co" : "CHUA CO");
    Serial.printf("Apps: %u, file system: %s\n", (unsigned)app_store::list().size(), app_store::ready() ? "ok" : "LOI");
    Serial.printf("Heap: %u, PSRAM: %u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
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
    web_portal::begin(hooks);
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
    board_hw::begin();
    deviceCfg = settings_store::loadDevice();
    board_hw::setFlipped(deviceCfg.flipped);
    board_hw::setBrightnessSetting(deviceCfg.brightness);
    board_hw::setSoundEnabled(deviceCfg.sound);
    chat.begin();
    ui::drawSplash(tft, "Claude Apps", CYD_BOARD_NAME, "Giu nut BOOT de hieu chinh cam ung");

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
    pollFaceAnswer();

    const uint32_t now = millis();
    face.loop(now);
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
    const bool animating = screen == Screen::App || screen == Screen::Games || screen == Screen::Face;
    delay(animating ? 1 : 5);
}
