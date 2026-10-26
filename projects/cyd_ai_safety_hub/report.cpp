#include "report.h"

#include <time.h>

namespace report {

namespace {

String num(float v, int decimals, const char *unit) {
    if (isnan(v)) return String("--");
    return String(v, decimals) + unit;
}

const char *kWeekday[] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};

}  // namespace

String nowText() {
    struct tm t;
    if (!getLocalTime(&t, 0) || t.tm_year < (2024 - 1900)) return String();
    char buf[48];
    snprintf(buf, sizeof(buf), "%s %02d/%02d/%04d %02d:%02d", kWeekday[t.tm_wday % 7], t.tm_mday, t.tm_mon + 1,
             t.tm_year + 1900, t.tm_hour, t.tm_min);
    return String(buf);
}

String durationText(uint32_t ms) {
    const uint32_t min = ms / 60000;
    if (min < 60) return String(min) + " phút";
    return String(min / 60) + " giờ " + String(min % 60) + " phút";
}

String statusText(const Snapshot &s, const safety::Thresholds &th) {
    char reasons[128];
    String t;
    t.reserve(700);
    t += "📊 TRẠNG THÁI\n";
    const String now = nowText();
    if (now.length()) t += "🕒 " + now + "\n";
    t += "\n🌿 Không khí: " + String(safety::airNameVi(s.air)) + "\n";
    t += "  CO₂: " + num(s.co2, 0, " ppm") + " | " + num(s.roomTempC, 1, "°C") + " | ẩm " + num(s.humidity, 0, "%") + "\n";
    t += "\n🔌 Sạc xe: " + String(safety::levelNameVi(s.level));
    if (s.level != safety::LEVEL_SAFE) t += " (" + String(safety::describeReasons(s.reasons, reasons, sizeof(reasons))) + ")";
    t += "\n";
    t += "  Nhiệt cục sạc: " + num(s.chargerTempC, 1, "°C");
    if (!isnan(s.riseCPerMin)) t += " (" + String(s.riseCPerMin, 1) + "°C/phút)";
    t += "\n";
    if (s.present.mq2) {
        t += "  Khói/khí: " + (isnan(s.smokePct) ? String("đang làm nóng cảm biến") : num(s.smokePct, 0, "%")) + "\n";
    } else {
        t += "  Khói/khí: --\n";
    }
    t += "  Điện: " + num(s.voltageV, 1, "V") + " | " + num(s.currentA, 2, "A");
    if (!isnan(s.voltageV) && !isnan(s.currentA)) t += " | " + String(s.voltageV * s.currentA, 0) + "W";
    t += "\n";
    if (s.charging) t += "  ⚡ Đang sạc " + durationText(s.chargeMs) + ", " + String(s.chargeWh, 0) + " Wh\n";
    if (s.silenced) t += "🔕 Còi đang tắt tạm thời\n";
    t += "\n🧪 Cảm biến: ";
    t += String("SCD41 ") + (s.present.scd41 ? "✅" : "❌") + "  MLX90614 " + (s.present.mlx ? "✅" : "❌") + "  " +
         (s.present.ina228 ? "INA228 " : "INA226 ") + (s.present.ina ? "✅" : "❌") + "  MQ-2 " +
         (s.present.mq2 ? "✅" : "❌");
    (void)th;
    return t;
}

String aiContext(const Snapshot &s, const safety::Thresholds &th) {
    char reasons[128];
    String t;
    t.reserve(500);
    t += "[Dữ liệu thiết bị lúc " + nowText() + "] ";
    t += "CO2=" + num(s.co2, 0, "ppm") + " (" + safety::airNameVi(s.air) + "), phòng " + num(s.roomTempC, 1, "°C") +
         ", ẩm " + num(s.humidity, 0, "%") + "; ";
    t += "sạc xe: mức " + String(safety::levelNameVi(s.level)) + " (" +
         safety::describeReasons(s.reasons, reasons, sizeof(reasons)) + "), ";
    t += "nhiệt cục sạc " + num(s.chargerTempC, 1, "°C") + ", tốc độ tăng " + num(s.riseCPerMin, 1, "°C/phút") + ", ";
    t += "khói " + (s.present.mq2 ? (isnan(s.smokePct) ? String("đang làm nóng") : num(s.smokePct, 0, "%")) : String("không có cảm biến")) + ", ";
    t += "điện áp " + num(s.voltageV, 1, "V") + ", dòng " + num(s.currentA, 2, "A") + ", ";
    t += s.charging ? ("đang sạc " + durationText(s.chargeMs) + " (" + String(s.chargeWh, 0) + "Wh)") : String("không sạc");
    t += s.silenced ? ", còi đang tắt tạm" : "";
    t += ". Ngưỡng: nhiệt " + String(th.tempWarnC, 0) + "/" + String(th.tempDangerC, 0) + "/" + String(th.tempEmergencyC, 0) +
         "°C, khói " + String(th.smokeWarnPct, 0) + "/" + String(th.smokeDangerPct, 0) + "%, dòng tối đa " +
         String(th.currentMaxA, 1) + "A, CO2 báo " + String(th.co2AlertPpm, 0) + "ppm.";
    return t;
}

String alertText(const Snapshot &s, safety::Level prevLevel) {
    char reasons[128];
    String t;
    t.reserve(400);
    switch (s.level) {
        case safety::LEVEL_EMERGENCY: t += "🚨🚨 KHẨN CẤP – NGUY CƠ CHÁY KHI SẠC XE!\n"; break;
        case safety::LEVEL_DANGER: t += "🟠 NGUY CƠ – kiểm tra khu vực sạc ngay!\n"; break;
        case safety::LEVEL_WATCH: t += "🟡 Theo dõi – có dấu hiệu bất thường khi sạc\n"; break;
        case safety::LEVEL_SAFE:
            t += prevLevel != safety::LEVEL_SAFE ? "🟢 Đã trở lại AN TOÀN\n" : "🟢 An toàn\n";
            break;
    }
    if (s.level != safety::LEVEL_SAFE) t += "Lý do: " + String(safety::describeReasons(s.reasons, reasons, sizeof(reasons))) + "\n";
    t += "Nhiệt cục sạc: " + num(s.chargerTempC, 1, "°C");
    if (!isnan(s.riseCPerMin)) t += " (" + String(s.riseCPerMin, 1) + "°C/phút)";
    t += "\nKhói/khí: " + num(s.smokePct, 0, "%") + " | Dòng: " + num(s.currentA, 2, "A") + " | Áp: " + num(s.voltageV, 1, "V");
    if (s.level == safety::LEVEL_EMERGENCY) {
        t += "\n\n👉 NGẮT ĐIỆN Ổ CẮM SẠC nếu an toàn để làm. Không dùng nước dập đám cháy pin/điện. "
             "Nếu có lửa/khói nhiều: rời khỏi nhà và gọi 114.";
    } else if (s.level == safety::LEVEL_DANGER) {
        t += "\n\n👉 Nên rút sạc và kiểm tra cục sạc, dây, pin.";
    }
    t += "\n(Gửi /im để tắt còi 10 phút)";
    return t;
}

String thresholdsText(const safety::Thresholds &th) {
    String t;
    t += "⚙️ NGƯỠNG CẢNH BÁO\n";
    t += "Nhiệt cục sạc: theo dõi " + String(th.tempWarnC, 0) + "°C, nguy cơ " + String(th.tempDangerC, 0) + "°C, khẩn cấp " +
         String(th.tempEmergencyC, 0) + "°C\n";
    t += "Nhiệt tăng nhanh: " + String(th.riseWarnCPerMin, 1) + "°C/phút\n";
    t += "Khói/khí: theo dõi " + String(th.smokeWarnPct, 0) + "%, nguy cơ " + String(th.smokeDangerPct, 0) + "%\n";
    t += "Dòng sạc tối đa: " + (th.currentMaxA > 0 ? String(th.currentMaxA, 1) + "A" : String("tắt")) + "\n";
    t += "Điện áp tối đa: " + (th.voltageMaxV > 0 ? String(th.voltageMaxV, 1) + "V" : String("tắt")) + "\n";
    t += "Báo CO₂ cao: " + String(th.co2AlertPpm, 0) + " ppm\n";
    t += "Đổi ngưỡng: nhắn cho AI, ví dụ \"đặt ngưỡng nhiệt nguy cơ 50 độ\"";
    return t;
}

}  // namespace report
