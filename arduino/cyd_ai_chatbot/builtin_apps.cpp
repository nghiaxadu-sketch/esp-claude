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

}  // namespace

const App kApps[] = {
    {"dong_ho", kClock},
    {"hen_gio", kTimer},
    {"xuc_xac", kDice},
    {"bo_dem", kCounter},
};
const size_t kAppCount = sizeof(kApps) / sizeof(kApps[0]);

const char *const kExampleForClaude = kTimer;

}  // namespace builtin_apps
