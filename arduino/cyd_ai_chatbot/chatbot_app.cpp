// Application logic of the CYD AI chatbot (kept out of the .ino so Arduino's prototype generator leaves it alone).

#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

#include "chatbot_app.h"
#include "board_pins.h"
#include "chat_ui.h"
#include "claude_client.h"
#include "config.h"
#include "settings_store.h"
#include "touch_xpt2046.h"

namespace {

constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kWifiRetryIntervalMs = 30000;
constexpr uint32_t kClockSyncWaitMs = 5000;
constexpr uint32_t kStatusRefreshMs = 1000;
constexpr uint32_t kTouchRepeatGuardMs = 120;
constexpr uint32_t kBootHoldWindowMs = 1500;
constexpr int16_t kCalMargin = 30;
constexpr int16_t kCalMinRawSpan = 400;
constexpr uint32_t kCalStableMs = 300;
constexpr size_t kSerialLineMax = 512;
constexpr const char *kNtpServer1 = "pool.ntp.org";
constexpr const char *kNtpServer2 = "time.google.com";
constexpr time_t kValidEpoch = 1700000000;  // anything earlier means NTP has not answered yet

Adafruit_ST7789 tft(&SPI, CYD_PIN_LCD_CS, CYD_PIN_LCD_DC, CYD_PIN_LCD_RST);
TouchXpt2046 touch(SPI, CYD_PIN_TOUCH_CS, kScreenW, kScreenH);
ChatUi ui(tft);
ClaudeClient claude;
AppSettings settings;
std::vector<ChatMessage> history;

bool touchWasDown = false;
uint32_t lastTouchMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastWifiAttemptMs = 0;
String serialLine;

// ---------------------------------------------------------------- hardware helpers

void setLed(uint8_t r, uint8_t g, uint8_t b)
{
#if RGB_LED_ENABLED
    rgbLedWrite(CYD_PIN_RGB_LED, (uint16_t)r * RGB_LED_LEVEL / 255, (uint16_t)g * RGB_LED_LEVEL / 255,
                (uint16_t)b * RGB_LED_LEVEL / 255);
#endif
}

void setBacklight(uint8_t percent)
{
    const uint32_t maxDuty = (1U << BACKLIGHT_PWM_BITS) - 1;
    ledcWrite(CYD_PIN_LCD_BACKLIGHT, maxDuty * min<uint8_t>(percent, 100) / 100);
}

void initDisplay()
{
    // All devices on the shared bus must be deselected before the first transfer.
    for (int8_t cs : {CYD_PIN_LCD_CS, CYD_PIN_TOUCH_CS, CYD_PIN_SD_CS}) {
        pinMode(cs, OUTPUT);
        digitalWrite(cs, HIGH);
    }
    SPI.begin(CYD_PIN_SPI_SCK, CYD_PIN_SPI_MISO, CYD_PIN_SPI_MOSI);

    ledcAttach(CYD_PIN_LCD_BACKLIGHT, BACKLIGHT_PWM_HZ, BACKLIGHT_PWM_BITS);
    setBacklight(LCD_BRIGHTNESS_PERCENT);

    tft.init(LCD_NATIVE_WIDTH, LCD_NATIVE_HEIGHT);
    tft.setSPISpeed(LCD_SPI_HZ);
    tft.setRotation(LCD_ROTATION);
    tft.invertDisplay(LCD_INVERT_COLORS);
    touch.begin();
}

// ---------------------------------------------------------------- touch calibration

// Waits for a stable press and its release; returns the averaged raw reading.
TouchRaw waitForCalibrationTap()
{
    TouchRaw raw = {};
    while (true) {
        TouchRaw sample;
        if (touch.readRaw(sample)) {
            int32_t sx = 0, sy = 0, n = 0;
            const uint32_t start = millis();
            while (millis() - start < kCalStableMs) {
                if (touch.readRaw(sample)) {
                    sx += sample.x;
                    sy += sample.y;
                    ++n;
                }
                delay(5);
            }
            if (n > 0) {
                raw.x = sx / n;
                raw.y = sy / n;
                while (touch.readRaw(sample)) {  // wait for release
                    delay(10);
                }
                delay(150);
                return raw;
            }
        }
        delay(10);
    }
}

void drawCrosshair(int16_t x, int16_t y, uint16_t color)
{
    tft.drawFastHLine(x - 10, y, 21, color);
    tft.drawFastVLine(x, y - 10, 21, color);
    tft.drawCircle(x, y, 6, color);
}

TouchRaw calibrationPoint(int16_t x, int16_t y)
{
    ui.drawSplash("Hieu chinh cam ung", "Cham va giu vao tam dau +", "(dung que nhua hoac mong tay)");
    drawCrosshair(x, y, ST77XX_WHITE);
    const TouchRaw raw = waitForCalibrationTap();
    Serial.printf("[touch] screen(%d,%d) -> raw(%d,%d)\n", x, y, raw.x, raw.y);
    return raw;
}

void runTouchCalibration()
{
    while (true) {
        const int16_t left = kCalMargin, right = kScreenW - kCalMargin;
        const int16_t top = kCalMargin, bottom = kScreenH - kCalMargin;
        const TouchRaw p0 = calibrationPoint(left, top);
        const TouchRaw p1 = calibrationPoint(right, top);
        const TouchRaw p2 = calibrationPoint(left, bottom);

        // Moving along screen X should change one raw axis much more than the other.
        const bool swap = abs(p1.y - p0.y) > abs(p1.x - p0.x);
        TouchCalibration cal;
        cal.magic = TouchCalibration::kMagic;
        cal.swapXy = swap;
        cal.rawX0 = swap ? p0.y : p0.x;
        cal.rawX1 = swap ? p1.y : p1.x;
        cal.rawY0 = swap ? p0.x : p0.y;
        cal.rawY1 = swap ? p2.x : p2.y;
        cal.screenX0 = left;
        cal.screenX1 = right;
        cal.screenY0 = top;
        cal.screenY1 = bottom;

        if (abs(cal.rawX1 - cal.rawX0) >= kCalMinRawSpan && abs(cal.rawY1 - cal.rawY0) >= kCalMinRawSpan) {
            touch.setCalibration(cal);
            settings_store::saveTouchCalibration(cal);
            ui.drawSplash("Xong!", "Da luu hieu chinh cam ung.");
            delay(800);
            return;
        }
        ui.drawSplash("Chua dat", "Cac diem cham qua gan nhau, lam lai nhe.");
        delay(1500);
    }
}

bool bootButtonHeld()
{
    pinMode(CYD_PIN_BOOT_BUTTON, INPUT_PULLUP);
    const uint32_t start = millis();
    while (millis() - start < kBootHoldWindowMs) {
        if (digitalRead(CYD_PIN_BOOT_BUTTON) == LOW) {
            return true;
        }
        delay(10);
    }
    return false;
}

// ---------------------------------------------------------------- Wi-Fi and clock

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

void syncClock()
{
    configTime(TIMEZONE_OFFSET_SECONDS, 0, kNtpServer1, kNtpServer2);
    const uint32_t start = millis();
    while (time(nullptr) < kValidEpoch && millis() - start < kClockSyncWaitMs) {
        delay(100);
    }
}

bool connectWifi(bool showProgress)
{
    lastWifiAttemptMs = millis();
    if (settings.wifiSsid.length() == 0) {
        return false;
    }
    if (showProgress) {
        ui.drawSplash("Dang ket noi Wi-Fi", settings.wifiSsid.c_str());
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiConnectTimeoutMs) {
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[wifi] connect failed");
        return false;
    }
    Serial.printf("[wifi] connected, IP %s\n", WiFi.localIP().toString().c_str());
    syncClock();
    return true;
}

void refreshStatus()
{
    String status;
    if (WiFi.status() != WL_CONNECTED) {
        status = "Mat Wi-Fi";
    } else {
        status = clockText();
        if (status.length() == 0) {
            status = WiFi.localIP().toString();
        }
    }
    ui.setStatus(status);
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
    ui.addMessage(Speaker::User, question);
    if (WiFi.status() != WL_CONNECTED && !connectWifi(false)) {
        ui.addMessage(Speaker::System, "Chua co Wi-Fi. Go /wifi TEN|MATKHAU qua Serial.");
        setLed(255, 0, 0);
        return;
    }

    history.push_back({ChatRole::User, question});
    trimHistory();

    ui.setBusy(true);
    setLed(0, 0, 255);
    String reply, error;
    const bool ok = claude.ask(history, reply, error);
    ui.setBusy(false);

    if (ok) {
        history.push_back({ChatRole::Assistant, reply});
        ui.addMessage(Speaker::Claude, reply);
        Serial.printf("Claude: %s\n", reply.c_str());
        setLed(0, 255, 0);
    } else {
        history.pop_back();  // keep user/assistant turns alternating
        ui.addMessage(Speaker::System, error);
        Serial.printf("[claude] %s\n", error.c_str());
        setLed(255, 0, 0);
    }
}

void clearConversation()
{
    history.clear();
    ui.clearChat();
    ui.addMessage(Speaker::System, "Da xoa hoi thoai. Bam \"Nhap\" de hoi Claude.");
    setLed(0, 0, 0);
}

// ---------------------------------------------------------------- Serial commands

void printHelp()
{
    Serial.println("Lenh Serial:");
    Serial.println("  /wifi TEN_WIFI|MAT_KHAU   luu Wi-Fi va ket noi lai");
    Serial.println("  /key sk-ant-...           luu API key");
    Serial.println("  /model claude-haiku-5-5   doi model");
    Serial.println("  /cal                      hieu chinh cam ung");
    Serial.println("  /clear                    xoa hoi thoai");
    Serial.println("  /status                   xem trang thai");
    Serial.println("  (go chu binh thuong de hoi Claude)");
}

void printStatus()
{
    Serial.printf("Wi-Fi: %s (%s)\n", settings.wifiSsid.c_str(),
                  WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "chua ket noi");
    Serial.printf("Model: %s, API key: %s\n", claude.model().c_str(), claude.hasApiKey() ? "da co" : "CHUA CO");
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
        WiFi.disconnect();
        Serial.println(connectWifi(false) ? "Wi-Fi OK" : "Wi-Fi loi, kiem tra ten/mat khau");
        refreshStatus();
    } else if (line.startsWith("/key ")) {
        settings.apiKey = line.substring(5);
        settings.apiKey.trim();
        settings_store::saveApiKey(settings.apiKey);
        claude.configure(settings.apiKey, settings.model);
        Serial.println("Da luu API key.");
    } else if (line.startsWith("/model ")) {
        settings.model = line.substring(7);
        settings.model.trim();
        settings_store::saveModel(settings.model);
        claude.configure(settings.apiKey, settings.model);
        Serial.printf("Model: %s\n", settings.model.c_str());
    } else if (line == "/cal") {
        runTouchCalibration();
        ui.showChat();
    } else if (line == "/clear") {
        clearConversation();
    } else if (line == "/status") {
        printStatus();
    } else if (line.startsWith("/")) {
        printHelp();
    } else {
        if (ui.keyboardVisible()) {
            ui.showChat();
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
                handleSerialCommand(serialLine);
                serialLine = "";
            }
        } else if (serialLine.length() < kSerialLineMax) {
            serialLine += c;
        }
    }
}

// ---------------------------------------------------------------- touch

void pollTouch()
{
    int16_t x, y;
    const bool down = touch.read(x, y);
    const uint32_t now = millis();
    if (down && !touchWasDown && now - lastTouchMs > kTouchRepeatGuardMs) {
        lastTouchMs = now;
        switch (ui.handleTouch(x, y)) {
        case UiAction::Send: {
            const String question = ui.input();
            ui.clearInput();
            ui.showChat();
            askClaude(question);
            break;
        }
        case UiAction::ClearChat:
            clearConversation();
            break;
        case UiAction::None:
            break;
        }
    }
    touchWasDown = down;
}

}  // namespace

void chatbotSetup()
{
    Serial.begin(kSerialBaud);
    setLed(0, 0, 0);
    initDisplay();
    ui.begin();
    ui.drawSplash("Claude Chat", "NM-CYD-C5 - giu nut BOOT de hieu chinh cam ung");

    settings = settings_store::load();
    claude.configure(settings.apiKey, settings.model);

    TouchCalibration cal;
    const bool haveCal = settings_store::loadTouchCalibration(cal);
    if (haveCal) {
        touch.setCalibration(cal);
    }
    if (bootButtonHeld() || !haveCal) {
        runTouchCalibration();
    }

    const bool wifiOk = connectWifi(true);
    ui.showChat();
    ui.addMessage(Speaker::System, "Xin chao! Bam \"Nhap\" de hoi Claude, hoac go cau hoi trong Serial Monitor.");
    if (!wifiOk) {
        ui.addMessage(Speaker::System, "Chua ket noi Wi-Fi. Go /wifi TEN|MATKHAU trong Serial Monitor (115200).");
    }
    if (!claude.hasApiKey()) {
        ui.addMessage(Speaker::System, "Chua co API key. Go /key sk-ant-... trong Serial Monitor.");
    }
    refreshStatus();
    printHelp();
    printStatus();
}

void chatbotLoop()
{
    pollSerial();
    pollTouch();

    const uint32_t now = millis();
    if (now - lastStatusMs > kStatusRefreshMs) {
        lastStatusMs = now;
        refreshStatus();
    }
    if (WiFi.status() != WL_CONNECTED && settings.wifiSsid.length() > 0 &&
        now - lastWifiAttemptMs > kWifiRetryIntervalMs) {
        connectWifi(false);
    }
    delay(5);
}
