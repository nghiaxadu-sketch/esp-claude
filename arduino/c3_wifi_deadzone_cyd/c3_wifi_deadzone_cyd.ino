// Dự án #3 (bản CYD) - Wi-Fi Dead Zone Finder trên màn 2.8"
// Board: RockBase-iot NM-CYD-C5 (ESP32-C5, Wi-Fi 6 hai băng tần 2.4/5 GHz). Copy secrets.h.example -> secrets.h rồi điền Wi-Fi.
// Bản gốc cho ESP32-2432S028: chỉ đổi cyd.h (chân/driver màn), cổng loa và dòng hiển thị kênh.
#include <WiFi.h>
#include "cyd.h"
#include "secrets.h"            // WIFI_SSID, WIFI_PASS (không commit file này)

const int8_t SPEAKER_PIN = -1;  // NM-CYD-C5 không có loa sẵn. Gắn buzzer vào GPIO trống ở header P1 rồi điền số chân (-1 = tắt)
const int DEAD_ZONE_DBM = -80;

// Trung bình trượt 10 mẫu để số không nhảy loạn
const int N = 10;
int samples[N];
int idx = 0, count = 0;
bool wasConnected = false;

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

void beep(unsigned int freq, unsigned long ms) {
  if (SPEAKER_PIN >= 0) tone(SPEAKER_PIN, freq, ms);
}

void connectWifi() {
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_BLACK);
  tft.setCursor(10, 10);
  tft.print("Dang ket noi Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
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

  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_BLACK);
  tft.setCursor(10, 210);
  int ch = WiFi.channel();                   // C5 có cả băng 5 GHz (kênh 36...165)
  tft.printf("Kenh %3d %s Tuc thoi %4d", ch, ch > 14 ? "5G" : "2G", raw);

  if (avg >= -60) rgb(false, true, false);
  else if (avg >= DEAD_ZONE_DBM) rgb(true, true, false);
  else { rgb(true, false, false); beep(2000, 80); }

  Serial.printf("RSSI=%d avg=%d\n", raw, avg);
  delay(300);
}
