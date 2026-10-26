// Cấu hình phần cứng CYD (ESP32-2432S028R "Cheap Yellow Display") và hằng số chung.
#pragma once

#define APP_NAME "CYD AI Safety Hub"
#define APP_VERSION "1.0.0"

// ---------------- Màn hình ----------------
// Đa số CYD 1 cổng micro-USB dùng ILI9341. Bản 2 cổng (USB-C + micro-USB, "CYD2USB") dùng ST7789:
// nếu màn trắng / hình bị lệch thì đổi thành 1.
#ifndef CYD_PANEL_ST7789
#define CYD_PANEL_ST7789 0
#endif
// Màu bị âm bản (đen thành trắng) thì đổi thành 1
#ifndef CYD_PANEL_INVERT
#define CYD_PANEL_INVERT 0
#endif

#define PIN_TFT_SCLK 14
#define PIN_TFT_MOSI 13
#define PIN_TFT_MISO 12
#define PIN_TFT_CS 15
#define PIN_TFT_DC 2
#define PIN_TFT_BL 21

#define PIN_TOUCH_SCLK 25
#define PIN_TOUCH_MOSI 32
#define PIN_TOUCH_MISO 39
#define PIN_TOUCH_CS 33
#define PIN_TOUCH_IRQ 36

// ---------------- Có sẵn trên board ----------------
#define PIN_LED_R 4   // LED RGB, mức thấp = sáng
#define PIN_LED_G 16
#define PIN_LED_B 17
#define PIN_SPEAKER 26  // cổng loa (qua mạch khuếch đại) -> dùng làm còi
#define PIN_BOOT_BTN 0

// ---------------- Cổng mở rộng ----------------
// CN1 (JST 1.25mm 4 chân): GND, GPIO22, GPIO27, 3V3 -> bus I2C cho SCD41, MLX90614, INA226/INA228
#define PIN_I2C_SDA 27
#define PIN_I2C_SCL 22
#define I2C_FREQ_HZ 50000  // MLX90614 yêu cầu <= 100 kHz; 50 kHz cho dây dài ổn định hơn
// P3 (JST 1.25mm 4 chân): GND, GPIO35, GPIO22, GPIO21. GPIO35 chỉ đọc vào, có ADC -> MQ-2 (AO qua cầu chia áp)
#define PIN_MQ2_ADC 35

// Cầu chia áp MQ-2 (AO ra tới 5V, ADC ESP32 chỉ đọc tới ~3.1V):
// AO -- R1 10k -- GPIO35 -- R2 15k -- GND  => tối đa 3.0V ở chân ADC; Vthực = Vadc * (R1+R2)/R2
#define MQ2_DIVIDER_RATIO (25.0f / 15.0f)

// ---------------- Địa chỉ I2C ----------------
#define I2C_ADDR_SCD41 0x62
#define I2C_ADDR_MLX90614 0x5A
#define I2C_ADDR_INA_FIRST 0x40  // INA226/INA228 dò từ 0x40..0x4F
#define I2C_ADDR_INA_LAST 0x4F

// ---------------- Mặc định ----------------
#define DEFAULT_SHUNT_OHM 0.01f       // module INA226/INA228 hay dùng R100 (0.1Ω) -> nên thay R010 (0.01Ω) để đo tới ~8A
#define DEFAULT_MODEL "claude-opus-5-5"
#define DEFAULT_EFFORT "low"          // chat nhanh, tiết kiệm; có thể đổi medium/high trong trang web
#define TZ_VIETNAM "ICT-7"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"

// ---------------- Chu kỳ ----------------
#define SENSOR_PERIOD_MS 2000
#define DISPLAY_PERIOD_MS 1000
#define HISTORY_PERIOD_MS 60000   // 1 mẫu/phút
#define HISTORY_LEN 180           // 3 giờ
#define ALERT_REPEAT_EMERGENCY_MS 60000
#define ALERT_REPEAT_DANGER_MS 300000
#define CO2_ALERT_REPEAT_MS 3600000

// ---------------- Telegram / AI ----------------
#define MAX_PAIRED_CHATS 4
#define CHAT_HISTORY_MESSAGES 10
#define CHAT_HISTORY_MAX_BYTES 1500
#define AI_MAX_TOOL_ROUNDS 5
#define AI_MAX_TOKENS 4096
#define TELEGRAM_MSG_MAX 3900
