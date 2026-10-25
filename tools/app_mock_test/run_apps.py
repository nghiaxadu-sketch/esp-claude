"""Chạy thử các app Lua trong fatfs_image/system/apps bằng Lua 5.4 + module giả lập (không cần board).

    pip install lupa
    python3 tools/app_mock_test/run_apps.py application/edge_agent/fatfs_image/system/apps [app ...]

Mỗi app chạy ở 480x480 và 320x240; "STOP" là lúc mock dừng vòng lặp, không phải lỗi.
"""
import sys, json, os
from lupa import lua54
APPS = sys.argv[1]
MOCK = r'''
local W, H = ...
local calls = 0
local function obj()
  local o = {}
  return setmetatable(o, {__index = function(t, k)
    if k == "get_size" then return function() return W, H end end
    if k == "get_value" then return function() return 50 end end
    if k == "on" then return function(self, ev, fn) _G.__cbs[#_G.__cbs+1] = fn; return 1 end end
    return function() return true end
  end})
end
_G.__cbs = {}
local lvgl = setmetatable({}, {__index = function(t, k)
  if k == "process_events" then return function() return 0 end end
  return function() return obj() end end})
local delay = { delay_ms = function() calls = calls + 1
  if calls == 3 then for _, f in ipairs(_G.__cbs) do local ok, e = pcall(f); if not ok and not tostring(e):find("STOP") then error("CB: "..tostring(e)) end end end
  if calls > 40 then error("STOP") end end }
local t0 = 0
local system = { millis = function() t0 = t0 + 700; return t0 end, date = function(f) return "2026-10-25 12:00" end,
  time = function() return 0 end, ip = function() return "192.168.1.9" end,
  info = function() return { uptime_s = 100, sram_free = 100000, psram_free = 4000000, psram_total = 8000000, wifi_ssid = "home", wifi_rssi = -50 } end }
local files = {}
local storage = { get_root_dir = function() return "/fatfs" end,
  join_path = function(...) return table.concat({...}, "/") end,
  exists = function(p) return files[p] ~= nil or p == "/fatfs/photos" or p == "/fatfs/music" end,
  mkdir = function(p) files[p] = "dir"; return true end,
  write_file = function(p, c) files[p] = c; return true end,
  read_file = function(p) return files[p] end,
  listdir = function(p) if p:find("photos") then return {{name="IMG_1.jpg", type="file"},{name="a.txt",type="file"}} end
     return {{name="song.mp3", type="file"}} end,
  get_free_space = function() return {total=1e6, free=5e5, used=5e5} end }
local frame = setmetatable({}, {__index = function(t,k)
  if k == "info" then return function() return {width=640, height=480, pixel_format="RGB565"} end end
  if k == "data" then return function() return "xx" end end
  return function() return true end end, __close = function() end})
local image = { load_file = function() return frame end, resize = function() return frame end, convert = function() return frame end, save_file = function() return true end, JPEG = 1 }
local wx = '{"current":{"temperature_2m":20.4,"relative_humidity_2m":80,"weather_code":63,"wind_speed_10m":12,"apparent_temperature":21},"daily":{"time":["2026-10-25","2026-10-26","2026-10-27","2026-10-28"],"weather_code":[1,2,61,95],"temperature_2m_max":[28,29,27,26],"temperature_2m_min":[22,23,21,20]}}'
local capability = { call = function(name, req) return true, "HTTP 200 OK\n" .. wx end }
local touches = 0
local screen = setmetatable({}, {__index = function(t,k)
  if k == "info" then return function() return {width=W, height=H, touch_available=true} end end
  if k == "measure_text" then return function() return 40, 20 end end
  if k == "touch" then return function() touches = touches + 1; if touches > 300 then error("STOP") end
     if touches % 3 == 0 then return {points={{id=1,x=W//2,y=H-30}}} end return {points={}} end end
  return function() return true end end})
local display = { open = function() return screen end, color = function() return 1 end }
local sensor = { read = function() return {accel={x=100,y=-200,z=16384}, gyro={x=1,y=2,z=3}} end, close = function() return true end }
local imu = { new = function() return sensor end }
local out = { get_volume = function() return 40 end, set_volume = function() return true end, play_tone = function() return true end, close = function() return true end }
local player = { play = function() return true end, poll = function() return {state="finished"} end, pause=function() return true end, resume=function() return true end, close=function() return true end }
local audio = { open_output = function() return out end, player = function() return player end }
local camera = { list_devices = function() return {{path="/dev/video0"}} end, open = function() return true end, get_frame = function() return frame end, close = function() return true end }
package.preload.lvgl = function() return lvgl end
package.preload.delay = function() return delay end
package.preload.system = function() return system end
package.preload.storage = function() return storage end
package.preload.image = function() return image end
package.preload.capability = function() return capability end
package.preload.display = function() return display end
package.preload.imu = function() return imu end
package.preload.audio = function() return audio end
package.preload.camera = function() return camera end
'''
fail = 0
for app in (sys.argv[2:] or sorted(os.listdir(APPS))):
    if app in ("dino", "flappybird", "app_store"): continue
    for (w, h) in [(480, 480), (320, 240)]:
        L = lua54.LuaRuntime()
        g = L.globals()
        L.execute("json = {}")
        g.json.decode = lambda s: L.table_from(json.loads(s), recursive=True)
        g.json.encode = lambda v: "{}"
        L.execute("package.preload.json = function() return json end")
        L.execute(MOCK, w, h)
        src = open(f"{APPS}/{app}/scripts/{app}.lua").read()
        try:
            L.execute("local src, name = ...; local f = assert(load(src, '@/system/apps/x/scripts/' .. name)); args = {}; return f()", src, app)
            print(f"OK   {app} {w}x{h} (exited)")
        except Exception as e:
            msg = str(e)
            if "STOP" in msg and "CB:" not in msg: print(f"OK   {app} {w}x{h}")
            else: fail += 1; print(f"FAIL {app} {w}x{h}: {msg[:600]}")
sys.exit(1 if fail else 0)
