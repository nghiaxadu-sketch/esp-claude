-- Ashen Depths: static game data (tuning constants, skills, monsters, loot tables).
local K = {}
K.DT_MAX = 0.08
K.MAP_W = 44
K.MAP_H = 44
K.ROOM_TRIES = 90
K.ROOM_MAX = 11
K.ROOM_MIN_W = 4
K.ROOM_MAX_W = 9
K.ROOM_MIN_H = 4
K.ROOM_MAX_H = 8
K.TILES_ON_SHORT_SIDE = 11
K.LIGHT_RADIUS = 5.6
K.LIGHT_LEVELS = 6
K.MEMORY_LEVEL = 1
K.AGGRO_STEPS = 12
K.ACTIVE_RANGE = 13
K.BOSS_AGGRO_STEPS = 18
K.BOSS_EVERY = 5
K.MONSTER_HP_SCALE = 0.42
K.MONSTER_DMG_SCALE = 0.3
K.MONSTER_XP_SCALE = 0.28
K.BELT_MAX = 5
K.PICKUP_RANGE = 0.7
K.AUTO_STUCK_TIME = 1.5
K.MSG_TTL = 3.2
K.MSG_MAX = 3

K.T_WALL = 0
K.T_FLOOR = 1
K.T_STAIRS = 2
K.T_SHRINE = 3
K.T_SHRINE_USED = 4

K.HERO_RADIUS = 0.3
K.HERO_SPEED = 3.4
K.ATTACK_CD = 0.42
K.ATTACK_RANGE = 1.05
K.ATTACK_ARC_COS = 0.35
K.AUTO_AIM_RANGE = 3.2
K.SKILL_AIM_RANGE = 7.0
K.HP_REGEN = 0.35
K.MP_REGEN = 1.4
K.CRIT_CHANCE = 0.10
K.CRIT_MULT = 2.0
K.WEAK_MULT = 1.5
K.RESIST_MULT = 0.6
K.POTION_HEAL = 0.45

local ELEMENTS = { "fire", "frost", "storm" }
local BEATS = { fire = "frost", frost = "storm", storm = "fire" }

local SKILLS = {
    fire = { label = "FIRE", cost = 6, cd = 0.7, unlock = 1, speed = 8.0, radius = 1.3, mult = 1.6 },
    frost = { label = "ICE", cost = 11, cd = 3.0, unlock = 3, radius = 2.6, mult = 1.1, slow = 3.0 },
    storm = { label = "BOLT", cost = 9, cd = 1.4, unlock = 5, range = 5.5, chain = 3, chain_range = 3.2, mult = 1.3 },
}

local MONSTERS = {
    imp = { name = "Ash Imp", hp = 15, dmg = 5, speed = 2.7, r = 0.27, xp = 7, range = 0.85, cd = 0.9 },
    skeleton = { name = "Bone Warrior", hp = 28, dmg = 8, speed = 1.9, r = 0.31, xp = 11, range = 0.95, cd = 1.1 },
    zombie = { name = "Rotting Husk", hp = 48, dmg = 12, speed = 1.15, r = 0.36, xp = 15, range = 1.0, cd = 1.4 },
    archer = { name = "Bone Archer", hp = 20, dmg = 7, speed = 1.7, r = 0.29, xp = 13, range = 5.0, cd = 1.7, ranged = true },
    boss = { name = "The Ash Warden", hp = 420, dmg = 17, speed = 1.55, r = 0.62, xp = 220, range = 1.35, cd = 1.25, boss = true },
}
local MONSTER_POOL = { "imp", "imp", "skeleton", "skeleton", "zombie", "archer" }
K.CHAMPION_CHANCE = 0.08

local RARITIES = {
    { key = "normal", mult = 1.0, affixes = 0, weight = 60 },
    { key = "magic", mult = 1.35, affixes = 1, weight = 28 },
    { key = "rare", mult = 1.75, affixes = 2, weight = 10 },
    { key = "unique", mult = 2.4, affixes = 3, weight = 2 },
}
local ITEM_BASES = {
    weapon = { "Short Sword", "Falchion", "War Axe", "Bastard Sword", "Runed Blade", "Doom Cleaver" },
    armor = { "Leather Jerkin", "Ring Mail", "Scale Mail", "Plate Coat", "Ashen Plate", "Warden Mail" },
    ring = { "Copper Band", "Bone Loop", "Silver Signet", "Ember Ring" },
}
local UNIQUE_NAMES = { "Emberheart", "Winter's Wake", "Stormcaller's Oath", "The Grave Lantern", "Ashbringer", "Hollow Crown" }
local ELEMENT_PREFIX = { fire = "Searing", frost = "Glacial", storm = "Thundering" }
local SUFFIXES = { hp = "of the Bear", mp = "of the Magus", str = "of Might", armor = "of Warding" }
local SLOT_ORDER = { "weapon", "armor", "ring" }

K.DROP_GOLD = 0.5
K.DROP_POTION = 0.11
K.DROP_ITEM = 0.13

return {
    K = K,
    ELEMENTS = ELEMENTS,
    BEATS = BEATS,
    SKILLS = SKILLS,
    MONSTERS = MONSTERS,
    MONSTER_POOL = MONSTER_POOL,
    RARITIES = RARITIES,
    ITEM_BASES = ITEM_BASES,
    UNIQUE_NAMES = UNIQUE_NAMES,
    ELEMENT_PREFIX = ELEMENT_PREFIX,
    SUFFIXES = SUFFIXES,
    SLOT_ORDER = SLOT_ORDER,
}
