-- Album: browse JPEG photos saved under DATA/photos (the Camera app saves there).
local lvgl, delay, storage, image = require("lvgl"), require("delay"), require("storage"), require("image")

local C = { bg = "#000000", card = "#1c1f26", text = "#ffffff", muted = "#9aa2b0", accent = "#ff5a1f" }
local PHOTO_DIR = storage.join_path(storage.get_root_dir(), "photos")
local running = true
local photos, index = {}, 1
local view, name_label, box_w, box_h

local function is_jpeg(name)
    local n = name:lower()
    return n:sub(-4) == ".jpg" or n:sub(-5) == ".jpeg"
end

local function scan()
    photos = {}
    if not storage.exists(PHOTO_DIR) then return end
    for _, e in ipairs(storage.listdir(PHOTO_DIR)) do
        if e.type ~= "dir" and is_jpeg(e.name) then photos[#photos + 1] = e.name end
    end
    table.sort(photos, function(x, y) return x > y end) -- newest first (IMG_<time>.jpg)
end

local function show()
    if #photos == 0 then
        view:add_flag("HIDDEN")
        name_label:set_text("No photos yet - take one with Camera")
        return
    end
    if index > #photos then index = 1 elseif index < 1 then index = #photos end
    local name = photos[index]
    local frames = {}
    local ok, err = pcall(function()
        frames[1] = image.load_file(storage.join_path(PHOTO_DIR, name))
        local info = frames[1]:info()
        local scale = math.min(box_w / info.width, box_h / info.height)
        local w = math.max(2, math.floor(info.width * scale) // 2 * 2)
        local h = math.max(2, math.floor(info.height * scale))
        frames[2] = image.resize(frames[1], { width = w, height = h })
        view:set_raw_rgb565(w, h, frames[2]:data())
        view:set_pos((box_w - w) // 2, (box_h - h) // 2)
    end)
    for _, f in ipairs(frames) do pcall(f.release, f) end
    if ok then
        view:clear_flag("HIDDEN")
        name_label:set_text(string.format("%d / %d   %s", index, #photos, name))
    else
        view:add_flag("HIDDEN")
        name_label:set_text("Cannot open " .. name)
        print("[album] " .. tostring(err))
    end
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
    lvgl.label(scr, { text = "Album", x = pad + 54, y = pad + 8, w = W - 2 * pad - 60, h = 28, text_color = C.text, font = font })

    box_w, box_h = W - 2 * pad, H - pad * 3 - 48 - 50
    local frame = lvgl.object(scr, { x = pad, y = pad + 48, w = box_w, h = box_h, bg_color = C.card, radius = 12, border_width = 0, pad = 0 })
    frame:clear_flag("SCROLLABLE")
    view = lvgl.image(frame, { x = 0, y = 0 })
    view:clear_flag("CLICKABLE")
    name_label = lvgl.label(scr, { text = "", x = pad + 56, y = H - pad - 34, w = W - 2 * pad - 112, h = 24, text_color = C.muted, font = font })
    local prev = lvgl.button(scr, { text = "<", x = pad, y = H - pad - 44, w = 48, h = 40, radius = 20, bg_color = C.card, text_color = C.accent, border_width = 0, font = font })
    local nxt = lvgl.button(scr, { text = ">", x = W - pad - 48, y = H - pad - 44, w = 48, h = 40, radius = 20, bg_color = C.card, text_color = C.accent, border_width = 0, font = font })
    prev:on("clicked", function() index = index - 1; show() end)
    nxt:on("clicked", function() index = index + 1; show() end)
    scr:load()
    scan()
    show()
    while running do
        lvgl.process_events(0)
        delay.delay_ms(30)
    end
end

local ok, err = xpcall(run, debug.traceback)
pcall(lvgl.deinit)
if not ok then error(err) end
