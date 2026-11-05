-- Ashen Depths: a dark, element-driven dungeon crawler (action RPG) for ESP-Claw.
--
-- Descend through procedurally generated catacombs lit only by your torch,
-- slay monsters, collect loot of rising rarity and master three elements:
-- Fire beats Frost, Frost beats Storm, Storm beats Fire.
--
-- Touch controls: left half = virtual stick, right side = attack, skills, potion.
-- Button-only boards: the hero hunts automatically, a press casts a skill.

local display = require("display")
local delay = require("delay")
local sys = require("system")
local sfx_mod = require("ashen_audio")
local dungeon = require("ashen_dungeon")
local button_ok, button = pcall(require, "button")

local TAG = "[ashen_depths]"
local floor, sqrt, min, max, abs = math.floor, math.sqrt, math.min, math.max, math.abs
local random, cos, sin, pi = math.random, math.cos, math.sin, math.pi

-- ---------------------------------------------------------------------------
-- Arguments
-- ---------------------------------------------------------------------------
local a = type(args) == "table" and args or {}

local function int_arg(key, default)
    local v = a[key]
    if type(v) == "number" then return floor(v) end
    return default
end

local function str_arg(key, default)
    local v = a[key]
    if type(v) == "string" and v ~= "" then return v end
    return default
end

local BUTTON_PIN = int_arg("pin", int_arg("button_gpio", 0))
local BUTTON_ACTIVE_LEVEL = int_arg("active_level", 0)
local HAS_BUTTON_ARG = type(a.pin) == "number" or type(a.button_gpio) == "number"
local INPUT_MODE = str_arg("input_mode", HAS_BUTTON_ARG and "button" or "touch")
local FRAME_MS = max(16, min(200, int_arg("frame_ms", 33)))
local RUN_MS = int_arg("run_ms", 0)
local SAMPLE_RATE = int_arg("sample_rate_hz", 16000)
local SOUND_VOLUME = int_arg("volume", 80)
local START_DEPTH = max(1, int_arg("start_depth", 1))

-- ---------------------------------------------------------------------------
-- Tuning constants
-- ---------------------------------------------------------------------------
local D = require("ashen_data")
local K = D.K
local ELEMENTS, BEATS, SKILLS, MONSTERS = D.ELEMENTS, D.BEATS, D.SKILLS, D.MONSTERS
local MONSTER_POOL, RARITIES, SLOT_ORDER = D.MONSTER_POOL, D.RARITIES, D.SLOT_ORDER

-- ---------------------------------------------------------------------------
-- Display & colors
-- ---------------------------------------------------------------------------
local display_ok, screen = pcall(display.open)
if not display_ok then
    print(TAG .. " ERROR: display init failed: " .. tostring(screen))
    return
end
local screen_info = screen:info()
local W, H = screen_info.width, screen_info.height
if W <= 0 or H <= 0 then
    print(TAG .. " ERROR: invalid display size")
    pcall(screen.close, screen)
    return
end

local color_cache = {}
local function rgb(r, g, b, alpha)
    r, g, b = floor(max(0, min(255, r))), floor(max(0, min(255, g))), floor(max(0, min(255, b)))
    alpha = floor(alpha or 255)
    local key = (alpha << 24) | (r << 16) | (g << 8) | b
    local c = color_cache[key]
    if c == nil then
        c = display.color(r, g, b, alpha)
        color_cache[key] = c
    end
    return c
end

local C = {
    black = rgb(0, 0, 0),
    white = rgb(240, 236, 226),
    text = rgb(214, 200, 170),
    dim = rgb(130, 116, 96),
    gold = rgb(232, 190, 80),
    red = rgb(196, 30, 30),
    blood = rgb(120, 10, 10),
    blue = rgb(40, 80, 210),
    panel = rgb(18, 14, 12, 210),
    panel_edge = rgb(110, 84, 50),
    hurt = rgb(255, 40, 40, 90),
    slow = rgb(140, 200, 255),
    xp = rgb(170, 140, 60),
    shadow = rgb(0, 0, 0, 110),
}
local ELEMENT_COLOR = { fire = { 255, 110, 40 }, frost = { 110, 190, 255 }, storm = { 235, 225, 90 } }
local RARITY_COLOR = {
    normal = rgb(220, 220, 220), magic = rgb(100, 130, 255),
    rare = rgb(240, 220, 70), unique = rgb(220, 150, 50),
}
local function ecol(elem, k, alpha)
    local e = ELEMENT_COLOR[elem]
    k = k or 1
    return rgb(e[1] * k, e[2] * k, e[3] * k, alpha)
end

-- Integer-safe draw helpers (the display API rejects non-integer numbers).
local function R(x, y, w, h, c) screen:fill_rect(floor(x), floor(y), floor(w), floor(h), c) end
local function RR(x, y, w, h, r, c) screen:fill_round_rect(floor(x), floor(y), floor(w), floor(h), floor(r), c) end
local function SRR(x, y, w, h, r, c) screen:stroke_round_rect(floor(x), floor(y), floor(w), floor(h), floor(r), c) end
local function CI(x, y, r, c) if r >= 1 then screen:fill_circle(floor(x), floor(y), floor(r), c) end end
local function SC(x, y, r, c) if r >= 1 then screen:stroke_circle(floor(x), floor(y), floor(r), c) end end
local function LN(x0, y0, x1, y1, c) screen:line(floor(x0), floor(y0), floor(x1), floor(y1), c) end
local function TRI(x1, y1, x2, y2, x3, y3, c)
    screen:fill_triangle(floor(x1), floor(y1), floor(x2), floor(y2), floor(x3), floor(y3), c)
end
local function ARC(x, y, r, a0, a1, c)
    if r >= 1 then screen:arc(floor(x), floor(y), floor(r), a0, a1, c) end
end
local function TX(x, y, s, size, c)
    screen:text(floor(x), floor(y), s, { font_size = size, color = c })
end
local function measure(s, size)
    return screen:measure_text(s, { font_size = size })
end
local function CTX(cx, y, s, size, c)
    local tw = measure(s, size)
    TX(cx - tw // 2, y, s, size, c)
end

local SHORT = min(W, H)
local TILE = max(14, SHORT // K.TILES_ON_SHORT_SIDE)
local FONT_S = max(8, min(14, SHORT // 22))
local FONT_M = max(10, min(20, SHORT // 15))
local FONT_L = max(16, min(40, SHORT // 7))
local BTN_R = max(14, floor(SHORT * 0.085))
local MARGIN = max(6, SHORT // 40)
local ORB_R = max(12, floor(SHORT * 0.075))

-- Tile palettes indexed by light level (1 = memory, K.LIGHT_LEVELS = brightest).
local FLOOR_BASE = { { 92, 76, 58 }, { 84, 70, 54 }, { 98, 82, 62 } }
local GROUT_BASE = { 52, 42, 34 }
local WALL_TOP_BASE = { 34, 32, 40 }
local WALL_RIM_BASE = { 88, 84, 96 }
local WALL_FACE_BASE = { 136, 108, 78 }
local MORTAR_BASE = { 70, 52, 38 }
local STAIRS_BASE = { 40, 30, 26 }
local SPRITE_SCALE = 1.25
local PAL = { floor = {}, grout = {}, wall = {}, rim = {}, face = {}, mortar = {}, stairs = {} }
local function build_palettes()
    for lv = 1, K.LIGHT_LEVELS do
        local k = lv == K.MEMORY_LEVEL and 0.22 or (0.3 + 0.7 * (lv - 1) / (K.LIGHT_LEVELS - 1))
        local warm = (lv - 1) / (K.LIGHT_LEVELS - 1) * 18
        PAL.floor[lv] = {}
        for v = 1, #FLOOR_BASE do
            local f = FLOOR_BASE[v]
            PAL.floor[lv][v] = rgb(f[1] * k + warm, f[2] * k + warm * 0.5, f[3] * k)
        end
        local function shade(base, tint) return rgb(base[1] * k + tint, base[2] * k + tint * 0.5, base[3] * k) end
        PAL.grout[lv] = shade(GROUT_BASE, 0)
        PAL.wall[lv] = shade(WALL_TOP_BASE, 0)
        PAL.rim[lv] = shade(WALL_RIM_BASE, warm * 0.5)
        PAL.face[lv] = shade(WALL_FACE_BASE, warm)
        PAL.mortar[lv] = shade(MORTAR_BASE, 0)
        PAL.stairs[lv] = shade(STAIRS_BASE, 0)
    end
end
build_palettes()

-- ---------------------------------------------------------------------------
-- Audio
-- ---------------------------------------------------------------------------
local snd = nil
local function sfx(kind) if snd then snd:play(kind) end end

-- ---------------------------------------------------------------------------
-- Game state
-- ---------------------------------------------------------------------------
local map, seen = {}, {}
local dist_map, dist_origin, bfs_queue = {}, -1, {}
local rooms = {}
local monsters, projectiles, loot, effects, floaters, particles = {}, {}, {}, {}, {}, {}
local messages = {}
local hero
local depth = 1
local state = "title"
local state_t = 0
local game_time = 0
local best_depth = 1
local boss_ref = nil
local stairs_x, stairs_y = 0, 0
local stairs_open = true
local cam_x, cam_y = 0, 0
local shake = 0

local idx = dungeon.idx
local function tile_at(x, y) return dungeon.tile_at(map, x, y) end
local function solid(x, y) return tile_at(floor(x), floor(y)) == K.T_WALL end

local function dist(ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    return sqrt(dx * dx + dy * dy)
end

local function push_message(text, color)
    table.insert(messages, 1, { text = text, color = color or C.text, ttl = K.MSG_TTL })
    if #messages > K.MSG_MAX then messages[#messages] = nil end
end

local function float_text(x, y, text, color)
    floaters[#floaters + 1] = { x = x, y = y, text = text, color = color, ttl = 0.9 }
end

local function burst(x, y, color, count, speed)
    for _ = 1, count do
        local ang = random() * 2 * pi
        local sp = (0.4 + random()) * (speed or 2.5)
        particles[#particles + 1] = { x = x, y = y, vx = cos(ang) * sp, vy = sin(ang) * sp, ttl = 0.3 + random() * 0.35, color = color }
    end
end

-- ---------------------------------------------------------------------------
-- Items
-- ---------------------------------------------------------------------------
local items = require("ashen_items")
local item_score, make_item = items.score, items.make

-- ---------------------------------------------------------------------------
-- Hero
-- ---------------------------------------------------------------------------
local function recalc_hero()
    local h = hero
    local str, armor, hp, mp = h.base_str, 0, 0, 0
    local dmin, dmax = 2, 4
    h.elem = { fire = 0, frost = 0, storm = 0 }
    for _, slot in ipairs(SLOT_ORDER) do
        local it = h.gear[slot]
        if it then
            str, armor, hp, mp = str + it.str, armor + it.armor, hp + it.hp, mp + it.mp
            if slot == "weapon" then dmin, dmax = it.dmin, it.dmax end
            for e, v in pairs(it.elem) do h.elem[e] = h.elem[e] + v end
        end
    end
    h.dmin = dmin + str // 4
    h.dmax = dmax + str // 3
    h.armor = armor
    h.maxhp = 60 + h.lvl * 12 + hp
    h.maxmp = 28 + h.lvl * 5 + mp
    h.hp = min(h.hp, h.maxhp)
    h.mp = min(h.mp, h.maxmp)
end

local function new_hero()
    hero = {
        x = 0, y = 0, r = K.HERO_RADIUS, fx = 1, fy = 0, lvl = 1, xp = 0, xp_next = 60, base_str = 10,
        hp = 1, mp = 1, gold = 0, potions = 2, kills = 0, atk_cd = 0, swing = 0, hurt_t = 0,
        cd = { fire = 0, frost = 0, storm = 0 }, buff_t = 0, gear = {}, walk = 0,
    }
    hero.gear.weapon = make_item("weapon", 1, 0, RARITIES[1])
    recalc_hero()
    hero.hp, hero.mp = hero.maxhp, hero.maxmp
end

local function gain_xp(amount)
    hero.xp = hero.xp + amount
    while hero.xp >= hero.xp_next do
        hero.xp = hero.xp - hero.xp_next
        hero.lvl = hero.lvl + 1
        hero.xp_next = floor(hero.xp_next * 1.45 + 30)
        hero.base_str = hero.base_str + 2
        recalc_hero()
        hero.hp, hero.mp = hero.maxhp, hero.maxmp
        push_message("LEVEL UP! You are now level " .. hero.lvl, C.gold)
        for e, sk in pairs(SKILLS) do
            if sk.unlock == hero.lvl then push_message("New skill: " .. sk.label, ecol(e)) end
        end
        effects[#effects + 1] = { kind = "levelup", x = hero.x, y = hero.y, ttl = 1.0, life = 1.0 }
        sfx("level")
    end
end

local function pickup_item(it)
    local cur = hero.gear[it.slot]
    local col = RARITY_COLOR[it.rarity]
    if item_score(it) > item_score(cur) then
        hero.gear[it.slot] = it
        recalc_hero()
        push_message("Equipped " .. it.name, col)
        if cur then hero.gold = hero.gold + cur.value end
    else
        hero.gold = hero.gold + it.value
        push_message("Sold " .. it.name .. " +" .. it.value .. "g", col)
    end
    sfx("loot")
end

-- ---------------------------------------------------------------------------
-- Dungeon generation
-- ---------------------------------------------------------------------------
local room_center = dungeon.room_center
local function random_floor_in(r) return dungeon.random_floor_in(map, r) end

local function spawn_monster(kind, x, y, champion)
    local t = MONSTERS[kind]
    local scale = depth - 1
    local m = {
        kind = kind, t = t, x = x, y = y, r = t.r, name = t.name,
        maxhp = floor(t.hp * (1 + K.MONSTER_HP_SCALE * scale)), dmg = t.dmg * (1 + K.MONSTER_DMG_SCALE * scale),
        xp = floor(t.xp * (1 + K.MONSTER_XP_SCALE * scale)), speed = t.speed, cd = random() * t.cd,
        wind = 0, slow_t = 0, flash = 0, elem = ELEMENTS[random(1, #ELEMENTS)], anim = random() * 10,
    }
    if t.boss then
        m.elem = ELEMENTS[((depth // K.BOSS_EVERY) - 1) % #ELEMENTS + 1]
        m.maxhp = floor(t.hp * (1 + 0.5 * (depth // K.BOSS_EVERY - 1)))
        m.dmg = t.dmg * (1 + 0.3 * (depth // K.BOSS_EVERY - 1))
        m.summon_t, m.nova_t = 6, 4
    elseif champion then
        m.champion = true
        m.name = "Champion " .. t.name
        m.maxhp, m.dmg, m.xp, m.speed = m.maxhp * 2, m.dmg * 1.35, m.xp * 3, m.speed * 1.15
    end
    m.hp = m.maxhp
    monsters[#monsters + 1] = m
    return m
end

local function generate_level()
    seen = {}
    monsters, projectiles, loot, effects, floaters, particles = {}, {}, {}, {}, {}, {}
    boss_ref, dist_origin = nil, -1
    dungeon.generate(map, rooms)
    for i = 1, K.MAP_W * K.MAP_H do seen[i] = false end

    local first, last = rooms[1], rooms[#rooms]
    hero.x, hero.y = room_center(first)
    hero.x, hero.y = hero.x + 0.5, hero.y + 0.5
    local sx, sy = room_center(last)
    stairs_x, stairs_y = sx, sy
    map[idx(sx, sy)] = K.T_STAIRS

    local is_boss = depth % K.BOSS_EVERY == 0
    stairs_open = not is_boss
    if is_boss then
        boss_ref = spawn_monster("boss", sx + 0.5, sy - 1.5 < last.y and sy + 0.5 or sy - 1.5)
    end
    if #rooms > 3 and random() < 0.65 then
        local sr = rooms[random(2, #rooms - 1)]
        local tx, ty = random_floor_in(sr)
        map[idx(floor(tx), floor(ty))] = K.T_SHRINE
    end
    for i = 2, #rooms do
        local count = random(1, 2 + min(4, depth // 2))
        if i == #rooms and is_boss then count = 1 end
        for _ = 1, count do
            local mx, my = random_floor_in(rooms[i])
            spawn_monster(MONSTER_POOL[random(1, #MONSTER_POOL)], mx, my, random() < K.CHAMPION_CHANCE)
        end
    end
end

-- ---------------------------------------------------------------------------
-- Pathing: BFS distance field from the hero (shared by monsters and auto-hunt)
-- ---------------------------------------------------------------------------
local function update_dist_map()
    local hx, hy = floor(hero.x), floor(hero.y)
    local origin = idx(hx, hy)
    if origin == dist_origin then return end
    dist_origin = origin
    dungeon.build_field(map, dist_map, bfs_queue, hx, hy)
end

local function path_dist(x, y) return dungeon.field_at(dist_map, x, y) end
local function step_toward_hero(x, y) return dungeon.step_down(map, dist_map, x, y) end
local function hero_step_toward(x, y) return dungeon.first_step_to(map, dist_map, x, y) end

-- ---------------------------------------------------------------------------
-- Movement & combat
-- ---------------------------------------------------------------------------
local function blocked(x, y, r)
    return solid(x - r, y - r) or solid(x + r, y - r) or solid(x - r, y + r) or solid(x + r, y + r)
end

-- When the main axis is blocked by a corner but the tile ahead is open,
-- slide the cross axis toward the tile center so 1-wide corridors are easy to enter.
local function corner_assist(e, along, amount)
    local r = e.r
    local reach = (amount > 0 and 1 or -1) * (r + abs(amount))
    local ahead_free
    if along == "y" then ahead_free = not solid(e.x, e.y + reach) else ahead_free = not solid(e.x + reach, e.y) end
    if not ahead_free then return end
    local center = along == "y" and floor(e.x) + 0.5 or floor(e.y) + 0.5
    local cur = along == "y" and e.x or e.y
    local step = min(abs(amount), abs(center - cur)) * (center > cur and 1 or -1)
    if step == 0 then return end
    if along == "y" and not blocked(e.x + step, e.y, r) then e.x = e.x + step end
    if along == "x" and not blocked(e.x, e.y + step, r) then e.y = e.y + step end
end

local function move_entity(e, dx, dy)
    local r = e.r
    local moved_x = dx ~= 0 and not blocked(e.x + dx, e.y, r)
    if moved_x then e.x = e.x + dx end
    local moved_y = dy ~= 0 and not blocked(e.x, e.y + dy, r)
    if moved_y then e.y = e.y + dy end
    if dy ~= 0 and not moved_y and abs(dx) <= abs(dy) then corner_assist(e, "y", dy) end
    if dx ~= 0 and not moved_x and abs(dy) <= abs(dx) then corner_assist(e, "x", dx) end
end

local function nearest_monster(x, y, range, exclude)
    local best, bd = nil, range
    for _, m in ipairs(monsters) do
        if m.hp > 0 and not (exclude and exclude[m]) then
            local d = dist(x, y, m.x, m.y)
            if d < bd then best, bd = m, d end
        end
    end
    return best, bd
end

local function drop_loot(m)
    local bonus = m.t.boss and 0.6 or (m.champion and 0.3 or 0)
    local function place(kind, data)
        -- Keep drops near the tile center so they never hide in a wall corner.
        local x, y = floor(m.x) + 0.5 + (random() - 0.5) * 0.4, floor(m.y) + 0.5 + (random() - 0.5) * 0.4
        loot[#loot + 1] = { kind = kind, x = x, y = y, data = data, bob = random() * 6 }
    end
    if random() < K.DROP_GOLD or m.t.boss then place("gold", floor((4 + depth * 3) * (0.6 + random()) * (m.t.boss and 8 or 1))) end
    if random() < K.DROP_POTION then place("potion") end
    local items = m.t.boss and 2 or ((random() < K.DROP_ITEM + bonus) and 1 or 0)
    for i = 1, items do
        local force = (m.t.boss and i == 1) and RARITIES[3] or nil
        place("item", make_item(SLOT_ORDER[random(1, #SLOT_ORDER)], depth + hero.lvl // 2, bonus, force))
    end
end

local function kill_monster(m)
    hero.kills = hero.kills + 1
    gain_xp(m.xp)
    burst(m.x, m.y, ecol(m.elem, 0.8), m.t.boss and 40 or 12, m.t.boss and 4 or 2.5)
    drop_loot(m)
    if m.t.boss then
        stairs_open = true
        boss_ref = nil
        push_message(m.name .. " has fallen! The stairs open.", C.gold)
        shake = 0.6
    end
end

local function hurt_monster(m, amount, elem)
    if m.hp <= 0 then return end
    local mult, col = 1, C.white
    if elem then
        mult = mult * (1 + (hero.elem[elem] or 0) / 100)
        if BEATS[elem] == m.elem then mult, col = mult * K.WEAK_MULT, ecol(elem)
        elseif elem == m.elem then mult, col = mult * K.RESIST_MULT, C.dim end
    end
    if hero.buff_t > 0 then mult = mult * 1.5 end
    local crit = random() < K.CRIT_CHANCE
    if crit then mult, col = mult * K.CRIT_MULT, C.gold end
    local dmg = max(1, floor(amount * mult))
    m.hp = m.hp - dmg
    m.flash = 0.12
    m.wind = 0
    m.hit_aware = true
    float_text(m.x, m.y - m.r, (crit and "!" or "") .. dmg, col)
    burst(m.x, m.y, C.red, 4, 1.5)
    if m.hp <= 0 then kill_monster(m) end
end

local function roll_hero_damage()
    return random(hero.dmin, max(hero.dmin, hero.dmax))
end

local function hurt_hero(amount, elem)
    if state ~= "play" then return end
    local reduce = hero.armor / (hero.armor + 30 + depth * 6)
    local dmg = max(1, floor(amount * (1 - reduce) + 0.5))
    hero.hp = hero.hp - dmg
    hero.hurt_t = 0.25
    float_text(hero.x, hero.y - 0.4, "-" .. dmg, elem and ecol(elem) or C.red)
    sfx("hurt")
    if hero.hp <= 0 then
        hero.hp = 0
        state, state_t = "dead", 0
        best_depth = max(best_depth, depth)
        sfx("death")
        print(string.format("%s hero died depth=%d level=%d kills=%d", TAG, depth, hero.lvl, hero.kills))
    end
end

local function face_target(range)
    local m = nearest_monster(hero.x, hero.y, range)
    if m then
        local d = max(0.001, dist(hero.x, hero.y, m.x, m.y))
        hero.fx, hero.fy = (m.x - hero.x) / d, (m.y - hero.y) / d
    end
    return m
end

local function hero_attack()
    if hero.atk_cd > 0 then return end
    face_target(K.AUTO_AIM_RANGE)
    hero.atk_cd, hero.swing = K.ATTACK_CD, 0.2
    sfx("swing")
    local hit = false
    for _, m in ipairs(monsters) do
        if m.hp > 0 then
            local dx, dy = m.x - hero.x, m.y - hero.y
            local d = sqrt(dx * dx + dy * dy)
            if d <= K.ATTACK_RANGE + m.r and (d < 0.2 or (dx * hero.fx + dy * hero.fy) / d >= K.ATTACK_ARC_COS) then
                hurt_monster(m, roll_hero_damage(), nil)
                hit = true
            end
        end
    end
    if hit then sfx("hit") end
end

local function skill_ready(e)
    local sk = SKILLS[e]
    return hero.lvl >= sk.unlock and hero.cd[e] <= 0 and hero.mp >= sk.cost
end

local function cast_skill(e, quiet)
    if not skill_ready(e) then
        if not quiet and hero.lvl < SKILLS[e].unlock then push_message(SKILLS[e].label .. " unlocks at level " .. SKILLS[e].unlock, C.dim) end
        return false
    end
    local sk = SKILLS[e]
    hero.mp = hero.mp - sk.cost
    hero.cd[e] = sk.cd
    sfx("cast")
    local base = roll_hero_damage() * sk.mult + hero.lvl * 2
    if e == "fire" then
        face_target(K.SKILL_AIM_RANGE)
        projectiles[#projectiles + 1] = {
            x = hero.x + hero.fx * 0.4, y = hero.y + hero.fy * 0.4, vx = hero.fx * sk.speed, vy = hero.fy * sk.speed,
            owner = "hero", elem = "fire", dmg = base, r = 0.18, ttl = 1.4, kind = "fireball",
        }
    elseif e == "frost" then
        effects[#effects + 1] = { kind = "nova", x = hero.x, y = hero.y, ttl = 0.45, life = 0.45, radius = sk.radius, elem = "frost" }
        for _, m in ipairs(monsters) do
            if m.hp > 0 and dist(hero.x, hero.y, m.x, m.y) <= sk.radius + m.r then
                m.slow_t = sk.slow
                hurt_monster(m, base, "frost")
            end
        end
    else
        local target = face_target(sk.range)
        if not target then
            hero.mp, hero.cd[e] = hero.mp + sk.cost, 0
            if not quiet then push_message("No target for BOLT", C.dim) end
            return false
        end
        local from_x, from_y, hit = hero.x, hero.y, {}
        for _ = 1, sk.chain + 1 do
            if not target then break end
            hit[target] = true
            effects[#effects + 1] = { kind = "bolt", x = from_x, y = from_y, x2 = target.x, y2 = target.y, ttl = 0.22, life = 0.22, seed = random(1, 999) }
            hurt_monster(target, base, "storm")
            from_x, from_y = target.x, target.y
            base = base * 0.8
            target = nearest_monster(from_x, from_y, sk.chain_range, hit)
        end
    end
    return true
end

local function drink_potion()
    if hero.potions <= 0 then push_message("No potions left", C.dim) return end
    if hero.hp >= hero.maxhp then return end
    hero.potions = hero.potions - 1
    hero.hp = min(hero.maxhp, hero.hp + floor(hero.maxhp * K.POTION_HEAL))
    effects[#effects + 1] = { kind = "heal", x = hero.x, y = hero.y, ttl = 0.5, life = 0.5 }
    sfx("loot")
end

local SHRINES = {
    function() hero.buff_t = 40; push_message("Fiery Shrine: +50% damage", ecol("fire")) end,
    function() hero.hp = hero.maxhp; push_message("Healing Shrine: wounds closed", C.red) end,
    function() hero.mp = hero.maxmp; push_message("Mana Shrine: power restored", C.blue) end,
    function()
        local g = 20 + depth * 15
        hero.gold = hero.gold + g
        push_message("Shrine of Fortune: +" .. g .. " gold", C.gold)
    end,
    function() hero.potions = K.BELT_MAX; push_message("Shrine of Plenty: belt refilled", C.red) end,
}

-- ---------------------------------------------------------------------------
-- Update
-- ---------------------------------------------------------------------------
local function update_monster(m, dt)
    m.anim = m.anim + dt
    m.flash = max(0, m.flash - dt)
    m.slow_t = max(0, m.slow_t - dt)
    m.cd = m.cd - dt
    local d = dist(m.x, m.y, hero.x, hero.y)
    if d > K.ACTIVE_RANGE then return end
    local pd = path_dist(m.x, m.y)
    local aware = pd and (pd <= (m.t.boss and K.BOSS_AGGRO_STEPS or K.AGGRO_STEPS) or m.hit_aware)
    if not aware then return end
    local speed = m.speed * (m.slow_t > 0 and 0.45 or 1)

    if m.t.boss then
        m.summon_t, m.nova_t = m.summon_t - dt, m.nova_t - dt
        if m.nova_t <= 0 and d < 7 then
            m.nova_t = 4.5 - min(2, depth / 10)
            for i = 0, 11 do
                local ang = i * pi / 6 + m.anim
                projectiles[#projectiles + 1] = { x = m.x, y = m.y, vx = cos(ang) * 3.6, vy = sin(ang) * 3.6, owner = "enemy", elem = m.elem, dmg = m.dmg * 0.6, r = 0.16, ttl = 2.2, kind = "orb" }
            end
        end
        if m.summon_t <= 0 and d < 8 then
            m.summon_t = 9
            for _ = 1, 2 do
                local sx, sy = m.x + (random() - 0.5) * 2, m.y + (random() - 0.5) * 2
                if not blocked(sx, sy, 0.3) then spawn_monster("imp", sx, sy, false).hit_aware = true end
            end
            push_message(m.name .. " calls for aid!", ecol(m.elem))
        end
    end

    if m.wind > 0 then
        m.wind = m.wind - dt
        if m.wind <= 0 and d <= m.t.range + hero.r + 0.25 then hurt_hero(m.dmg, m.elem) end
        return
    end

    if m.t.ranged then
        if d <= m.t.range and m.cd <= 0 then
            m.cd = m.t.cd
            local inv = 1 / max(0.001, d)
            projectiles[#projectiles + 1] = { x = m.x, y = m.y, vx = (hero.x - m.x) * inv * 6, vy = (hero.y - m.y) * inv * 6, owner = "enemy", elem = m.elem, dmg = m.dmg, r = 0.12, ttl = 1.5, kind = "arrow" }
            return
        end
        if d < 2.5 then
            local inv = 1 / max(0.001, d)
            move_entity(m, (m.x - hero.x) * inv * speed * dt, (m.y - hero.y) * inv * speed * dt)
            return
        end
    elseif d <= m.t.range + hero.r then
        if m.cd <= 0 then m.cd, m.wind = m.t.cd, 0.28 end
        return
    end

    local tx, ty
    if d < 1.6 then
        tx, ty = hero.x, hero.y
    else
        tx, ty = step_toward_hero(m.x, m.y)
        tx, ty = tx or hero.x, ty or hero.y
    end
    local dx, dy = tx - m.x, ty - m.y
    local len = sqrt(dx * dx + dy * dy)
    if len > 0.01 then move_entity(m, dx / len * speed * dt, dy / len * speed * dt) end
end

local active_list = {}
local function separate_monsters()
    local n = 0
    for _, m in ipairs(monsters) do
        if m.hp > 0 and abs(m.x - hero.x) < K.ACTIVE_RANGE and abs(m.y - hero.y) < K.ACTIVE_RANGE then
            n = n + 1
            active_list[n] = m
        end
    end
    for i = 1, n do
        local m1 = active_list[i]
        for j = i + 1, n do
            local m2 = active_list[j]
            local dx, dy = m2.x - m1.x, m2.y - m1.y
            local rr = m1.r + m2.r
            if abs(dx) < rr and abs(dy) < rr then
                local d = sqrt(dx * dx + dy * dy)
                if d < rr and d > 0.001 then
                    local push = (rr - d) * 0.5 / d
                    move_entity(m1, -dx * push, -dy * push)
                    move_entity(m2, dx * push, dy * push)
                end
            end
        end
    end
end

local function explode(p)
    effects[#effects + 1] = { kind = "blast", x = p.x, y = p.y, ttl = 0.3, life = 0.3, radius = SKILLS.fire.radius, elem = p.elem }
    for _, m in ipairs(monsters) do
        if m.hp > 0 and dist(p.x, p.y, m.x, m.y) <= SKILLS.fire.radius + m.r then hurt_monster(m, p.dmg, p.elem) end
    end
    burst(p.x, p.y, ecol("fire"), 10, 3)
    shake = max(shake, 0.12)
end

local function update_projectiles(dt)
    for i = #projectiles, 1, -1 do
        local p = projectiles[i]
        p.x, p.y, p.ttl = p.x + p.vx * dt, p.y + p.vy * dt, p.ttl - dt
        local dead = p.ttl <= 0 or solid(p.x, p.y)
        if not dead then
            if p.owner == "hero" then
                for _, m in ipairs(monsters) do
                    if m.hp > 0 and dist(p.x, p.y, m.x, m.y) <= m.r + p.r then dead = true break end
                end
            elseif dist(p.x, p.y, hero.x, hero.y) <= K.HERO_RADIUS + p.r then
                hurt_hero(p.dmg, p.elem)
                dead = true
            end
        end
        if dead then
            if p.kind == "fireball" then explode(p) end
            table.remove(projectiles, i)
        end
    end
end

local function update_loot()
    for i = #loot, 1, -1 do
        local l = loot[i]
        if dist(l.x, l.y, hero.x, hero.y) < K.PICKUP_RANGE then
            local taken = true
            if l.kind == "gold" then
                hero.gold = hero.gold + l.data
                float_text(l.x, l.y, "+" .. l.data .. "g", C.gold)
                sfx("loot")
            elseif l.kind == "potion" then
                if hero.potions < K.BELT_MAX then
                    hero.potions = hero.potions + 1
                    float_text(l.x, l.y, "Potion", C.red)
                    sfx("loot")
                else
                    taken = false
                end
            else
                pickup_item(l.data)
            end
            if taken then table.remove(loot, i) end
        end
    end
end

local function update_fx(dt)
    for _, list in ipairs({ effects, floaters, particles }) do
        for i = #list, 1, -1 do
            local e = list[i]
            e.ttl = e.ttl - dt
            if e.vx then e.x, e.y = e.x + e.vx * dt, e.y + e.vy * dt; e.vx, e.vy = e.vx * 0.9, e.vy * 0.9 end
            if list == floaters then e.y = e.y - dt * 0.9 end
            if e.ttl <= 0 then table.remove(list, i) end
        end
    end
    for i = #messages, 1, -1 do
        messages[i].ttl = messages[i].ttl - dt
        if messages[i].ttl <= 0 then table.remove(messages, i) end
    end
    shake = max(0, shake - dt)
end

local function enter_depth(new_depth)
    depth = new_depth
    best_depth = max(best_depth, depth)
    generate_level()
    state, state_t = "transition", 0
    print(string.format("%s depth=%d level=%d gold=%d kills=%d", TAG, depth, hero.lvl, hero.gold, hero.kills))
    push_message("Depth " .. depth .. (depth % K.BOSS_EVERY == 0 and " - a great evil stirs..." or ""), depth % K.BOSS_EVERY == 0 and C.red or C.text)
end

local function update_world(dt, mx, my)
    local h = hero
    h.atk_cd = max(0, h.atk_cd - dt)
    h.swing = max(0, h.swing - dt)
    h.hurt_t = max(0, h.hurt_t - dt)
    h.buff_t = max(0, h.buff_t - dt)
    for e in pairs(h.cd) do h.cd[e] = max(0, h.cd[e] - dt) end
    h.hp = min(h.maxhp, h.hp + K.HP_REGEN * dt)
    h.mp = min(h.maxmp, h.mp + K.MP_REGEN * dt)

    local mag = sqrt(mx * mx + my * my)
    if mag > 0.05 then
        h.fx, h.fy = mx / mag, my / mag
        local sp = K.HERO_SPEED * min(1, mag) * dt
        move_entity(h, h.fx * sp, h.fy * sp)
        h.walk = h.walk + dt * 10
    end

    update_dist_map()
    for _, m in ipairs(monsters) do
        if m.hp > 0 then update_monster(m, dt) end
    end
    separate_monsters()
    for i = #monsters, 1, -1 do
        if monsters[i].hp <= 0 then table.remove(monsters, i) end
    end
    update_projectiles(dt)
    update_loot()

    local tx, ty = floor(h.x), floor(h.y)
    local t = tile_at(tx, ty)
    if t == K.T_SHRINE then
        map[idx(tx, ty)] = K.T_SHRINE_USED
        SHRINES[random(1, #SHRINES)]()
        effects[#effects + 1] = { kind = "levelup", x = h.x, y = h.y, ttl = 0.8, life = 0.8 }
        sfx("level")
    elseif t == K.T_STAIRS and state == "play" then
        if stairs_open then
            sfx("stairs")
            enter_depth(depth + 1)
        elseif not h.stairs_warned then
            h.stairs_warned = true
            push_message("A dark seal binds the stairs. Slay the Warden!", C.red)
        end
    end
end

-- ---------------------------------------------------------------------------
-- Input
-- ---------------------------------------------------------------------------
local input_mode = "none"
local button_handle, button_last = nil, 1
local touch_prev, touch_btn = {}, {}
local stick = { id = nil, ox = 0, oy = 0, x = 0, y = 0 }
local pressed, held = {}, {}
local tapped = false
local auto = { potion_t = 0, stuck = 0, x = 0, y = 0 }

local ATK_X = W - MARGIN - floor(BTN_R * 1.35)
local ATK_Y = H - MARGIN - floor(BTN_R * 1.35)
local ATK_R = floor(BTN_R * 1.35)
local BUTTONS = { { id = "attack", x = ATK_X, y = ATK_Y, r = ATK_R } }
do
    -- Skills orbit the attack button; spacing keeps neighbouring buttons from overlapping.
    local ring = { { "fire", 180 }, { "frost", 225 }, { "storm", 270 } }
    local d = max(ATK_R + BTN_R + max(4, BTN_R // 3), floor((BTN_R + 4) / sin(pi / 8)) + 1)
    for _, b in ipairs(ring) do
        local ang = b[2] * pi / 180
        BUTTONS[#BUTTONS + 1] = { id = b[1], x = floor(ATK_X + cos(ang) * d), y = floor(ATK_Y + sin(ang) * d), r = BTN_R }
    end
end
-- The potion sits under the mana orb, away from the combat cluster.
BUTTONS[#BUTTONS + 1] = { id = "potion", x = W - MARGIN - ORB_R - 3, y = MARGIN + ORB_R * 3 + BTN_R // 2 + 8, r = BTN_R }
local STICK_R = floor(BTN_R * 1.6)

local function button_at(x, y)
    for _, b in ipairs(BUTTONS) do
        local dx, dy = x - b.x, y - b.y
        if dx * dx + dy * dy <= (b.r + 6) * (b.r + 6) then return b.id end
    end
    return nil
end

local function init_input()
    if INPUT_MODE ~= "button" and screen_info.touch_available then
        input_mode = "touch"
        return true
    end
    if not button_ok then
        print(TAG .. " ERROR: no touch screen and button module unavailable")
        return false
    end
    local err
    button_handle, err = button.new(BUTTON_PIN, BUTTON_ACTIVE_LEVEL)
    if not button_handle then
        print(TAG .. " ERROR: button.new failed: " .. tostring(err))
        return false
    end
    button_last = button.get_key_level(button_handle) or 1
    input_mode = "button"
    return true
end

local function poll_touch()
    local pts = screen:touch().points
    local cur = {}
    for _, p in ipairs(pts) do
        cur[p.id] = true
        if stick.id == p.id then
            stick.x, stick.y = p.x, p.y
        elseif not touch_prev[p.id] then
            if state ~= "play" then
                tapped = true
            else
                local b = button_at(p.x, p.y)
                if b then
                    pressed[b] = true
                    touch_btn[p.id] = b
                elseif p.x < W * 0.55 and stick.id == nil then
                    stick.id, stick.ox, stick.oy, stick.x, stick.y = p.id, p.x, p.y, p.x, p.y
                end
            end
        end
        if touch_btn[p.id] then held[touch_btn[p.id]] = true end
    end
    if stick.id ~= nil and not cur[stick.id] then stick.id = nil end
    for id in pairs(touch_btn) do
        if not cur[id] then touch_btn[id] = nil end
    end
    touch_prev = cur
end

local function poll_button()
    local level = button.get_key_level(button_handle)
    if level == nil then return end
    if level == BUTTON_ACTIVE_LEVEL and button_last ~= BUTTON_ACTIVE_LEVEL then
        if state == "play" then pressed.auto_cast = true else tapped = true end
    end
    button_last = level
end

local function stick_vector()
    if stick.id == nil then return 0, 0 end
    local dx, dy = stick.x - stick.ox, stick.y - stick.oy
    local len = sqrt(dx * dx + dy * dy)
    if len < STICK_R * 0.15 then return 0, 0 end
    local k = min(1, len / STICK_R) / len
    return dx * k, dy * k
end

-- Button-only boards: walk toward the nearest reachable monster, else the stairs.
local function auto_hunt(dt)
    auto.potion_t = auto.potion_t - dt
    local target, best = nil, K.AGGRO_STEPS + 6
    for _, m in ipairs(monsters) do
        local pd = path_dist(m.x, m.y)
        if m.hp > 0 and pd and pd < best and not m.auto_ignore then target, best = m, pd end
    end
    if pressed.auto_cast then
        if not (cast_skill("storm", true) or cast_skill("fire", true) or cast_skill("frost", true)) then drink_potion() end
    end
    if hero.hp < hero.maxhp * 0.3 and hero.potions > 0 and auto.potion_t <= 0 then
        drink_potion()
        auto.potion_t = 2
    end
    local gx, gy
    local goal = target
    if target then
        if dist(hero.x, hero.y, target.x, target.y) <= K.ATTACK_RANGE + target.r * 0.5 then
            hero_attack()
            return 0, 0
        end
        gx, gy = hero_step_toward(target.x, target.y)
        if dist(hero.x, hero.y, target.x, target.y) < 1.5 then gx, gy = target.x, target.y end
    end
    if not target then
        local want = nil
        for _, l in ipairs(loot) do
            if path_dist(l.x, l.y) and not l.auto_ignore and not (l.kind == "potion" and hero.potions >= K.BELT_MAX) then
                want = l
                break
            end
        end
        goal = want
        if want then
            gx, gy = hero_step_toward(want.x, want.y)
            if dist(hero.x, hero.y, want.x, want.y) < 1.2 then gx, gy = want.x, want.y end
        else
            gx, gy = hero_step_toward(stairs_x + 0.5, stairs_y + 0.5)
            if not gx then gx, gy = stairs_x + 0.5, stairs_y + 0.5 end
        end
    end
    -- Give up on goals the hero cannot reach (e.g. blocked by a wandering monster pack).
    if dist(hero.x, hero.y, auto.x, auto.y) < 0.02 then auto.stuck = auto.stuck + dt else auto.stuck = 0 end
    auto.x, auto.y = hero.x, hero.y
    if auto.stuck > K.AUTO_STUCK_TIME and goal then
        goal.auto_ignore = true
        auto.stuck = 0
    end
    if not gx then return 0, 0 end
    local dx, dy = gx - hero.x, gy - hero.y
    local len = sqrt(dx * dx + dy * dy)
    if len < 0.05 then return 0, 0 end
    return dx / len, dy / len
end

-- ---------------------------------------------------------------------------
-- Rendering
-- ---------------------------------------------------------------------------
local function to_screen(x, y) return floor(x * TILE - cam_x), floor(y * TILE - cam_y) end

local function light_level(x, y, radius)
    local d = dist(x, y, hero.x, hero.y)
    if d >= radius then return 0 end
    return min(K.LIGHT_LEVELS, 2 + floor((1 - d / radius) * (K.LIGHT_LEVELS - 1)))
end

-- Walls use a 3/4 view: dark top, lit brick face toward the camera, rims on open sides.
local function draw_wall(tx, ty, sx, sy, lv, face_h)
    local below = tile_at(tx, ty + 1) ~= K.T_WALL
    local above = tile_at(tx, ty - 1) ~= K.T_WALL
    local left = tile_at(tx - 1, ty) ~= K.T_WALL
    local right = tile_at(tx + 1, ty) ~= K.T_WALL
    if not (below or above or left or right) then return end
    R(sx, sy, TILE, TILE, PAL.wall[lv])
    local rim = PAL.rim[lv]
    if above then R(sx, sy, TILE, 2, rim) end
    if left then R(sx, sy, 2, TILE, rim) end
    if right then R(sx + TILE - 2, sy, 2, TILE, rim) end
    if below then
        local fy = sy + TILE - face_h
        R(sx, fy, TILE, face_h, PAL.face[lv])
        R(sx, fy, TILE, 1, rim)
        local mortar = PAL.mortar[lv]
        local mid = fy + face_h // 2
        R(sx, mid, TILE, 1, mortar)
        local off = (tx % 2) * (TILE // 2)
        R(sx + (TILE // 4 + off) % TILE, fy + 1, 1, face_h // 2 - 1, mortar)
        R(sx + (TILE * 3 // 4 + off) % TILE, mid + 1, 1, face_h - face_h // 2 - 1, mortar)
    end
end

local function draw_tiles(radius)
    local tx0, ty0 = floor(cam_x / TILE) - 1, floor(cam_y / TILE) - 1
    local tx1, ty1 = tx0 + W // TILE + 3, ty0 + H // TILE + 3
    local face_h = max(4, TILE * 9 // 20)
    local r2 = radius * radius
    for ty = max(0, ty0), min(K.MAP_H - 1, ty1) do
        local sy = ty * TILE - cam_y
        for tx = max(0, tx0), min(K.MAP_W - 1, tx1) do
            local i = idx(tx, ty)
            local t = map[i]
            local dx, dy = tx + 0.5 - hero.x, ty + 0.5 - hero.y
            local d2 = dx * dx + dy * dy
            local lv = 0
            if d2 < r2 then
                lv = min(K.LIGHT_LEVELS, 2 + floor((1 - sqrt(d2) / radius) * (K.LIGHT_LEVELS - 1)))
                seen[i] = true
            elseif seen[i] then
                lv = K.MEMORY_LEVEL
            end
            if lv > 0 then
                local sx = tx * TILE - cam_x
                if t == K.T_WALL then
                    draw_wall(tx, ty, sx, sy, lv, face_h)
                else
                    R(sx, sy, TILE, TILE, PAL.floor[lv][(tx * 7 + ty * 13) % 3 + 1])
                    R(sx, sy, TILE, 1, PAL.grout[lv])
                    R(sx, sy, 1, TILE, PAL.grout[lv])
                    if t == K.T_STAIRS then
                        local st = PAL.stairs[lv]
                        local steps = 4
                        for s = 0, steps - 1 do
                            local inset = s * TILE // (steps * 2)
                            R(sx + inset, sy + s * TILE // steps, TILE - inset * 2, TILE // steps - 1, st)
                        end
                        if not stairs_open and lv > K.MEMORY_LEVEL then SC(sx + TILE / 2, sy + TILE / 2, TILE * 0.45, C.red) end
                    elseif t == K.T_SHRINE or t == K.T_SHRINE_USED then
                        local glow = t == K.T_SHRINE and (lv > K.MEMORY_LEVEL)
                        local cx, cy = sx + TILE / 2, sy + TILE / 2
                        RR(cx - TILE * 0.28, cy - TILE * 0.1, TILE * 0.56, TILE * 0.45, 2, PAL.wall[lv])
                        CI(cx, cy - TILE * 0.18, TILE * 0.2, glow and rgb(160, 90, 255, 200 + floor(sin(game_time * 4) * 50)) or PAL.face[lv])
                    end
                end
            end
        end
    end
end

local function draw_hero()
    local sx, sy = to_screen(hero.x, hero.y)
    local r = floor(K.HERO_RADIUS * TILE * SPRITE_SCALE)
    local bob = floor(sin(hero.walk) * 1.5)
    CI(sx, sy + r * 0.7, r * 0.9, C.shadow)
    local cloak = hero.hurt_t > 0 and rgb(220, 60, 60) or rgb(120, 34, 30)
    CI(sx, sy + bob, r, cloak)
    CI(sx, sy + bob - r * 0.25, r * 0.62, rgb(170, 150, 130))
    CI(sx + hero.fx * r * 0.35, sy + bob - r * 0.3 + hero.fy * r * 0.2, r * 0.22, rgb(40, 30, 30))
    -- Sword: swings through an arc while attacking.
    local base_ang = math.atan(hero.fy, hero.fx)
    local swing = hero.swing > 0 and (hero.swing / 0.2 - 0.5) * 2.2 or 0.6
    local ang = base_ang + swing
    local len = TILE * 0.75
    local wcol = RARITY_COLOR[hero.gear.weapon and hero.gear.weapon.rarity or "normal"]
    LN(sx + cos(ang) * r * 0.6, sy + bob + sin(ang) * r * 0.6, sx + cos(ang) * len, sy + bob + sin(ang) * len, wcol)
    LN(sx + cos(ang) * r * 0.6 + 1, sy + bob + sin(ang) * r * 0.6, sx + cos(ang) * len + 1, sy + bob + sin(ang) * len, wcol)
    if hero.swing > 0 then
        local deg = base_ang * 180 / pi
        ARC(sx, sy, TILE * K.ATTACK_RANGE * 0.85, deg - 60, deg + 60, rgb(255, 240, 200, 160))
    end
    if hero.buff_t > 0 then SC(sx, sy + bob, r + 2, ecol("fire", 1, 160)) end
end

local function draw_monster(m, lv)
    local sx, sy = to_screen(m.x, m.y)
    local r = floor(m.r * TILE * SPRITE_SCALE)
    local k = 0.45 + 0.55 * lv / K.LIGHT_LEVELS
    local flash = m.flash > 0
    local wobble = floor(sin(m.anim * 6) * 1.5)
    CI(sx, sy + r * 0.7, r * 0.9, C.shadow)
    local ec = ELEMENT_COLOR[m.elem]
    local function tint(cr, cg, cb)
        if flash then return C.white end
        return rgb((cr * 0.75 + ec[1] * 0.25) * k, (cg * 0.75 + ec[2] * 0.25) * k, (cb * 0.75 + ec[3] * 0.25) * k)
    end
    local eye = ecol(m.elem, 1)
    if m.kind == "imp" then
        TRI(sx - r, sy - r * 0.4, sx - r * 0.5, sy - r * 1.3, sx - r * 0.1, sy - r * 0.6, tint(150, 40, 30))
        TRI(sx + r, sy - r * 0.4, sx + r * 0.5, sy - r * 1.3, sx + r * 0.1, sy - r * 0.6, tint(150, 40, 30))
        CI(sx, sy + wobble, r, tint(170, 60, 40))
        CI(sx - r * 0.35, sy - r * 0.2 + wobble, max(1, r // 4), eye)
        CI(sx + r * 0.35, sy - r * 0.2 + wobble, max(1, r // 4), eye)
    elseif m.kind == "skeleton" or m.kind == "archer" then
        local bone = tint(210, 205, 185)
        RR(sx - r * 0.6, sy, r * 1.2, r * 1.1, 2, tint(150, 145, 130))
        CI(sx, sy - r * 0.3 + wobble, r * 0.75, bone)
        CI(sx - r * 0.28, sy - r * 0.35 + wobble, max(1, r // 4), C.black)
        CI(sx + r * 0.28, sy - r * 0.35 + wobble, max(1, r // 4), C.black)
        if m.kind == "archer" then
            local ang = math.atan(hero.y - m.y, hero.x - m.x) * 180 / pi
            ARC(sx, sy, r * 1.3, ang - 50, ang + 50, tint(140, 100, 60))
        else
            LN(sx + r, sy - r, sx + r, sy + r, tint(170, 170, 180))
        end
    elseif m.kind == "zombie" then
        RR(sx - r, sy - r + wobble, r * 2, r * 2, r // 2, tint(80, 110, 60))
        R(sx - r * 0.5, sy - r * 0.4 + wobble, max(2, r // 3), max(2, r // 3), eye)
        R(sx + r * 0.2, sy - r * 0.4 + wobble, max(2, r // 3), max(2, r // 3), eye)
        R(sx - r * 0.5, sy + r * 0.3 + wobble, r, max(1, r // 5), tint(40, 20, 20))
    else
        for i = 0, 7 do
            local ang = i * pi / 4 + m.anim
            TRI(sx + cos(ang) * r * 1.35, sy + sin(ang) * r * 1.35, sx + cos(ang + 0.4) * r * 0.8, sy + sin(ang + 0.4) * r * 0.8,
                sx + cos(ang - 0.4) * r * 0.8, sy + sin(ang - 0.4) * r * 0.8, tint(60, 30, 30))
        end
        CI(sx, sy, r, tint(50, 25, 25))
        CI(sx, sy, r * 0.7, tint(90, 40, 30))
        CI(sx - r * 0.3, sy - r * 0.2, max(2, r // 5), eye)
        CI(sx + r * 0.3, sy - r * 0.2, max(2, r // 5), eye)
    end
    if m.champion then SC(sx, sy, r + 3, rgb(90, 120, 255)) end
    if m.slow_t > 0 then SC(sx, sy, r + 1, C.slow) end
    if m.wind > 0 then SC(sx, sy, r + 4, rgb(255, 60, 40, 200)) end
    if m.hp < m.maxhp and not m.t.boss then
        local bw = max(TILE // 2, r * 2)
        R(sx - bw // 2, sy - r - 7, bw, 3, C.blood)
        R(sx - bw // 2, sy - r - 7, bw * m.hp / m.maxhp, 3, C.red)
    end
end

local function draw_loot()
    for _, l in ipairs(loot) do
        local lv = light_level(l.x, l.y, K.LIGHT_RADIUS + 1)
        if lv > 0 then
            local sx, sy = to_screen(l.x, l.y)
            local bob = floor(sin(game_time * 3 + l.bob) * 2)
            local s = max(3, TILE // 5)
            if l.kind == "gold" then
                CI(sx, sy + bob, s, C.gold)
                CI(sx - 1, sy + bob - 1, max(1, s // 2), rgb(255, 230, 140))
            elseif l.kind == "potion" then
                CI(sx, sy + bob + 1, s, C.red)
                R(sx - 1, sy + bob - s - 2, 3, 4, rgb(200, 190, 170))
            else
                local col = RARITY_COLOR[l.data.rarity]
                if l.data.rarity ~= "normal" then
                    local beam = TILE * (l.data.rarity == "magic" and 1 or 2)
                    R(sx - 2, sy - beam, 4, beam, rgb(255, 255, 255, 40))
                    R(sx - 1, sy - beam, 2, beam, col)
                end
                RR(sx - s, sy - s + bob, s * 2, s * 2, 2, col)
                SRR(sx - s, sy - s + bob, s * 2, s * 2, 2, C.black)
            end
        end
    end
end

local function draw_projectiles()
    for _, p in ipairs(projectiles) do
        local sx, sy = to_screen(p.x, p.y)
        if p.kind == "arrow" then
            LN(sx, sy, sx - p.vx * 2.5, sy - p.vy * 2.5, rgb(200, 180, 140))
            CI(sx, sy, 2, ecol(p.elem))
        else
            local r = max(3, floor(p.r * TILE))
            CI(sx, sy, r + 2, ecol(p.elem, 1, 90))
            CI(sx, sy, r, ecol(p.elem))
            CI(sx, sy, max(1, r // 2), rgb(255, 250, 220))
        end
    end
end

local function draw_effects()
    for _, e in ipairs(effects) do
        local sx, sy = to_screen(e.x, e.y)
        local t = 1 - e.ttl / e.life
        if e.kind == "nova" or e.kind == "blast" then
            local rr = e.radius * TILE * (e.kind == "nova" and t or (0.4 + 0.6 * t))
            local alpha = floor(220 * (1 - t))
            if e.kind == "blast" then CI(sx, sy, rr, ecol(e.elem, 1, alpha // 2)) end
            SC(sx, sy, rr, ecol(e.elem, 1, alpha))
            SC(sx, sy, rr - 2, ecol(e.elem, 1, alpha))
        elseif e.kind == "bolt" then
            local x2, y2 = to_screen(e.x2, e.y2)
            local px, py, segs = sx, sy, 5
            local rnd = e.seed
            for s = 1, segs do
                local f = s / segs
                rnd = (rnd * 73 + 41) % 997
                local jx = s < segs and ((rnd % 13) - 6) * TILE / 24 or 0
                local jy = s < segs and (((rnd // 13) % 13) - 6) * TILE / 24 or 0
                local nx, ny = sx + (x2 - sx) * f + jx, sy + (y2 - sy) * f + jy
                LN(px, py, nx, ny, ecol("storm"))
                LN(px + 1, py, nx + 1, ny, rgb(255, 255, 240))
                px, py = nx, ny
            end
        elseif e.kind == "levelup" then
            local rr = TILE * (0.4 + t * 1.2)
            SC(sx, sy, rr, rgb(240, 200, 80, floor(255 * (1 - t))))
            R(sx - 2, sy - TILE * 2 * (1 - t), 4, TILE * 2 * (1 - t), rgb(255, 230, 140, 120))
        elseif e.kind == "heal" then
            SC(sx, sy, TILE * (0.3 + t * 0.5), rgb(255, 80, 80, floor(255 * (1 - t))))
        end
    end
    for _, p in ipairs(particles) do
        local sx, sy = to_screen(p.x, p.y)
        R(sx, sy, 2, 2, p.color)
    end
    for _, f in ipairs(floaters) do
        local sx, sy = to_screen(f.x, f.y)
        CTX(sx + 1, sy + 1, f.text, FONT_S, C.black)
        CTX(sx, sy, f.text, FONT_S, f.color)
    end
end

local function draw_orb(cx, cy, r, frac, full_col, empty_col, label)
    CI(cx, cy, r + 3, rgb(60, 48, 30))
    CI(cx, cy, r + 1, rgb(140, 110, 60))
    CI(cx, cy, r, empty_col)
    local top = cy + r - floor(2 * r * max(0, min(1, frac)))
    for y = max(top, cy - r), cy + r, 2 do
        local dy = y - cy
        local hw = floor(sqrt(max(0, r * r - dy * dy)))
        if hw > 0 then R(cx - hw, y, hw * 2, 2, full_col) end
    end
    CI(cx - r * 0.35, cy - r * 0.4, max(2, r // 5), rgb(255, 255, 255, 90))
    CTX(cx, cy - FONT_S // 2, label, FONT_S, C.white)
end

local function draw_skill_button(b)
    local cx, cy, r = b.x, b.y, b.r
    if b.id == "attack" then
        CI(cx, cy, r, held.attack and rgb(140, 110, 70, 220) or rgb(70, 54, 36, 190))
        SC(cx, cy, r, C.panel_edge)
        LN(cx - r * 0.4, cy + r * 0.4, cx + r * 0.4, cy - r * 0.4, C.white)
        LN(cx - r * 0.4 + 1, cy + r * 0.4, cx + r * 0.4 + 1, cy - r * 0.4, C.white)
        LN(cx - r * 0.3, cy + r * 0.1, cx - r * 0.1, cy + r * 0.3, C.white)
        return
    end
    if b.id == "potion" then
        CI(cx, cy, r, rgb(60, 14, 14, 200))
        SC(cx, cy, r, C.panel_edge)
        CI(cx, cy + r * 0.15, r * 0.45, hero.potions > 0 and C.red or C.dim)
        R(cx - r * 0.12, cy - r * 0.55, r * 0.24, r * 0.35, rgb(200, 190, 170))
        TX(cx + r * 0.35, cy + r * 0.2, tostring(hero.potions), FONT_S, C.white)
        return
    end
    local sk = SKILLS[b.id]
    local locked = hero.lvl < sk.unlock
    local ready = skill_ready(b.id)
    CI(cx, cy, r, ready and ecol(b.id, 0.55, 220) or rgb(40, 34, 30, 200))
    SC(cx, cy, r, locked and C.dim or ecol(b.id))
    if hero.cd[b.id] > 0 then
        ARC(cx, cy, r - 2, -90, -90 + 360 * hero.cd[b.id] / sk.cd, C.white)
    end
    CTX(cx, cy - FONT_S // 2 - 1, locked and ("L" .. sk.unlock) or sk.label, FONT_S, locked and C.dim or C.white)
end

local function draw_hud()
    local orb_y = MARGIN + ORB_R + 3
    draw_orb(MARGIN + ORB_R + 3, orb_y, ORB_R, hero.hp / hero.maxhp, rgb(180, 20, 20), rgb(40, 8, 8), tostring(floor(hero.hp)))
    draw_orb(W - MARGIN - ORB_R - 3, orb_y, ORB_R, hero.mp / hero.maxmp, rgb(30, 60, 200), rgb(8, 10, 40), tostring(floor(hero.mp)))
    local bar_x = MARGIN * 2 + ORB_R * 2 + 6
    local bar_w = W - bar_x * 2
    R(bar_x, MARGIN, bar_w, 4, rgb(30, 24, 16))
    R(bar_x, MARGIN, bar_w * hero.xp / hero.xp_next, 4, C.xp)
    local line = string.format("Lv %d   Depth %d   %dg", hero.lvl, depth, hero.gold)
    CTX(W // 2, MARGIN + 7, line, FONT_S, C.text)
    if boss_ref and boss_ref.hp > 0 and dist(hero.x, hero.y, boss_ref.x, boss_ref.y) < K.LIGHT_RADIUS + 3 then
        local bw = min(W - bar_x * 2, W * 0.6)
        local by = MARGIN + 10 + FONT_S + 4
        CTX(W // 2, by, boss_ref.name, FONT_S, ecol(boss_ref.elem))
        R(W // 2 - bw // 2, by + FONT_S + 3, bw, 6, C.blood)
        R(W // 2 - bw // 2, by + FONT_S + 3, bw * boss_ref.hp / boss_ref.maxhp, 6, C.red)
    end
    local my = H - MARGIN - FONT_S - 2
    for i, msg in ipairs(messages) do
        local tw = measure(msg.text, FONT_S)
        local x = input_mode == "touch" and MARGIN or (W - tw) // 2
        TX(x + 1, my + 1, msg.text, FONT_S, C.black)
        TX(x, my, msg.text, FONT_S, msg.color)
        my = my - FONT_S - 4
        if i >= K.MSG_MAX then break end
    end
    if input_mode == "touch" then
        for _, b in ipairs(BUTTONS) do draw_skill_button(b) end
        if stick.id ~= nil then
            SC(stick.ox, stick.oy, STICK_R, rgb(200, 180, 140, 120))
            local vx, vy = stick_vector()
            CI(stick.ox + vx * STICK_R, stick.oy + vy * STICK_R, STICK_R * 0.4, rgb(200, 180, 140, 130))
        end
    end
    if hero.hurt_t > 0 then
        R(0, 0, W, 3, C.red); R(0, H - 3, W, 3, C.red); R(0, 0, 3, H, C.red); R(W - 3, 0, 3, H, C.red)
    end
end

local function draw_world()
    local flicker = sin(game_time * 9) * 0.12 + sin(game_time * 23) * 0.06
    local radius = K.LIGHT_RADIUS + flicker
    local sh = shake > 0 and floor((random() - 0.5) * TILE * 0.3) or 0
    cam_x = floor(hero.x * TILE - W / 2) + sh
    cam_y = floor(hero.y * TILE - H / 2) + sh
    draw_tiles(radius)
    draw_loot()
    for _, m in ipairs(monsters) do
        local lv = light_level(m.x, m.y, radius + 0.6)
        if lv > 0 or m.t.boss and dist(m.x, m.y, hero.x, hero.y) < radius + 2.5 then draw_monster(m, max(1, lv)) end
    end
    draw_hero()
    draw_projectiles()
    draw_effects()
end

local function draw_panel(title, title_col, lines, footer)
    local pw = min(W - MARGIN * 4, FONT_L * 12)
    local ph = FONT_L + 22 + #lines * (FONT_M + 6) + (footer and FONT_S + 16 or 0)
    local px, py = (W - pw) // 2, (H - ph) // 2
    RR(px, py, pw, ph, 8, C.panel)
    SRR(px, py, pw, ph, 8, C.panel_edge)
    SRR(px + 3, py + 3, pw - 6, ph - 6, 6, rgb(60, 44, 26))
    CTX(W // 2 + 2, py + 12 + 2, title, FONT_L, C.black)
    CTX(W // 2, py + 12, title, FONT_L, title_col)
    local y = py + FONT_L + 20
    for _, l in ipairs(lines) do
        CTX(W // 2, y, l[1], FONT_M, l[2] or C.text)
        y = y + FONT_M + 6
    end
    if footer and floor(game_time * 2) % 2 == 0 then CTX(W // 2, y + 6, footer, FONT_S, C.dim) end
end

local function action_word()
    return input_mode == "touch" and "TAP" or "PRESS"
end

local function draw_title()
    local cx, cy = W // 2, H // 2
    for i = 6, 1, -1 do
        CI(cx, cy + FONT_L, SHORT * 0.08 * i + sin(game_time * 7 + i) * 2, rgb(30 + i * 4, 14 + i * 2, 8, 255))
    end
    CI(cx, cy + FONT_L, SHORT * 0.06, rgb(255, 150, 50, 200))
    CI(cx, cy + FONT_L - 2, SHORT * 0.035 + sin(game_time * 11) * 2, rgb(255, 230, 140))
    draw_panel("ASHEN DEPTHS", C.red, {
        { "Fire > Frost > Storm > Fire", C.text },
        { "Best depth reached: " .. best_depth, C.gold },
    }, action_word() .. " TO DESCEND")
end

local function render()
    screen:begin({ clear = C.black })
    if state == "title" then
        draw_title()
    else
        draw_world()
        draw_hud()
        if state == "transition" then
            local alpha = floor(255 * max(0, 1 - state_t / 1.2))
            R(0, 0, W, H, rgb(0, 0, 0, alpha))
            CTX(W // 2, H // 2 - FONT_L // 2, "DEPTH " .. depth, FONT_L, rgb(214, 200, 170, alpha))
        elseif state == "dead" then
            R(0, 0, W, H, rgb(40, 0, 0, min(160, floor(state_t * 200))))
            draw_panel("YOU HAVE DIED", C.red, {
                { string.format("Depth %d  -  Level %d", depth, hero.lvl) },
                { string.format("%d kills  -  %d gold", hero.kills, hero.gold), C.gold },
            }, state_t > 1.5 and (action_word() .. " TO RISE AGAIN") or nil)
        end
    end
    screen:present()
end

-- ---------------------------------------------------------------------------
-- Main loop
-- ---------------------------------------------------------------------------
local function start_run()
    new_hero()
    messages = {}
    enter_depth(START_DEPTH)
    push_message("Find the stairs. Slay what stirs.", C.text)
end

local function step(dt)
    pressed, held, tapped = {}, {}, false
    if input_mode == "touch" then poll_touch() else poll_button() end
    state_t = state_t + dt
    game_time = game_time + dt

    if state == "title" then
        if tapped then start_run() end
        return
    elseif state == "dead" then
        update_fx(dt)
        if tapped and state_t > 1.5 then start_run() end
        return
    elseif state == "transition" then
        if state_t >= 0.6 then state = "play" end
    end

    local mx, my
    if input_mode == "touch" then
        mx, my = stick_vector()
        if pressed.attack or held.attack then hero_attack() end
        if pressed.fire then cast_skill("fire") end
        if pressed.frost then cast_skill("frost") end
        if pressed.storm then cast_skill("storm") end
        if pressed.potion then drink_potion() end
    else
        mx, my = auto_hunt(dt)
    end
    update_world(dt, mx, my)
    update_fx(dt)
end

local function cleanup()
    if button_handle then
        pcall(button.off, button_handle)
        pcall(button.close, button_handle)
        button_handle = nil
    end
    if snd then
        snd:close()
        snd = nil
    end
    pcall(screen.close, screen)
end

local run_ok, run_err = xpcall(function()
    if not init_input() then return end
    snd = sfx_mod.create({ sample_rate = SAMPLE_RATE, volume = SOUND_VOLUME, tag = TAG })
    math.randomseed(floor(sys.millis()) & 0x7fffffff)
    print(string.format("%s ready screen=%dx%d tile=%d input=%s", TAG, W, H, TILE, input_mode))
    local t0 = sys.millis()
    local last = t0
    while true do
        local now = sys.millis()
        if RUN_MS > 0 and now - t0 >= RUN_MS then break end
        local dt = min(K.DT_MAX, max(0, (now - last) / 1000))
        last = now
        snd:drain()
        step(dt)
        render()
        local spent = sys.millis() - now
        delay.delay_ms(max(1, floor(FRAME_MS - spent)))
    end
end, debug.traceback)

cleanup()
if not run_ok then print(TAG .. " ERROR: " .. tostring(run_err)) end
print(TAG .. " done")
