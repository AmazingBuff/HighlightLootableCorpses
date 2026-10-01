# Rendering

Status: current

Canonical path: `docs/features/rendering.md`

Last verified against: 2026-09-28

## Purpose

The renderer draws the silhouette, outline, and icon modes for lootable corpses while leaving the game's native interface readable. Highlights are composed into the current interface target immediately before the original Skyrim interface draw.

## Scope and non-goals

In scope:

- Pre-UI composition through `MenuManager::DrawInterfaceStart` for flat Skyrim SE/AE.
- Explicit borrowed framebuffer RTV selection, full-target dimensions, and D3D11 pipeline-state restoration.
- Scan scheduling through the retained Present callback.

Out of scope:

- Skyrim VR, arbitrary subrect viewports, pixel readback, extra full-frame color textures, worker rendering, and HLSL changes.
- Runtime compatibility claims for particular Skyrim, ENB, or Community Shaders versions. The local implementation is source-backed; in-game validation remains pending on the remote game machine.

## Architecture

`Renderer::install` runs at SKSE `kPostLoad` and installs `UiRenderHook` with the source-backed `void(int64_t)` signature and `REL::RelocationID(79947, 82084)`. Community Shaders installs its later wrapper at `kPostPostLoad`, so the chain is Community Shaders preparation, this renderer, and then the original Skyrim interface draw. The hook calls the original exactly once.

The engine calls `DrawInterfaceStart` every rendered frame even when the menu system suppresses HUD content (inventory and other blocking menus, pause menus, loading screens), so the pre-UI callback consults `MenuVisibility` first: a main-thread `MenuOpenCloseEvent` sink re-evaluates the engine's UI state (`UI::IsShowingMenus`, `IsApplicationMenuOpen`, `GameIsPaused`, non-always-open menus on the stack) into an atomic verdict the render thread reads; MCP framework windows are checked live because they bypass the engine menu system. When the verdict is false the overlay skips the frame exactly like the native interface does.

At `kDataLoaded`, normal subsystem readiness is enabled and the optional Present callback is installed. Present only calls `OverlayDirector::on_present` to schedule the existing SKSE task scan; it never draws. The pre-UI callback reads `RendererData::renderTargets[RE::RENDER_TARGETS::kFRAMEBUFFER].RTV`, resolves that borrowed view to an `ID3D11Texture2D`, derives the active RTV mip dimensions, and calls the canonical `OverlayDirector::draw` path. The path uses a zero-origin full-target viewport and does not access a swapchain.

The draw modes use a single `D3D11StateCapture` owner. It captures the active RTV/UAV range, IA vertex slots 0 and 1 with their strides and offsets, shader resources and constant buffers used by the passes, and inherited GS/HS/DS stages. The owner clears those geometry stages for the overlay and restores them, including on an early return. On feature level 11.1 it preserves the 64 output UAV slots supported by the runtime; older contexts use the first eight slots used by the plugin.

Straight-color silhouette and icon output uses RGB `SRC_ALPHA`/`INV_SRC_ALPHA` and alpha `ONE`/`INV_SRC_ALPHA`. Premultiplied outline output uses `ONE`/`INV_SRC_ALPHA` for both RGB and alpha. The transparent interface target is then composited by Community Shaders after native UI has been drawn.

## Code map

| Repository-relative path | Stable symbol or entry point | Responsibility |
| --- | --- | --- |
| [`src/render/renderer.cpp`](../../src/render/renderer.cpp) | `OverlayDirector::on_pre_ui_draw`, `OverlayDirector::draw` | Select the current framebuffer RTV, gate frames, and draw all three modes through one canonical path. |
| [`src/render/ui_render_hook.h`](../../src/render/ui_render_hook.h) | `UiRenderHook::install` | Install the early Detours entry hook transactionally and keep the original call chain. |
| [`src/ui/menu_visibility.h`](../../src/ui/menu_visibility.h) | `MenuVisibility::overlay_allowed` | Mirror the engine's HUD-visibility decision (menu stack, pause and application counters, `menuSystemVisible`) into a cached atomic verdict refreshed by a main-thread `MenuOpenCloseEvent` sink. |
| [`src/render/present_hook.cpp`](../../src/render/present_hook.cpp) | `PresentHook::install` | Retain an idempotent scan scheduler without drawing or acquiring a swapchain target. |
| [`src/render/dx11/d3d11_util.cpp`](../../src/render/dx11/d3d11_util.cpp) | `D3D11StateCapture` | Own capture, temporary geometry-stage clearing, restoration, and COM release. |
| [`src/render/dx11/common_states.cpp`](../../src/render/dx11/common_states.cpp) | `CommonStates` | Create the shared D3D11 blend and pipeline states, including separate alpha factors for straight color. |
| [`src/render/mask/mask_overlay.cpp`](../../src/render/mask/mask_overlay.cpp) | `MaskOverlay::draw` | Render silhouette and outline modes into the borrowed output RTV. |
| [`src/render/icon/icon_overlay.cpp`](../../src/render/icon/icon_overlay.cpp) | `IconOverlay::draw` | Render icon mode into the borrowed output RTV. |
| [`tests/render_state_capture.cpp`](../../tests/render_state_capture.cpp) | `main` | Exercise WARP MRT/UAV, IA slot 1, inherited stages, and early-return restoration. |
| [`tests/pre_ui_composition.cpp`](../../tests/pre_ui_composition.cpp) | `main` | Exercise two Detours, CS-like RTV redirect, alpha composition, and native UI coverage. |

## Interfaces

- `UiRenderHook::Callback` receives the engine's `int64_t` argument and must not retain engine state.
- `OverlayDirector::draw` receives a borrowed `ID3D11RenderTargetView*` and explicit width/height for one callback scope. It owns no game RTV or swapchain resource.
- The flat SE/AE relocation is the supported entry target. No VR relocation is claimed.

## Invariants

- The pre-UI entry hook is attempted once at `kPostLoad`; failure disables highlighting and never falls back to Present drawing.
- The original interface function is called exactly once by the hook, and rendering occurs at most once for each nonzero engine frame.
- Present schedules scans even when the UI is hidden; it never issues overlay draw calls.
- RTV dimensions come from the current RTV resource and view subresource, not `RendererData::renderTargets[].texture` or a cached swapchain buffer.
- State capture releases every interface it receives and restores the active output bindings and modified shader stages before scope exit.
- The main full-frame viewport starts at `(0, 0)`; subrect and VR behavior are unsupported.

## Failure modes

| Failure | Expected handling | Verification |
| --- | --- | --- |
| Detours transaction or attach fails | Log one useful error, disable the pre-UI highlighter, and do not install a Present drawing fallback. | `UiRenderHook::install` and Release build. |
| Address Library has no mapping for the source-backed relocation | Treat the existing Address Library/runtime dependency as an installation error; no late hook or Present drawing fallback is claimed. | `UiRenderHook::install` source review and runtime validation on the game machine. |
| Framebuffer RTV is unavailable or not a Texture2D view | Skip that frame while leaving scan scheduling active. | `OverlayDirector::on_pre_ui_draw` source review. |
| RTV resource query or dimensions are invalid | Skip that frame without retaining the resource. | `target_dimensions` source review. |
| Lazy shader or D3D11 resource initialization fails | Skip rendering until the existing initialization path succeeds; do not alter scan behavior. | Release build and focused composition scenario. |
| Overlay exits early during a D3D11 pass | `D3D11StateCapture` destructor restores state and releases captured interfaces. | `render_state_capture_test.exe`. |

## Dependencies

- Skyrim SE/AE, SKSE, CommonLibSSE-NG, and D3D11 are existing runtime/build dependencies.
- Microsoft Detours is a new static build-only vcpkg dependency pinned by the repository's existing vcpkg baseline; no Detours DLL or Community Shaders runtime dependency is added.
- Community Shaders source revision [`2f2919a71bed6132b125e41781304c8f6f73d002`](https://github.com/doodlum/skyrim-community-shaders/tree/2f2919a71bed6132b125e41781304c8f6f73d002) supplied the integration facts through [`Hooks.cpp`](https://github.com/doodlum/skyrim-community-shaders/blob/2f2919a71bed6132b125e41781304c8f6f73d002/src/Hooks.cpp), [`XSEPlugin.cpp`](https://github.com/doodlum/skyrim-community-shaders/blob/2f2919a71bed6132b125e41781304c8f6f73d002/src/XSEPlugin.cpp), [`Upscaling.cpp`](https://github.com/doodlum/skyrim-community-shaders/blob/2f2919a71bed6132b125e41781304c8f6f73d002/src/Features/Upscaling.cpp), and [`HDRDisplay.cpp`](https://github.com/doodlum/skyrim-community-shaders/blob/2f2919a71bed6132b125e41781304c8f6f73d002/src/Features/HDRDisplay.cpp). Microsoft references include [`Detours usage`](https://github.com/microsoft/Detours/wiki/Using-Detours), [`OMSetRenderTargetsAndUnorderedAccessViews`](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargetsandunorderedaccessviews), [`OMGetRenderTargets`](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omgetrendertargets), and [`IAGetVertexBuffers`](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-iagetvertexbuffers). No upstream source is vendored or copied.

## Tests and verification

- `cmake --preset Release -B build/pre-ui -DBUILD_RENDER_TESTS=ON` — configure the fresh build tree with pinned vcpkg dependencies.
- `cmake --build build/pre-ui --config Release --target render_state_capture_test pre_ui_composition_test HighlightLootableCorpses` — build both focused scenarios and the plugin.
- `build/pre-ui/Release/render_state_capture_test.exe` — WARP state restoration scenario; expected output is `render_state_capture: PASS`.
- `build/pre-ui/Release/pre_ui_composition_test.exe` — two-Detours and pixel composition scenario; expected exit code is 0.
- `git diff --check` and the repository link/symbol checks — verify the logical diff and navigation.

## Safe modification guidance

1. Keep target acquisition in `OverlayDirector::on_pre_ui_draw` and pass only a borrowed RTV plus explicit dimensions to `draw`.
2. Preserve the `kPostLoad` hook ordering, once-per-frame gate, original-call chain, transparent-target blend semantics, and RAII state owner.
3. Re-run the two focused scenarios and the Release plugin build after changes to hook ordering, D3D11 bindings, or blend states.

## Synchronized files

| Path | Why it must stay synchronized |
| --- | --- |
| `src/render/renderer.cpp`, `src/render/ui_render_hook.*`, `src/render/present_hook.*`, `src/main.cpp` | Lifecycle ordering, hook chain, readiness, and scan scheduling are one integration boundary. |
| `src/render/dx11/d3d11_util.*`, `src/render/dx11/common_states.*`, `src/render/mask/*`, `src/render/icon/*` | State ownership, target lifetime, and alpha semantics must match every drawing pass. |
| `CMakeLists.txt`, `vcpkg.json` | Detours include/library discovery and the pinned static dependency must remain aligned. |
| `tests/render_state_capture.cpp`, `tests/pre_ui_composition.cpp` | Focused tests exercise the state and composition invariants documented here. |
| `README.md`, `README_CN.md`, `CHANGELOG.md` | User-facing behavior, build dependency, and validation limits must stay truthful. |

## Related history

- The renderer previously drew from the Present callback using a cached swapchain RTV. The pre-UI path keeps scan scheduling there and moves drawing to the source-backed interface boundary so native HUD and menu content is drawn afterward.
