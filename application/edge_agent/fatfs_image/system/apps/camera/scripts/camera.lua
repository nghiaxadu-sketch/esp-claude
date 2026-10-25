-- Camera: live preview; tap the shutter to save DATA/photos/IMG_<time>.jpg (view them in Album).
-- Tap "<" (top-left) to quit.
local display, dly, sys, storage = require("display"), require("delay"), require("system"), require("storage")
local image = require("image")
local cam_ok, camera = pcall(require, "camera")

local PHOTO_DIR = storage.join_path(storage.get_root_dir(), "photos")
local EXIT = 56

local ok, screen = pcall(display.open)
if not ok then print("[camera] ERROR: display: " .. tostring(screen)); return end
local info = screen:info()
local W, H = info.width, info.height
local FS = math.max(14, math.min(24, H // 18))
local WHITE, RED, BLACK = display.color(255, 255, 255), display.color(229, 57, 53), display.color(0, 0, 0)
local MUTED = display.color(154, 162, 176)
local SH_R = math.max(22, math.min(W, H) // 10)
local SH_X, SH_Y = W // 2, H - SH_R - 12

local function message(lines)
    screen:begin({ clear = BLACK })
    screen:text(12, 10, "<", { font_size = FS + 4, color = MUTED })
    for i, t in ipairs(lines) do
        local tw = screen:measure_text(t, { font_size = FS })
        screen:text((W - tw) // 2, H // 2 - FS + (i - 1) * (FS + 8), t, { font_size = FS, color = i == 1 and WHITE or MUTED })
    end
    screen:present()
end

local function wait_exit()
    if not info.touch_available then dly.delay_ms(3000); return end
    while true do
        local p = screen:touch().points[1]
        if p and p.x < EXIT and p.y < EXIT then return end
        dly.delay_ms(50)
    end
end

local opened = false
local function open_camera()
    if not cam_ok then return nil, "Camera module not in firmware" end
    local devs = camera.list_devices()
    if #devs == 0 then return nil, "No camera on this board" end
    local w, h = math.min(W, 640), math.min(H, 480)
    local okc, err = pcall(camera.open, devs[1].path, { width = w, height = h, format = { "RGBP", "YUYV", "JPEG" }, nearest = true })
    if not okc then return nil, tostring(err) end
    opened = true
    return true
end

local function mkdir_photos()
    if not storage.exists(PHOTO_DIR) then storage.mkdir(PHOTO_DIR) end
end

local function capture(frame)
    mkdir_photos()
    local path = storage.join_path(PHOTO_DIR, "IMG_" .. sys.date("%Y%m%d_%H%M%S") .. ".jpg")
    local fi = frame:info()
    if fi.pixel_format == "JPEG" then
        image.save_file(path, frame)
    else
        local jpeg <close> = image.convert(frame, image.JPEG)
        image.save_file(path, jpeg)
    end
    return path
end

local function run()
    local okc, err = open_camera()
    if not okc then
        message({ err, "Tap < to go back" })
        wait_exit()
        return
    end
    local touch_down, flash_until, saved = false, 0, ""
    while true do
        local shoot = false
        if info.touch_available then
            local p = screen:touch().points[1]
            local pressed = p ~= nil and not touch_down
            touch_down = p ~= nil
            if pressed then
                if p.x < EXIT and p.y < EXIT then return end
                local dx, dy = p.x - SH_X, p.y - SH_Y
                if dx * dx + dy * dy <= (SH_R + 10) * (SH_R + 10) then shoot = true end
            end
        end
        do
            local frame <close> = camera.get_frame(1000)
            if shoot then
                local okp, res = pcall(capture, frame)
                saved = okp and ("Saved " .. res:match("[^/]+$")) or ("Save failed")
                if not okp then print("[camera] " .. tostring(res)) end
                flash_until = sys.millis() + 1500
            end
            screen:begin({ clear = BLACK })
            screen:image(0, 0, frame, { mode = "contain", width = W, height = H })
        end
        screen:text(12, 10, "<", { font_size = FS + 4, color = WHITE })
        screen:fill_circle(SH_X, SH_Y, SH_R, WHITE)
        screen:fill_circle(SH_X, SH_Y, SH_R - 6, RED)
        if sys.millis() < flash_until then
            local tw = screen:measure_text(saved, { font_size = FS })
            screen:fill_round_rect((W - tw) // 2 - 10, 10, tw + 20, FS + 12, 8, BLACK)
            screen:text((W - tw) // 2, 16, saved, { font_size = FS, color = WHITE })
        end
        screen:present()
        dly.delay_ms(10)
    end
end

local ok2, err2 = xpcall(run, debug.traceback)
if opened then pcall(camera.close) end
pcall(screen.close, screen)
if not ok2 then print("[camera] ERROR: " .. tostring(err2)) end
