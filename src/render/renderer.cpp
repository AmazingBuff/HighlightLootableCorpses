//
// Created by AmazingBuff on 2026/9/13.
//

#include "renderer.h"

#include "present_hook.h"
#include "render_util.h"

#include "config/config.h"
#include "icon/icon_layout.h"
#include "icon/icon_overlay.h"
#include "mask/mask_overlay.h"
#include "render/dx11/common_states.h"
#include "render/geometry/render_geometry_cache.h"
#include "render/shader_manager.h"
#include "search/corpse_finder.h"
#include "ui/pulse_timer.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr float Hold_Fraction = 0.10f;

    // Capacity cap of the equip-invalidation inbox: posts beyond it are dropped. A dropped
    // invalidation self-heals - the stale entry still falls to the root check or eviction - so
    // the main-thread sink can never make the render thread allocate unboundedly.
    constexpr size_t Max_Equip_Inbox = 64;

    float pulse_alpha(float progress)
    {
        return progress <= Hold_Fraction ? 1.0f : std::clamp(1.0f - (progress - Hold_Fraction) / (1.0f - Hold_Fraction), 0.0f, 1.0f);
    }

    float corpse_alpha(Config const& cfg, float distance, float alpha)
    {
        float const fade_range = std::max(cfg.max_distance - cfg.fade_start_distance, 1.0f);
        float const f = distance <= cfg.fade_start_distance ? 1.0f : 1.0f - std::clamp((distance - cfg.fade_start_distance) / fade_range, 0.0f, 1.0f);
        float const fade = std::pow(f, cfg.fade_power);
        return std::clamp(cfg.min_opacity + fade * alpha * (1.0f - cfg.min_opacity), cfg.min_opacity, 1.0f);
    }

    RE::BSGraphics::ViewData const* update_view_data(RE::NiCamera const* camera)
    {
        RE::BSGraphics::ViewData const* view_data = nullptr;
        if (RE::BSGraphics::State* state = RE::BSGraphics::State::GetSingleton())
        {
            RE::BSGraphics::State::RUNTIME_DATA& state_rt = state->GetRuntimeData();
            for (RE::BSGraphics::CameraStateData const& cam_data : state_rt.cameraDataCacheA)
            {
                if (cam_data.referenceCamera == camera)
                {
                    view_data = std::addressof(cam_data.GetCameraStateRuntimeData().camViewData);
                    break;
                }
            }
            if (!view_data && !state_rt.cameraDataCacheA.empty())
                view_data = std::addressof(state_rt.cameraDataCacheA.front().GetCameraStateRuntimeData().camViewData);
        }
        return view_data;
    }

    bool project_icon_tip(RE::NiCamera* camera, RE::BSGraphics::ViewData const* view_data,
        DirectX::XMFLOAT3 const& point, float width, float height, DirectX::XMFLOAT2& tip)
    {
        float px = 0.0f, py = 0.0f, depth = 0.0f;
        if (project(camera, point, width, height, px, py, depth))
            return Icon::icon_screen_tip(px, py, tip);
        if (!view_data)
            return false;
        Matrix const& matrix = view_data->viewProjMatrixUnjittered._11 != 0.0f ? view_data->viewProjMatrixUnjittered : view_data->viewProjMat;
        DirectX::XMFLOAT4 clip{};
        DirectX::XMStoreFloat4(&clip, DirectX::XMVector4Transform(DirectX::XMVectorSet(point.x, point.y, point.z, 1.0f), matrix));
        return Icon::icon_clip_tip(clip, width, height, tip);
    }

    class OverlayDirector
    {
    public:
        static OverlayDirector& instance()
        {
            static OverlayDirector s_instance;
            return s_instance;
        }

        // Main thread (equip sink): queue a corpse whose equipment changed. The sink performs no
        // cache access - the cache is render-thread-only - this inbox is the ONLY shared state
        // between the threads. Posts past the cap are dropped (see Max_Equip_Inbox).
        void queue_invalidation(RE::FormID form_id)
        {
            std::lock_guard<std::mutex> const lock(m_invalidations_mutex);
            if (m_invalidations.size() < Max_Equip_Inbox)
                m_invalidations.push_back(form_id);
        }

        void on_present(REX::W32::IDXGISwapChain* swap_chain)
        {
            std::lock_guard<std::mutex> const draw_lock(m_draw_mutex);

            m_icon_overlay.end_frame();
            schedule_scan();

            RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
            if (!renderer)
                return;

            RE::BSGraphics::RendererData& rt = renderer->GetRuntimeData();
            REX::W32::ID3D11Device* device = rt.forwarder;
            REX::W32::ID3D11DeviceContext* context = rt.context;
            if (!device || !context)
                return;

            RE::BSGraphics::State* bs_state = RE::BSGraphics::State::GetSingleton();
            uint32_t const frame = bs_state ? bs_state->GetFrameCount() : 0;

            bool const skip_draw = (frame != 0) && (frame == m_last_drawn_frame);
            if (!skip_draw)
            {
                if (frame != 0)
                    m_last_drawn_frame = frame;

                draw(swap_chain, device, context);
            }
        }

    private:
        OverlayDirector() :
            m_scan_in_flight(false),
            m_last_drawn_frame(std::numeric_limits<uint32_t>::max()),
            m_back_buffer(nullptr),
            m_render_target(nullptr),
            m_ready(false) {}

        void schedule_scan()
        {
            std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
            if (now - m_last_scan >= std::chrono::milliseconds(Setting::instance().get_config().scan_interval_ms) &&
                !m_scan_in_flight.exchange(true)) {
                m_last_scan = now;
                SKSE::GetTaskInterface()->AddTask([this]
                {
                    // search() always runs: the corpse list has consumers besides the overlay
                    // (the MCP menu reads the snapshot even with rendering disabled). It is
                    // detection-only now - loot filtering, bounds and distance - and no longer
                    // touches the scene graph for geometry. Mask-geometry collection happens on
                    // the render thread inside draw(), behind the form-id LRU cache: a cache
                    // miss collects that corpse's geometries on the spot, so a corpse that
                    // becomes visible after a camera turn highlights the same frame instead of
                    // waiting up to one scan interval.
                    CorpseScan::instance().search();

                    m_scan_in_flight.store(false);
                });
            }
        }

        // Render thread (head of the mask branch): swap out the queued invalidations in one
        // short critical section. Ids that are not cache keys are simply dropped by the caller -
        // erase of a missing key is a no-op, so no pre-filtering is needed and the cache is the
        // only judge of its own key set.
        std::vector<RE::FormID> drain_invalidations()
        {
            std::vector<RE::FormID> drained;
            std::lock_guard<std::mutex> const lock(m_invalidations_mutex);
            drained.swap(m_invalidations);
            return drained;
        }

        void draw(REX::W32::IDXGISwapChain* swap_chain, REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context)
        {
            if (!init(swap_chain, device) || !update_back_buffer(swap_chain, device))
                return;

            REX::W32::D3D11_TEXTURE2D_DESC desc{};
            m_back_buffer->GetDesc(&desc);

            uint32_t w = desc.width;
            uint32_t h = desc.height;
            if (w <= 0 || h <= 0)
            {
                RE::BSGraphics::ScreenSize const screen = RE::BSGraphics::Renderer::GetScreenSize();
                w = screen.width;
                h = screen.height;
            }
            if (w <= 0 || h <= 0)
                return;

            std::vector<CorpseScan::CorpseInfo> corpses = CorpseScan::instance().snapshot();
            if (corpses.empty())
                return;

            Config const& cfg = Setting::instance().get_config();
            bool const pulse_mode = cfg.hotkey_mode == Config::HotkeyMode::e_pulse;
            bool const pulse_active = pulse_mode && cfg.enabled && PulseTimer::instance().active();
            if (pulse_mode && !pulse_active)
                return;

            if (pulse_active || cfg.enabled)
            {
                float const pulse = pulse_active ? pulse_alpha(PulseTimer::instance().progress()) : 1.0f;
                Color color;
                color.decode(cfg.outline_color);
                RE::NiCamera* camera = RE::Main::WorldRootCamera();

                if (pulse > 0.f) {
                    if (cfg.display_mode == Config::DisplayMode::e_icon)
                    {

                        RE::BSGraphics::ViewData const* view_data = update_view_data(camera);

                        m_icon_overlay.begin_frame(w, h);

                        std::vector<Icon::IconCandidate> candidates;
                        candidates.reserve(corpses.size());
                        for (CorpseScan::CorpseInfo const& corpse : corpses)
                        {
                            DirectX::XMFLOAT3 const top = Icon::icon_anchor(render_cast(corpse.bound_min), render_cast(corpse.bound_max));
                            DirectX::XMFLOAT2 tip{};
                            if (!project_icon_tip(camera, view_data, top, static_cast<float>(w), static_cast<float>(h), tip))
                                continue;

                            float const alpha = pulse * corpse_alpha(cfg, corpse.distance, color.a());
                            candidates.emplace_back(corpse.form_id, render_cast(corpse.anchor), tip, corpse.distance, alpha);
                        }
                        std::vector<Icon::IconMarker> const markers = Icon::icon_marker(candidates, static_cast<float>(cfg.icon_radius), cfg.max_distance, Max_Corpse_Count);

                        std::vector<Icon::IconVertex> vertices;
                        for (Icon::IconMarker const& marker : std::views::reverse(markers))
                        {
                            Icon::IconGeometry const geometry = Icon::icon_geometry(marker, { color.r(), color.g(), color.b() }, static_cast<float>(w), static_cast<float>(h));
                            if (vertices.size() + geometry.count <= Icon::Icon_Max_Vertex_Count)
                                vertices.insert(vertices.end(), geometry.vertices.begin(), geometry.vertices.begin() + static_cast<int64_t>(geometry.count));
                        }
                        m_icon_overlay.draw(context, m_render_target, vertices, *m_states);
                        m_icon_overlay.end_frame();

                    }
                    else
                    {
                        if (!m_mask_overlay.begin_frame(device, w, h))
                        {
                            m_mask_overlay.end_frame();
                            return;
                        }

                        // One outer element per drawn corpse. Each snapshot corpse is culled on
                        // anchor/radius (a culled corpse never touches the cache - recency tracks
                        // drawn corpses), then resolved through the render-thread form-id LRU
                        // cache: a hit reuses the cached geometries, a miss collects them this
                        // frame via collect_render_geometries (no scan-interval delay after a
                        // camera turn); empty collections are not cached, so an unresolvable
                        // corpse retries next frame. target_index is rewritten on a LOCAL copy -
                        // the cache is never mutated - and the copy moves into the draw list,
                        // keeping the nested-vector handoff to MaskOverlay::draw unchanged.
                        //
                        // Invalidation: the equip-invalidation inbox (the only cross-thread
                        // structure) is drained at frame head and drained corpse ids are erased
                        // from the cache; on every cache hit the corpse's current 3D root is
                        // compared against the root captured at collection time, so a rebuilt 3D
                        // (save load, memory purge, base swap) or gear change invalidates the
                        // entry immediately and the corpse re-collects on the miss path. An
                        // unresolved ref keeps the entry - the corpse has left the snapshot
                        // anyway.
                        for (RE::FormID const invalidated : drain_invalidations())
                            m_geometry_cache.erase(invalidated);

                        std::vector<DirectX::XMFLOAT4> colors;
                        colors.reserve(corpses.size());
                        std::vector<std::vector<RenderGeometry>> render_geometries;
                        render_geometries.reserve(corpses.size());

                        uint32_t visible_index = 0;
                        size_t total_render_geometries = 0;
                        for (CorpseScan::CorpseInfo const& corpse : corpses)
                        {
                            if (visible_index >= Max_Corpse_Count)
                            {
                                logger::warn("Mask overlay: corpse cap {} reached, extra targets not drawn", Max_Corpse_Count);
                                break;
                            }
                            if (camera && !camera->PointInFrustum(corpse.anchor, corpse.radius))
                                continue;

                            // The ref resolves once per corpse so the hit path can validate the
                            // cached root; the miss path needed the ref anyway.
                            RE::TESForm* form = RE::TESForm::LookupByID(corpse.form_id);
                            RE::TESObjectREFR const* ref = form ? form->AsReference() : nullptr;
                            RE::NiAVObject* const current_root = ref ? ref->GetCurrent3D() : nullptr;

                            std::vector<RenderGeometry> corpse_geometries;
                            RenderGeometryCache::Hit const cached = m_geometry_cache.lookup(corpse.form_id);
                            if (cached.geometries)
                            {
                                // Hit: validate the cached entry's 3D root against the corpse's
                                // current one. A mismatch means the 3D was rebuilt (gear change
                                // without an event, save load, memory purge, base swap) - drop
                                // the entry and take the miss path so this frame re-collects with
                                // the fresh root. An unresolved ref keeps the entry: the corpse
                                // has left the snapshot anyway, so drawing it this frame is
                                // harmless (same behavior as before invalidation existed).
                                if (ref && current_root != cached.root3d)
                                    m_geometry_cache.erase(corpse.form_id);
                                else
                                    corpse_geometries = *cached.geometries;
                            }

                            // Miss path (a fresh miss, or the hit above was just invalidated):
                            // collect now and cache with the root captured this frame. Empty
                            // collections are not cached (insert rejects them), so an
                            // unresolvable corpse retries next frame.
                            if (corpse_geometries.empty())
                            {
                                if (ref)
                                    collect_render_geometries(ref, corpse_geometries);
                                m_geometry_cache.insert(corpse.form_id, current_root, std::move(corpse_geometries));
                            }
                            if (corpse_geometries.empty())
                                continue;

                            if (total_render_geometries + corpse_geometries.size() > Max_Render_Geometries_Per_Frame)
                            {
                                logger::warn("Mask overlay: render-geometry cap {} reached, extra geometry dropped", Max_Render_Geometries_Per_Frame);
                                break;
                            }
                            total_render_geometries += corpse_geometries.size();

                            for (RenderGeometry& draw : corpse_geometries)
                                draw.target_index = visible_index;
                            colors.emplace_back(color.r(), color.g(), color.b(), pulse * corpse_alpha(cfg, corpse.distance, color.a()));
                            render_geometries.push_back(std::move(corpse_geometries));
                            ++visible_index;
                        }

                        if (!render_geometries.empty())
                            m_mask_overlay.draw(device, context, camera, m_render_target, desc.width, desc.height, colors, render_geometries, *m_states);
                        m_mask_overlay.end_frame();
                    }
                }
            }
        }

        bool init(REX::W32::IDXGISwapChain* swap_chain, REX::W32::ID3D11Device* device)
        {
            if (m_ready)
                return true;

            // All HLSL passes are compiled once, before any overlay picks them up.
            if (!ShaderManager::instance().compile())
                return false;

            // CommonStates is the local REX::W32-typed mirror; it takes the REX device pointer directly.
            m_states = std::make_unique<CommonStates>(device);
            if (!m_states || !m_states->valid() || !m_icon_overlay.init(device) || !m_mask_overlay.init(device))
                return false;

            // The REX mirror's GetBuffer takes the REX IID constant directly (same GUID value as __uuidof).
            REX::W32::ID3D11Texture2D* buffer = nullptr;
            REX::W32::HRESULT const hr = swap_chain->GetBuffer(0, REX::W32::IID_ID3D11Texture2D, reinterpret_cast<void**>(&buffer));
            if (!REX::W32::SUCCESS(hr) || !buffer)
                return false;

            if (buffer == m_back_buffer)
            {
                buffer->Release();
                m_ready = true;
                return false;
            }

            if (m_render_target)
            {
                m_render_target->Release();
                m_render_target = nullptr;
            }
            if (m_back_buffer)
            {
                m_back_buffer->Release();
                m_back_buffer = nullptr;
            }

            m_back_buffer = buffer;
            REX::W32::HRESULT const rtv_hr = device->CreateRenderTargetView(m_back_buffer, nullptr, &m_render_target);
            if (!REX::W32::SUCCESS(rtv_hr) || !m_render_target)
            {
                logger::error("Failed to create backbuffer RTV: {:X}", static_cast<unsigned int>(rtv_hr));
                return false;
            }

            m_ready = true;
            return m_ready;
        }

        bool update_back_buffer(REX::W32::IDXGISwapChain* swap_chain, REX::W32::ID3D11Device* device)
        {
            REX::W32::ID3D11Texture2D* buffer = nullptr;
            REX::W32::HRESULT const hr = swap_chain->GetBuffer(0, REX::W32::IID_ID3D11Texture2D, reinterpret_cast<void**>(&buffer));
            if (!REX::W32::SUCCESS(hr) || !buffer)
                return false;

            if (buffer == m_back_buffer)
            {
                buffer->Release();
                return true;
            }

            if (m_render_target)
            {
                m_render_target->Release();
                m_render_target = nullptr;
            }
            if (m_back_buffer)
            {
                m_back_buffer->Release();
                m_back_buffer = nullptr;
            }

            m_back_buffer = buffer;

            REX::W32::HRESULT const rtv_hr = device->CreateRenderTargetView(m_back_buffer, nullptr, &m_render_target);
            if (!REX::W32::SUCCESS(rtv_hr) || !m_render_target)
            {
                logger::error("Failed to create backbuffer RTV: {:X}", static_cast<unsigned int>(rtv_hr));
                return false;
            }
            return true;
        }

    private:
        std::chrono::steady_clock::time_point m_last_scan;
        std::atomic<bool> m_scan_in_flight;

        uint32_t m_last_drawn_frame;
        std::mutex m_draw_mutex;

        REX::W32::ID3D11Texture2D* m_back_buffer;
        REX::W32::ID3D11RenderTargetView* m_render_target;

        std::unique_ptr<CommonStates> m_states;
        Icon::IconOverlay m_icon_overlay;
        Mask::MaskOverlay m_mask_overlay;

        // Render-thread-only: accessed exclusively from draw() on the Present thread (no mutex);
        // the scan task never collects geometry, so it never touches the cache.
        RenderGeometryCache m_geometry_cache;

        // Equip-invalidation inbox: the ONLY cross-thread state in the cache design. The
        // main-thread equip sink pushes affected corpse form ids, the render thread drains it at
        // the head of the mask branch each frame; both sides only ever touch it under
        // m_invalidations_mutex. Capacity-capped by Max_Equip_Inbox (posts are dropped when
        // full - a dropped invalidation self-heals via the root check or eviction).
        std::mutex m_invalidations_mutex;
        std::vector<RE::FormID> m_invalidations;

        bool m_ready;
    };

    void present_callback(REX::W32::IDXGISwapChain* swap_chain)
    {
        OverlayDirector::instance().on_present(swap_chain);
    }

    // Main-thread equip sink: whenever an equippable item is equipped on or taken off an actor,
    // queue that actor's form id for cache invalidation on the render thread. The sink performs
    // NO cache access (the cache is render-thread-only) - it only pushes into the OverlayDirector
    // inbox, the single shared structure, and stays silent per event.
    //
    // baseObject is filtered by form type: spells, abilities and shouts fire TESEquipEvent too
    // but have no mesh, so only Armor/Weapon/Light/Ammo pass. Dedup is unnecessary - the render
    // side erases per drained id and erase of a missing key is a no-op.
    class EquipHandler final : public RE::BSTEventSink<RE::TESEquipEvent>
    {
    public:
        static EquipHandler* instance()
        {
            static EquipHandler s_instance;
            return &s_instance;
        }

        RE::BSEventNotifyControl ProcessEvent(
            RE::TESEquipEvent const* event,
            [[maybe_unused]] RE::BSTEventSource<RE::TESEquipEvent>* source) override
        {
            if (event && event->actor)
            {
                RE::TESForm const* const base = RE::TESForm::LookupByID(event->baseObject);
                if (base)
                {
                    RE::FormType const type = base->GetFormType();
                    if (type == RE::FormType::Armor || type == RE::FormType::Weapon ||
                        type == RE::FormType::Light || type == RE::FormType::Ammo)
                    {
                        OverlayDirector::instance().queue_invalidation(event->actor->GetFormID());
                    }
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void Renderer::install()
{
    // Equip invalidation must exist before gameplay: registering here (kPostLoadGame) keeps the
    // whole render lifecycle in one place and events before the first save load are irrelevant
    // (no corpses to invalidate yet). AddEventSink deduplicates identical sinks, so the repeated
    // kPostLoadGame message on every load is harmless.
    RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(EquipHandler::instance());
    logger::info("Registered equip-event sink for render-geometry cache invalidation"sv);

    (void)PresentHook::instance().install(&present_callback);
}

PLUGIN_NAMESPACE_END
