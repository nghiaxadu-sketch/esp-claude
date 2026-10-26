// Test chạy trên máy tính cho phần logic thuần (không cần board):
//   g++ -std=c++17 -Wall -Wextra -Werror -I.. -o test_logic test_logic.cpp ../safety_logic.cpp ../text_util.cpp && ./test_logic
#include <cmath>
#include <cstdio>
#include <string>

#include "../safety_logic.h"
#include "../sensor_math.h"
#include "../text_util.h"

static int gFails = 0;
static int gChecks = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        ++gChecks;                                                      \
        if (!(cond)) {                                                  \
            ++gFails;                                                   \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

static bool near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) <= eps; }

using namespace safety;

static void testAssess() {
    Thresholds th;
    Inputs in;
    CHECK(assess(in, th).level == LEVEL_SAFE);  // không có cảm biến -> an toàn

    in.chargerTempC = 40;
    CHECK(assess(in, th).level == LEVEL_SAFE);
    in.chargerTempC = 46;
    CHECK(assess(in, th).level == LEVEL_WATCH);
    CHECK(assess(in, th).reasons & R_TEMP_HIGH);
    in.chargerTempC = 56;
    CHECK(assess(in, th).level == LEVEL_DANGER);
    in.chargerTempC = 71;
    CHECK(assess(in, th).level == LEVEL_EMERGENCY);

    // Khói nguy cơ + nhiệt độ hơi cao -> khẩn cấp
    Inputs b;
    b.smokePct = 45;
    CHECK(assess(b, th).level == LEVEL_DANGER);
    b.chargerTempC = 46;
    CHECK(assess(b, th).level == LEVEL_EMERGENCY);

    // 2 tín hiệu nguy cơ cùng lúc -> khẩn cấp
    Inputs c;
    c.currentA = 11;  // > 10 A
    CHECK(assess(c, th).level == LEVEL_DANGER);
    CHECK(assess(c, th).reasons & R_OVERCURRENT);
    c.tempRiseCPerMin = 4.5f;  // >= 2x2
    CHECK(assess(c, th).level == LEVEL_EMERGENCY);

    // Dòng gần ngưỡng -> theo dõi; tắt ngưỡng dòng -> an toàn
    Inputs d;
    d.currentA = 9.5f;
    CHECK(assess(d, th).level == LEVEL_WATCH);
    Thresholds off = th;
    off.currentMaxA = 0;
    CHECK(assess(d, off).level == LEVEL_SAFE);

    // Điện áp vượt ngưỡng chỉ khi bật
    Inputs e;
    e.voltageV = 60;
    CHECK(assess(e, th).level == LEVEL_SAFE);
    Thresholds vth = th;
    vth.voltageMaxV = 58.8f;
    CHECK(assess(e, vth).level == LEVEL_DANGER);

    // Mất cảm biến khi đang sạc -> theo dõi
    Inputs f;
    f.sensorFault = true;
    CHECK(assess(f, th).level == LEVEL_WATCH);
    CHECK(assess(f, th).reasons & R_SENSOR_FAULT);
}

static void testMonitorHysteresis() {
    Thresholds th;
    Monitor m(30000);
    Inputs hot;
    hot.chargerTempC = 60;
    Inputs ok;
    ok.chargerTempC = 30;
    CHECK(m.update(hot, th, 0));          // lên ngay
    CHECK(m.level() == LEVEL_DANGER);
    CHECK(!m.update(ok, th, 1000));       // chưa hạ
    CHECK(m.level() == LEVEL_DANGER);
    CHECK(!m.update(ok, th, 20000));
    CHECK(m.level() == LEVEL_DANGER);
    CHECK(m.update(ok, th, 31500));       // đủ 30 giây -> hạ
    CHECK(m.level() == LEVEL_SAFE);
    CHECK(m.reasons() == R_NONE);
    // Dao động: nóng lại giữa chừng thì reset bộ đếm hạ mức
    m.update(hot, th, 40000);
    m.update(ok, th, 41000);
    m.update(hot, th, 60000);
    CHECK(!m.update(ok, th, 75000));
    CHECK(m.level() == LEVEL_DANGER);
}

static void testChargeSession() {
    ChargeSession s;
    uint32_t t = 0;
    CHECK(s.update(0.0f, 54.0f, t) == ChargeSession::EV_NONE);
    t += 2000;
    CHECK(s.update(2.0f, 54.0f, t) == ChargeSession::EV_STARTED);
    CHECK(s.charging());
    // sạc 1 giờ, mỗi 2 giây: 54V x 2A = 108 W -> ~108 Wh
    for (int i = 0; i < 1800; ++i) {
        t += 2000;
        s.update(2.0f, 54.0f, t);
    }
    CHECK(near(s.energyWh(), 108.0f, 1.0f));
    // dòng về 0 nhưng chưa đủ 60 giây
    t += 2000;
    CHECK(s.update(0.0f, 54.0f, t) == ChargeSession::EV_NONE);
    CHECK(s.charging());
    t += 30000;
    CHECK(s.update(0.0f, 54.0f, t) == ChargeSession::EV_NONE);
    t += 31000;
    CHECK(s.update(0.0f, 54.0f, t) == ChargeSession::EV_FINISHED);
    CHECK(!s.charging());
    CHECK(s.durationMs(t) >= 3600000u && s.durationMs(t) < 3700000u);
    // Phiên ngắn không báo "sạc xong"
    ChargeSession q;
    q.update(1.0f, 12.0f, 0);
    q.update(0.0f, 12.0f, 10000);
    CHECK(q.update(0.0f, 12.0f, 80000) == ChargeSession::EV_NONE);
    CHECK(!q.charging());
    // NAN (không có cảm biến) không bao giờ bắt đầu sạc
    ChargeSession n;
    CHECK(n.update(NAN, NAN, 0) == ChargeSession::EV_NONE);
    CHECK(!n.charging());
}

static void testRiseRate() {
    RiseRate r;
    CHECK(std::isnan(r.value()));
    uint32_t t = 0;
    float temp = 30;
    r.update(temp, t);
    for (int i = 0; i < 60; ++i) {  // tăng 3°C/phút trong 2 phút
        t += 2000;
        temp += 0.1f;
        r.update(temp, t);
    }
    CHECK(r.value() > 2.5f && r.value() < 3.2f);
    // Nhiệt ổn định -> tốc độ về gần 0
    for (int i = 0; i < 150; ++i) {
        t += 2000;
        r.update(temp, t);
    }
    CHECK(std::fabs(r.value()) < 0.2f);
    CHECK(std::isnan(r.update(NAN, t)));
}

static void testSmokeBaseline() {
    SmokeBaseline b;
    uint32_t t = 0;
    CHECK(std::isnan(b.update(600, t)));  // đang làm nóng
    while (t < SmokeBaseline::kWarmupMs + 2000) {
        t += 2000;
        b.update(600, t);
    }
    CHECK(b.warmedUp());
    t += 2000;
    CHECK(near(b.update(600, t), 0.0f, 1.0f));
    t += 2000;
    const float p = b.update(900, t);  // khói: tăng 50%
    CHECK(p > 45 && p < 55);
    // Khói kéo dài 10 phút: nền không được "học" theo khói
    for (int i = 0; i < 300; ++i) {
        t += 2000;
        b.update(900, t);
    }
    t += 2000;
    CHECK(b.update(900, t) > 40);
    CHECK(std::isnan(b.update(NAN, t)));
}

static void testAirAndNames() {
    CHECK(airLevel(NAN) == AIR_UNKNOWN);
    CHECK(airLevel(457) == AIR_GOOD);
    CHECK(airLevel(873) == AIR_OK);
    CHECK(airLevel(1200) == AIR_STUFFY);
    CHECK(airLevel(1600) == AIR_BAD);
    char buf[128];
    CHECK(std::string(describeReasons(R_NONE, buf, sizeof(buf))) == "bình thường");
    CHECK(std::string(describeReasons(R_TEMP_HIGH | R_SMOKE, buf, sizeof(buf))) == "nhiệt độ cao, phát hiện khói/khí");
    char tiny[10];
    describeReasons(R_TEMP_HIGH | R_SMOKE, tiny, sizeof(tiny));  // không tràn bộ đệm
    CHECK(std::string(tiny).size() < sizeof(tiny));
    CHECK(std::string(levelNameAscii(LEVEL_EMERGENCY)) == "KHAN CAP");
}

static void testText() {
    using namespace textutil;
    CHECK(foldToAscii("Không khí tốt") == "Khong khi tot");
    CHECK(foldToAscii("Đang thu thập, hơi bí, nên mở cửa") == "Dang thu thap, hoi bi, nen mo cua");
    CHECK(foldToAscii("ỳ ỷ ỹ ỵ Ỹ") == "y y y y Y");
    CHECK(foldToAscii("Nhiệt 30°C…") == "Nhiet 30oC...");
    CHECK(foldToAscii("Xin chào 👋🏻!") == "Xin chao !");
    CHECK(foldToAscii("\xFF\xFE") == "??");  // UTF-8 lỗi không làm crash

    // Cắt tin nhắn: không cắt giữa ký tự, ghép lại đúng nguyên văn
    std::string longText;
    for (int i = 0; i < 400; ++i) longText += "Cảnh báo sạc xe điện ";
    const auto parts = splitUtf8(longText, 1000);
    std::string joined;
    bool sizesOk = true;
    for (const auto &p : parts) {
        joined += p;
        if (p.size() > 1000) sizesOk = false;
        if (!p.empty() && (static_cast<unsigned char>(p[0]) & 0xC0) == 0x80) sizesOk = false;
    }
    CHECK(joined == longText);
    CHECK(sizesOk);
    CHECK(parts.size() >= 9);
    CHECK(splitUtf8("ngắn", 100).size() == 1);
    CHECK(splitUtf8("", 100).empty());
    // Chuỗi không có dấu cách vẫn cắt được
    std::string noSpace(5000, 'x');
    CHECK(splitUtf8(noSpace, 1000).size() == 5);

    const std::string tr = truncateUtf8("Tiếng Việt có dấu", 9);
    CHECK(tr.size() <= 9);
    CHECK(tr.compare(tr.size() - 3, 3, "\xE2\x80\xA6") == 0);
    CHECK(truncateUtf8("abc", 9) == "abc");

    const auto lines = wrapAscii("Hoi minh bat cu dieu gi qua Telegram", 10, 10);
    CHECK(lines.size() == 5);  // "Hoi minh" "bat cu" "dieu gi" "qua" "Telegram"
    for (const auto &l : lines) CHECK(l.size() <= 10);
    CHECK(wrapAscii("a\nb", 10, 10).size() == 2);
    CHECK(wrapAscii("aaaaaaaaaaaaaaaaaaaaaaaaa", 10, 10).size() == 3);
    CHECK(wrapAscii("one two three four five", 4, 2).size() == 2);  // giới hạn số dòng
}

static void testSensorMath() {
    using namespace sensormath;
    // Ví dụ trong datasheet Sensirion: CRC(0xBEEF) = 0x92
    const uint8_t beef[2] = {0xBE, 0xEF};
    CHECK(sensirionCrc8(beef, 2) == 0x92);
    // SCD41: ví dụ datasheet 0x6667 -> ~25°C, 0x5EB9 -> ~37%RH
    CHECK(near(scd4xTemperatureC(0x6667), 25.0f, 0.05f));
    CHECK(near(scd4xHumidityPct(0x5EB9), 37.0f, 0.05f));
    // SMBus PEC: CRC-8 (đa thức 0x07) của "123456789" = 0xF4
    const uint8_t chk[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(smbusPec(chk, 9) == 0xF4);
    // MLX90614: 0x3AF7 = 15095 * 0.02 - 273.15 = 28.75°C
    CHECK(near(mlxRawToC(0x3AF7), 28.75f, 0.01f));
    // INA226
    CHECK(near(ina226BusV(0x2580), 12.0f, 0.001f));          // 9600 * 1.25 mV
    CHECK(near(ina226ShuntMv(static_cast<int16_t>(-400)), -1.0f, 0.0001f));
    // INA228: 54V / 195.3125µV = 276480 (0x43800) -> thanh ghi 24 bit = <<4
    CHECK(near(ina228BusV(0x43800u << 4), 54.0f, 0.001f));
    // shunt -10 mV: -32000 LSB (20 bit bù 2) <<4
    const uint32_t neg = (static_cast<uint32_t>(-32000) & 0xFFFFF) << 4;
    CHECK(near(ina228ShuntMv(neg), -10.0f, 0.001f));
    CHECK(ina228Signed20(0x7FFFF0) == 0x7FFFF);
}

int main() {
    testAssess();
    testMonitorHysteresis();
    testChargeSession();
    testRiseRate();
    testSmokeBaseline();
    testAirAndNames();
    testText();
    testSensorMath();
    std::printf("%d/%d checks passed\n", gChecks - gFails, gChecks);
    return gFails == 0 ? 0 : 1;
}
