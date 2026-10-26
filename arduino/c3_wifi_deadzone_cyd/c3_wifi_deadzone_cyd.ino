// Dự án #3 (bản CYD) - Wi-Fi Dead Zone Finder trên màn 2.8"
// Board: RockBase-iot NM-CYD-C5 (ESP32-C5). Copy secrets.h.example -> secrets.h rồi điền Wi-Fi.
// Arduino IDE: ESP32C5 Dev Module, Flash 16MB, Partition "16M Flash (3MB APP/9.9MB FATFS)",
// USB CDC On Boot "Enabled". Thư viện: Adafruit GFX Library, Adafruit ST7735 and ST7789 Library.
#include <WiFi.h>
#include "cyd_c5.h"
#include "secrets.h"            // WIFI_SSID, WIFI_PASS (không commit file này)

// NM-CYD-C5 không có loa. Nếu gắn loa/buzzer vào một chân trống ở header P1, điền số GPIO vào đây.
// -1 = tắt tiếng bíp (đèn RGB vẫn báo).
const int8_t SPEAKER_PIN = -1;
const int DEAD_ZONE_DBM = -80;
const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

// Trung bình trượt 10 mẫu để số không nhảy loạn
const int N = 10;
int samples[N];
int idx = 0, count = 0;
bool wasConnected = false;

void beep(unsigned int freq, unsigned long durationMs) {
  if (SPEAKER_PIN >= 0) tone(SPEAKER_PIN, freq, durationMs);
}

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
  if (r >= -80) return C_ORANGE;
  return C_RED;
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
  tft.print(WIFI_SSID);
  tft.setTextSize(3);
  tft.setCursor(200, 75);
  tft.print("dBm");
}

void setup() {
  Serial.begin(115200);
  cydBegin();
  connectWifi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected || count == 0) {
      tft.fillScreen(C_BLACK);
      tft.setTextSize(3);
      tft.setTextColor(C_RED);
      tft.setCursor(10, 80);
      tft.println("MAT KET NOI");
      tft.setTextSize(2);
      tft.setCursor(10, 120);
      tft.print("Dang thu lai...");
      count = 0;                 // bỏ mẫu cũ, đo lại từ đầu khi có sóng
    }
    wasConnected = false;
    rgb(true, false, false);
    beep(1500, 100);
    WiFi.reconnect();
    delay(2000);
    return;
  }
  if (!wasConnected) { drawStatic(); wasConnected = true; }

  int raw = WiFi.RSSI();
  int avg = averageRssi(raw);
  uint16_t color = levelColor(avg);

  tft.setTextSize(6);
  tft.setTextColor(color, C_BLACK);
  tft.setCursor(20, 55);
  tft.printf("%4d", avg);

  tft.setTextSize(3);
  tft.setCursor(20, 125);
  tft.printf("%-10s", label(avg));          // in đủ 10 ký tự để xóa chữ cũ

  // Thanh ngang: -100 dBm = rỗng, -30 dBm = đầy
  int bar = map(constrain(avg, -100, -30), -100, -30, 0, 300);
  tft.fillRect(10, 170, bar, 24, color);
  tft.fillRect(10 + bar, 170, 300 - bar, 24, C_DARK);

  // ESP32-C5 có Wi-Fi 2.4GHz và 5GHz: kênh > 14 là băng 5GHz.
  int channel = WiFi.channel();
  char info[27];
  snprintf(info, sizeof(info), "K%3d %s Tuc thoi%4d    ", channel, channel > 14 ? "5GHz" : "2.4G", raw);
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_BLACK);
  tft.setCursor(10, 210);
  tft.print(info);

  if (avg >= -60) rgb(false, true, false);
  else if (avg >= DEAD_ZONE_DBM) rgb(true, true, false);
  else { rgb(true, false, false); beep(2000, 80); }

  Serial.printf("RSSI=%d avg=%d ch=%d\n", raw, avg, channel);
  delay(300);
}
