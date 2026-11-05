-- Ashen Depths: optional sound effects built from synthesized tones.
-- Every call is a no-op when the board has no audio output.

local floor, sin, pi = math.floor, math.sin, math.pi

local UAC_FLUSH_PCM_BYTES = 4000 -- min PCM bytes for a UAC host write
local TONE_AMPLITUDE = 14000

-- kind = { start_hz, end_hz, duration_ms }
local SFX_TONES = {
    swing = { 420, 260, 50 },
    hit = { 200, 110, 70 },
    cast = { 500, 900, 110 },
    hurt = { 150, 90, 120 },
    loot = { 900, 1400, 90 },
    level = { 520, 1300, 260 },
    death = { 220, 60, 600 },
    stairs = { 300, 150, 300 },
}

local function build_tone(rate, freq_from, freq_to, duration_ms)
    local frames = floor(rate * duration_ms / 1000)
    local chunks, phase = {}, 0
    for i = 1, frames do
        local t = i / frames
        phase = phase + 2 * pi * (freq_from + (freq_to - freq_from) * t) / rate
        local s = floor(sin(phase) * TONE_AMPLITUDE * (1 - t * 0.7))
        local u = s < 0 and (s + 65536) or s
        chunks[i] = string.char(u & 0xFF, (u >> 8) & 0xFF)
    end
    local pcm = table.concat(chunks)
    if #pcm < UAC_FLUSH_PCM_BYTES then pcm = pcm .. string.rep("\0", UAC_FLUSH_PCM_BYTES - #pcm) end
    return pcm
end

local Sfx = {}
Sfx.__index = Sfx

-- config = { sample_rate = number, volume = number, tag = string }
local function create(config)
    local self = setmetatable({ output = nil, cache = {}, pending = nil, tag = config.tag or "" }, Sfx)
    local audio_ok, audio = pcall(require, "audio")
    if not audio_ok then return self end
    local ok, out = pcall(audio.open_output, { sample_rate = config.sample_rate, volume = config.volume })
    if not ok or not out then
        print(self.tag .. " WARN: audio unavailable: " .. tostring(out))
        return self
    end
    self.output = out
    for kind, tone in pairs(SFX_TONES) do
        self.cache[kind] = build_tone(config.sample_rate, tone[1], tone[2], tone[3])
    end
    return self
end

function Sfx:play(kind)
    if self.output and self.cache[kind] then self.pending = self.cache[kind] end
end

function Sfx:drain()
    if not self.pending or not self.output then return end
    local ok, err = self.output:write(self.pending)
    if ok or (err and not tostring(err):find("busy", 1, true)) then self.pending = nil end
end

function Sfx:close()
    if self.output then
        pcall(self.output.close, self.output)
        self.output = nil
    end
end

return { create = create }
