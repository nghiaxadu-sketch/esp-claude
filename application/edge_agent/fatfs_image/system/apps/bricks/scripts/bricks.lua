-- Bricks: breakout. Drag on the screen to move the paddle; tap "<" (top-left) to quit.
local display, dly, sys = require("display"), require("delay"), require("system")

local ok, screen = pcall(display.open)
if not ok then print("[bricks] ERROR: display: " .. tostring(screen)); return end
local info = screen:info()
local W, H = info.width, info.height
if not info.touch_available then
    print("[bricks] ERROR: touch screen required")
    screen:close()
    return
end

local COL = {
    bg = display.color(0, 0, 0), text = display.color(255, 255, 255), muted = display.color(154, 162, 176),
    paddle = display.color(255, 255, 255), ball = display.color(255, 90, 31), card = display.color(28, 31, 38),
}
local ROW_COLORS = {
    display.color(255, 90, 31), display.color(230, 57, 53), display.color(255, 255, 255),
    display.color(120, 128, 140), display.color(255, 160, 60),
}

local FRAME_MS = 16
local TOP = math.max(36, H // 10)
local COLS, ROWS = 8, 5
local GAP = math.max(2, W // 120)
local BW = (W - GAP * (COLS + 1)) // COLS
local BH = math.max(8, H // 28)
local PW, PH = math.max(40, W // 5), math.max(6, H // 48)
local PY = H - PH - math.max(14, H // 20)
local R = math.max(4, W // 60)
local FS = math.max(14, math.min(28, H // 16))
local EXIT = 48

local bricks, alive
local px, bx, by, vx, vy
local score, lives, level, state = 0, 3, 1, "ready"
local running = true

local function reset_ball()
    bx, by = px + PW / 2, PY - R - 1
    local speed = (W / 110) * (1 + (level - 1) * 0.15)
    vx, vy = speed * 0.6, -speed
    state = "ready"
end

local function new_level()
    bricks, alive = {}, 0
    for r = 1, ROWS do
        for c = 1, COLS do
            bricks[#bricks + 1] = { x = GAP + (c - 1) * (BW + GAP), y = TOP + GAP + (r - 1) * (BH + GAP), color = ROW_COLORS[r], on = true, pts = (ROWS - r + 1) * 10 }
            alive = alive + 1
        end
    end
    reset_ball()
end

local function new_game()
    score, lives, level = 0, 3, 1
    px = (W - PW) // 2
    new_level()
end

local touch_down = false
local function input()
    local pts = screen:touch().points
    local p = pts[1]
    local pressed = p ~= nil and not touch_down
    touch_down = p ~= nil
    if not p then return end
    if pressed and p.x < EXIT and p.y < EXIT then running = false; return end
    px = math.max(0, math.min(W - PW, p.x - PW // 2))
    if pressed then
        if state == "ready" then state = "play"
        elseif state == "over" then new_game() end
    end
    if state == "ready" then bx = px + PW / 2 end
end

local function step(df)
    if state ~= "play" then return end
    bx, by = bx + vx * df, by + vy * df
    if bx - R < 0 then bx = R; vx = -vx end
    if bx + R > W then bx = W - R; vx = -vx end
    if by - R < TOP then by = TOP + R; vy = -vy end
    -- Paddle: bounce angle depends on hit position.
    if vy > 0 and by + R >= PY and by + R <= PY + PH + 6 and bx >= px - R and bx <= px + PW + R then
        local hit = (bx - (px + PW / 2)) / (PW / 2)
        local speed = math.sqrt(vx * vx + vy * vy)
        vx = speed * hit * 0.9
        vy = -math.sqrt(math.max(speed * speed - vx * vx, speed * speed * 0.2))
        by = PY - R
    end
    for _, b in ipairs(bricks) do
        if b.on and bx + R > b.x and bx - R < b.x + BW and by + R > b.y and by - R < b.y + BH then
            b.on, alive, score = false, alive - 1, score + b.pts
            local ox = math.min(bx + R - b.x, b.x + BW - (bx - R))
            local oy = math.min(by + R - b.y, b.y + BH - (by - R))
            if ox < oy then vx = -vx else vy = -vy end
            break
        end
    end
    if alive == 0 then level = level + 1; new_level() end
    if by - R > H then
        lives = lives - 1
        if lives <= 0 then state = "over" else reset_ball() end
    end
end

local function center(y, text, size, color)
    local tw = screen:measure_text(text, { font_size = size })
    screen:text((W - tw) // 2, y, text, { font_size = size, color = color })
end

local function render()
    screen:begin({ clear = COL.bg })
    screen:text(10, (TOP - FS) // 2, "<", { font_size = FS, color = COL.muted })
    center((TOP - FS) // 2, string.format("SCORE %d", score), FS, COL.text)
    local lv = string.format("L%d  %s", level, string.rep("o", lives))
    local lw = screen:measure_text(lv, { font_size = FS })
    screen:text(W - lw - 10, (TOP - FS) // 2, lv, { font_size = FS, color = COL.ball })
    for _, b in ipairs(bricks) do
        if b.on then screen:fill_round_rect(b.x, b.y, BW, BH, 3, b.color) end
    end
    screen:fill_round_rect(math.floor(px), PY, PW, PH, PH // 2, COL.paddle)
    screen:fill_circle(math.floor(bx), math.floor(by), R, COL.ball)
    if state == "ready" then center(H // 2 + 20, "TAP TO START", FS, COL.muted) end
    if state == "over" then
        screen:fill_round_rect(W // 8, H // 2 - FS * 2, W * 3 // 4, FS * 4, 14, COL.card)
        center(H // 2 - FS - 4, "GAME OVER", FS + 4, COL.text)
        center(H // 2 + 8, "TAP TO RETRY", FS, COL.muted)
    end
    screen:present()
end

new_game()
local last = sys.millis()
local ok2, err = xpcall(function()
    while running do
        local now = sys.millis()
        local df = math.min(3, math.max(0, now - last) / FRAME_MS)
        last = now
        input()
        step(df)
        render()
        local sl = FRAME_MS - (sys.millis() - now)
        if sl > 0 then dly.delay_ms(sl) end
    end
end, debug.traceback)
pcall(screen.close, screen)
if not ok2 then print("[bricks] ERROR: " .. tostring(err)) end
