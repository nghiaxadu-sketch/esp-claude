// Wi-Fi joining, Settings actions, Serial commands, web page hooks, hourly chime, weather and the start-up
// screen: the parts of the main flow that are not screens or touch handling (see chatbot_internal.h).
#include "chatbot_internal.h"

namespace chatbot {

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


}  // namespace chatbot
