# Highlight Lootable Corpses

[English](README.md) | [简体中文](README_CN.md)

A corpse-detection overlay for **Skyrim Special Edition / Anniversary Edition** (SKSE plugin). It highlights every **lootable corpse** near you — dead NPCs and creatures, ash piles left behind by reanimated enemies, and static corpse containers — so you never lose sight of a kill behind tall grass, bushes, rocks, or terrain again. Corpses disappear from the overlay as soon as they no longer contain anything you can take.

## Features

- **Lootable-only highlighting** — corpses whose inventory still holds takeable items are highlighted; fully looted corpses drop out of the overlay
- **See through everything** — highlights are drawn after the scene renders and ignore scene depth, so grass, bushes, walls, and hills never hide a corpse
- **Three display modes** — `silhouette` (filled mask of the nearest highlighted surface per pixel), `outline` (bright core plus outward colored halo), and `icon` (downward arrows above corpse bounds; nearby crowded targets share a larger double arrow)
- **Dead-on placement** — positions come from ragdoll bodies and Havok collision bounds, with largest-cluster handling after dismemberment and a geometry fallback
- **Ash pile support** — ash piles from reanimated / disintegrated enemies (including Dawnguard / Dragonborn variants such as Soul Embers and Ash Spawn) are checked through the original actor they point to
- **Static corpse support** — container-type corpses such as draugr ambush / wrapped corpses, burnt corpses, ghost corpses, spider-web corpses, frozen mammoths, and more, including DLC variants
- **Optional loot filter** — show only corpses whose inventory contains quest items, keys, enchanted gear, high-value items (worth at least `HighValueThreshold` gold), books (spell / skill / unread flags), or consumables (arrows, ingredients, potions, scrolls, soul gems)
- **Hide-searched persistence** — optionally stop highlighting corpses you have searched at least once, even if you took nothing; the marks persist per save game in the SKSE co-save
- **Hotkey** — toggle the overlay on/off or trigger a timed pulse highlight with a single key (unbound by default; rebind via the `Hotkey` INI key)
- **In-game settings panel** — every option can be adjusted live in the Mod Control Panel (requires [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352)); changes take effect immediately
- **QuickLoot IE support** — with the [QuickLoot IE](https://www.nexusmods.com/skyrimspecialedition/mods/120075) fork installed, opening its loot menu counts as searching that corpse (optional)
- **Lightweight** — a throttled background scan plus minimal per-frame work; negligible frame-time impact

## Requirements

- [Skyrim Special Edition / AE](https://store.steampowered.com/app/489830/) **1.6.629 or newer** (1.6.1170 and 1.7.99 verified). SE 1.5.97 and Skyrim VR are supported theoretically.
- [SKSE64](https://skse.silverlock.org/) matching your game version
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (All in one)
- Optional: [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) — enables the in-game settings panel. Without it the mod works normally and is configured through the INI file.
- Optional: [QuickLoot IE](https://www.nexusmods.com/skyrimspecialedition/mods/120075) — opening its loot menu counts as searching that corpse for the hide-searched feature.

## Installation

1. Install SKSE64 and Address Library (links above).
2. Install this mod with your mod manager (MO2 / Vortex), or manually copy `HighlightLootableCorpses.dll` into `Data\SKSE\Plugins\`.
3. Launch the game. A default configuration file is created automatically at `Data\SKSE\Plugins\HighlightLootableCorpses.ini` on first run.

Log file (for troubleshooting):

```text
Documents\My Games\Skyrim Special Edition\SKSE\HighlightLootableCorpses.log
```

## Usage

- By default the overlay is **off** (`Enabled=false`) and no hotkey is bound (`Hotkey=0`). Enable it in the settings panel, or set `Enabled=true` / a `Hotkey` key code in the INI.
- Once enabled, the plugin scans at `ScanIntervalMs` (default 100 ms) and highlights every lootable corpse within `MaxDistance` (default 500 game units).
- To toggle or pulse the overlay with a key, set the `Hotkey` key in the INI to a virtual-key code (0 = unbound):
  - `HotkeyMode=0` (constant): pressing the key turns the overlay on/off.
  - `HotkeyMode=1` (pulse): pressing the key highlights unsearched corpses for `PulseDurationMs`, then they fade out.
- Take everything from a corpse and its highlight vanishes — including ash piles and static corpse containers.
- Optionally hide corpses you have searched at least once — even if you took nothing, they stay hidden across sessions: the marks persist per save game in the SKSE co-save and are cleaned up automatically (`HideSearchedEnabled` in the INI, or the "Hide Searched Corpses" checkbox in the panel). Works with vanilla activation and QuickLoot IE.
- Open **Mod Control Panel → Highlight Lootable Corpses → Settings** to tune everything live (color, distance, fade, loot filter, scan interval). Changes apply immediately; use the *Save* button to write them to the INI. The hotkey is suspended while the panel is open.

## Configuration

All options live in `Data\SKSE\Plugins\HighlightLootableCorpses.ini`. Every option is also editable in the in-game panel. The INI is automatically rewritten whenever you save the game, and the panel's *Save* button writes it on demand. Values below are the first-run defaults.

### [General]

| Key | Default | Range | Description |
| --- | --- | --- | --- |
| `Enabled` | `false` | — | Whether the mod is active. |
| `Hotkey` | `0` | 0–0xFE | Virtual-key code for the hotkey; 0 = unbound. |
| `HotkeyMode` | `0` | 0–1 | 0 = constant (toggle on/off), 1 = pulse (timed highlight). |
| `PulseDurationMs` | `500` | 500–30000 | Pulse mode: highlight lifetime in milliseconds before fully fading out. |
| `ScanIntervalMs` | `100` | 100–1000 | Corpse scan interval in milliseconds. |

### [Display]

| Key | Default | Range | Description |
| --- | --- | --- | --- |
| `DisplayMode` | `0` | 0–2 | 0 = silhouette (filled mask), 1 = outline (bright core plus glow), 2 = icon (distance-scaled arrows above corpses; nearby crowded targets share a double arrow). |
| `OutlineThickness` | `1` | 1–20 | Outline glow size; larger values widen the bright rim and outer halo. |
| `IconRadius` | `5` | 5–20 | Icon base half-width in pixels. |
| `OutlineColor` | `00FF66` | ARGB hex | Highlight color. |
| `MinOpacity` | `0.0` | 0–1 | Minimum opacity at max distance. |
| `MaxDistance` | `500` | 500–5000 | Search radius in game units (~7 m default; 70 units ≈ 1 m). |
| `FadeStartDistance` | `0` | 0–MaxDistance | Distance where fading begins (fully opaque below). |
| `FadePower` | `0.1` | 0.1–4 | Fade curve exponent (higher = faster fade). |

Out-of-range values are clamped when loaded: an invalid `DisplayMode` (> 2) becomes outline, an invalid `HotkeyMode` (> 1) becomes constant, and an out-of-range `Hotkey` (> 0xFE) becomes unbound.

### [LootFilter]

| Key | Default | Range | Description |
| --- | --- | --- | --- |
| `HideSearchedEnabled` | `false` | — | Stop highlighting corpses the player has searched (activated) at least once, even if nothing was taken. |
| `ValueFilterEnabled` | `false` | — | Only highlight corpses matching the categories below. |
| `ValueQuestItems` | `false` | — | Quest items. |
| `ValueKeys` | `false` | — | Keys. |
| `ValueEnchanted` | `false` | — | Enchanted equipment. |
| `ValueHighValue` | `false` | — | Any single item worth >= `HighValueThreshold` gold. |
| `HighValueThreshold` | `0` | 0–500 | High-value threshold in gold (gold piles count by amount). |
| `BookFilterMode` | `0` | 0–0xFF | Bit flag: 1 = spell books, 2 = skill books, 4 = unread books, 7 = all. |
| `ValueConsumables` | `false` | — | Arrows, ingredients, potions, scrolls, soul gems. |

Icon distance scaling: icons shrink smoothly from 1.25x nearby to 0.75x far away (smoothstep); clustered corpses share a 1.2x larger double arrow, capped at 1.5x overall.

## Performance

- The scan runs as a task on the game's main thread at `ScanIntervalMs` and is **detection-only** (loot filtering, bounds, distance) — it no longer touches the scene graph for geometry.
- Mask-geometry collection happens on the render thread behind a form-id LRU cache (32 corpses, matching the corpse cap): the first time a corpse appears in view its geometry is collected on that very frame, so a fast camera turn highlights a newly visible corpse immediately instead of waiting up to one scan interval; steady-state frames reuse the cached geometry with no scene-graph traversal. Cached geometry is refreshed when the cache evicts it (recency follows the drawn corpses).
- Per-frame cost is minimal: the render pass culls off-screen targets by frustum, refreshes colors, and compacts the draw list from the cache.
- Work per corpse is bounded: at most 32 corpses are tracked and at most 256 render geometries are drawn per frame.
- Relative cost per mode: **icon < silhouette < outline** — icon mode is the cheapest, silhouette mode is in the middle, outline mode is the most expensive (it computes a glow halo per target).

## Compatibility

- The loot check reads the same merged inventory the container UI uses (base container + runtime changes), so a corpse is highlighted exactly when something is still takeable. Script-added and player-dropped items in a corpse count as loot, same as vanilla.
- No gameplay records are modified; the overlay is pure rendering. Safe to add or remove on an existing save.
- Hide-searched marks live in the SKSE co-save and are keyed by the plugin's own record id; stale form ids are dropped when a save loads. Adding or removing this mod cannot corrupt a save.
- Should be compatible with any loot or ESP mod; load order does not matter.

## Troubleshooting

- **Nothing shows up** — the overlay starts disabled (`Enabled=false`); enable it in the panel or the INI. Check that corpses are actually within `MaxDistance` and still contain loot, and that Address Library is installed.
- **No in-game panel** — SKSE Menu Framework is not installed; the rest of the mod still works. Configure via the INI at `Data\SKSE\Plugins\HighlightLootableCorpses.ini`.
- **Weird colors on HDR displays** — the game's HDR back buffer has only 2-bit alpha, so half-transparent highlights are quantized; lower `MinOpacity` for a subtler look.
- Check the log at `Documents\My Games\Skyrim Special Edition\SKSE\HighlightLootableCorpses.log` — it records detection decisions and any setup problems.

## Building from source

Requirements: Visual Studio 2022, CMake, and [vcpkg](https://vcpkg.io/) (set the `VCPKG_ROOT` environment variable). CommonLibSSE-NG is consumed as a git submodule (`extern/CommonLibSSE`), so initialize submodules first.

```text
git submodule update --init --recursive
cmake --preset Release
cmake --build build --config Release
```

The plugin DLL is written to:

```text
build/Release/HighlightLootableCorpses.dll
```

Builds target Skyrim SE and AE (VR support is compiled out). Dependencies (via vcpkg): fmt, spdlog, SimpleIni, DirectXMath, DirectXTK.

## License

Distributed under the [GPL-3.0](LICENSE) license.

## Credits

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) by alandtse & contributors
- [SKSE](https://skse.silverlock.org/) by the SKSE team
- [SKSE Menu Framework](https://github.com/QTR-Modding/SKSE-Menu-Framework-3) & [SKSE-MCP](https://github.com/QTR-Modding/SKSE-MCP) by QTR-Modding
