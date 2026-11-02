#include "obd_ui.h"

#include <algorithm>
#include <math.h>

#include "audio_player.h"
#include "ble_link.h"
#include "settings_store.h"
#include "wifi_manager.h"

namespace {

using namespace ui;

constexpr const char *kKeyLast = "obdlast";
constexpr const char *kKeyView = "obdview";
constexpr uint32_t kScanMs = 6000;
constexpr uint32_t kRedrawMs = 400;
constexpr int16_t kListTop = 70;
constexpr int16_t kRowH = 30;
constexpr int16_t kStatusY = kScreenH - kFooterH - 12;
constexpr size_t kTabCount = 4;
constexpr size_t kMaxCandidates = 24;
constexpr size_t kSensorValueChars = 18;
constexpr const char *const kTabs[kTabCount] = {"Tong quan", "Cam bien", "Ma loi", "Thong tin"};
constexpr const char *const kViewNames[] = {"Tong quan (km/h)", "Tong quan (v/p)", "Tat ca cam bien"};
// Name fragments of common ELM327 adapters (shown first in the list).
constexpr const char *const kAdapterNames[] = {"OBD",     "ELM",   "VLINK",    "VIECAR", "VGATE",  "KONNWEI", "ICAR",
                                               "SCANNER", "SCAN",  "OBDLINK",  "VEEPEAK", "BAFX",  "LELINK",  "CARISTA",
                                               "AUTOPHIX", "ANCEL", "THINKDIAG", "VPECKER", "TONWON", "V017",  "WIFI_OBD"};

// Overview tiles: the big gauge, then four small values.
struct Tile {
    uint8_t pid;
    const char *label;
};
constexpr Tile kSmallTiles[4] = {
    {0x05, "NUOC MAT"},
    {obd::kPidVoltageFromAtrv, "DIEN AP"},
    {0x0F, "KHI NAP"},
    {0x11, "BUOM GA"},
};

bool looksLikeAdapter(const String &name)
{
    String upper = name;
    upper.toUpperCase();
    for (const char *k : kAdapterNames) {
        if (upper.indexOf(k) >= 0) {
            return true;
        }
    }
    return false;
}

String fieldOf(const String &record, int index)
{
    int start = 0;
    for (int i = 0; i < index; ++i) {
        start = record.indexOf('|', start);
        if (start < 0) {
            return String();
        }
        ++start;
    }
    const int end = record.indexOf('|', start);
    return end < 0 ? record.substring(start) : record.substring(start, end);
}

const char *kindText(obd::DtcKind kind)
{
    switch (kind) {
    case obd::DtcKind::Pending: return "cho xac nhan";
    case obd::DtcKind::Permanent: return "vinh vien";
    case obd::DtcKind::Stored:
    default: return "da luu";
    }
}

}  // namespace

ObdUi::ObdUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
}

void ObdUi::show(bool autoConnect)
{
    visible_ = true;
    lastRecord_ = settings_store::loadText(kKeyLast);
    const int32_t view = settings_store::loadInt(kKeyView, 0);
    view_ = view >= 0 && view < (int32_t)View::Count ? (View)view : View::OverviewSpeed;
    if (session_.state() == ObdState::Running) {
        page_ = view_ == View::AllSensors ? Page::Sensors : Page::Overview;
        draw();
        return;
    }
    page_ = Page::Connect;
    connectNote_ = "";
    audio_player::stop();  // radio + Bluetooth do not fit in RAM together
    draw();
    if (autoConnect && lastRecord_.length() > 0) {
        connectLast();
    } else {
        startScan();
    }
}

void ObdUi::hide()
{
    visible_ = false;
    session_.stop();
    ble_link::end();
}

void ObdUi::saveLast(const String &record)
{
    lastRecord_ = record;
    settings_store::saveText(kKeyLast, record);
}

void ObdUi::startScan()
{
    candidates_.clear();
    connectNote_ = "Dang quet...";
    if (link_ == Link::Ble) {
        String error;
        if (!ble_link::begin(error) || !ble_link::startScan(kScanMs, true, false)) {
            connectNote_ = error.length() > 0 ? error : String("Khong quet duoc Bluetooth");
        }
        scanStartMs_ = millis();
        drawCandidates();
        return;
    }
    ble_link::end();
    drawCandidates();
    for (const WifiNetwork &n : wifi_manager::scan()) {
        if (candidates_.size() >= kMaxCandidates) {
            break;
        }
        candidates_.push_back({ellipsize(toAscii(n.ssid), 30), n.ssid, 0, (int)n.rssi, n.open, looksLikeAdapter(n.ssid)});
    }
    std::stable_sort(candidates_.begin(), candidates_.end(),
                     [](const Candidate &a, const Candidate &b) { return a.likely && !b.likely; });
    connectNote_ = candidates_.empty()
                       ? "Khong thay mang nao. Cam ELM327 Wi-Fi vao cong OBD (den phai sang) roi Quet lai."
                       : "Thay " + String((unsigned)candidates_.size()) + " mang Wi-Fi";
    drawCandidates();
}

void ObdUi::collectAdverts()
{
    std::vector<ble_link::Advert> adverts;
    if (ble_link::takeAdverts(adverts) == 0) {
        return;
    }
    for (const ble_link::Advert &a : adverts) {
        auto it = std::find_if(candidates_.begin(), candidates_.end(),
                               [&](const Candidate &c) { return c.address == a.address; });
        if (it != candidates_.end()) {
            it->rssi = a.rssi;
            if (it->name.length() == 0 && a.name.length() > 0) {
                it->name = ellipsize(toAscii(a.name), 30);
                it->likely = looksLikeAdapter(a.name);
            }
            continue;
        }
        if (candidates_.size() < kMaxCandidates) {
            candidates_.push_back(
                {ellipsize(toAscii(a.name), 30), a.address, a.addressType, a.rssi, false, looksLikeAdapter(a.name)});
        }
    }
    std::stable_sort(candidates_.begin(), candidates_.end(), [](const Candidate &a, const Candidate &b) {
        if (a.likely != b.likely) {
            return a.likely;
        }
        if ((a.name.length() > 0) != (b.name.length() > 0)) {
            return a.name.length() > 0;
        }
        return a.rssi > b.rssi;
    });
    connectNote_ = "Thay " + String((unsigned)candidates_.size()) + " thiet bi BLE";
    if (page_ == Page::Connect && visible_) {
        drawCandidates();
    }
}

void ObdUi::connectTo(const Candidate &c)
{
    pendingSsid_ = "";
    if (link_ == Link::Wifi && !c.open) {
        pendingSsid_ = c.address;
        return;
    }
    connectNote_ = "Dang ket noi " + c.name + "...";
    drawCandidates();
    if (link_ == Link::Ble) {
        if (session_.startBle(c.address, c.addressType, c.name)) {
            saveLast("B|" + c.address + "|" + String(c.addressType) + "|" + c.name);
        }
    } else {
        ble_link::end();
        session_.startWifi(c.address, String());
        saveLast("W|" + c.address + "|");
    }
    connectNote_ = session_.statusText();
    drawCandidates();
}

void ObdUi::connectWifi(const String &ssid, const String &password)
{
    link_ = Link::Wifi;
    page_ = Page::Connect;
    visible_ = true;
    draw();
    ble_link::end();
    session_.startWifi(ssid, password);
    saveLast("W|" + ssid + "|" + password);
    connectNote_ = session_.statusText();
    drawCandidates();
}

void ObdUi::connectLast()
{
    if (lastRecord_.startsWith("B|")) {
        link_ = Link::Ble;
        connectNote_ = "Dang noi lai " + fieldOf(lastRecord_, 3) + "...";
        drawCandidates();
        String error;
        if (ble_link::begin(error)) {
            session_.startBle(fieldOf(lastRecord_, 1), (uint8_t)fieldOf(lastRecord_, 2).toInt(), fieldOf(lastRecord_, 3));
        }
        connectNote_ = session_.statusText();
    } else if (lastRecord_.startsWith("W|")) {
        link_ = Link::Wifi;
        ble_link::end();
        session_.startWifi(fieldOf(lastRecord_, 1), fieldOf(lastRecord_, 2));
        connectNote_ = session_.statusText();
    }
    draw();
}

void ObdUi::onConnected()
{
    page_ = view_ == View::AllSensors ? Page::Sensors : Page::Overview;
    session_.setPollAll(page_ == Page::Sensors);
    draw();
}

// ---------------------------------------------------------------- drawing

void ObdUi::draw()
{
    if (!visible_) {
        return;
    }
    gfx_.fillScreen(kColorBg);
    shownStatus_ = "";
    switch (page_) {
    case Page::Connect: drawConnect(); break;
    case Page::Overview: drawOverview(true); break;
    case Page::Sensors: drawSensors(); break;
    case Page::Dtc: drawDtc(); break;
    case Page::Info: drawInfo(); break;
    }
    if (page_ != Page::Connect) {
        drawTabs();
    }
}

void ObdUi::drawTabs()
{
    static const Page kPages[kTabCount] = {Page::Overview, Page::Sensors, Page::Dtc, Page::Info};
    for (size_t i = 0; i < kTabCount; ++i) {
        drawButton(gfx_, footerSlot(i, kTabCount), kTabs[i], kPages[i] == page_ ? kColorAccent : kColorButton,
                   kColorWhite, 1);
    }
}

void ObdUi::drawStatusLine()
{
    const String text = page_ == Page::Connect ? connectNote_ : session_.statusText();
    if (text == shownStatus_) {
        return;
    }
    shownStatus_ = text;
    const int16_t y = page_ == Page::Connect ? kListTop - 14 : kStatusY;
    gfx_.fillRect(0, y, kScreenW, kFontH + 2, kColorBg);
    const bool bad = session_.state() == ObdState::Error;
    drawCentered(gfx_, ellipsize(text, kScreenW / kFontW), 0, kScreenW, y, 1, bad ? kColorDanger : kColorMuted);
}

void ObdUi::drawConnect()
{
    drawBackHeader(gfx_, "OBD-II", "ELM327");
    const String via = link_ == Link::Ble ? "Ket noi qua: Bluetooth - cham de doi" : "Ket noi qua: Wi-Fi - cham de doi";
    drawButton(gfx_, {kMargin, 26, kScreenW - 2 * kMargin, 18}, via.c_str(), kColorHeader, kColorText, 1);
    String last = "Lan truoc: ";
    last += lastRecord_.length() > 0 ? fieldOf(lastRecord_, lastRecord_.startsWith("B|") ? 3 : 1) + " - cham de noi"
                                     : String("(chua co)");
    drawButton(gfx_, {kMargin, 46, kScreenW - 2 * kMargin, 18}, ellipsize(last, 50).c_str(),
               lastRecord_.length() > 0 ? kColorOk : kColorHeader, lastRecord_.length() > 0 ? kColorWhite : kColorMuted, 1);
    drawCandidates();
    drawButton(gfx_, footerSlot(0, 3), "Quet lai", kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(1, 3), "Du lieu mau", kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(2, 3), kViewNames[(size_t)view_], kColorAccent, kColorWhite, 1);
}

void ObdUi::drawCandidates()
{
    if (!visible_ || page_ != Page::Connect) {
        return;
    }
    shownStatus_ = "";
    drawStatusLine();
    gfx_.fillRect(0, kListTop, kScreenW, (int16_t)kConnectRows * kRowH, kColorBg);
    for (size_t i = 0; i < kConnectRows && i < candidates_.size(); ++i) {
        const Candidate &c = candidates_[i];
        const int16_t y = kListTop + (int16_t)i * kRowH;
        gfx_.fillRoundRect(kMargin, y + 1, kScreenW - 2 * kMargin, kRowH - 2, 4, c.likely ? kColorAccent : kColorHeader);
        gfx_.setTextSize(1);
        gfx_.setTextColor(c.likely ? kColorWhite : kColorText);
        gfx_.setCursor(10, y + 6);
        gfx_.print(c.name.length() > 0 ? c.name : String("Khong ten"));
        gfx_.setTextColor(c.likely ? kColorWhite : kColorMuted);
        gfx_.setCursor(10, y + 17);
        String detail = link_ == Link::Ble ? c.address : String(c.open ? "Mang mo" : "Co mat khau");
        detail += "  " + String(c.rssi) + " dBm";
        gfx_.print(detail);
    }
}

String ObdUi::valueText(uint8_t pid) const
{
    float v;
    if (!session_.value(pid, v)) {
        return session_.supported(pid) ? "--" : "khong ho tro";
    }
    if (pid == 0x03) {
        return obd::fuelSystemText((uint8_t)v);
    }
    const obd::PidInfo *info = obd::findPid(pid);
    char buf[24];
    snprintf(buf, sizeof(buf), "%.*f", info != nullptr ? info->decimals : 0, v);
    return String(buf);
}

void ObdUi::drawOverview(bool full)
{
    if (full) {
        drawBackHeader(gfx_, "OBD-II", ellipsize(session_.deviceName(), 22));
    }
    const bool speedFirst = view_ != View::OverviewRpm;
    const uint8_t mainPid = speedFirst ? 0x0D : 0x0C;
    const uint8_t secondPid = speedFirst ? 0x0C : 0x0D;
    // Big gauge: an arc from 0 to the maximum, value in the middle.
    const int16_t cx = 92, cy = 120, r = 66;
    const float maxValue = speedFirst ? 200 : 8000;
    float v = 0;
    const bool have = session_.value(mainPid, v);
    if (full) {
        gfx_.fillCircle(cx, cy, r, kColorHeader);
    }
    const int steps = 30;
    for (int i = 0; i <= steps; ++i) {
        const float a = (-225 + 270.0f * i / steps) * 3.14159265f / 180;
        const bool lit = have && i <= (int)(steps * fminf(v, maxValue) / maxValue);
        gfx_.fillCircle(cx + (int16_t)(cosf(a) * (r - 7)), cy + (int16_t)(sinf(a) * (r - 7)), 3,
                        lit ? kColorAccent : kColorDivider);
    }
    gfx_.fillRect(cx - 50, cy - 18, 100, 44, kColorHeader);
    drawCentered(gfx_, have ? valueText(mainPid) : String("--"), cx - 50, 100, cy - 16, 4, kColorText);
    drawCentered(gfx_, speedFirst ? "TOC DO km/h" : "VONG TUA v/p", cx - 50, 100, cy + 18, 1, kColorMuted);
    // Second value and the four small tiles on the right.
    const int16_t tx = 176, tw = 140, th = 36;
    struct Item {
        uint8_t pid;
        const char *label;
    };
    const Item items[5] = {{secondPid, speedFirst ? "VONG TUA" : "TOC DO"},
                           {kSmallTiles[0].pid, kSmallTiles[0].label},
                           {kSmallTiles[1].pid, kSmallTiles[1].label},
                           {kSmallTiles[2].pid, kSmallTiles[2].label},
                           {kSmallTiles[3].pid, kSmallTiles[3].label}};
    for (int i = 0; i < 5; ++i) {
        const int16_t y = 26 + i * (th + 2);
        const obd::PidInfo *info = obd::findPid(items[i].pid);
        if (full) {
            gfx_.fillRoundRect(tx, y, tw, th, 4, kColorHeader);
            gfx_.setTextSize(1);
            gfx_.setTextColor(kColorMuted);
            gfx_.setCursor(tx + 6, y + 4);
            gfx_.print(items[i].label);
        }
        String text = valueText(items[i].pid) + " " + (info != nullptr ? info->unit : "");
        while (text.length() < 10) {
            text += ' ';  // overwrite the previous, possibly longer value
        }
        gfx_.setTextSize(2);
        gfx_.setTextColor(kColorText, kColorHeader);
        gfx_.setCursor(tx + 6, y + 16);
        gfx_.print(ellipsize(text, 10));
    }
    drawStatusLine();
}

void ObdUi::drawSensors()
{
    drawBackHeader(gfx_, "Tat ca cam bien");
    size_t count = 0;
    const obd::PidInfo *table = obd::pidTable(count);
    std::vector<const obd::PidInfo *> shown;
    for (size_t i = 0; i < count; ++i) {
        if (session_.supported(table[i].pid)) {
            shown.push_back(&table[i]);
        }
    }
    if (sensorTop_ >= shown.size()) {
        sensorTop_ = 0;
    }
    for (size_t row = 0; row < kSensorRows && sensorTop_ + row < shown.size(); ++row) {
        const obd::PidInfo &p = *shown[sensorTop_ + row];
        const int16_t y = kHeaderH + 2 + (int16_t)row * 21;
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorText);
        gfx_.setCursor(6, y + 6);
        gfx_.print(ellipsize(p.nameVi, 30));
        String v = valueText(p.pid) + " " + p.unit;
        while (v.length() < kSensorValueChars) {
            v = " " + v;  // fixed width, so a shorter value erases the longer one before it
        }
        gfx_.setTextColor(kColorAccent, kColorBg);
        gfx_.setCursor(kScreenW - 6 - (int16_t)v.length() * kFontW, y + 6);
        gfx_.print(v);
        gfx_.drawFastHLine(6, y + 20, kScreenW - 12, kColorDivider);
    }
    const String page = String((unsigned)(sensorTop_ / kSensorRows + 1)) + "/" +
                        String((unsigned)((shown.size() + kSensorRows - 1) / kSensorRows));
    drawCentered(gfx_, "Cham nua tren / duoi de lat trang  " + page, 0, kScreenW, kStatusY, 1, kColorMuted);
}

void ObdUi::drawDtc()
{
    drawBackHeader(gfx_, "Ma loi", session_.dtcsRead() ? String((unsigned)session_.dtcs().size()) + " ma" : String());
    if (session_.dtcsBusy()) {
        drawCentered(gfx_, "Dang doc ma loi...", 0, kScreenW, 100, 2, kColorText);
    } else if (!session_.dtcsRead()) {
        drawCentered(gfx_, "Cham \"Doc\" de doc ma loi", 0, kScreenW, 100, 1, kColorMuted);
    } else if (session_.dtcs().empty()) {
        drawCentered(gfx_, "Khong co ma loi.", 0, kScreenW, 100, 2, kColorOk);
    }
    int16_t y = kHeaderH + 4;
    for (const obd::Dtc &d : session_.dtcs()) {
        if (y > kScreenH - kFooterH - 62) {
            break;
        }
        const char *desc = obd::dtcDescription(d.code);
        gfx_.setTextSize(2);
        gfx_.setTextColor(kColorAccent);
        gfx_.setCursor(6, y);
        gfx_.print(d.code);
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorMuted);
        gfx_.setCursor(74, y);
        gfx_.print(String(obd::dtcSystem(d.code[0])) + " - " + kindText(d.kind));
        gfx_.setTextColor(kColorText);
        gfx_.setCursor(74, y + 10);
        gfx_.print(ellipsize(desc != nullptr ? desc : "Ma rieng cua hang / chua co mo ta", 40));
        y += 26;
    }
    const Rect readBtn = {kMargin, (int16_t)(kScreenH - kFooterH - 34), 150, 30};
    const Rect clearBtn = {(int16_t)(kScreenW - kMargin - 150), (int16_t)(kScreenH - kFooterH - 34), 150, 30};
    drawButton(gfx_, readBtn, "Doc", kColorButton, kColorWhite);
    drawButton(gfx_, clearBtn, clearArmed_ ? "Cham lan nua" : "Xoa ma loi", clearArmed_ ? kColorDanger : kColorButton,
               kColorWhite);
}

void ObdUi::drawInfo()
{
    drawBackHeader(gfx_, "Thong tin");
    int16_t y = 30;
    auto line = [&](const char *label, const String &value) {
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorMuted);
        gfx_.setCursor(8, y);
        gfx_.print(label);
        gfx_.setTextColor(kColorText);
        gfx_.setCursor(110, y);
        gfx_.print(ellipsize(value, 34));
        y += 18;
    };
    const char *via = session_.transport() == ObdTransport::Ble    ? "Bluetooth BLE"
                      : session_.transport() == ObdTransport::Wifi ? "Wi-Fi (socket)"
                      : session_.transport() == ObdTransport::Demo ? "Du lieu mau"
                                                                   : "-";
    line("Adapter", session_.deviceName());
    line("Ket noi", via);
    line("Giao thuc", session_.protocolText().length() > 0 ? session_.protocolText() : String("-"));
    line("So khung (VIN)", session_.vin().length() > 0 ? session_.vin() : String("chua doc duoc"));
    line("Dien ap accu", valueText(obd::kPidVoltageFromAtrv) + " V");
    line("Trang thai", session_.statusText());
    line("Man hinh dau", kViewNames[(size_t)view_]);
    drawCentered(gfx_, "Ngat ket noi: cham \"<\"", 0, kScreenW, kStatusY - 10, 1, kColorMuted);
}

// ---------------------------------------------------------------- input

ObdAction ObdUi::back()
{
    if (page_ != Page::Connect) {
        session_.stop();
        page_ = Page::Connect;
        draw();
        startScan();
        return ObdAction::None;
    }
    return ObdAction::Exit;
}

ObdAction ObdUi::press(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        return back();
    }
    if (page_ == Page::Connect) {
        if (y >= 26 && y < 44) {
            link_ = link_ == Link::Ble ? Link::Wifi : Link::Ble;
            draw();
            startScan();
            return ObdAction::None;
        }
        if (y >= 46 && y < 64 && lastRecord_.length() > 0) {
            connectLast();
            return ObdAction::None;
        }
        if (footerSlot(0, 3).contains(x, y)) {
            session_.stop();
            startScan();
        } else if (footerSlot(1, 3).contains(x, y)) {
            ble_link::end();
            session_.startDemo();
            onConnected();
        } else if (footerSlot(2, 3).contains(x, y)) {
            view_ = (View)(((uint8_t)view_ + 1) % (uint8_t)View::Count);
            settings_store::saveInt(kKeyView, (int32_t)view_);
            drawButton(gfx_, footerSlot(2, 3), kViewNames[(size_t)view_], kColorAccent, kColorWhite, 1);
        } else if (y >= kListTop && y < kListTop + (int16_t)kConnectRows * kRowH) {
            const size_t i = (size_t)((y - kListTop) / kRowH);
            if (i < candidates_.size()) {
                const Candidate c = candidates_[i];
                connectTo(c);
                if (pendingSsid_.length() > 0 && link_ == Link::Wifi && !c.open) {
                    return ObdAction::NeedWifiPassword;
                }
            }
        }
        return ObdAction::None;
    }
    static const Page kPages[kTabCount] = {Page::Overview, Page::Sensors, Page::Dtc, Page::Info};
    for (size_t i = 0; i < kTabCount; ++i) {
        if (footerSlot(i, kTabCount).contains(x, y)) {
            page_ = kPages[i];
            clearArmed_ = false;
            session_.setPollAll(page_ == Page::Sensors);
            if (page_ == Page::Dtc && !session_.dtcsRead()) {
                session_.requestDtcs();
            }
            dtcsShown_ = false;
            draw();
            return ObdAction::None;
        }
    }
    if (page_ == Page::Sensors) {
        if (y < kScreenH / 2) {
            sensorTop_ = sensorTop_ >= kSensorRows ? sensorTop_ - kSensorRows : 0;
        } else {
            sensorTop_ += kSensorRows;
        }
        draw();
    } else if (page_ == Page::Dtc && y >= kScreenH - kFooterH - 34 && y < kScreenH - kFooterH - 4) {
        if (x < kScreenW / 2) {
            clearArmed_ = false;
            session_.requestDtcs();
        } else if (clearArmed_) {
            clearArmed_ = false;
            session_.requestClearDtcs();
        } else {
            clearArmed_ = true;
        }
        dtcsShown_ = false;
        draw();
    }
    return ObdAction::None;
}

void ObdUi::loop(uint32_t nowMs)
{
    session_.loop(nowMs);
    if (!visible_) {
        return;
    }
    if (page_ == Page::Connect) {
        if (link_ == Link::Ble && ble_link::ready()) {
            collectAdverts();
            if (!ble_link::scanning() && scanStartMs_ != 0 && session_.state() == ObdState::Idle) {
                scanStartMs_ = 0;
                if (candidates_.empty()) {
                    connectNote_ = "Khong thay thiet bi BLE. Cam ELM327 vao cong OBD (den phai sang) roi Quet lai.";
                } else {
                    connectNote_ = "Da dung quet - cham Quet lai";
                }
                drawCandidates();
            }
        }
        if (session_.state() == ObdState::Running) {
            onConnected();
            return;
        }
        if (session_.state() != ObdState::Idle) {
            connectNote_ = session_.statusText();
        }
        drawStatusLine();
        return;
    }
    if (nowMs - lastDrawMs_ < kRedrawMs) {
        return;
    }
    lastDrawMs_ = nowMs;
    if (session_.state() == ObdState::Error && page_ != Page::Info) {
        drawStatusLine();
    }
    switch (page_) {
    case Page::Overview: drawOverview(false); break;
    case Page::Sensors: drawSensors(); break;
    case Page::Dtc:
        if (!dtcsShown_ && !session_.dtcsBusy()) {
            dtcsShown_ = true;
            draw();
        }
        break;
    case Page::Info:
    case Page::Connect: break;
    }
}
