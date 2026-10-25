-- Music: play audio files from DATA/music (copy .mp3/.wav there via the web Files page).
local lvgl, delay, storage = require("lvgl"), require("delay"), require("storage")
local audio_ok, audio = pcall(require, "audio")

local C = { bg = "#000000", card = "#1c1f26", text = "#ffffff", muted = "#9aa2b0", accent = "#ff5a1f" }
local MUSIC_DIR = storage.join_path(storage.get_root_dir(), "music")
local EXT = { mp3 = true, wav = true, aac = true, m4a = true, flac = true, ogg = true, opus = true }
local running = true
local tracks, index = {}, 1
local output, player
local playing, paused = false, false

local function scan()
    tracks = {}
    if not storage.exists(MUSIC_DIR) then pcall(storage.mkdir, MUSIC_DIR); return end
    for _, e in ipairs(storage.listdir(MUSIC_DIR)) do
        local ext = e.name:match("%.(%w+)$")
        if e.type ~= "dir" and ext and EXT[ext:lower()] then tracks[#tracks + 1] = e.name end
    end
    table.sort(tracks)
end

local function run()
    lvgl.init()
    local font = lvgl.font_load({ size = 18, cache_size = 96 })
    lvgl.set_default_font(font)
    local scr = lvgl.create_screen()
    local W, H = scr:get_size()
    local big = lvgl.font_load({ size = math.max(22, math.min(32, W // 14)), cache_size = 48 })
    scr:set_style({ bg_color = C.bg, pad = 0 })
    scr:clear_flag("SCROLLABLE")
    local pad = math.max(8, W // 30)
    local back = lvgl.button(scr, { text = "<", x = pad, y = pad, w = 44, h = 40, radius = 20, bg_color = C.card, text_color = C.text, border_width = 0, font = font })
    back:on("clicked", function() running = false end)
    lvgl.label(scr, { text = "Music", x = pad + 54, y = pad + 8, w = W - 2 * pad - 60, h = 28, text_color = C.text, font = font })

    -- Vinyl disc like the launcher icon.
    local disc = math.min(W - 4 * pad, H - 48 - 160)
    local dx = (W - disc) // 2
    local rec = lvgl.object(scr, { x = dx, y = pad + 52, w = disc, h = disc, radius = disc // 2, bg_color = C.card, border_width = 2, border_color = "#2c313b", pad = 0 })
    rec:clear_flag("SCROLLABLE"); rec:clear_flag("CLICKABLE")
    local hub = disc // 3
    local center = lvgl.object(rec, { x = (disc - hub) // 2 - 2, y = (disc - hub) // 2 - 2, w = hub, h = hub, radius = hub // 2, bg_color = C.accent, border_width = 0, pad = 0 })
    center:clear_flag("CLICKABLE")

    local ty = pad + 60 + disc
    local title = lvgl.label(scr, { text = "", x = pad, y = ty, w = W - 2 * pad, h = 36, text_color = C.text, font = big })
    local state = lvgl.label(scr, { text = "", x = pad, y = ty + 36, w = W - 2 * pad, h = 24, text_color = C.muted, font = font })

    local bw, by = 64, H - pad - 52
    local prev = lvgl.button(scr, { text = "|<", x = W // 2 - bw * 3 // 2 - pad, y = by, w = bw, h = 48, radius = 24, bg_color = C.card, text_color = C.text, border_width = 0, font = font })
    local play = lvgl.button(scr, { text = ">", x = W // 2 - bw // 2, y = by, w = bw, h = 48, radius = 24, bg_color = C.accent, text_color = C.bg, border_width = 0, font = font })
    local nxt = lvgl.button(scr, { text = ">|", x = W // 2 + bw // 2 + pad, y = by, w = bw, h = 48, radius = 24, bg_color = C.card, text_color = C.text, border_width = 0, font = font })
    scr:load()

    scan()
    if not audio_ok then
        state:set_text("Audio module not in firmware")
    else
        local out, err = audio.open_output()
        if out then
            output = out
            player = audio.player({ output = output })
        else
            state:set_text("No speaker: " .. tostring(err):sub(1, 30))
        end
    end

    local function refresh_text(note)
        if #tracks == 0 then
            title:set_text("No music")
            state:set_text("Copy .mp3 to /music (web Files)")
            return
        end
        title:set_text((tracks[index]:gsub("%.%w+$", "")))
        state:set_text(note or string.format("%d / %d", index, #tracks))
        play:set_text(playing and not paused and "||" or ">")
    end

    local function start(i)
        if #tracks == 0 or not player then refresh_text(); return end
        index = ((i - 1) % #tracks) + 1
        local ok, err = player:play(storage.join_path(MUSIC_DIR, tracks[index]))
        playing, paused = ok and true or false, false
        refresh_text(ok and nil or ("Cannot play: " .. tostring(err):sub(1, 30)))
    end

    play:on("clicked", function()
        if not player then return end
        if not playing then start(index)
        elseif paused then player:resume(); paused = false; refresh_text()
        else player:pause(); paused = true; refresh_text("Paused") end
    end)
    prev:on("clicked", function() start(index - 1) end)
    nxt:on("clicked", function() start(index + 1) end)
    refresh_text()

    local ticks = 0
    while running do
        lvgl.process_events(0)
        ticks = ticks + 1
        if player and playing and not paused and ticks % 10 == 0 then
            local st = player:poll()
            if st and (st.state == "finished" or st.state == "stopped" or st.state == "error") then
                if st.state == "error" then playing = false; refresh_text("Playback error")
                else start(index + 1) end -- auto-advance
            end
        end
        delay.delay_ms(30)
    end
end

local ok, err = xpcall(run, debug.traceback)
if player then pcall(player.close, player) end
if output then pcall(output.close, output) end
pcall(lvgl.deinit)
if not ok then error(err) end
