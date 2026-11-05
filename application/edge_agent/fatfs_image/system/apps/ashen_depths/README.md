# Ashen Depths

A dark, top-down action RPG (dungeon crawler) written in Lua for the `display` module. Descend through procedurally generated catacombs lit only by your torch, slay monsters, pick up loot of rising rarity and master three elements.

## Gameplay

- **Dungeons**: every depth is a new random layout of rooms and corridors. Tiles outside the torch radius fade into explored memory or stay black.
- **Elements**: Fire beats Frost, Frost beats Storm, Storm beats Fire. Each monster has an element, shown by its tint. A strong element deals 150% damage and the same element deals 60%.
- **Skills**: FIRE (fireball, explodes in an area) from level 1, ICE (frost nova, slows enemies) from level 3, and BOLT (chain lightning) from level 5.
- **Monsters**: Ash Imp, Bone Warrior, Rotting Husk and Bone Archer, plus blue-ringed Champions with more HP and damage. Every 5th depth is guarded by **The Ash Warden**, which fires rings of orbs and summons imps. Its seal keeps the stairs locked until it dies.
- **Loot**: gold, health potions (belt of 5), and weapons, armor and rings in Normal (white), Magic (blue), Rare (yellow) and Unique (orange) rarities. When you pick up an item, it is equipped if it scores better than your current gear; otherwise it is sold for gold.
- **Shrines**: purple shrines give a random blessing, such as +50% damage, a full heal, full mana, gold or a full belt.

## Controls

| Input | Action |
| --- | --- |
| Touch, left side | Virtual stick (move) |
| Big sword button | Attack (hold to keep swinging, auto-aims at nearby enemies) |
| FIRE / ICE / BOLT | Cast a skill (cooldown is shown as a ring) |
| Potion button (under the mana orb) | Drink a health potion |
| Button-only boards | The hero hunts automatically; a press casts the best ready skill |

## Arguments (`launcher.json` → `args`)

| Key | Default | Meaning |
| --- | --- | --- |
| `input_mode` | `touch` (or `button` when `pin` is set) | Force the input mode |
| `pin` / `button_gpio` | `0` | Button GPIO for button mode |
| `active_level` | `0` | Pressed level of the button |
| `frame_ms` | `33` | Target frame time |
| `run_ms` | `0` | Exit after this many ms (`0` = run until stopped) |
| `volume` / `sample_rate_hz` | `80` / `16000` | Optional sound effects |
| `start_depth` | `1` | Start at a given depth (handy for testing the boss on depth 5) |

## Files

- `scripts/ashen_depths.lua`: entry point, game loop, combat, rendering and input.
- `scripts/ashen_data.lua`: tuning constants, skills, monsters and loot tables.
- `scripts/ashen_items.lua`: item generation (rarity, affixes, names, score).
- `scripts/ashen_dungeon.lua`: room/corridor generation and BFS path fields used by monsters and the auto-hunt mode.
- `scripts/ashen_audio.lua`: optional synthesized sound effects.
