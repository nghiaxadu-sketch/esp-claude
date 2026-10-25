-- Weather: current conditions and a 3-day forecast from Open-Meteo (no API key).
-- args: { city = "Hai Phong", lat = 20.8449, lon = 106.6881, refresh_min = 15 }
-- Writes a one-line summary to DATA/cache/weather/home.txt for the home screen.
local lvgl, storage, json = require("lvgl"), require("storage"), require("json")
local capability, delay, sys = require("capability"), require("delay"), require("system")

local a = type(args) == "table" and args or {}
local CITY = type(a.city) == "string" and a.city ~= "" and a.city or "Hai Phong"
local LAT = type(a.lat) == "number" and a.lat or 20.8449
local LON = type(a.lon) == "number" and a.lon or 106.6881
local REFRESH_MS = math.max(5, math.floor(type(a.refresh_min) == "number" and a.refresh_min or 15)) * 60000

local root = storage.get_root_dir()
local cache_dir = storage.join_path(root, "cache", "weather")
local cache_json = storage.join_path(cache_dir, "last.json")
local cache_home = storage.join_path(cache_dir, "home.txt")

local C = { bg = "#000000", card = "#1c1f26", text = "#ffffff", muted = "#9aa2b0", accent = "#ff5a1f", line = "#2c313b" }

-- WMO weather interpretation codes, shortened to fit small screens.
local WMO = {
    [0] = "Clear", [1] = "Mostly clear", [2] = "Partly cloudy", [3] = "Cloudy",
    [45] = "Fog", [48] = "Fog",
    [51] = "Drizzle", [53] = "Drizzle", [55] = "Drizzle", [56] = "Drizzle", [57] = "Drizzle",
    [61] = "Light rain", [63] = "Rain", [65] = "Heavy rain", [66] = "Rain", [67] = "Rain",
    [71] = "Snow", [73] = "Snow", [75] = "Snow", [77] = "Snow",
    [80] = "Showers", [81] = "Showers", [82] = "Heavy showers",
    [85] = "Snow", [86] = "Snow",
    [95] = "Storm", [96] = "Storm", [99] = "Storm",
}
local DAYS = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" }

local running = true
local ui = {}
local font, font_big, width, height
local font_big_size = 0

local function mkdirs(path)
    local cur = ""
    for part in path:gmatch("[^/]+") do
        cur = cur .. "/" .. part
        if not storage.exists(cur) then storage.mkdir(cur) end
    end
end

local function cond_text(code)
    return WMO[math.floor(tonumber(code) or -1)] or "--"
end

local function round(v)
    if type(v) ~= "number" then return nil end
    return math.floor(v + 0.5)
end

-- "2026-10-25" -> weekday name (Zeller-style, no os.time needed).
local function weekday(date)
    local y, m, d = tostring(date):match("^(%d+)-(%d+)-(%d+)")
    y, m, d = tonumber(y), tonumber(m), tonumber(d)
    if not y then return "--" end
    if m < 3 then m = m + 12; y = y - 1 end
    local k, j = y % 100, y // 100
    local h = (d + (13 * (m + 1)) // 5 + k + k // 4 + j // 4 + 5 * j) % 7
    return DAYS[((h + 6) % 7) + 1]
end

local function fetch()
    local url = string.format(
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f" ..
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,apparent_temperature" ..
        "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=4",
        LAT, LON)
    local ok, out, err = capability.call("http_request", { url = url, method = "GET", timeout_ms = 15000, max_body_bytes = 8000 })
    if not ok then return nil, tostring(err or out or "request failed") end
    if not tostring(out):match("^HTTP 200[\r\n ]") then return nil, (tostring(out):match("^[^\n]*") or "HTTP error") end
    local body = tostring(out):match("^[^\n]*\n(.*)$")
    local dok, data = pcall(json.decode, body or "")
    if not dok or type(data) ~= "table" or type(data.current) ~= "table" then return nil, "bad response" end
    return data
end

local function save_cache(data)
    pcall(function()
        mkdirs(cache_dir)
        storage.write_file(cache_json, json.encode(data))
        local t = round(data.current.temperature_2m)
        if t then
            storage.write_file(cache_home, string.format("%dC  %s  \194\183  %s", t, cond_text(data.current.weather_code), CITY))
        end
    end)
end

local function load_cache()
    if not storage.exists(cache_json) then return nil end
    local ok, data = pcall(function() return json.decode(storage.read_file(cache_json)) end)
    return ok and type(data) == "table" and type(data.current) == "table" and data or nil
end

local function label(parent, text, x, y, w, h, color, f)
    return lvgl.label(parent, { text = text, x = x, y = y, w = w, h = h, text_color = color or C.text, font = f or font })
end

local function box(parent, x, y, w, h, color, radius)
    local o = lvgl.object(parent, { x = x, y = y, w = w, h = h, bg_color = color or C.card, radius = radius or 14, border_width = 0, pad = 0 })
    o:clear_flag("SCROLLABLE")
    return o
end

local function build()
    local scr = lvgl.create_screen()
    width, height = scr:get_size()
    scr:set_style({ bg_color = C.bg, pad = 0 })
    scr:clear_flag("SCROLLABLE")
    local pad = math.max(8, width // 30)

    local back = lvgl.button(scr, { text = "<", x = pad, y = pad, w = 44, h = 40, radius = 20, bg_color = C.card, text_color = C.text, border_width = 0, font = font })
    back:on("clicked", function() running = false end)
    label(scr, "Weather", pad + 54, pad + 8, width - 2 * pad - 110, 28, C.text)
    local refresh = lvgl.button(scr, { text = "R", x = width - pad - 44, y = pad, w = 44, h = 40, radius = 20, bg_color = C.card, text_color = C.accent, border_width = 0, font = font })
    refresh:on("clicked", function() ui.force = true end)

    local top = pad + 48
    local card_h = math.floor(height * 0.40)
    local card = box(scr, pad, top, width - 2 * pad, card_h, C.card, 18)
    local big_h = math.floor(card_h * 0.48)
    ui.temp = label(card, "--", 14, 6, math.floor((width - 2 * pad) * 0.55), big_h, C.text, font_big)
    -- The bundled font has no degree sign, so draw a small ring instead.
    ui.deg = lvgl.object(card, { x = 0, y = 14, w = 14, h = 14, radius = 7, bg_opa = 0, border_width = 3, border_color = C.text, pad = 0 })
    ui.deg:clear_flag("CLICKABLE")
    ui.cond = label(card, "Loading...", 16, big_h + 8, width - 2 * pad - 32, 26, C.text)
    ui.city = label(card, CITY, 16, big_h + 36, width - 2 * pad - 32, 24, C.muted)
    ui.extra = label(card, "", 16, card_h - 30, width - 2 * pad - 32, 24, C.muted)

    local fy = top + card_h + pad
    local fh = height - fy - pad - 26
    local fw = (width - 2 * pad - 2 * pad) // 3
    ui.days = {}
    for i = 1, 3 do
        local c = box(scr, pad + (i - 1) * (fw + pad), fy, fw, fh, C.card, 14)
        ui.days[i] = {
            name = label(c, "--", 6, 6, fw - 12, 24, C.accent),
            cond = label(c, "", 6, 6 + math.max(24, fh // 3), fw - 12, 24, C.text),
            temp = label(c, "", 6, fh - 30, fw - 12, 24, C.muted),
        }
    end
    ui.status = label(scr, "", pad, height - pad - 22, width - 2 * pad, 22, C.muted)
    scr:load()
end

local function show(data, note)
    local cur = data.current
    local t = round(cur.temperature_2m)
    ui.temp:set_text(t and tostring(t) or "--")
    if t then
        local tw = #tostring(t) * math.floor(font_big_size * 0.56)
        ui.deg:set_pos(14 + tw + 4, 14)
        ui.deg:clear_flag("HIDDEN")
    else
        ui.deg:add_flag("HIDDEN")
    end
    ui.cond:set_text(cond_text(cur.weather_code))
    local feels = round(cur.apparent_temperature)
    ui.extra:set_text(string.format("Humidity %s%%   Wind %s km/h%s",
        tostring(round(cur.relative_humidity_2m) or "--"), tostring(round(cur.wind_speed_10m) or "--"),
        feels and ("   Feels " .. feels .. "C") or ""))
    local d = data.daily or {}
    for i = 1, 3 do
        local idx = i + 1 -- index 1 is today; show the next three days
        local row = ui.days[i]
        local date = (d.time or {})[idx]
        row.name:set_text(date and weekday(date) or "--")
        row.cond:set_text(cond_text((d.weather_code or {})[idx]))
        local hi, lo = round((d.temperature_2m_max or {})[idx]), round((d.temperature_2m_min or {})[idx])
        row.temp:set_text((hi and lo) and string.format("%d / %d", hi, lo) or "")
    end
    ui.status:set_text(note or ("Updated " .. sys.date("%H:%M")))
end

local function refresh()
    ui.status:set_text("Updating...")
    lvgl.process_events(0)
    local data, err = fetch()
    if data then
        save_cache(data)
        show(data)
        return true
    end
    print("[weather] fetch failed: " .. tostring(err))
    local cached = load_cache()
    if cached then
        show(cached, "Offline - showing last data")
    else
        ui.cond:set_text("No data")
        ui.status:set_text("Check Wi-Fi: " .. tostring(err):sub(1, 40))
    end
    return false
end

local function run()
    lvgl.init()
    font = lvgl.font_load({ size = 18, cache_size = 96 })
    lvgl.set_default_font(font)
    local probe = lvgl.create_screen()
    local w, h = probe:get_size()
    probe:delete()
    font_big_size = math.max(40, math.min(96, math.floor(math.min(w, h) * 0.20)))
    font_big = lvgl.font_load({ size = font_big_size, cache_size = 16 })
    build()
    local cached = load_cache()
    if cached then show(cached, "Last data") end
    lvgl.process_events(0)
    refresh()
    local last = sys.millis()
    while running do
        lvgl.process_events(0)
        if ui.force or sys.millis() - last >= REFRESH_MS then
            ui.force = false
            refresh()
            last = sys.millis()
        end
        delay.delay_ms(30)
    end
end

local ok, err = xpcall(run, debug.traceback)
pcall(lvgl.deinit)
if not ok then error(err) end
