// Shared state and helpers of the main application flow, split over chatbot_app.cpp (screens, touch,
// apps, chat) and chatbot_services.cpp (Wi-Fi, Settings actions, Serial commands, web hooks, start-up).
// Private to the sketch: only those two files include it.
#pragma once

#include <WiFi.h>
#include <esp_system.h>
#include <map>
#include <time.h>
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


// Defined in chatbot_app.cpp.
extern Adafruit_GFX &tft;
extern ChatUi chat;
extern LauncherUi launcher;
extern WifiUi wifiScreen;
extern TouchKeyboard keyboard;
extern SettingsUi settingsUi;
extern GamesUi games;
extern MapUi mapUi;
extern FaceUi face;
extern ClockUi clockUi;
extern CalendarUi calendarUi;
extern MediaUi media;
extern ObdUi obdUi;
extern TpmsUi tpmsUi;
extern PinUi pinUi;
extern ClaudeClient claude;
extern ClaudeAsync claudeAsync;
extern AppBuilder builder;
extern GfxCanvas canvas;
extern BoardDevice device;
extern mini_app::MiniApp runningApp;
extern AppSettings settings;
extern DeviceSettings deviceCfg;
extern ProfileSettings profile;
extern std::vector<ChatMessage> history;
extern std::vector<ChatMessage> faceHistory;
extern Screen screen;
extern Screen pressScreen;
extern InputPurpose inputPurpose;
extern std::vector<AppTile> tiles;
extern std::vector<WifiNetwork> scanResults;
extern std::map<String, String> lastErrors;
extern String runningAppId;
extern String editAppId;
extern String pendingSsid;
extern String pendingWebCreate;
extern String serialLine;
extern bool touchDown;
extern bool longFired;
extern bool swallowTouch;
extern bool deviceDirty;
extern bool screenAsleep;
extern bool wifiFromSettings;
extern uint32_t lastActivityMs;
extern int16_t pressX;
extern int16_t pressY;
extern int16_t lastX;
extern int16_t lastY;
extern uint32_t pressStartMs;
extern uint32_t lastTouchSeenMs;
extern bool bootWasDown;
extern bool wasConnected;
extern uint32_t lastStatusMs;
extern uint32_t lastWifiAttemptMs;
extern int16_t pressDy;
extern bool appsUnlocked;
extern PinPurpose pinPurpose;
extern LauncherAction pendingLocked;
extern size_t pendingLockedApp;
extern String pendingWebSsid;
extern String pendingWebPassword;
extern bool timezoneSynced;
extern int lastChimeHour;
extern uint32_t chimeStepMs;
extern uint8_t chimeBeepsLeft;
extern qr_code::Code apQr;

// Functions of both files.
String clockText();
String statusText();
String launcherStatusText();
String wifiStatusText();
int32_t timezoneOffset();
String timezoneText();
void startClockSync(bool wait);
void hideAll();
void useDarkPalette();
void showLauncher(const String &notice = String());
String resetReasonText();
SettingsInfo settingsInfo();
void showSettings();
void resumeSettings();
void showGames();
void showMap();
void showFace();
String locationName();
void showClock();
void showCalendar();
void showMedia(MediaMode mode);
void showObd(bool autoConnect);
void showTpms();
void showPin(PinPurpose purpose, PinMode mode, const char *title);
void showApSetup();
String profilePrompt();
String chatPrompt();
void showChat();
void showKeyboard(InputPurpose purpose, const char *title, const String &initial, const char *placeholder, size_t maxChars, const char *doneLabel);
void showWifi(bool rescan);
void rememberWifi(const String &ssid, const String &password);
bool connectSaved(bool showProgress);
void connectTo(const String ssid, const String password);
void selectNetwork(const WifiNetwork net);
void maintainWifi();
void trimHistory();
void askClaude(const String &question);
void clearConversation();
void trimFaceHistory();
void askFace(const String &question);
void pollFaceAnswer();
void applyDevice(const DeviceSettings &next);
void saveDeviceIfDirty();
void nextModel();
void syncTimezone();
void setLocation(const String &text);
void forgetWifi(size_t index);
void handleSettingsAction(SettingsAction action);
void openApp(const String &id);
void exitApp();
String appNameOf(const String &json);
void buildApp(const String &request, const String &editId);
String installAppJson(const String &json);
void deleteApp(const AppTile &tile);
void handleKeyboardDone();
void handleKeyboardCancel();
bool needsUnlock(LauncherAction action);
void handlePinResult(PinResult result);
void handleLauncherActionUnlocked(LauncherAction action, size_t appIndex);
void handleLauncherAction(LauncherAction action);
void handleMenuAction(MenuAction action);
void onPress(int16_t x, int16_t y);
void goBack();
bool swipeBackAllowed();
void onDrag(int16_t x, int16_t y);
void onRelease();
void onTap(int16_t x, int16_t y, int16_t dx);
void onLongPress(int16_t x, int16_t y);
uint32_t longPressMs();
void wakeScreen();
void maintainScreenSleep(uint32_t now);
void pollTouch();
void pollBootButton();
void printHelp();
void printStatus();
void handleSerialCommand(String line);
void pollSerial();
void startWebPortal();
void runPendingWebWifi();
void maintainHourlyChime(uint32_t now);
void maintainWeather(uint32_t now);
void runStartup();
void runPendingWebCreate();
void refreshStatus();

}  // namespace chatbot
