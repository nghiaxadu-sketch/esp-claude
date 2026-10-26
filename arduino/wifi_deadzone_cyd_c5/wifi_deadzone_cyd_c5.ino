// Du an #3 (ban NM-CYD-C5) - Wi-Fi Dead Zone Finder tren man 2.8" cua ESP32-C5.
// Board: RockBase-iot NM-CYD-C5 (ESP32-C5, ST7789 320x240). Khong can cyd.h.
//
// Arduino IDE: board "ESP32C5 Dev Module" (esp32 core >= 3.3), Flash Size 16MB,
// Partition "16M Flash (3MB APP/9.9MB FATFS)", USB CDC On Boot "Enabled".
// Thu vien: "Adafruit ST7735 and ST7789 Library", "Adafruit GFX Library".
// Copy secrets.h.example -> secrets.h roi dien Wi-Fi (khong commit file nay).
//
// ESP32-C5 co Wi-Fi 6 hai bang tan, nen RSSI do duoc ca 2.4 GHz va 5 GHz (hien o dong duoi).
// Nhan nut BOOT de xoa cac mau cu va do lai tu dau khi di sang cho khac.

#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>

#if __has_include("secrets.h")
#include "secrets.h"  // WIFI_SSID, WIFI_PASS
#else
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif

// ---- Chan cua NM-CYD-C5 (xem application/edge_agent/boards/rockbase-iot/nm_cyd_c5) ----
constexpr int8_t PIN_SCK = 6, PIN_MISO = 2, PIN_MOSI = 7;
constexpr int8_t PIN_LCD_CS = 23, PIN_LCD_DC = 24, PIN_LCD_RST = -1, PIN_BACKLIGHT = 25;
constexpr int8_t PIN_TOUCH_CS = 1, PIN_SD_CS = 10;  // giu o muc cao de khong tranh bus SPI
constexpr int8_t PIN_WS2812 = 27;
constexpr int8_t PIN_BOOT = 0;  // nhan = muc thap
constexpr int8_t PIN_BUZZER = -1;  // board khong co loa; noi buzzer vao 1 chan GPIO roi ghi so chan vao day

// ---- Man hinh ----
constexpr uint16_t LCD_NATIVE_W = 240, LCD_NATIVE_H = 320;
constexpr uint8_t LCD_ROTATION = 1;  // thu 3 neu hinh bi nguoc
constexpr bool LCD_INVERT = false;   // true neu mau bi am ban
constexpr uint32_t LCD_SPI_HZ = 40000000;
constexpr uint32_t BACKLIGHT_HZ = 5000;
constexpr uint8_t BACKLIGHT_BITS = 10;
constexpr uint8_t BACKLIGHT_PERCENT = 80;
constexpr uint8_t LED_LEVEL = 24;  // 0..255, WS2812 rat choi

constexpr uint16_t C_BLACK = 0x0000, C_WHITE = 0xFFFF, C_RED = 0xF800, C_GREEN = 0x07E0;
constexpr uint16_t C_YELLOW = 0xFFE0, C_ORANGE = 0xFD20, C_NAVY = 0x000F, C_DARK = 0x2104;

// ---- Nguong do ----
constexpr int DEAD_ZONE_DBM = -80;
constexpr int N = 10;  // trung binh truot 10 mau de so khong nhay loan
constexpr int BAR_W = 300;
constexpr int RSSI_BAR_MIN = -100, RSSI_BAR_MAX = -30;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t RECONNECT_DELAY_MS = 2000;
constexpr uint32_t SAMPLE_PERIOD_MS = 300;
constexpr uint32_t BEEP_LOST_HZ = 1500, BEEP_DEAD_HZ = 2000;
constexpr uint32_t BEEP_LOST_MS = 100, BEEP_DEAD_MS = 80;
constexpr uint8_t LAST_24GHZ_CHANNEL = 14;

Adafruit_ST7789 tft(&SPI, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST);

int samples[N];
int idx = 0, count = 0;
bool wasConnected = false;
bool bootWasDown = false;

int averageRssi(int v) {
  samples[idx] = v;
  idx = (idx + 1) % N;
  if (count < N) count++;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += samples[i];
  return sum / count;
}

const char* label(int r) {
  if (r >= -50) return "RAT TOT";
  if (r >= -60) return "TOT";
  if (r >= -70) return "TRUNG BINH";
  if (r >= -80) return "YEU";
  return "VUNG CHET";
}

uint16_t levelColor(int r) {
  if (r >= -60) return C_GREEN;
  if (r >= -70) return C_YELLOW;
  if (r >= DEAD_ZONE_DBM) return C_ORANGE;
  return C_RED;
}

void rgb(bool r, bool g, bool b) {
  rgbLedWrite(PIN_WS2812, r ? LED_LEVEL : 0, g ? LED_LEVEL : 0, b ? LED_LEVEL : 0);
}

void beep(uint32_t hz, uint32_t ms) {
  if (PIN_BUZZER >= 0) tone(PIN_BUZZER, hz, ms);
}

void initDisplay() {
  for (int8_t cs : {PIN_LCD_CS, PIN_TOUCH_CS, PIN_SD_CS}) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  ledcAttach(PIN_BACKLIGHT, BACKLIGHT_HZ, BACKLIGHT_BITS);
  ledcWrite(PIN_BACKLIGHT, ((1U << BACKLIGHT_BITS) - 1) * BACKLIGHT_PERCENT / 100);
  tft.init(LCD_NATIVE_W, LCD_NATIVE_H);
  tft.setSPISpeed(LCD_SPI_HZ);
  tft.setRotation(LCD_ROTATION);
  tft.invertDisplay(LCD_INVERT);
  tft.setTextWrap(false);
  tft.fillScreen(C_BLACK);
}

void connectWifi() {
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_BLACK);
  tft.setCursor(10, 10);
  tft.print("Dang ket noi Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_CONNECT_TIMEOUT_MS) delay(250);
}

void drawStatic() {
  tft.fillScreen(C_BLACK);
  tft.fillRect(0, 0, 320, 32, C_NAVY);
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 9);
  tft.print(WiFi.SSID());
  tft.setTextSize(3);
  tft.setCursor(200, 75);
  tft.print("dBm");
}

void drawLostScreen() {
  tft.fillScreen(C_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(C_RED);
  tft.setCursor(10, 80);
  tft.println("MAT KET NOI");
  tft.setTextSize(2);
  tft.setCursor(10, 120);
  tft.print("Dang thu lai...");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  initDisplay();
  connectWifi();
}

void loop() {
  // Nut BOOT: bo mau cu, do lai tu dau.
  const bool bootDown = digitalRead(PIN_BOOT) == LOW;
  if (bootDown && !bootWasDown) count = idx = 0;
  bootWasDown = bootDown;

  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected || count == 0) {
      drawLostScreen();
      count = 0;  // bo mau cu, do lai tu dau khi co song
    }
    wasConnected = false;
    rgb(true, false, false);
    beep(BEEP_LOST_HZ, BEEP_LOST_MS);
    WiFi.reconnect();
    delay(RECONNECT_DELAY_MS);
    return;
  }
  if (!wasConnected) {
    drawStatic();
    wasConnected = true;
  }

  const int raw = WiFi.RSSI();
  const int avg = averageRssi(raw);
  const uint16_t color = levelColor(avg);

  tft.setTextSize(6);
  tft.setTextColor(color, C_BLACK);
  tft.setCursor(20, 55);
  tft.printf("%4d", avg);

  tft.setTextSize(3);
  tft.setCursor(20, 125);
  tft.printf("%-10s", label(avg));  // in du 10 ky tu de xoa chu cu

  // Thanh ngang: -100 dBm = rong, -30 dBm = day
  const int bar = map(constrain(avg, RSSI_BAR_MIN, RSSI_BAR_MAX), RSSI_BAR_MIN, RSSI_BAR_MAX, 0, BAR_W);
  tft.fillRect(10, 170, bar, 24, color);
  tft.fillRect(10 + bar, 170, BAR_W - bar, 24, C_DARK);

  const uint8_t ch = WiFi.channel();
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_BLACK);
  tft.setCursor(10, 210);
  tft.printf("%s k%-3d Tuc thoi %4d ", ch > LAST_24GHZ_CHANNEL ? "5G " : "2.4G", ch, raw);

  if (avg >= -60) rgb(false, true, false);
  else if (avg >= DEAD_ZONE_DBM) rgb(true, true, false);
  else {
    rgb(true, false, false);
    beep(BEEP_DEAD_HZ, BEEP_DEAD_MS);
  }

  Serial.printf("RSSI=%d avg=%d ch=%u\n", raw, avg, ch);
  delay(SAMPLE_PERIOD_MS);
}
