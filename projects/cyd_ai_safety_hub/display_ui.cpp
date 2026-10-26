#include "display_ui.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <time.h>

#include <string>
#include <vector>

#include "app_state.h"
#include "settings_store.h"
#include "text_util.h"

namespace {

// ---------------- Cấu hình phần cứng màn CYD cho LovyanGFX ----------------
class LGFX_CYD : public lgfx::LGFX_Device {
#if CYD_PANEL_ST7789
    lgfx::Panel_ST7789 panel_;
#else
    lgfx::Panel_ILI9341 panel_;
#endif
    lgfx::Bus_SPI bus_;
    lgfx::Light_PWM light_;
    lgfx::Touch_XPT2046 touch_;

public:
    LGFX_CYD() {
        {
            auto cfg = bus_.config();
            cfg.spi_host = SPI2_HOST;  // HSPI
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = PIN_TFT_SCLK;
            cfg.pin_mosi = PIN_TFT_MOSI;
            cfg.pin_miso = PIN_TFT_MISO;
            cfg.pin_dc = PIN_TFT_DC;
            bus_.config(cfg);
            panel_.setBus(&bus_);
        }
        {
            auto cfg = panel_.config();
            cfg.pin_cs = PIN_TFT_CS;
            cfg.pin_rst = -1;
            cfg.pin_busy = -1;
            cfg.panel_width = 240;
            cfg.panel_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable = true;
            cfg.invert = CYD_PANEL_INVERT;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            panel_.config(cfg);
        }
        {
            auto cfg = light_.config();
            cfg.pin_bl = PIN_TFT_BL;
            cfg.invert = false;
            cfg.freq = 12000;
            cfg.pwm_channel = 7;
            light_.config(cfg);
            panel_.setLight(&light_);
        }
        {
            auto cfg = touch_.config();
            cfg.x_min = 300;
            cfg.x_max = 3900;
            cfg.y_min = 200;
            cfg.y_max = 3700;
            cfg.pin_int = PIN_TOUCH_IRQ;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.spi_host = SPI3_HOST;  // VSPI, chân riêng cho XPT2046
            cfg.freq = 1000000;
            cfg.pin_sclk = PIN_TOUCH_SCLK;
            cfg.pin_mosi = PIN_TOUCH_MOSI;
            cfg.pin_miso = PIN_TOUCH_MISO;
            cfg.pin_cs = PIN_TOUCH_CS;
            touch_.config(cfg);
            panel_.setTouch(&touch_);
        }
        setPanel(&panel_);
    }
};

LGFX_CYD lcd;

// ---------------- Bố cục ----------------
constexpr int W = 320;
constexpr int H = 240;
constexpr int HEADER_H = 22;
constexpr int FOOTER_Y = 208;
constexpr int TAB_W = W / DisplayUi::PAGE_COUNT;
constexpr int CONTENT_Y = HEADER_H + 2;
constexpr int GRAPH_X = 8, GRAPH_Y = 132, GRAPH_W = 190, GRAPH_H = 70;
constexpr int BADGE_X = 8, BADGE_Y = 28, BADGE_W = 304, BADGE_H = 42;
constexpr int BOX_W = 100, BOX_H = 62, BOX_GAP = 2, BOX_Y0 = 76;
constexpr uint32_t GRAPH_REFRESH_MS = 60000;

const uint16_t C_BG = 0x0000;
const uint16_t C_PANEL = lgfx::color565(24, 32, 48);
const uint16_t C_TEXT = 0xFFFF;
const uint16_t C_DIM = lgfx::color565(140, 150, 165);
const uint16_t C_GREEN = lgfx::color565(40, 200, 90);
const uint16_t C_YELLOW = lgfx::color565(250, 200, 30);
const uint16_t C_ORANGE = lgfx::color565(255, 120, 20);
const uint16_t C_RED = lgfx::color565(230, 30, 30);
const uint16_t C_BLUE = lgfx::color565(60, 140, 255);
const uint16_t C_MASCOT = lgfx::color565(235, 245, 255);

uint16_t levelColor(safety::Level lv) {
    switch (lv) {
        case safety::LEVEL_SAFE: return C_GREEN;
        case safety::LEVEL_WATCH: return C_YELLOW;
        case safety::LEVEL_DANGER: return C_ORANGE;
        case safety::LEVEL_EMERGENCY: return C_RED;
    }
    return C_DIM;
}

uint16_t airColor(safety::AirLevel a) {
    switch (a) {
        case safety::AIR_GOOD: return C_GREEN;
        case safety::AIR_OK: return C_BLUE;
        case safety::AIR_STUFFY: return C_YELLOW;
        case safety::AIR_BAD: return C_RED;
        default: return C_DIM;
    }
}

String fmt(float v, int dec, const char *unit) {
    if (isnan(v)) return String("--");
    return String(v, dec) + unit;
}

const char *kTabNames[DisplayUi::PAGE_COUNT] = {"KHONG KHI", "SAC XE", "TRO LY AI"};

uint32_t hashStr(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) h = (h ^ static_cast<uint8_t>(*s++)) * 16777619u;
    return h;
}

}  // namespace

// ---------------- Khởi tạo ----------------

void DisplayUi::begin() {
    lcd.init();
    lcd.setRotation(1);  // ngang 320x240
    lcd.setBrightness(200);
    lcd.fillScreen(C_BG);
}

bool DisplayUi::touchHeldAtBoot() {
    int32_t x, y;
    return lcd.getTouchRaw(&x, &y) > 0;
}

void DisplayUi::calibrateTouch(bool force) {
    uint16_t cal[8];
    if (!force && settings::loadTouchCalibration(cal)) {
        lcd.setTouchCalibrate(cal);
        return;
    }
    lcd.fillScreen(C_BG);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(C_TEXT, C_BG);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.drawString("HIEU CHINH CAM UNG", W / 2, H / 2 - 20);
    lcd.drawString("Cham vao dau mui ten o 4 goc", W / 2, H / 2 + 4);
    lcd.calibrateTouch(cal, C_TEXT, C_BG, 15);
    settings::saveTouchCalibration(cal);
    lcd.setTouchCalibrate(cal);
    needFull_ = true;
}

void DisplayUi::showMessage(const char *title, const char *line1, const char *line2, const char *line3) {
    messageMode_ = true;
    lcd.fillScreen(C_BG);
    lcd.fillRect(0, 0, W, 34, C_PANEL);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setFont(&fonts::Font4);
    lcd.setTextColor(C_TEXT, C_PANEL);
    lcd.drawString(title, W / 2, 17);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(C_TEXT, C_BG);
    const char *lines[3] = {line1, line2, line3};
    for (int i = 0; i < 3; ++i) {
        if (lines[i]) lcd.drawString(lines[i], W / 2, 80 + i * 36);
    }
}

void DisplayUi::showPortal(const char *ssid, const char *pass) {
    char l1[64], l2[64];
    snprintf(l1, sizeof(l1), "Wi-Fi: %s", ssid);
    snprintf(l2, sizeof(l2), "Mat khau: %s", pass);
    showMessage("CAI DAT THIET BI", l1, l2, "Mo trinh duyet: 192.168.4.1");
}

// ---------------- Vòng lặp ----------------

void DisplayUi::tick() {
    if (messageMode_) {
        bool portal;
        {
            StateLock l;
            portal = AppState::dev.portalActive;
        }
        if (portal) return;
        messageMode_ = false;
        needFull_ = true;
    }
    handleTouch();
    const uint32_t now = millis();
    if (!needFull_ && now - lastDrawMs_ < DISPLAY_PERIOD_MS) return;
    lastDrawMs_ = now;

    safety::Level lv;
    {
        StateLock l;
        lv = AppState::snap.level;
    }
    if (lv == safety::LEVEL_EMERGENCY) {
        drawEmergency((now / 1000) % 2 == 0);
        emergencyShown_ = true;
        return;
    }
    if (emergencyShown_) {
        emergencyShown_ = false;
        needFull_ = true;
    }
    if (needFull_) {
        drawFrame();
        needFull_ = false;
    }
    drawHeader();
    switch (page_) {
        case PAGE_AIR:
            drawAirValues();
            if (now - lastGraphMs_ >= GRAPH_REFRESH_MS) drawGraph();
            break;
        case PAGE_CHARGE:
            drawChargeValues();
            break;
        case PAGE_AI:
            drawAiPage();
            break;
        default:
            break;
    }
}

void DisplayUi::handleTouch() {
    int32_t x, y;
    const bool touched = lcd.getTouch(&x, &y) > 0;
    const bool pressEdge = touched && !wasTouched_;
    wasTouched_ = touched;
    if (!pressEdge) return;
    if (touchCb_) touchCb_();

    safety::Level lv;
    {
        StateLock l;
        lv = AppState::snap.level;
    }
    // Màn khẩn cấp hoặc chạm vào ô mức cảnh báo -> tắt còi
    if (lv == safety::LEVEL_EMERGENCY ||
        (page_ == PAGE_CHARGE && lv != safety::LEVEL_SAFE && y >= BADGE_Y && y < BADGE_Y + BADGE_H)) {
        if (silenceCb_) silenceCb_();
        return;
    }
    if (y >= FOOTER_Y) {
        const int tab = constrain(x / TAB_W, 0, PAGE_COUNT - 1);
        if (tab != page_) {
            page_ = static_cast<Page>(tab);
            needFull_ = true;
        }
    }
}

// ---------------- Khung chung ----------------

void DisplayUi::drawFrame() {
    lcd.fillScreen(C_BG);
    lastMascot_ = 255;
    lastAiHash_ = 0;
    drawFooter();
    switch (page_) {
        case PAGE_AIR:
            drawAirStatic();
            drawGraph();
            break;
        case PAGE_CHARGE:
            drawChargeStatic();
            break;
        default:
            break;
    }
}

void DisplayUi::drawHeader() {
    char buf[40];
    struct tm t;
    if (getLocalTime(&t, 0) && t.tm_year > (2024 - 1900)) {
        static const char *kDays[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
        snprintf(buf, sizeof(buf), "%02d:%02d  %s %02d/%02d", t.tm_hour, t.tm_min, kDays[t.tm_wday % 7], t.tm_mday, t.tm_mon + 1);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }
    DeviceInfo dev;
    safety::Level lv;
    bool silenced;
    {
        StateLock l;
        dev = AppState::dev;
        lv = AppState::snap.level;
        silenced = AppState::snap.silenced;
    }
    lcd.fillRect(0, 0, W, HEADER_H, C_PANEL);
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(textdatum_t::middle_left);
    lcd.setTextColor(C_TEXT, C_PANEL);
    lcd.drawString(buf, 6, HEADER_H / 2);
    lcd.setTextDatum(textdatum_t::middle_right);
    String right;
    if (silenced) right += "COI TAT  ";
    right += dev.wifiOk ? "WiFi" : "WiFi X";
    right += dev.telegramOk ? "  TG" : "";
    lcd.setTextColor(dev.wifiOk ? C_TEXT : C_ORANGE, C_PANEL);
    lcd.drawString(right.c_str(), W - 24, HEADER_H / 2);
    lcd.fillCircle(W - 11, HEADER_H / 2, 6, levelColor(lv));
}

void DisplayUi::drawFooter() {
    for (int i = 0; i < PAGE_COUNT; ++i) {
        const bool active = i == page_;
        const uint16_t bg = active ? C_BLUE : C_PANEL;
        lcd.fillRoundRect(i * TAB_W + 2, FOOTER_Y + 2, TAB_W - 4, H - FOOTER_Y - 4, 6, bg);
        lcd.setFont(&fonts::Font2);
        lcd.setTextDatum(textdatum_t::middle_center);
        lcd.setTextColor(C_TEXT, bg);
        lcd.drawString(kTabNames[i], i * TAB_W + TAB_W / 2, FOOTER_Y + (H - FOOTER_Y) / 2);
    }
}

// ---------------- Trang không khí ----------------

void DisplayUi::drawAirStatic() {
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(textdatum_t::top_left);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.drawString("CO2 (ppm)", 8, CONTENT_Y + 4);
    lcd.drawString("CO2 3 gio", GRAPH_X, GRAPH_Y - 16);
    lcd.drawString("Nhiet do", 214, 116);
    lcd.drawString("Do am", 214, 160);
}

void DisplayUi::drawAirValues() {
    Snapshot s;
    {
        StateLock l;
        s = AppState::snap;
    }
    const uint16_t col = airColor(s.air);
    lcd.setTextDatum(textdatum_t::top_left);
    lcd.setFont(&fonts::Font7);
    lcd.setTextColor(col, C_BG);
    lcd.setTextPadding(190);
    lcd.drawString(isnan(s.co2) ? "---" : String(static_cast<int>(s.co2)).c_str(), 8, CONTENT_Y + 24);
    lcd.setFont(&fonts::Font2);
    lcd.setTextPadding(200);
    lcd.drawString(s.present.scd41 ? safety::airNameAscii(s.air) : "Chua cam SCD41", 8, CONTENT_Y + 78);
    lcd.setTextPadding(0);

    lcd.setFont(&fonts::Font4);
    lcd.setTextColor(C_TEXT, C_BG);
    lcd.setTextPadding(100);
    lcd.drawString(fmt(s.roomTempC, 1, " C").c_str(), 214, 132);
    lcd.drawString(fmt(s.humidity, 0, " %").c_str(), 214, 176);
    lcd.setTextPadding(0);

    if (lastMascot_ != s.air) {
        lastMascot_ = s.air;
        drawMascot(s.air);
    }
}

// Linh vật dễ thương đổi nét mặt theo chất lượng không khí
void DisplayUi::drawMascot(uint8_t airLevel) {
    const int cx = 262, cy = 66, r = 34;
    lcd.fillRect(cx - r - 6, cy - r - 16, 2 * r + 12, 2 * r + 22, C_BG);
    // Mầm lá trên đầu
    lcd.drawLine(cx, cy - r, cx, cy - r - 10, C_GREEN);
    lcd.fillEllipse(cx - 7, cy - r - 10, 7, 4, C_GREEN);
    lcd.fillEllipse(cx + 7, cy - r - 12, 7, 4, C_GREEN);
    lcd.fillCircle(cx, cy, r, C_MASCOT);
    const uint16_t ink = lgfx::color565(40, 40, 60);
    const uint16_t cheek = lgfx::color565(255, 170, 190);
    lcd.fillCircle(cx - 20, cy + 8, 5, cheek);
    lcd.fillCircle(cx + 20, cy + 8, 5, cheek);
    if (airLevel == safety::AIR_BAD) {
        // mắt X, miệng méo
        lcd.drawLine(cx - 15, cy - 10, cx - 7, cy - 2, ink);
        lcd.drawLine(cx - 15, cy - 2, cx - 7, cy - 10, ink);
        lcd.drawLine(cx + 7, cy - 10, cx + 15, cy - 2, ink);
        lcd.drawLine(cx + 7, cy - 2, cx + 15, cy - 10, ink);
        lcd.drawArc(cx, cy + 20, 9, 8, 200, 340, ink);
    } else {
        lcd.fillCircle(cx - 11, cy - 6, 4, ink);
        lcd.fillCircle(cx + 11, cy - 6, 4, ink);
        if (airLevel == safety::AIR_GOOD || airLevel == safety::AIR_OK) {
            lcd.drawArc(cx, cy + 4, 10, 9, 20, 160, ink);  // cười
        } else {
            lcd.drawFastHLine(cx - 7, cy + 12, 14, ink);  // bình thản / hơi khó chịu
        }
    }
}

void DisplayUi::drawGraph() {
    lastGraphMs_ = millis();
    uint16_t count, head;
    {
        StateLock l;
        count = AppState::hist.count;
        head = AppState::hist.head;
        memcpy(graphBuf_, AppState::hist.co2, sizeof(graphBuf_));
    }
    lcd.fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, C_PANEL);
    const int minV = 400, maxV = 2000;
    auto yOf = [&](int v) { return GRAPH_Y + GRAPH_H - 1 - (constrain(v, minV, maxV) - minV) * (GRAPH_H - 2) / (maxV - minV); };
    // đường 1000 ppm (nét đứt)
    const int y1000 = yOf(1000);
    for (int x = GRAPH_X; x < GRAPH_X + GRAPH_W; x += 6) lcd.drawFastHLine(x, y1000, 3, C_YELLOW);
    lcd.setFont(&fonts::Font0);
    lcd.setTextDatum(textdatum_t::bottom_right);
    lcd.setTextColor(C_YELLOW, C_PANEL);
    lcd.drawString("1000", GRAPH_X + GRAPH_W - 2, y1000 - 1);
    if (count < 2) return;
    const int start = (head - count + HISTORY_LEN) % HISTORY_LEN;
    int prevX = -1, prevY = -1;
    for (int i = 0; i < count; ++i) {
        const int16_t v = graphBuf_[(start + i) % HISTORY_LEN];
        const int x = GRAPH_X + 1 + (HISTORY_LEN - count + i) * (GRAPH_W - 2) / (HISTORY_LEN - 1);
        if (v < 0) {
            prevX = -1;
            continue;
        }
        const int y = yOf(v);
        if (prevX >= 0) lcd.drawLine(prevX, prevY, x, y, C_BLUE);
        prevX = x;
        prevY = y;
    }
}

// ---------------- Trang sạc xe ----------------

void DisplayUi::drawChargeStatic() {
    static const char *kLabels[6] = {"Nhiet cuc sac", "Khoi / khi", "Toc do tang", "Dien ap", "Dong sac", "Cong suat"};
    for (int i = 0; i < 6; ++i) {
        const int x = 8 + (i % 3) * (BOX_W + BOX_GAP + 1);
        const int y = BOX_Y0 + (i / 3) * (BOX_H + BOX_GAP);
        lcd.fillRoundRect(x, y, BOX_W, BOX_H, 6, C_PANEL);
        lcd.setFont(&fonts::Font2);
        lcd.setTextDatum(textdatum_t::top_left);
        lcd.setTextColor(C_DIM, C_PANEL);
        lcd.drawString(kLabels[i], x + 6, y + 4);
    }
}

void DisplayUi::drawValueBox(int x, int y, int w, int h, const char *label, const char *value, uint16_t color) {
    (void)label;
    (void)h;
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(textdatum_t::top_left);
    lcd.setTextColor(color, C_PANEL);
    lcd.setTextPadding(w - 10);
    lcd.drawString(value, x + 6, y + 26);
    lcd.setTextPadding(0);
}

void DisplayUi::drawChargeValues() {
    Snapshot s;
    safety::Thresholds th;
    {
        StateLock l;
        s = AppState::snap;
        th = AppState::th;
    }
    // Ô mức cảnh báo
    const uint16_t lc = levelColor(s.level);
    lcd.fillRoundRect(BADGE_X, BADGE_Y, BADGE_W, BADGE_H, 8, lc);
    lcd.setFont(&fonts::Font4);
    lcd.setTextDatum(textdatum_t::middle_left);
    lcd.setTextColor(C_BG, lc);
    lcd.drawString(safety::levelNameAscii(s.level), BADGE_X + 10, BADGE_Y + BADGE_H / 2);
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(textdatum_t::middle_right);
    String info;
    if (s.level != safety::LEVEL_SAFE) {
        info = s.silenced ? "Coi dang tat" : "Cham de tat coi";
    } else if (s.charging) {
        const uint32_t min = s.chargeMs / 60000;
        info = "Dang sac " + String(min / 60) + "h" + String(min % 60) + "p " + String(s.chargeWh, 0) + "Wh";
    } else {
        info = "Khong sac";
    }
    lcd.drawString(info.c_str(), BADGE_X + BADGE_W - 10, BADGE_Y + BADGE_H / 2);

    auto colorFor = [&](float v, float warn, float danger) {
        if (isnan(v)) return C_DIM;
        if (danger > 0 && v >= danger) return C_RED;
        if (warn > 0 && v >= warn) return C_YELLOW;
        return C_TEXT;
    };
    const float watts = (!isnan(s.voltageV) && !isnan(s.currentA)) ? s.voltageV * s.currentA : NAN;
    const String smoke = !s.present.mq2 ? String("--") : (isnan(s.smokePct) ? String("lam nong") : fmt(s.smokePct, 0, "%"));
    const String values[6] = {fmt(s.chargerTempC, 1, " C"), smoke, fmt(s.riseCPerMin, 1, " C/p"),
                              fmt(s.voltageV, 1, " V"), fmt(s.currentA, 2, " A"), fmt(watts, 0, " W")};
    const uint16_t colors[6] = {colorFor(s.chargerTempC, th.tempWarnC, th.tempDangerC),
                                colorFor(s.smokePct, th.smokeWarnPct, th.smokeDangerPct),
                                colorFor(s.riseCPerMin, th.riseWarnCPerMin, 2 * th.riseWarnCPerMin),
                                colorFor(s.voltageV, 0, th.voltageMaxV),
                                colorFor(s.currentA, th.currentMaxA * 0.9f, th.currentMaxA),
                                C_TEXT};
    for (int i = 0; i < 6; ++i) {
        const int x = 8 + (i % 3) * (BOX_W + BOX_GAP + 1);
        const int y = BOX_Y0 + (i / 3) * (BOX_H + BOX_GAP);
        drawValueBox(x, y, BOX_W, BOX_H, nullptr, values[i].c_str(), colors[i]);
    }
}

// ---------------- Trang AI ----------------

void DisplayUi::drawAiPage() {
    AiView ai;
    DeviceInfo dev;
    bool paired = settings::anyChatPaired();
    {
        StateLock l;
        ai = AppState::ai;
        dev = AppState::dev;
    }
    char status[64];
    snprintf(status, sizeof(status), "%s|%d|%d|%s|%s", dev.ip, ai.busy, paired, dev.pin, dev.aiConfigured ? "y" : "n");
    const uint32_t h = hashStr(ai.lastAnswer) ^ (hashStr(ai.lastQuestion) * 31u) ^ hashStr(status);
    if (h == lastAiHash_) return;
    lastAiHash_ = h;

    lcd.fillRect(0, CONTENT_Y, W, FOOTER_Y - CONTENT_Y, C_BG);
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(textdatum_t::top_left);
    int y = CONTENT_Y + 2;
    if (!dev.aiConfigured) {
        lcd.setTextColor(C_ORANGE, C_BG);
        lcd.drawString("Chua nhap Anthropic API key.", 8, y);
        y += 18;
        lcd.setTextColor(C_TEXT, C_BG);
        lcd.drawString("Mo trang web cua thiet bi de cai dat:", 8, y);
        y += 18;
    }
    if (!paired) {
        lcd.setTextColor(C_YELLOW, C_BG);
        lcd.drawString("Ghep doi Telegram: nhan cho bot", 8, y);
        lcd.setFont(&fonts::Font4);
        lcd.drawString((String("/pair ") + dev.pin).c_str(), 8, y + 18);
        lcd.setFont(&fonts::Font2);
        y += 50;
    }
    if (ai.lastQuestion[0]) {
        lcd.setTextColor(C_DIM, C_BG);
        const std::vector<std::string> q = textutil::wrapAscii(ai.lastQuestion, 44, 2);
        for (const std::string &line : q) {
            lcd.drawString(line.c_str(), 8, y);
            y += 16;
        }
        y += 2;
    }
    lcd.setTextColor(ai.busy ? C_YELLOW : C_TEXT, C_BG);
    const int maxLines = (FOOTER_Y - 20 - y) / 16;
    if (maxLines > 0) {
        const char *ans = ai.lastAnswer[0] ? ai.lastAnswer : "Hoi minh bat cu dieu gi qua Telegram hoac trang web!";
        std::vector<std::string> lines = textutil::wrapAscii(ans, 44, 64);
        // Đang trả lời: hiện phần cuối; xong: hiện phần đầu
        const size_t from = (ai.busy && lines.size() > static_cast<size_t>(maxLines)) ? lines.size() - maxLines : 0;
        for (size_t i = from; i < lines.size() && i < from + maxLines; ++i) {
            lcd.drawString(lines[i].c_str(), 8, y);
            y += 16;
        }
    }
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextDatum(textdatum_t::bottom_left);
    String web = dev.wifiOk ? (String("Web: http://") + dev.ip + "  (mk: " + dev.pin + ")") : String("Chua ket noi Wi-Fi");
    lcd.drawString(web.c_str(), 8, FOOTER_Y - 2);
}

// ---------------- Khẩn cấp ----------------

void DisplayUi::drawEmergency(bool blinkOn) {
    Snapshot s;
    {
        StateLock l;
        s = AppState::snap;
    }
    const uint16_t bg = blinkOn ? C_RED : C_BG;
    const uint16_t fg = blinkOn ? C_TEXT : C_RED;
    lcd.fillScreen(bg);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setFont(&fonts::FreeSansBold24pt7b);
    lcd.setTextColor(fg, bg);
    lcd.drawString("KHAN CAP!", W / 2, 50);
    lcd.setFont(&fonts::Font4);
    lcd.drawString("NGUY CO CHAY KHI SAC", W / 2, 98);
    char reasons[128];
    safety::describeReasons(s.reasons, reasons, sizeof(reasons));
    const std::string r = textutil::foldToAscii(reasons);
    lcd.setFont(&fonts::Font2);
    lcd.drawString(r.c_str(), W / 2, 130);
    const String vals = "Nhiet " + fmt(s.chargerTempC, 1, "C") + "  Khoi " + fmt(s.smokePct, 0, "%") + "  " + fmt(s.currentA, 1, "A");
    lcd.drawString(vals.c_str(), W / 2, 152);
    lcd.drawString("NGAT DIEN O CAM SAC NGAY!", W / 2, 180);
    lcd.drawString(s.silenced ? "(Coi dang tat)" : "Cham man hinh de tat coi 10 phut", W / 2, 212);
}
