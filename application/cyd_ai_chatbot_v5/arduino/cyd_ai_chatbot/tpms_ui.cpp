#include "tpms_ui.h"

#include "audio_player.h"
#include "ble_link.h"
#include "board_hw.h"
#include "settings_store.h"

namespace {

using namespace ui;

constexpr const char *kKeyStored = "tpms";
constexpr uint32_t kStoredMagic = 0x54504D31;  // "TPM1"
constexpr uint32_t kRedrawMs = 1000;
constexpr uint32_t kDemoEveryMs = 2000;
constexpr uint32_t kAlarmEveryMs = 10000;
constexpr uint32_t kAlarmToneHz = 2200;
constexpr uint32_t kAlarmToneMs = 400;
constexpr float kLimitStepKpa = 10;
constexpr float kMinLimitKpa = 100;
constexpr float kMaxLimitKpa = 450;
constexpr int16_t kTyreW = 96;
constexpr int16_t kTyreH = 62;
constexpr int16_t kStatusY = kScreenH - kFooterH - 12;
constexpr size_t kSettingRows = 7;
constexpr int16_t kSettingRowH = 25;
constexpr size_t kFooterButtons = 4;
constexpr uint8_t kPressureUnits = 3;
constexpr uint8_t kTempUnits = 2;

uint16_t statusColor(tpms::Status s)
{
    switch (s) {
    case tpms::Status::Normal: return kColorOk;
    case tpms::Status::LowBattery: return kColorWarn;
    case tpms::Status::NotPaired:
    case tpms::Status::NoData: return kColorDivider;
    default: return kColorDanger;
    }
}

}  // namespace

TpmsUi::TpmsUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
}

void TpmsUi::load()
{
    Stored st = {};
    if (settings_store::loadBlob(kKeyStored, &st, sizeof(st)) != sizeof(st) || st.magic != kStoredMagic) {
        return;
    }
    for (size_t i = 0; i < tpms::kSlotCount; ++i) {
        st.ids[i][tpms::kIdLength - 1] = '\0';
        if (st.ids[i][0] != '\0') {
            tyres_.assign(i, st.ids[i]);
        }
    }
    limits_ = st.limits;
    pressureUnit_ = (tpms::PressureUnit)(st.pressureUnit % kPressureUnits);
    tempUnit_ = (tpms::TempUnit)(st.tempUnit % kTempUnits);
    sound_ = st.sound;
}

void TpmsUi::save()
{
    if (demo_) {
        return;  // demo sensors are never saved
    }
    Stored st = {};
    st.magic = kStoredMagic;
    for (size_t i = 0; i < tpms::kSlotCount; ++i) {
        snprintf(st.ids[i], sizeof(st.ids[i]), "%s", tyres_.slot(i).id);
    }
    st.limits = limits_;
    st.pressureUnit = (uint8_t)pressureUnit_;
    st.tempUnit = (uint8_t)tempUnit_;
    st.sound = sound_;
    settings_store::saveBlob(kKeyStored, &st, sizeof(st));
}

void TpmsUi::show()
{
    visible_ = true;
    page_ = Page::Tyres;
    mode_ = Mode::View;
    if (!demo_) {
        tyres_ = tpms::Tyres();
        load();
    }
    audio_player::stop();
    draw();
    startListening();
}

void TpmsUi::hide()
{
    visible_ = false;
    ble_link::end();
}

void TpmsUi::startListening()
{
    if (demo_) {
        return;
    }
    String error;
    if (!ble_link::begin(error) || !ble_link::startScan(0, false, true)) {
        status_ = (error.length() > 0 ? error : String("Khong quet duoc Bluetooth")) + " - cham de thu lai";
    } else {
        status_ = "Dang nghe cam bien...";
    }
    drawStatusLine(status_, kColorMuted);
}

Rect TpmsUi::tyreRect(size_t slot) const
{
    static const int16_t kPos[tpms::kSlotCount][2] = {
        {6, kHeaderH + 6}, {(int16_t)(kScreenW - 6 - kTyreW), kHeaderH + 6},
        {6, kHeaderH + 6 + kTyreH + 18}, {(int16_t)(kScreenW - 6 - kTyreW), kHeaderH + 6 + kTyreH + 18},
        {(int16_t)((kScreenW - kTyreW) / 2), kHeaderH + 6 + 2 * kTyreH + 22},
    };
    return {kPos[slot][0], kPos[slot][1], kTyreW, slot == 4 ? (int16_t)(kTyreH - 24) : kTyreH};
}

void TpmsUi::draw()
{
    if (!visible_) {
        return;
    }
    gfx_.fillScreen(kColorBg);
    if (page_ == Page::Tyres) {
        drawTyres();
    } else {
        drawSettings();
    }
}

void TpmsUi::drawTyre(size_t slot)
{
    const uint32_t now = millis();
    const Rect r = tyreRect(slot);
    const tpms::Status st = tyres_.status(slot, limits_, now);
    const tpms::Slot &s = tyres_.slot(slot);
    const bool selected = (mode_ == Mode::Swap && swapFirst_ == (int)slot) ||
                          (mode_ == Mode::Pair && tyres_.learning() == (int)slot);
    gfx_.fillRoundRect(r.x, r.y, r.w, r.h, 6, kColorHeader);
    gfx_.drawRoundRect(r.x, r.y, r.w, r.h, 6, selected ? kColorAccent : statusColor(st));
    gfx_.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 6, selected ? kColorAccent : statusColor(st));
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(r.x + 5, r.y + 4);
    gfx_.print(tpms::slotName(slot));
    char buf[24];
    if (s.hasReading) {
        tpms::formatPressure(s.last.pressureKpa, pressureUnit_, buf, sizeof(buf));
        drawCentered(gfx_, buf, r.x, r.w, r.y + 15, slot == 4 ? 1 : 2, kColorText);
        if (slot != 4) {
            char temp[12];
            tpms::formatTemperature(s.last.temperatureC, tempUnit_, temp, sizeof(temp));
            snprintf(buf, sizeof(buf), "%s  pin %u%%", temp, (unsigned)s.last.batteryPercent);
            drawCentered(gfx_, buf, r.x, r.w, r.y + 34, 1, kColorMuted);
        }
    }
    String line = tpms::statusText(st);
    if (s.hasReading && st != tpms::Status::Lost && slot != 4) {
        tpms::formatAgo(now - s.lastMs, buf, sizeof(buf));
        line = buf;
    }
    drawCentered(gfx_, ellipsize(line, r.w / kFontW), r.x, r.w, r.y + r.h - 11, 1, statusColor(st) == kColorDivider
                                                                                       ? kColorMuted
                                                                                       : statusColor(st));
}

void TpmsUi::drawStatusLine(const String &text, uint16_t color)
{
    if (!visible_ || page_ != Page::Tyres) {
        return;
    }
    gfx_.fillRect(0, kStatusY, kScreenW, kFontH + 2, kColorBg);
    drawCentered(gfx_, ellipsize(text, kScreenW / kFontW), 0, kScreenW, kStatusY, 1, color);
}

void TpmsUi::drawFooter()
{
    drawButton(gfx_, footerSlot(0, kFooterButtons), mode_ == Mode::Pair ? "Xong" : "Ghep",
               mode_ == Mode::Pair ? kColorAccent : kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(1, kFooterButtons), mode_ == Mode::Swap ? "Xong" : "Doi lop",
               mode_ == Mode::Swap ? kColorAccent : kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(2, kFooterButtons), "Cai dat", kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(3, kFooterButtons), demo_ ? "Tat mau" : "Du lieu mau", demo_ ? kColorAccent : kColorButton,
               kColorWhite, 1);
}

void TpmsUi::drawTyres()
{
    drawBackHeader(gfx_, "Ap suat lop", demo_ ? "DU LIEU MAU" : "");
    // Car body between the tyres.
    const int16_t bodyX = 6 + kTyreW + 10, bodyW = kScreenW - 2 * (6 + kTyreW + 10);
    const int16_t bodyY = kHeaderH + 8, bodyH = 2 * kTyreH + 14;
    gfx_.drawRoundRect(bodyX, bodyY, bodyW, bodyH, 18, kColorMuted);
    gfx_.drawRoundRect(bodyX + 1, bodyY + 1, bodyW - 2, bodyH - 2, 18, kColorMuted);
    gfx_.fillRoundRect(bodyX + 12, bodyY + 22, bodyW - 24, 22, 6, kColorDivider);  // windscreen
    gfx_.drawFastHLine(bodyX + 8, bodyY + bodyH / 2 + 10, bodyW - 16, kColorDivider);
    drawCentered(gfx_, "TRUOC", bodyX, bodyW, bodyY + 6, 1, kColorMuted);
    for (size_t i = 0; i < tpms::kSlotCount; ++i) {
        drawTyre(i);
    }
    drawStatusLine(status_, kColorMuted);
    drawFooter();
}

void TpmsUi::drawSettings()
{
    drawBackHeader(gfx_, "Cai dat ap suat lop", "Tu luu sau moi lan cham");
    char lowText[16], highText[16];
    tpms::formatPressure(limits_.lowKpa, pressureUnit_, lowText, sizeof(lowText));
    tpms::formatPressure(limits_.highKpa, pressureUnit_, highText, sizeof(highText));
    const String rows[kSettingRows][2] = {
        {"Bao ap thap  (- / +)", lowText},
        {"Bao ap cao  (- / +)", highText},
        {"Canh bao ap thap", limits_.alertLow ? "Bat" : "Tat"},
        {"Canh bao ap cao", limits_.alertHigh ? "Bat" : "Tat"},
        {"Am canh bao", sound_ ? "Bat" : "Tat"},
        {"Don vi ap suat", tpms::pressureUnitName(pressureUnit_)},
        {"Don vi nhiet do", tempUnit_ == tpms::TempUnit::Celsius ? "C" : "F"},
    };
    for (size_t i = 0; i < kSettingRows; ++i) {
        const int16_t y = kHeaderH + 4 + (int16_t)i * kSettingRowH;
        gfx_.fillRoundRect(kMargin, y, kScreenW - 2 * kMargin, kSettingRowH - 3, 4, kColorRow);
        gfx_.setTextSize(1);
        gfx_.setTextColor(kColorText);
        gfx_.setCursor(10, y + 7);
        gfx_.print(rows[i][0]);
        gfx_.setTextColor(kColorAccent);
        gfx_.setCursor(kScreenW - 10 - (int16_t)rows[i][1].length() * kFontW, y + 7);
        gfx_.print(rows[i][1]);
    }
    drawCentered(gfx_, "Gioi han: cham nua trai = -, nua phai = +", 0, kScreenW, kScreenH - 12, 1, kColorMuted);
}

TpmsAction TpmsUi::back()
{
    if (page_ == Page::Settings) {
        page_ = Page::Tyres;
        draw();
        return TpmsAction::None;
    }
    if (mode_ != Mode::View) {
        mode_ = Mode::View;
        tyres_.learn(-1);
        swapFirst_ = -1;
        draw();
        return TpmsAction::None;
    }
    return TpmsAction::Exit;
}

TpmsAction TpmsUi::press(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        return back();
    }
    if (page_ == Page::Settings) {
        if (y < kHeaderH + 4) {
            return TpmsAction::None;
        }
        const size_t row = (size_t)((y - kHeaderH - 4) / kSettingRowH);
        const float step = x < kScreenW / 2 ? -kLimitStepKpa : kLimitStepKpa;
        switch (row) {
        case 0: limits_.lowKpa = constrain(limits_.lowKpa + step, kMinLimitKpa, limits_.highKpa - kLimitStepKpa); break;
        case 1: limits_.highKpa = constrain(limits_.highKpa + step, limits_.lowKpa + kLimitStepKpa, kMaxLimitKpa); break;
        case 2: limits_.alertLow = !limits_.alertLow; break;
        case 3: limits_.alertHigh = !limits_.alertHigh; break;
        case 4: sound_ = !sound_; break;
        case 5: pressureUnit_ = (tpms::PressureUnit)(((uint8_t)pressureUnit_ + 1) % kPressureUnits); break;
        case 6: tempUnit_ = (tpms::TempUnit)(((uint8_t)tempUnit_ + 1) % kTempUnits); break;
        default: return TpmsAction::None;
        }
        save();
        draw();
        return TpmsAction::None;
    }
    for (size_t i = 0; i < kFooterButtons; ++i) {
        if (!footerSlot(i, kFooterButtons).contains(x, y)) {
            continue;
        }
        if (i == 0) {
            mode_ = mode_ == Mode::Pair ? Mode::View : Mode::Pair;
            tyres_.learn(-1);
            status_ = mode_ == Mode::Pair ? "Cham lop can ghep, roi bom/xa hoi de cam bien phat song"
                                          : "Dang nghe cam bien...";
        } else if (i == 1) {
            mode_ = mode_ == Mode::Swap ? Mode::View : Mode::Swap;
            swapFirst_ = -1;
            status_ = mode_ == Mode::Swap ? "Cham 2 lop de doi cho cho nhau" : "Dang nghe cam bien...";
        } else if (i == 2) {
            page_ = Page::Settings;
        } else {
            demo_ = !demo_;
            tyres_ = tpms::Tyres();
            if (demo_) {
                ble_link::end();
                status_ = "Du lieu mau - lop sau phai dang non hoi";
            } else {
                load();
                startListening();
            }
        }
        draw();
        return TpmsAction::None;
    }
    if (status_.endsWith("cham de thu lai")) {
        startListening();
    }
    for (size_t slot = 0; slot < tpms::kSlotCount; ++slot) {
        if (!tyreRect(slot).contains(x, y)) {
            continue;
        }
        if (mode_ == Mode::Pair) {
            tyres_.learn((int)slot);
            status_ = String("Dang cho cam bien moi cho ") + tpms::slotName(slot) + "...";
        } else if (mode_ == Mode::Swap) {
            if (swapFirst_ < 0) {
                swapFirst_ = (int)slot;
                status_ = String("Cham lop thu hai de doi voi ") + tpms::slotName(slot);
            } else if (swapFirst_ != (int)slot) {
                tyres_.swap((size_t)swapFirst_, slot);
                status_ = String("Da doi ") + tpms::slotShort((size_t)swapFirst_) + " va " + tpms::slotShort(slot);
                swapFirst_ = -1;
                save();
            }
        } else {
            const tpms::Slot &s = tyres_.slot(slot);
            status_ = s.id[0] != '\0' ? String(tpms::slotName(slot)) + ": cam bien " + s.id
                                      : String(tpms::slotName(slot)) + ": chua ghep - bam Ghep";
        }
        draw();
        return TpmsAction::None;
    }
    return TpmsAction::None;
}

void TpmsUi::processAdverts(uint32_t nowMs)
{
    std::vector<ble_link::Advert> adverts;
    ble_link::takeAdverts(adverts);
    for (const ble_link::Advert &a : adverts) {
        tpms::Reading r;
        if (!tpms::decode((const uint8_t *)a.manufacturerData.data(), a.manufacturerData.size(), r)) {
            String upper = a.name;
            upper.toUpperCase();
            if (upper.indexOf("TPMS") >= 0 && unknownSensor_ != a.address) {
                unknownSensor_ = a.address;
                Serial.printf("[tpms] unknown format from %s (%s), %u bytes\n", a.name.c_str(), a.address.c_str(),
                              (unsigned)a.manufacturerData.size());
            }
            continue;
        }
        lastPacketMs_ = nowMs;
        const bool wasLearning = tyres_.learning() >= 0;
        const int slot = tyres_.feed(r, nowMs);
        if (slot >= 0) {
            if (wasLearning && tyres_.learning() < 0) {
                status_ = String("Da ghep ") + r.id + " vao " + tpms::slotName((size_t)slot);
                save();
            }
            if (visible_ && page_ == Page::Tyres) {
                drawTyre((size_t)slot);
            }
        }
    }
}

void TpmsUi::feedDemo(uint32_t nowMs)
{
    if (nowMs - lastDemoMs_ < kDemoEveryMs) {
        return;
    }
    lastDemoMs_ = nowMs;
    for (size_t i = 0; i < tpms::kSlotCount; ++i) {
        const tpms::Reading r = tpms::demoReading(i, nowMs);
        tyres_.assign(i, r.id);
        tyres_.feed(r, nowMs);
    }
}

void TpmsUi::alarm(uint32_t nowMs)
{
    if (!sound_ || nowMs - lastAlarmMs_ < kAlarmEveryMs || !tyres_.anyAlert(limits_, nowMs)) {
        return;
    }
    lastAlarmMs_ = nowMs;
    board_hw::beep(kAlarmToneHz, kAlarmToneMs);
    board_hw::setLed(255, 0, 0);
}

void TpmsUi::loop(uint32_t nowMs)
{
    if (!visible_) {
        return;
    }
    if (demo_) {
        feedDemo(nowMs);
    } else if (ble_link::ready()) {
        processAdverts(nowMs);
    }
    alarm(nowMs);
    if (page_ == Page::Tyres && nowMs - lastRedrawMs_ >= kRedrawMs) {
        lastRedrawMs_ = nowMs;
        for (size_t i = 0; i < tpms::kSlotCount; ++i) {
            drawTyre(i);  // "x giay truoc" and lost-signal states change with time
        }
        drawStatusLine(status_, kColorMuted);
    }
}
