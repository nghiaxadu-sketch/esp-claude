#include "builtin_apps.h"

namespace builtin_apps {
namespace {

const char *const kClock = R"JSON({
 "name": "Đồng hồ", "color": "#1E88E5", "bg": "#000000",
 "tick": 1000, "on_tick": "",
 "items": [
  {"type": "label", "x": 0, "y": 50, "w": 320, "align": "center", "size": 6, "color": "#FFFFFF",
   "text": "{pad(hour(),2)}:{pad(minute(),2)}"},
  {"type": "label", "x": 0, "y": 110, "w": 320, "align": "center", "size": 3, "color": "#90CAF9",
   "text": "{pad(second(),2)}"},
  {"type": "label", "x": 0, "y": 160, "w": 320, "align": "center", "size": 2, "color": "#BDBDBD",
   "text": "{pick(weekday(),'Chủ nhật','Thứ 2','Thứ 3','Thứ 4','Thứ 5','Thứ 6','Thứ 7')}, {day()}/{month()}/{year()}"},
  {"type": "label", "x": 0, "y": 200, "w": 320, "align": "center", "size": 1, "color": "#9E9E9E",
   "show": "time() == 0", "text": "Chưa đồng bộ giờ (cần Wi-Fi)"}
 ]
})JSON";

const char *const kTimer = R"JSON({
 "name": "Hẹn giờ", "color": "#E53935", "bg": "#101014",
 "vars": {"total": 300, "t": 300, "run": 0},
 "persist": ["total"],
 "init": "t = total",
 "tick": 1000,
 "on_tick": "if (run && t > 0) { t -= 1; if (t == 0) { run = 0; beep(1000, 600); led(255, 0, 0) } }",
 "items": [
  {"type": "label", "x": 0, "y": 10, "w": 320, "align": "center", "size": 2, "color": "#BDBDBD", "text": "Hẹn giờ"},
  {"type": "label", "x": 0, "y": 44, "w": 320, "align": "center", "size": 7,
   "color": "{t == 0 ? 'red' : (run ? 'white' : 'gold')}", "text": "{mmss(t)}"},
  {"type": "bar", "x": 20, "y": 112, "w": 280, "h": 10, "value": "t", "max": "total", "color": "#E53935"},
  {"type": "button", "x": 10, "y": 134, "w": 70, "h": 40, "text": "1'", "on_tap": "total = 60; t = total; run = 0; led(0,0,0); save()"},
  {"type": "button", "x": 88, "y": 134, "w": 70, "h": 40, "text": "5'", "on_tap": "total = 300; t = total; run = 0; led(0,0,0); save()"},
  {"type": "button", "x": 166, "y": 134, "w": 70, "h": 40, "text": "15'", "on_tap": "total = 900; t = total; run = 0; led(0,0,0); save()"},
  {"type": "button", "x": 244, "y": 134, "w": 66, "h": 40, "text": "25'", "on_tap": "total = 1500; t = total; run = 0; led(0,0,0); save()"},
  {"type": "button", "x": 10, "y": 184, "w": 145, "h": 46, "color": "{run ? '#F57C00' : '#2E7D32'}",
   "text": "{run ? 'Tạm dừng' : 'Bắt đầu'}", "on_tap": "if (t == 0) { t = total } run = !run; led(0,0,0)"},
  {"type": "button", "x": 165, "y": 184, "w": 145, "h": 46, "text": "Làm lại", "on_tap": "run = 0; t = total; led(0,0,0)"}
 ]
})JSON";

const char *const kDice = R"JSON({
 "name": "Xúc xắc", "color": "#43A047", "bg": "#1B5E20",
 "vars": {"v": 0, "n": 0, "roll": 0},
 "timers": [{"ms": 80, "do": "if (roll > 0) { v = rand(1, 6); roll -= 1; if (roll == 0) { n += 1; beep(1500, 60) } }"}],
 "on_touch": "if (roll == 0) { roll = 10 }",
 "items": [
  {"type": "label", "x": 0, "y": 10, "w": 320, "align": "center", "size": 2, "text": "Chạm để tung"},
  {"type": "rect", "x": 100, "y": 40, "w": 120, "h": 120, "radius": 16, "color": "white"},
  {"type": "label", "x": 100, "y": 76, "w": 120, "align": "center", "size": 6, "color": "#1B5E20", "show": "v == 0", "text": "?"},
  {"type": "circle", "x": 160, "y": 100, "r": 11, "color": "black", "show": "v == 1 || v == 3 || v == 5"},
  {"type": "circle", "x": 130, "y": 70, "r": 11, "color": "black", "show": "v >= 2"},
  {"type": "circle", "x": 190, "y": 130, "r": 11, "color": "black", "show": "v >= 2"},
  {"type": "circle", "x": 190, "y": 70, "r": 11, "color": "black", "show": "v >= 4"},
  {"type": "circle", "x": 130, "y": 130, "r": 11, "color": "black", "show": "v >= 4"},
  {"type": "circle", "x": 130, "y": 100, "r": 11, "color": "black", "show": "v == 6"},
  {"type": "circle", "x": 190, "y": 100, "r": 11, "color": "black", "show": "v == 6"},
  {"type": "label", "x": 0, "y": 180, "w": 320, "align": "center", "size": 2, "color": "#C8E6C9",
   "text": "{n == 0 ? '' : 'Lần ' + n + ': ' + v}"}
 ]
})JSON";

const char *const kCounter = R"JSON({
 "name": "Bộ đếm", "color": "#8E24AA", "bg": "#000000",
 "vars": {"c": 0},
 "persist": ["c"],
 "items": [
  {"type": "label", "x": 0, "y": 12, "w": 320, "align": "center", "size": 2, "color": "#CE93D8", "text": "Bộ đếm"},
  {"type": "label", "x": 0, "y": 52, "w": 320, "align": "center", "size": 8, "text": "{c}"},
  {"type": "button", "x": 10, "y": 136, "w": 300, "h": 50, "color": "#8E24AA", "size": 3, "text": "+1",
   "on_tap": "c += 1; beep(2000, 30); save()"},
  {"type": "button", "x": 10, "y": 194, "w": 145, "h": 40, "text": "-1", "on_tap": "c = max(0, c - 1); save()"},
  {"type": "button", "x": 165, "y": 194, "w": 145, "h": 40, "color": "#B71C1C", "text": "Về 0", "on_tap": "c = 0; save()"}
 ]
})JSON";

const char *const kWeather = R"JSON({
 "name": "Thời tiết", "color": "#FB8C00", "bg": "#000000",
 "vars": {"u": "https://api.open-meteo.com/v1/forecast?latitude=20.86&longitude=106.68&current=temperature_2m&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=Asia%2FBangkok&forecast_days=5",
  "need": 1, "loading": 1, "err": 0, "t": 0, "mx": "", "mn": "", "wc": "",
  "hi0": 0, "lo0": 0, "c0": 0, "hi1": 0, "lo1": 0, "c1": 0, "hi2": 0, "lo2": 0, "c2": 0, "hi3": 0, "lo3": 0, "c3": 0, "hi4": 0, "lo4": 0, "c4": 0},
 "timers": [
  {"ms": 300, "do": "if (need && wifi()) { need = 0; loading = 1; t = http(u, 'current.temperature_2m'); mx = str(http(u, 'daily.temperature_2m_max')); mn = str(http(u, 'daily.temperature_2m_min')); wc = str(http(u, 'daily.weather_code')); err = len(mx) < 3; mx = sub(mx, 1, len(mx) - 2); mn = sub(mn, 1, len(mn) - 2); wc = sub(wc, 1, len(wc) - 2); hi0 = num(part(mx, ',', 0)); lo0 = num(part(mn, ',', 0)); c0 = num(part(wc, ',', 0)); hi1 = num(part(mx, ',', 1)); lo1 = num(part(mn, ',', 1)); c1 = num(part(wc, ',', 1)); hi2 = num(part(mx, ',', 2)); lo2 = num(part(mn, ',', 2)); c2 = num(part(wc, ',', 2)); hi3 = num(part(mx, ',', 3)); lo3 = num(part(mn, ',', 3)); c3 = num(part(wc, ',', 3)); hi4 = num(part(mx, ',', 4)); lo4 = num(part(mn, ',', 4)); c4 = num(part(wc, ',', 4)); loading = 0 }"},
  {"ms": 1800000, "do": "need = 1"}
 ],
 "on_touch": "if (err) { need = 1 }",
 "items": [
  {"type": "label", "x": 26, "y": 34, "size": 6, "color": "#FFFFFF", "show": "!loading && !err", "text": "{round(t)}"},
  {"type": "circle", "x": "34 + len(str(round(t))) * 36", "y": 40, "r": 5, "color": "#FFFFFF", "fill": false, "show": "!loading && !err"},
  {"type": "label", "x": 26, "y": 90, "size": 2, "color": "#FFB300", "show": "!loading && !err", "text": "{c0 == 0 ? 'Trời quang' : (c0 <= 2 ? 'Ít mây' : (c0 == 3 ? 'Nhiều mây' : (c0 <= 48 ? 'Sương mù' : (c0 <= 67 ? 'Có mưa' : (c0 <= 77 ? 'Tuyết' : (c0 <= 82 ? 'Mưa rào' : 'Dông'))))))}"},
  {"type": "label", "x": 26, "y": 112, "size": 1, "color": "#9E9E9E", "text": "Hải Phòng"},
  {"type": "label", "x": 26, "y": 60, "size": 2, "color": "#BDBDBD", "show": "loading || err", "text": "{!wifi() ? 'Cần Wi-Fi' : (err ? 'Lỗi tải, chạm để thử lại' : 'Đang tải...')}"},
  {"type": "circle", "x": 246, "y": 70, "r": 26, "color": "#FFB300", "show": "!loading && !err && c0 <= 2"},
  {"type": "line", "x": 246, "y": 30, "x2": 246, "y2": 38, "color": "#FFB300", "show": "!loading && !err && c0 <= 2"},
  {"type": "line", "x": 246, "y": 102, "x2": 246, "y2": 110, "color": "#FFB300", "show": "!loading && !err && c0 <= 2"},
  {"type": "line", "x": 206, "y": 70, "x2": 214, "y2": 70, "color": "#FFB300", "show": "!loading && !err && c0 <= 2"},
  {"type": "line", "x": 278, "y": 70, "x2": 286, "y2": 70, "color": "#FFB300", "show": "!loading && !err && c0 <= 2"},
  {"type": "line", "x": 218, "y": 42, "x2": 224, "y2": 48, "color": "#FFB300", "show": "!loading && !err && c0 <= 2"},
  {"type": "line", "x": 268, "y": 92, "x2": 274, "y2": 98, "color": "#FFB300", "show": "!loading && !err && c0 <= 2"},
  {"type": "line", "x": 274, "y": 42, "x2": 268, "y2": 48, "color": "#FFB300", "show": "!loading && !err && c0 <= 2"},
  {"type": "line", "x": 224, "y": 92, "x2": 218, "y2": 98, "color": "#FFB300", "show": "!loading && !err && c0 <= 2"},
  {"type": "circle", "x": 236, "y": 82, "r": 16, "color": "#CFD8DC", "show": "!loading && !err && c0 >= 2"},
  {"type": "circle", "x": 260, "y": 76, "r": 20, "color": "#CFD8DC", "show": "!loading && !err && c0 >= 2"},
  {"type": "rect", "x": 220, "y": 82, "w": 62, "h": 16, "radius": 8, "color": "#CFD8DC", "show": "!loading && !err && c0 >= 2"},
  {"type": "line", "x": 232, "y": 104, "x2": 228, "y2": 114, "color": "#42A5F5", "show": "!loading && !err && c0 >= 51"},
  {"type": "line", "x": 248, "y": 104, "x2": 244, "y2": 114, "color": "#42A5F5", "show": "!loading && !err && c0 >= 51"},
  {"type": "line", "x": 264, "y": 104, "x2": 260, "y2": 114, "color": "#42A5F5", "show": "!loading && !err && c0 >= 51"},
  {"type": "line", "x": 16, "y": 128, "x2": 304, "y2": 128, "color": "#333333"},
  {"type": "label", "x": 26, "y": 136, "size": 2, "color": "#BDBDBD", "text": "Hôm nay"},
  {"type": "circle", "x": 150, "y": 143, "r": 5, "color": "{c0 <= 1 ? '#FFB300' : (c0 <= 48 ? '#B0BEC5' : '#42A5F5')}", "show": "!loading && !err"},
  {"type": "label", "x": 190, "y": 136, "size": 2, "color": "#90A4AE", "show": "!loading && !err", "text": "{round(lo0)}°"},
  {"type": "label", "x": 250, "y": 136, "size": 2, "color": "#FFFFFF", "show": "!loading && !err", "text": "{round(hi0)}°"},
  {"type": "label", "x": 26, "y": 156, "size": 2, "color": "#BDBDBD", "text": "{pick(weekday() + 1, 'CN', 'Thứ 2', 'Thứ 3', 'Thứ 4', 'Thứ 5', 'Thứ 6', 'Thứ 7')}"},
  {"type": "circle", "x": 150, "y": 163, "r": 5, "color": "{c1 <= 1 ? '#FFB300' : (c1 <= 48 ? '#B0BEC5' : '#42A5F5')}", "show": "!loading && !err"},
  {"type": "label", "x": 190, "y": 156, "size": 2, "color": "#90A4AE", "show": "!loading && !err", "text": "{round(lo1)}°"},
  {"type": "label", "x": 250, "y": 156, "size": 2, "color": "#FFFFFF", "show": "!loading && !err", "text": "{round(hi1)}°"},
  {"type": "label", "x": 26, "y": 176, "size": 2, "color": "#BDBDBD", "text": "{pick(weekday() + 2, 'CN', 'Thứ 2', 'Thứ 3', 'Thứ 4', 'Thứ 5', 'Thứ 6', 'Thứ 7')}"},
  {"type": "circle", "x": 150, "y": 183, "r": 5, "color": "{c2 <= 1 ? '#FFB300' : (c2 <= 48 ? '#B0BEC5' : '#42A5F5')}", "show": "!loading && !err"},
  {"type": "label", "x": 190, "y": 176, "size": 2, "color": "#90A4AE", "show": "!loading && !err", "text": "{round(lo2)}°"},
  {"type": "label", "x": 250, "y": 176, "size": 2, "color": "#FFFFFF", "show": "!loading && !err", "text": "{round(hi2)}°"},
  {"type": "label", "x": 26, "y": 196, "size": 2, "color": "#BDBDBD", "text": "{pick(weekday() + 3, 'CN', 'Thứ 2', 'Thứ 3', 'Thứ 4', 'Thứ 5', 'Thứ 6', 'Thứ 7')}"},
  {"type": "circle", "x": 150, "y": 203, "r": 5, "color": "{c3 <= 1 ? '#FFB300' : (c3 <= 48 ? '#B0BEC5' : '#42A5F5')}", "show": "!loading && !err"},
  {"type": "label", "x": 190, "y": 196, "size": 2, "color": "#90A4AE", "show": "!loading && !err", "text": "{round(lo3)}°"},
  {"type": "label", "x": 250, "y": 196, "size": 2, "color": "#FFFFFF", "show": "!loading && !err", "text": "{round(hi3)}°"},
  {"type": "label", "x": 26, "y": 216, "size": 2, "color": "#BDBDBD", "text": "{pick(weekday() + 4, 'CN', 'Thứ 2', 'Thứ 3', 'Thứ 4', 'Thứ 5', 'Thứ 6', 'Thứ 7')}"},
  {"type": "circle", "x": 150, "y": 223, "r": 5, "color": "{c4 <= 1 ? '#FFB300' : (c4 <= 48 ? '#B0BEC5' : '#42A5F5')}", "show": "!loading && !err"},
  {"type": "label", "x": 190, "y": 216, "size": 2, "color": "#90A4AE", "show": "!loading && !err", "text": "{round(lo4)}°"},
  {"type": "label", "x": 250, "y": 216, "size": 2, "color": "#FFFFFF", "show": "!loading && !err", "text": "{round(hi4)}°"}
 ]
})JSON";

}  // namespace

const App kApps[] = {
    {"dong_ho", kClock},
    {"hen_gio", kTimer},
    {"xuc_xac", kDice},
    {"bo_dem", kCounter},
    {"thoi_tiet", kWeather},
};
const size_t kAppCount = sizeof(kApps) / sizeof(kApps[0]);

const char *const kExampleForClaude = kTimer;

}  // namespace builtin_apps
