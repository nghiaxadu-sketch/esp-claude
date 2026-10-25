-- Settings: device status (Wi-Fi, memory, storage, uptime) and speaker volume.
local lvgl, delay, sys, storage = require("lvgl"), require("delay"), require("system"), require("storage")
local audio_ok, audio = pcall(require, "audio")

local C = { bg = "#000000", card = "#1c1f26", text = "#ffffff", muted = "#9aa2b0", accent = "#ff5a1f" }
local running = true
local output

local function kb(n)
    if type(n) ~= "number" then return "--" end
    if n >= 1048576 then return string.format("%.1f MB", n / 1048576) end
    return string.format("%d KB", n // 1024)
end

local function uptime(s)
    s = math.floor(s or 0)
    return string.format("%dd %02dh %02dm", s // 86400, (s % 86400) // 3600, (s % 3600) // 60)
end

local function run()
    lvgl.init()
    local font = lvgl.font_load({ size = 18, cache_size = 96 })
    lvgl.set_default_font(font)
    local scr = lvgl.create_screen()
    local W, H = scr:get_size()
    scr:set_style({ bg_color = C.bg, pad = 0 })
    scr:clear_flag("SCROLLABLE")
    local pad = math.max(8, W // 30)
    local back = lvgl.button(scr, { text = "<", x = pad, y = pad, w = 44, h = 40, radius = 20, bg_color = C.card, text_color = C.text, border_width = 0, font = font })
    back:on("clicked", function() running = false end)
    lvgl.label(scr, { text = "Settings", x = pad + 54, y = pad + 8, w = W - 2 * pad - 60, h = 28, text_color = C.text, font = font })

    local body = lvgl.object(scr, { x = pad, y = pad + 50, w = W - 2 * pad, h = H - pad * 2 - 50, bg_opa = 0, border_width = 0, pad = 0 })
    body:set_flex({ flow = "column", main = "start", cross = "start" })
    body:set_style({ pad_row = 6 })
    body:set_scroll({ dir = "ver", scrollbar = "off" })

    local rows = {}
    local function row(name)
        local r = lvgl.object(body, { w = W - 2 * pad, h = 44, bg_color = C.card, radius = 12, border_width = 0, pad = 0 })
        r:clear_flag("SCROLLABLE")
        lvgl.label(r, { text = name, x = 12, y = 11, w = (W - 2 * pad) // 3, h = 24, text_color = C.muted, font = font })
        rows[name] = lvgl.label(r, { text = "--", x = (W - 2 * pad) // 3 + 12, y = 11, w = (W - 2 * pad) * 2 // 3 - 24, h = 24, text_color = C.text, font = font })
    end
    for _, n in ipairs({ "Wi-Fi", "IP", "Signal", "Time", "Uptime", "RAM free", "PSRAM", "Storage" }) do row(n) end

    -- Volume works only on boards with a speaker.
    if audio_ok then
        local ok, out = pcall(audio.open_output)
        if ok and out then
            output = out
            local r = lvgl.object(body, { w = W - 2 * pad, h = 64, bg_color = C.card, radius = 12, border_width = 0, pad = 0 })
            r:clear_flag("SCROLLABLE")
            local vl = lvgl.label(r, { text = "Volume", x = 12, y = 6, w = W - 2 * pad - 24, h = 22, text_color = C.muted, font = font })
            local cur = output:get_volume() or 50
            vl:set_text("Volume  " .. cur .. "%")
            local slider = lvgl.slider(r, { x = 16, y = 36, w = W - 2 * pad - 40, h = 12, min = 0, max = 100, value = cur, bg_color = C.accent })
            slider:on("value_changed", function()
                local v = slider:get_value()
                pcall(output.set_volume, output, v)
                vl:set_text("Volume  " .. v .. "%")
            end)
            slider:on("released", function() pcall(output.play_tone, output, 880, 80) end)
        end
    end
    scr:load()

    local last = -10000
    while running do
        lvgl.process_events(0)
        if sys.millis() - last >= 2000 then
            last = sys.millis()
            local info = sys.info()
            rows["Wi-Fi"]:set_text(info.wifi_ssid or "Not connected")
            rows["IP"]:set_text(sys.ip() or "--")
            rows["Signal"]:set_text(info.wifi_rssi and (info.wifi_rssi .. " dBm") or "--")
            rows["Time"]:set_text(sys.date("%Y-%m-%d %H:%M"))
            rows["Uptime"]:set_text(uptime(info.uptime_s))
            rows["RAM free"]:set_text(kb(info.sram_free))
            rows["PSRAM"]:set_text(info.psram_total and (kb(info.psram_free) .. " / " .. kb(info.psram_total)) or "none")
            local ok, fs = pcall(storage.get_free_space)
            rows["Storage"]:set_text(ok and fs and (kb(fs.free) .. " / " .. kb(fs.total)) or "--")
        end
        delay.delay_ms(30)
    end
end

local ok, err = xpcall(run, debug.traceback)
if output then pcall(output.close, output) end
pcall(lvgl.deinit)
if not ok then error(err) end
