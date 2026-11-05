-- Ashen Depths: dungeon layout generation and BFS path fields.
--
-- The map is a flat array indexed by idx(x, y) (0-based tile coordinates).
-- All functions mutate caller-owned tables in place so references stay valid.

local D = require("ashen_data")
local K = D.K

local floor, min, max, random = math.floor, math.min, math.max, math.random
local MAP_W, MAP_H = K.MAP_W, K.MAP_H
local T_WALL, T_FLOOR = K.T_WALL, K.T_FLOOR
local NEIGH = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }

local M = {}

function M.idx(x, y) return y * MAP_W + x + 1 end
local idx = M.idx

function M.tile_at(map, x, y)
    if x < 0 or y < 0 or x >= MAP_W or y >= MAP_H then return T_WALL end
    return map[idx(x, y)]
end
local tile_at = M.tile_at

function M.room_center(r) return r.x + r.w // 2, r.y + r.h // 2 end
local room_center = M.room_center

local function carve(map, x0, y0, x1, y1, t)
    for y = min(y0, y1), max(y0, y1) do
        for x = min(x0, x1), max(x0, x1) do map[idx(x, y)] = t end
    end
end

local function connect(map, r1, r2)
    local x1, y1 = room_center(r1)
    local x2, y2 = room_center(r2)
    if random() < 0.5 then
        carve(map, x1, y1, x2, y1, T_FLOOR); carve(map, x2, y1, x2, y2, T_FLOOR)
    else
        carve(map, x1, y1, x1, y2, T_FLOOR); carve(map, x1, y2, x2, y2, T_FLOOR)
    end
end

local function overlaps(r, rooms)
    for _, o in ipairs(rooms) do
        if r.x - 1 <= o.x + o.w and r.x + r.w + 1 >= o.x and r.y - 1 <= o.y + o.h and r.y + r.h + 1 >= o.y then
            return true
        end
    end
    return false
end

-- Fills `map` with rooms chained by L-shaped corridors and `rooms` with their rects.
function M.generate(map, rooms)
    for i = 1, MAP_W * MAP_H do map[i] = T_WALL end
    for i = #rooms, 1, -1 do rooms[i] = nil end
    for _ = 1, K.ROOM_TRIES do
        if #rooms >= K.ROOM_MAX then break end
        local w, h = random(K.ROOM_MIN_W, K.ROOM_MAX_W), random(K.ROOM_MIN_H, K.ROOM_MAX_H)
        local r = { x = random(1, MAP_W - w - 2), y = random(1, MAP_H - h - 2), w = w, h = h }
        if not overlaps(r, rooms) then
            carve(map, r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, T_FLOOR)
            if #rooms > 0 then connect(map, rooms[#rooms], r) end
            rooms[#rooms + 1] = r
        end
    end
    -- One extra corridor creates a loop so fights can be kited around.
    if #rooms >= 4 then connect(map, rooms[1], rooms[random(3, #rooms)]) end
end

function M.random_floor_in(map, r)
    for _ = 1, 30 do
        local x, y = random(r.x, r.x + r.w - 1), random(r.y, r.y + r.h - 1)
        if map[idx(x, y)] == T_FLOOR then return x + 0.5, y + 0.5 end
    end
    local cx, cy = room_center(r)
    return cx + 0.5, cy + 0.5
end

-- Breadth-first distance field from tile (ox, oy) over every walkable tile.
function M.build_field(map, field, queue, ox, oy)
    for i = 1, MAP_W * MAP_H do field[i] = nil end
    local origin = idx(ox, oy)
    local head, tail = 1, 1
    queue[1] = origin
    field[origin] = 0
    while head <= tail do
        local cur = queue[head]
        head = head + 1
        local d = field[cur] + 1
        local cx, cy = (cur - 1) % MAP_W, (cur - 1) // MAP_W
        for n = 1, 4 do
            local nx, ny = cx + NEIGH[n][1], cy + NEIGH[n][2]
            if tile_at(map, nx, ny) ~= T_WALL then
                local ni = idx(nx, ny)
                if field[ni] == nil then
                    field[ni] = d
                    tail = tail + 1
                    queue[tail] = ni
                end
            end
        end
    end
end

function M.field_at(field, x, y)
    return field[idx(floor(x), floor(y))]
end

-- Tile center one step closer to the field origin from world point (x, y).
function M.step_down(map, field, x, y)
    local tx, ty = floor(x), floor(y)
    local best = field[idx(tx, ty)]
    if not best then return nil end
    local bx, by = nil, nil
    for n = 1, 4 do
        local nx, ny = tx + NEIGH[n][1], ty + NEIGH[n][2]
        local d = tile_at(map, nx, ny) ~= T_WALL and field[idx(nx, ny)]
        if d and d < best then best, bx, by = d, nx, ny end
    end
    if bx then return bx + 0.5, by + 0.5 end
    return nil
end

-- First tile center on the path from the field origin toward world point (x, y).
function M.first_step_to(map, field, x, y)
    local tx, ty = floor(x), floor(y)
    local d = field[idx(tx, ty)]
    if not d then return nil end
    while d > 1 do
        local moved = false
        for n = 1, 4 do
            local nx, ny = tx + NEIGH[n][1], ty + NEIGH[n][2]
            local nd = tile_at(map, nx, ny) ~= T_WALL and field[idx(nx, ny)]
            if nd and nd == d - 1 then
                tx, ty, d, moved = nx, ny, nd, true
                break
            end
        end
        if not moved then return nil end
    end
    return tx + 0.5, ty + 0.5
end

return M
