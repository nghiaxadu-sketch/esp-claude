// Giao diện màn hình cảm ứng 2.8" của CYD: 3 trang (Không khí / Sạc xe / AI) + màn hình khẩn cấp.
#pragma once
#include <Arduino.h>

#include "config.h"

class DisplayUi {
public:
    enum Page : uint8_t { PAGE_AIR = 0, PAGE_CHARGE, PAGE_AI, PAGE_COUNT };
    using Callback = void (*)();

    void begin();
    // Đang chạm màn hình lúc khởi động -> người dùng muốn hiệu chỉnh lại cảm ứng
    bool touchHeldAtBoot();
    // Hiệu chỉnh cảm ứng nếu chưa có dữ liệu (hoặc force = true)
    void calibrateTouch(bool force);
    void showMessage(const char *title, const char *line1, const char *line2 = nullptr, const char *line3 = nullptr);
    void showPortal(const char *ssid, const char *pass);
    void tick();
    void onSilenceRequest(Callback cb) { silenceCb_ = cb; }
    void onTouchFeedback(Callback cb) { touchCb_ = cb; }

private:
    void handleTouch();
    void drawFrame();
    void drawHeader();
    void drawFooter();
    void drawAirStatic();
    void drawAirValues();
    void drawGraph();
    void drawMascot(uint8_t airLevel);
    void drawChargeStatic();
    void drawChargeValues();
    void drawAiPage();
    void drawEmergency(bool blinkOn);
    void drawValueBox(int x, int y, int w, int h, const char *label, const char *value, uint16_t color);

    Page page_ = PAGE_AIR;
    bool needFull_ = true;
    bool emergencyShown_ = false;
    bool messageMode_ = false;
    bool wasTouched_ = false;
    uint32_t lastDrawMs_ = 0;
    uint32_t lastGraphMs_ = 0;
    uint8_t lastMascot_ = 255;
    uint32_t lastAiHash_ = 0;
    Callback silenceCb_ = nullptr;
    Callback touchCb_ = nullptr;
    int16_t graphBuf_[HISTORY_LEN];
};
