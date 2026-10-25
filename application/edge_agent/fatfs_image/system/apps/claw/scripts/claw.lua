-- CLAW: how to talk to the on-device Claude agent (web chat address and tips).
local lvgl, delay, sys = require("lvgl"), require("delay"), require("system")

local C = { bg = "#000000", card = "#1c1f26", text = "#ffffff", muted = "#9aa2b0", accent = "#ff5a1f", red = "#e53935" }
local running = true

local function run()
    lvgl.init()
    local font = lvgl.font_load({ size = 18, cache_size = 96 })
    lvgl.set_default_font(font)
    local scr = lvgl.create_screen()
    local W, H = scr:get_size()
    local big = lvgl.font_load({ size = math.max(24, math.min(40, W // 12)), cache_size = 32 })
    scr:set_style({ bg_color = C.bg, pad = 0 })
    scr:clear_flag("SCROLLABLE")
    local pad = math.max(8, W // 30)
    local back = lvgl.button(scr, { text = "<", x = pad, y = pad, w = 44, h = 40, radius = 20, bg_color = C.card, text_color = C.text, border_width = 0, font = font })
    back:on("clicked", function() running = false end)
    lvgl.label(scr, { text = "CLAW", x = pad + 54, y = pad + 8, w = W - 2 * pad - 60, h = 28, text_color = C.text, font = font })

    local card = lvgl.object(scr, { x = pad, y = pad + 52, w = W - 2 * pad, h = H - pad * 2 - 52, bg_color = C.card, radius = 18, border_width = 0, pad = 0 })
    card:clear_flag("SCROLLABLE")
    local cw = W - 2 * pad - 32
    lvgl.label(card, { text = "make ideas", x = 16, y = 12, w = cw, h = 30, text_color = C.text, font = font })
    lvgl.label(card, { text = "run", x = 16, y = 38, w = cw, h = big and 50 or 30, text_color = C.accent, font = big })
    local status = lvgl.label(card, { text = "", x = 16, y = 100, w = cw, h = 26, text_color = C.red, font = font })
    local url = lvgl.label(card, { text = "", x = 16, y = 130, w = cw, h = 50, text_color = C.text, font = big })
    lvgl.label(card, { text = "Open this address in a browser on the same Wi-Fi to chat with Claude and build new apps. Saved apps appear on the launcher.",
        x = 16, y = 186, w = cw, h = H - pad * 2 - 52 - 196, text_color = C.muted, font = font })
    scr:load()

    local last = -10000
    while running do
        lvgl.process_events(0)
        if sys.millis() - last >= 2000 then
            last = sys.millis()
            local ip = sys.ip()
            if ip then
                status:set_text("Wi-Fi connected")
                status:set_style({ text_color = "#2e7d32" })
                url:set_text("http://" .. ip)
            else
                status:set_text("Wi-Fi not connected >")
                status:set_style({ text_color = C.red })
                url:set_text("192.168.4.1")
            end
        end
        delay.delay_ms(30)
    end
end

local ok, err = xpcall(run, debug.traceback)
pcall(lvgl.deinit)
if not ok then error(err) end
