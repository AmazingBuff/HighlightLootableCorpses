# Changelog

## [Unreleased]

### Changed

- Collect corpse highlight geometry on the render thread behind a form-id LRU cache (32 corpses): the first appearance of a corpse in view collects its geometry on that frame (no scan-interval delay after fast camera turns), and steady-state frames reuse the cached geometry without re-traversing the scene graph. A corpse's cached geometry is invalidated as soon as its equipment changes (taken or added by any mod) or its 3D is rebuilt (e.g. on save load), so the highlight always matches the current body composition.

- Restrict outline glow geometry and Gaussian passes to conservative projected target regions, clear the integer mask once per outline frame, and fall back to the full viewport when bounds are unavailable or unsafe.

- Replace hard outline bands with bright narrow cores and diffuse outward Gaussian halos while retaining independent target colors, fade and pulse alpha.

- Change icon markers to bordered downward arrows above corpse bounds, with bounded distance scaling and the existing opacity fade; retain the `IconRadius` key as base half-width and label its menu control "Icon Size".

- Change silhouettes to show the nearest highlighted surface per pixel, including intersecting targets, while preserving wall penetration.
- Add per-target highlight colors while preserving corpse color, pulse and distance fade settings.

- **BREAKING:** Rename the plugin to `HighlightLootableCorpses`. The DLL, INI, and log files are renamed. Migration: rename your `CorpseESP.ini` to `HighlightLootableCorpses.ini` to keep your settings, and replace the old DLL.
- The toggle hotkey is now unbound by default. Bind one via the new "Hotkey" button in the MCP menu — all keys are bindable (keyboard and mouse), including ESC/F1 (pressing them may close the panel, but the bind still applies; click the button again or wait 5s to cancel). Bind it via the `Hotkey` INI key alternatively. Hotkey toggles stay in sync with the menu's Enabled checkbox and are suspended while the menu is open.
- The INI is now written with a comment above each option (same style as the configuration reference in the README) and is saved automatically whenever the player makes a save game after changing settings in the MCP menu — pressing "Save to INI" is no longer required (that button still saves immediately).
- Loot filter category switches now default to **false** (opt-in per category), and their controls are greyed out in the MCP menu while the loot filter is off.
- **BREAKING:** Replace the corpse box wireframe with a `DisplayMode` setting (`silhouette`, `outline`, or `icon`) in the Display section: `silhouette` fills the wall-penetrating mesh silhouette in `OutlineColor`, `outline` draws a band just outside it, and `icon` shows a downward arrow above the corpse, with nearby crowded targets sharing a double arrow. The box wireframe is removed. All three modes fade with distance according to `MinOpacity`, `FadeStartDistance`, and `FadePower`.

### Added

- Add stable icon grouping for corpses close in both world and screen space, shown as larger double arrows with height and group-extent limits; select the nearest 16 groups after grouping all candidates.

- Add an optional "hide searched corpses" mode (`HideSearchedEnabled`, off by default): once the player searches a corpse, it stops being outlined — even if nothing was taken. Searching is always recorded (so corpses searched before enabling the option are hidden too once it is turned on). QuickLoot IE users are covered via its public API (opening the loot menu marks the corpse as searched); QuickLoot IE is an optional dependency and its absence falls back to vanilla activation events only.
- Add an experimental mesh outline mask renderer: corpse meshes — fully GPU-skinned via per-partition bone matrices — are rendered into an offscreen integer-ID mask (private depth for silhouettes, independent masks for outlines; both ignore scene depth) whenever corpses are in range, and a translucent on-screen overlay shows the silhouette for verification.
- Add the edge-detection outline pass consuming the mask: a full-screen pass dilates the mask by the configured `OutlineThickness` and draws an `OutlineColor` band just outside each corpse silhouette, penetrating walls; the silhouette interior is left to the verification overlay.
- Searched-corpses marks persist per save game via the SKSE co-save (record `HLCS`): form IDs are re-resolved on load (stale marks are dropped), marks are removed when the engine deletes a form, and loading a save without marks (or removing the plugin) leaves saves fully intact.

### Removed

- Remove the model-silhouette outline mode (`OutlineMode`) to eliminate its heavy performance cost.
- Remove the `SoulGemFilledOnly` loot filter option; soul gems always count as consumables.
- Remove the redundant `ShowOutline` option; the plugin toggle (`Enabled`) already covers it.

### Fixed

- Fix head parts (and other FaceGen-family meshes) missing from silhouettes and outlines. Their partition buffers carry no `VF_VERTEX` position attribute: the vertex data holds only UV/normal/tangent/color and the skin block, while the engine keeps the model-space positions in `BSDynamicTriShape::dynamicData` as a per-original-vertex float4 array (and keeps no source vertex buffer for skinned geometry at all). The skinned path's position calibration assumed a position attribute existed, so such meshes were either skipped ("no matching candidate") or drawn with garbage positions. Positionless partitions are now drawn from the partition buffers with the position stream rebuilt per frame from `dynamicData` into a scratch vertex buffer: the partition's index space is detected from its own index list, and the stream is filled identity-mapped for whole-mesh indexing or remapped through the partition's `vertexMap` for packed subsets, keeping every index the partition can reference covered; every mesh is validated individually and unusable ones are skipped with a logged reason.
- Fix icon markers appearing where no corpse is and moving illogically as the view rotates. Three causes: corpses whose world bounds could not be measured kept the default (0, 0, 0) bounding corners, so their markers projected at the world origin (the marker corners now fall back to the corpse anchor); points behind the camera could pass the projection checks and mirror onto the screen as ghost markers (both projection paths now reject them); and the icon grouping hysteresis read from a map that was rebuilt empty every frame, so grouping re-derived from scratch and markers jumped whenever a camera move changed screen-space distances (the previous frame's assignment is now persisted across frames).
- Preserve each target's outline when another highlighted target covers its silhouette, and merge crossing outlines with alpha blending.
- Prevent target style IDs above 255 from aliasing earlier entries; the existing geometry draw budget remains unchanged.

- Fix the QuickLoot IE compatibility layer never activating: SKSE loads plugins one by one in (alphabetical) scan order and `HighlightLootableCorpses` loads before `QuickLootIE`, so probing for its DLL inside `SKSEPlugin_Load` always failed. Detection now happens on the `kPostLoad` message (after every plugin's `SKSEPlugin_Load` has returned) and requires only API v20, matching the one call actually used.
- Support both QuickLoot IE API generations: 4.x (`GetQuickLootInterfaceV20` C export, vendored official header) and 3.x (PluginRequests over the SKSE messaging interface, vendored official header with the namespace renamed to avoid clashing with 4.x). Detection tries the 4.x export first and falls back to the 3.x handshake, so the searched-corpses marking works with either QuickLoot IE version.
- Searched-corpses marks now record only corpse-like references (dead actors, ash piles, static corpse containers — the same candidates the scan outlines). Previously every player-activated door, lever, or chest entered the set and grew the co-save record indefinitely.
- Fix box/loot state mismatches on leveled-list corpses (e.g. draugr weapons inherited via NPC templates): lootability is now judged from the same inventory view the loot menu uses — engine-initialized inventory changes merged with base-container entries and dropped-item lists, skipping unresolved leveled-list placeholder entries (QuickLoot IE approach).
- Fix boxes persisting on looted-empty corpses by ignoring resolved leveled-list placeholder entries in the searchable check.
- Stop drawing boxes on corpses whose loot has been fully looted by ignoring non-playable items in the searchable check.
- Prevent per-scan corpse diagnostics from flooding the log with unchanged entries.
- Fix the experimental outline mask renderer treating effect-shader attachment meshes (glow sprites, frost/trail art) on corpse 3D as regular static geometry — a mesh whose vertices are not ordinary model-space surface data could cover the whole screen in the debug overlay. Effect-shader geometry is now excluded, and every static draw must pass world-sphere and target-proximity checks before it is rendered (skipped draws are logged once with details). Static draws now derive the position vertex format by validating decoded positions against the engine's own model bound for each mesh (full-precision float32 positions are no longer misread as half precision), and meshes whose layout cannot be validated are skipped instead of being drawn as screen-covering garbage.
- Fix the experimental outline mask renderer's skinned path. Skinned shapes were rejected before their per-partition path was reached; the path then required an engine model bound and a bone-matrix count that engine-managed skinned meshes do not provide; layout selection was cached per vertex descriptor so meshes sharing one were drawn on another mesh's evidence; and the bone palette was built in partition-local index space while the vertex bone indices are indices into the skin's bone array, so most vertices sampled the wrong matrix and collapsed. Skinned shapes now reach their own path, are no longer gated on those values, calibrate their position format from the descriptor's attribute offsets and their bone-weight/index layout and stride from each mesh's own vertex data, validate every mesh individually, and build the bone palette in the skin's global bone index space (padded to the full upload size with a valid matrix). Non-skinned geometry attached to a corpse that has skinned meshes (such as icicles) is no longer included in the silhouette. Layouts that cannot be validated are skipped and every skip reason is logged.
