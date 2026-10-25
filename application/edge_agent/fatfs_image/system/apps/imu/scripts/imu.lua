-- IMU: tilt ball plus live accelerometer / gyroscope readings.
-- args: { device = "imu_sensor", period_ms = 40 }
local lvgl, delay = require("lvgl"), require("delay")
local imu_ok, imu = pcall(require, "imu")

local a = type(args) == "table" and args or {}
local DEVICE = type(a.device) == "string" and a.device or "imu_sensor"
local PERIOD_MS = math.max(20, math.floor(type(a.period_ms) == "number" and a.period_ms or 40))
local C = { bg = "#000000", card = "#1c1f26", text = "#ffffff", muted = "#9aa2b0", accent = "#ff5a1f", ring = "#2c313b" }

local running = true
local sensor

local function label(parent, text, x, y, w, h, color, font)
    return lvgl.label(parent, { text = text, x = x, y = y, w = w, h = h, text_color = color or C.text, font = font })
end

local function run()
    lvgl.init()
    local font = lvgl.font_load({ size = 18, cache_size = 64 })
    lvgl.set_default_font(font)
    local scr = lvgl.create_screen()
    local W, H = scr:get_size()
    scr:set_style({ bg_color = C.bg, pad = 0 })
    scr:clear_flag("SCROLLABLE")
    local pad = math.max(8, W // 30)
    local back = lvgl.button(scr, { text = "<", x = pad, y = pad, w = 44, h = 40, radius = 20, bg_color = C.card, text_color = C.text, border_width = 0, font = font })
    back:on("clicked", function() running = false end)
    label(scr, "IMU", pad + 54, pad + 8, W - 2 * pad - 60, 28)

    local area = math.min(W - 2 * pad, H - pad * 3 - 48 - 70)
    local cx, cy = W // 2, pad + 48 + area // 2 + pad
    local plate = lvgl.object(scr, { x = cx - area // 2, y = cy - area // 2, w = area, h = area, radius = area // 2, bg_color = C.card, border_width = 2, border_color = C.ring, pad = 0 })
    plate:clear_flag("SCROLLABLE")
    plate:clear_flag("CLICKABLE")
    local cross_h = lvgl.object(plate, { x = 0, y = area // 2 - 1, w = area, h = 2, bg_color = C.ring, border_width = 0, pad = 0, radius = 0 })
    local cross_v = lvgl.object(plate, { x = area // 2 - 1, y = 0, w = 2, h = area, bg_color = C.ring, border_width = 0, pad = 0, radius = 0 })
    cross_h:clear_flag("CLICKABLE"); cross_v:clear_flag("CLICKABLE")
    local ball_r = math.max(10, area // 12)
    local ball = lvgl.object(plate, { x = area // 2 - ball_r, y = area // 2 - ball_r, w = ball_r * 2, h = ball_r * 2, radius = ball_r, bg_color = C.accent, border_width = 0, pad = 0 })
    ball:clear_flag("CLICKABLE")

    local acc = label(scr, "", pad, H - pad - 56, W - 2 * pad, 24, C.text)
    local gyr = label(scr, "", pad, H - pad - 28, W - 2 * pad, 24, C.muted)

    if not imu_ok then
        acc:set_text("IMU module not in firmware")
    else
        local ok, s = pcall(imu.new, { device = DEVICE })
        if ok then sensor = s else acc:set_text("No IMU on this board"); gyr:set_text(tostring(s):sub(1, 48)) end
    end
    scr:load()

    local bx, by = 0, 0
    local limit = area // 2 - ball_r - 4
    while running do
        lvgl.process_events(0)
        if sensor then
            local ok, sample = pcall(sensor.read, sensor)
            if ok and sample then
                local ax, ay, az = sample.accel.x, sample.accel.y, sample.accel.z
                local mag = math.sqrt(ax * ax + ay * ay + az * az)
                if mag < 1 then mag = 1 end
                -- Smooth the target so the ball glides instead of jittering.
                bx = bx * 0.7 + (-ax / mag) * limit * 0.3
                by = by * 0.7 + (ay / mag) * limit * 0.3
                ball:set_pos(math.floor(area // 2 - ball_r + bx), math.floor(area // 2 - ball_r + by))
                acc:set_text(string.format("A  x %6d  y %6d  z %6d", ax, ay, az))
                gyr:set_text(string.format("G  x %6d  y %6d  z %6d", sample.gyro.x, sample.gyro.y, sample.gyro.z))
            end
        end
        delay.delay_ms(PERIOD_MS)
    end
end

local ok, err = xpcall(run, debug.traceback)
if sensor then pcall(sensor.close, sensor) end
pcall(lvgl.deinit)
if not ok then error(err) end
