-- Ashen Depths: item generation (rarity rolls, affixes, names, scoring).
local D = require("ashen_data")

local floor, max, min, random = math.floor, math.max, math.min, math.random
local ELEMENTS, RARITIES, ITEM_BASES = D.ELEMENTS, D.RARITIES, D.ITEM_BASES
local UNIQUE_NAMES, ELEMENT_PREFIX, SUFFIXES = D.UNIQUE_NAMES, D.ELEMENT_PREFIX, D.SUFFIXES

local function roll_rarity(bonus)
    bonus = bonus or 0
    local total, weights = 0, {}
    for i, r in ipairs(RARITIES) do
        weights[i] = r.weight * (i > 1 and (1 + bonus * 4) or 1)
        total = total + weights[i]
    end
    local roll = random() * total
    for i, r in ipairs(RARITIES) do
        roll = roll - weights[i]
        if roll <= 0 then return r end
    end
    return RARITIES[1]
end

local function item_score(it)
    if not it then return 0 end
    local s = (it.dmin + it.dmax) * 1.5 + it.armor * 2 + it.hp * 0.5 + it.mp * 0.4 + it.str * 2
    for _, e in ipairs(ELEMENTS) do s = s + (it.elem[e] or 0) * 0.6 end
    return s
end

local function make_item(slot, lvl, rarity_bonus, force_rarity)
    local rarity = force_rarity or roll_rarity(rarity_bonus)
    local bases = ITEM_BASES[slot]
    local tier = max(1, min(#bases, 1 + (lvl - 1) // 2 + random(-1, 1)))
    local it = { slot = slot, rarity = rarity.key, dmin = 0, dmax = 0, armor = 0, hp = 0, mp = 0, str = 0, elem = {} }
    local power = (2 + tier * 1.6 + lvl * 0.9) * rarity.mult
    if slot == "weapon" then
        it.dmin = floor(power * 0.8)
        it.dmax = floor(power * 1.35) + 1
    elseif slot == "armor" then
        it.armor = floor(power * 1.1)
    else
        it.mp = floor(power * 0.8)
    end
    local prefix, suffix
    for _ = 1, rarity.affixes do
        local roll = random(1, 5)
        if roll <= 2 then
            local e = ELEMENTS[random(1, #ELEMENTS)]
            it.elem[e] = (it.elem[e] or 0) + 10 + random(0, 8) + lvl * 2
            prefix = prefix or ELEMENT_PREFIX[e]
        elseif roll == 3 then
            it.hp = it.hp + floor(8 + lvl * 3 * rarity.mult)
            suffix = suffix or SUFFIXES.hp
        elseif roll == 4 then
            it.str = it.str + 2 + floor(lvl * 0.8)
            suffix = suffix or SUFFIXES.str
        else
            it.mp = it.mp + floor(6 + lvl * 2)
            it.armor = it.armor + floor(lvl * 0.8)
            suffix = suffix or SUFFIXES.mp
        end
    end
    local base = bases[tier]
    if rarity.key == "unique" then
        it.name = UNIQUE_NAMES[random(1, #UNIQUE_NAMES)]
    elseif rarity.key == "normal" then
        it.name = base
    else
        it.name = (prefix and (prefix .. " ") or "") .. base .. (suffix and (" " .. suffix) or "")
    end
    it.value = floor(item_score(it) * 1.6) + 1
    return it
end

return {
    roll_rarity = roll_rarity,
    score = item_score,
    make = make_item,
}
